// FLvibed controller host - the DualSense grips' stream (hapticwave.h). The pad's coils are two channels of a four-channel USB audio device: the render endpoint whose ContainerId
// is the pad's. WASAPI shared and event driven, a session of its own, the mix format read at open and refused when it has no back pair (wavesynth::WaveFormatRefusal). What
// is new to the DLL is looked up at run time, so it still loads on Windows 7. Unproven on any pad: the owner's card, P5 (docs\research\pad-native-features.md).
#include "hapticwave.h"

#include "log.h"
#include "sdlpad.h"
#include "../wave_synth.h"

namespace hapticwave
{
static_assert(sizeof(wavesynth::WaveFrame) == 12 * 4, "the logic and the host share this block");

static SRWLOCK g_lock = SRWLOCK_INIT; // guards g_frame, g_frameAt, g_clicks and g_why
static wavesynth::WaveFrame g_frame = {};
static DWORD g_frameAt = 0;
static char g_why[128] = {};
static HANDLE g_thread = nullptr;
static volatile LONG g_state = kOff, g_stop = 0;
static char g_path[260] = {}; // the pad's device path, read at the open
static bool g_byName = false;  // debug: find the speaker by name even where a ContainerId would (Wine's path, tested on Windows)
constexpr DWORD kFrameLeaseMs = 200;

static void Fail(const char* why)
{
    AcquireSRWLockExclusive(&g_lock);
    snprintf(g_why, sizeof(g_why), "%s", why);
    ReleaseSRWLockExclusive(&g_lock);
    InterlockedExchange(&g_state, kFailed);
    Log("haptics stream: %s - the motors take over", why);
}

// ---- the pad's ContainerId, through the device tree: cfgmgr32's property calls are looked up, Vista and later
using LocateFn = DWORD(WINAPI*)(DWORD*, const wchar_t*, ULONG);
using PropertyFn = DWORD(WINAPI*)(DWORD, const void*, ULONG*, BYTE*, ULONG*, ULONG);
struct DevProperty
{
    GUID fmtid;
    ULONG pid;
};
static const DevProperty kContainerId = {{0x8C7ED206, 0x3F8A, 0x4827, {0xB3, 0xAB, 0xAE, 0x9E, 0x1F, 0xAE, 0xFC, 0x6C}}, 2}; // DEVPKEY_Device_ContainerId

static bool PadContainer(const char* path, GUID& out)
{
    char instance[260];
    if (!wavesynth::InstanceFromPath(path, instance, sizeof(instance)))
        return false;
    wchar_t wide[260];
    if (!MultiByteToWideChar(CP_UTF8, 0, instance, -1, wide, 260))
        return false;
    const HMODULE cm = LoadLibraryA("cfgmgr32.dll");
    const LocateFn locate = cm ? reinterpret_cast<LocateFn>(GetProcAddress(cm, "CM_Locate_DevNodeW")) : nullptr;
    const PropertyFn property = cm ? reinterpret_cast<PropertyFn>(GetProcAddress(cm, "CM_Get_DevNode_PropertyW")) : nullptr;
    DWORD node = 0;
    if (!locate || !property || locate(&node, wide, 0) != 0)
        return false;
    ULONG type = 0, size = sizeof(GUID);
    return property(node, &kContainerId, &type, reinterpret_cast<BYTE*>(&out), &size, 0) == 0 && size == sizeof(GUID);
}

// The render endpoint in that container. The caller releases it.
static IMMDevice* FindEndpoint(const GUID& container)
{
    static const PROPERTYKEY kEndpointContainer = {{0x8C7ED206, 0x3F8A, 0x4827, {0xB3, 0xAB, 0xAE, 0x9E, 0x1F, 0xAE, 0xFC, 0x6C}}, 2}; // PKEY_Device_ContainerId
    using ClearFn = HRESULT(WINAPI*)(PROPVARIANT*);
    static const ClearFn clear = reinterpret_cast<ClearFn>(GetProcAddress(GetModuleHandleA("ole32.dll"), "PropVariantClear"));
    IMMDeviceEnumerator* en = nullptr;
    IMMDeviceCollection* all = nullptr;
    IMMDevice* found = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&en))) &&
        SUCCEEDED(en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &all)))
    {
        UINT n = 0;
        all->GetCount(&n);
        for (UINT i = 0; i < n && !found; ++i)
        {
            IMMDevice* dev = nullptr;
            IPropertyStore* store = nullptr;
            if (FAILED(all->Item(i, &dev)))
                continue;
            if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &store)))
            {
                PROPVARIANT v;
                PropVariantInit(&v);
                if (SUCCEEDED(store->GetValue(kEndpointContainer, &v)) && v.vt == VT_CLSID && v.puuid && IsEqualGUID(*v.puuid, container))
                    found = dev, dev = nullptr;
                if (clear)
                    clear(&v);
                store->Release();
            }
            if (dev)
                dev->Release();
        }
    }
    if (all)
        all->Release();
    if (en)
        en->Release();
    return found;
}

// The render endpoint by its name, where no ContainerId says which it is (Wine's device tree has none: the owner, 7 Nov 2026, feature parity): the one active
// endpoint whose friendly name holds the pad's name or "DualSense" (wavesynth::EndpointNameMatches). Two such endpoints is no answer: two pads, which is which.
// `count` says how many matched. The caller releases it.
static IMMDevice* FindEndpointByName(const char* padName, int& count)
{
    static const PROPERTYKEY kFriendlyName = {{0xA45C254E, 0xDF1C, 0x4EFD, {0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0}}, 14}; // PKEY_Device_FriendlyName
    using ClearFn = HRESULT(WINAPI*)(PROPVARIANT*);
    static const ClearFn clear = reinterpret_cast<ClearFn>(GetProcAddress(GetModuleHandleA("ole32.dll"), "PropVariantClear"));
    IMMDeviceEnumerator* en = nullptr;
    IMMDeviceCollection* all = nullptr;
    IMMDevice* found = nullptr;
    count = 0;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&en))) &&
        SUCCEEDED(en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &all)))
    {
        UINT n = 0;
        all->GetCount(&n);
        for (UINT i = 0; i < n; ++i)
        {
            IMMDevice* dev = nullptr;
            IPropertyStore* store = nullptr;
            if (FAILED(all->Item(i, &dev)))
                continue;
            if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &store)))
            {
                PROPVARIANT v;
                PropVariantInit(&v);
                if (SUCCEEDED(store->GetValue(kFriendlyName, &v)) && v.vt == VT_LPWSTR && wavesynth::EndpointNameMatches(v.pwszVal, padName))
                {
                    char name[160] = {};
                    WideCharToMultiByte(CP_UTF8, 0, v.pwszVal, -1, name, sizeof(name) - 1, nullptr, nullptr);
                    Log("haptics stream: the pad's speaker by name: '%s'", name);
                    if (++count == 1)
                        found = dev, dev = nullptr;
                }
                if (clear)
                    clear(&v);
                store->Release();
            }
            if (dev)
                dev->Release();
        }
    }
    if (all)
        all->Release();
    if (en)
        en->Release();
    if (count != 1 && found)
        found->Release(), found = nullptr;
    return found;
}

// The buffer: the back pair takes the grips, every other channel silence.
static void Fill(BYTE* data, int frames, int channels, int back, bool isFloat, const float* lr)
{
    if (isFloat)
    {
        float* out = reinterpret_cast<float*>(data);
        for (int i = 0; i < frames; ++i, out += channels)
        {
            memset(out, 0, sizeof(float) * channels);
            out[back] = lr[i * 2], out[back + 1] = lr[i * 2 + 1];
        }
        return;
    }
    short* out = reinterpret_cast<short*>(data);
    for (int i = 0; i < frames; ++i, out += channels)
    {
        memset(out, 0, sizeof(short) * channels);
        out[back] = static_cast<short>(lr[i * 2] * 32767), out[back + 1] = static_cast<short>(lr[i * 2 + 1] * 32767);
    }
}

static void Stream(IAudioClient* client, IAudioRenderClient* render, HANDLE ready, UINT32 size, int channels, int back, bool isFloat)
{
    wavesynth::Synth synth;
    std::vector<float> lr(static_cast<size_t>(size) * 2);
    if (FAILED(client->Start()))
    {
        Fail("the stream did not start");
        return;
    }
    InterlockedExchange(&g_state, kRunning);
    Log("haptics stream: running, %u frames a buffer, %d channels, the grips on %d and %d", size, channels, back, back + 1);
    while (!g_stop)
    {
        WaitForSingleObject(ready, 100);
        wavesynth::WaveFrame f;
        AcquireSRWLockExclusive(&g_lock);
        f = g_frame;
        g_frame.clicks[0] = g_frame.clicks[1] = 0; // a burst plays once
        const DWORD at = g_frameAt;
        ReleaseSRWLockExclusive(&g_lock);
        if (GetTickCount() - at >= kFrameLeaseMs) // the logic stopped asking: silence, whatever it last said
            f = wavesynth::WaveFrame{};
        synth.Set(f);
        UINT32 padding = 0;
        if (FAILED(client->GetCurrentPadding(&padding)))
        {
            Fail("the device went away");
            break;
        }
        const UINT32 n = size - padding;
        if (!n)
            continue;
        BYTE* data = nullptr;
        if (FAILED(render->GetBuffer(n, &data)))
        {
            Fail("the device stopped taking data");
            break;
        }
        synth.Render(lr.data(), static_cast<int>(n));
        Fill(data, static_cast<int>(n), channels, back, isFloat, lr.data());
        render->ReleaseBuffer(n, 0);
    }
    client->Stop();
}

static DWORD WINAPI Thread(void*)
{
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    GUID container = {};
    IMMDevice* dev = nullptr;
    IAudioClient* client = nullptr;
    IAudioRenderClient* render = nullptr;
    WAVEFORMATEX* format = nullptr;
    HANDLE ready = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    // a session of the stream's own: muting the game's session must not reach it, and this one must not reach the game's
    static const GUID kSession = {0x6F4C2A51, 0x8E1D, 0x4B7C, {0x9D, 0x31, 0x5A, 0x0E, 0x72, 0xC4, 0x18, 0xB6}};
    // the pad's speaker: by the ContainerId Windows gives it, else by its name (Wine; FLVIBED_WAVE_BYNAME=1 in the game's environment forces this path on Windows to test it)
    char forced[8] = {};
    g_byName = GetEnvironmentVariableA("FLVIBED_WAVE_BYNAME", forced, sizeof(forced)) > 0 && forced[0] == '1';
    if (!g_byName && PadContainer(g_path, container))
        dev = FindEndpoint(container);
    int named = 0;
    if (!dev)
    {
        char padName[128] = {};
        sdlpad::Name(padName, sizeof(padName));
        dev = FindEndpointByName(padName, named);
    }
    if (!dev)
        Fail(named > 1 ? "two pads' speakers match by name: which is this one's is not known"
                       : "the pad has no speaker endpoint: is it enabled (Windows: the Sound panel; Linux: the card's profile Surround 4.0)?");
    else if (FAILED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client))) || FAILED(client->GetMixFormat(&format)))
        Fail("the pad's speaker did not open");
    else
    {
        unsigned mask = 0;
        bool isFloat = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT, known = isFloat || (format->wFormatTag == WAVE_FORMAT_PCM && format->wBitsPerSample == 16);
        if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            const WAVEFORMATEXTENSIBLE* x = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
            mask = x->dwChannelMask;
            isFloat = x->SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT;
            known = isFloat ? format->wBitsPerSample == 32 : x->SubFormat.Data1 == WAVE_FORMAT_PCM && format->wBitsPerSample == 16;
        }
        const char* refusal = wavesynth::WaveFormatRefusal(format->nChannels, static_cast<int>(format->nSamplesPerSec), mask);
        const int back = wavesynth::BackChannel(mask);
        UINT32 size = 0;
        if (refusal)
            Fail(refusal);
        else if (!known || back < 0 || back + 1 >= format->nChannels)
            Fail("the pad's speaker has a sample format the stream does not write");
        else if (FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 200000, 0, format, &kSession)) ||
                 FAILED(client->SetEventHandle(ready)) || FAILED(client->GetBufferSize(&size)) ||
                 FAILED(client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render))))
            Fail("the pad's speaker refused the stream");
        else
            Stream(client, render, ready, size, format->nChannels, back, isFloat);
    }
    using FreeFn = void(WINAPI*)(void*);
    static const FreeFn free = reinterpret_cast<FreeFn>(GetProcAddress(GetModuleHandleA("ole32.dll"), "CoTaskMemFree"));
    if (format && free)
        free(format);
    if (render)
        render->Release();
    if (client)
        client->Release();
    if (dev)
        dev->Release();
    if (ready)
        CloseHandle(ready);
    if (SUCCEEDED(com))
        CoUninitialize();
    return 0;
}

int Open(int open)
{
    if (open)
    {
        if (!g_thread && g_state == kOff)
        {
            if (!sdlpad::Path(g_path, sizeof(g_path)))
            {
                Fail("no pad path");
                return g_state;
            }
            InterlockedExchange(&g_stop, 0);
            InterlockedExchange(&g_state, kOpening);
            g_thread = CreateThread(nullptr, 0, Thread, nullptr, 0, nullptr);
            if (!g_thread)
                Fail("no thread");
        }
    }
    else
    {
        if (g_thread)
        {
            InterlockedExchange(&g_stop, 1);
            if (WaitForSingleObject(g_thread, 2000) == WAIT_OBJECT_0)
                CloseHandle(g_thread);
            else
                Log("haptics stream: the thread did not stop within 2 s");
            g_thread = nullptr;
        }
        InterlockedExchange(&g_state, kOff);
        AcquireSRWLockExclusive(&g_lock);
        g_why[0] = 0;
        ReleaseSRWLockExclusive(&g_lock);
    }
    return g_state;
}

void Feed(const void* frame)
{
    const wavesynth::WaveFrame* f = static_cast<const wavesynth::WaveFrame*>(frame);
    AcquireSRWLockExclusive(&g_lock);
    const int c0 = g_frame.clicks[0], c1 = g_frame.clicks[1]; // (a frame between two buffers keeps its bursts)
    g_frame = *f;
    g_frame.clicks[0] += c0, g_frame.clicks[1] += c1;
    g_frameAt = GetTickCount();
    ReleaseSRWLockExclusive(&g_lock);
}

void Why(char* out, int len)
{
    AcquireSRWLockExclusive(&g_lock);
    snprintf(out, len, "%s", g_why);
    ReleaseSRWLockExclusive(&g_lock);
}
} // namespace hapticwave
