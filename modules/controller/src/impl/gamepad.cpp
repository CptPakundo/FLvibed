// Gamepad reader: XInput, the SDL backend and the virtual test pads into one pad state, plus hold times and rumble.
#include "gamepad.h"

#include "host.h"
#include "log.h"
#include "game/fl.h"
#include "../pad_backend.h"
#include "../shm.h"

typedef DWORD(WINAPI* XInputGetStateFn)(DWORD, XINPUT_STATE*);
typedef DWORD(WINAPI* XInputSetStateFn)(DWORD, XINPUT_VIBRATION*);

static XInputGetStateFn g_getState = nullptr;
static XInputSetStateFn g_setState = nullptr;

void Gamepad::Init()
{
    // A test game does not take the machine's own pad: the runs stay the same with one plugged in. Debug `padfallback hid` turns it on.
    // A Steam Deck: Steam Input is its native route, whatever else is plugged in. Community-reported, no Valve document (glyphs.cpp); with no Sony or Nintendo pad in the HID list nothing starts anyway.
    if (EnvVar("FLVIBED_BACKGROUND") == "1" || EnvVar("SteamDeck") == "1")
        candidateForced_ = 0;
    // Windows 7 has no xinput1_4 and xinput1_3 only with the DirectX runtime; xinput9_1_0 ships with every Windows
    const char* dlls[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
    for (const char* name : dlls)
    {
        if (SimWin7() && strcmp(name, "xinput9_1_0.dll") != 0)
            continue;
        HMODULE mod = LoadLibraryA(name);
        if (!mod)
            continue;
        g_getState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(mod, "XInputGetState"));
        g_setState = reinterpret_cast<XInputSetStateFn>(GetProcAddress(mod, "XInputSetState"));
        if (g_getState)
        {
            Log("XInput: using %s%s", name, SimWin7() ? " (FLVIBED_SIM_WIN7)" : "");
            return;
        }
    }
    Log("XInput: not available, only the virtual pad will work");
}

// An Xbox pad in the HID list (padbackend::XboxHid), for its trigger motors. `last`: the answer when the list cannot be read.
static bool HidListHasXbox(bool last)
{
    UINT n = 0;
    if (GetRawInputDeviceList(nullptr, &n, sizeof(RAWINPUTDEVICELIST)) != 0)
        return last;
    std::vector<RAWINPUTDEVICELIST> list(n);
    n = n ? GetRawInputDeviceList(list.data(), &n, sizeof(RAWINPUTDEVICELIST)) : 0;
    if (n == static_cast<UINT>(-1))
        return last;
    for (UINT i = 0; i < n && i < list.size(); ++i)
    {
        if (list[i].dwType != RIM_TYPEHID)
            continue;
        RID_DEVICE_INFO info = {};
        info.cbSize = sizeof(info);
        UINT size = sizeof(info);
        if (GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICEINFO, &info, &size) == static_cast<UINT>(-1))
            continue;
        wchar_t path[512] = {};
        UINT chars = 512;
        if (GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICENAME, path, &chars) != static_cast<UINT>(-1) &&
            padbackend::XboxHid(info.hid.dwVendorId, info.hid.usUsagePage, info.hid.usUsage, wcsstr(path, L"IG_") != nullptr))
            return true;
    }
    return false;
}

// Wine and Proton: ntdll says so. Trigger rumble through Wine's WGI could cost the plain rumble (docs\research\pad-native-features.md 2).
static bool OnWine()
{
    static const bool wine = GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != nullptr;
    return wine;
}

// A PlayStation or Nintendo pad in the HID list (padbackend::HidCandidate). RawInput: XInput hides the vendor, and this costs no device access.
static bool HidListHasCandidate(bool last)
{
    UINT n = 0;
    if (GetRawInputDeviceList(nullptr, &n, sizeof(RAWINPUTDEVICELIST)) != 0)
        return last;
    if (n == 0)
        return false;
    std::vector<RAWINPUTDEVICELIST> list(n);
    n = GetRawInputDeviceList(list.data(), &n, sizeof(RAWINPUTDEVICELIST));
    if (n == static_cast<UINT>(-1)) // (a device arrived between the two calls: the next look reads it)
        return last;
    for (UINT i = 0; i < n && i < list.size(); ++i)
    {
        if (list[i].dwType != RIM_TYPEHID)
            continue;
        RID_DEVICE_INFO info = {};
        info.cbSize = sizeof(info);
        UINT size = sizeof(info);
        if (GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICEINFO, &info, &size) == static_cast<UINT>(-1))
            continue;
        if (!padbackend::HidCandidate(info.hid.dwVendorId, info.hid.usUsagePage, info.hid.usUsage, false))
            continue;
        wchar_t path[512] = {};
        UINT chars = 512;
        const bool named = GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICENAME, path, &chars) != static_cast<UINT>(-1);
        if (!padbackend::HidCandidate(info.hid.dwVendorId, info.hid.usUsagePage, info.hid.usUsage, named && wcsstr(path, L"IG_")))
            continue;
        return true;
    }
    return false;
}

bool Gamepad::Candidate(float dt)
{
    if (candidateForced_ >= 0)
        return candidateForced_ != 0;
    hidTimer_ -= dt;
    if (hidTimer_ <= 0)
    {
        hidTimer_ = padbackend::kHidRescan;
        candidate_ = HidListHasCandidate(candidate_);
    }
    return candidate_;
}

// ---------------------------------------------------------------- the device layer: the system's XInput or the test pads
// The device-layer test pads (shm.h v2, IDEAS T36): while a test tool sets devActive, XInputGetState / SetState are
// answered from shared memory instead of the system's XInput, so the test pad goes through everything a real pad does.
static ShmBlock* DevBlock()
{
    ShmBlock* s = shm::Get();
    return s && s->version >= 2 && s->devActive ? s : nullptr;
}

bool Gamepad::DevActive() { return DevBlock() != nullptr; }

static DWORD GetState(DWORD slot, XINPUT_STATE* out)
{
    if (ShmBlock* s = DevBlock())
    {
        const ShmBlock::DevSlot& d = s->devSlot[slot & 3];
        if (!d.connected)
            return ERROR_DEVICE_NOT_CONNECTED;
        out->dwPacketNumber = d.packet;
        XINPUT_GAMEPAD& g = out->Gamepad;
        g.wButtons = static_cast<WORD>(d.buttons & 0xFFFF);
        g.bLeftTrigger = static_cast<BYTE>(d.triggers & 0xFF);
        g.bRightTrigger = static_cast<BYTE>((d.triggers >> 8) & 0xFF);
        g.sThumbLX = static_cast<SHORT>(Clampf(static_cast<float>(d.lx), -32768, 32767));
        g.sThumbLY = static_cast<SHORT>(Clampf(static_cast<float>(d.ly), -32768, 32767));
        g.sThumbRX = static_cast<SHORT>(Clampf(static_cast<float>(d.rx), -32768, 32767));
        g.sThumbRY = static_cast<SHORT>(Clampf(static_cast<float>(d.ry), -32768, 32767));
        return ERROR_SUCCESS;
    }
    return g_getState ? g_getState(slot, out) : ERROR_DEVICE_NOT_CONNECTED;
}

static void SetState(DWORD slot, XINPUT_VIBRATION* v)
{
    if (ShmBlock* s = DevBlock())
    {
        // the rumble command stream the tests read back (Get-FLDevRumble)
        const u32 n = ++s->devRumbleSeq;
        ShmBlock::DevRumble& r = s->devRumble[(n - 1) % 16];
        r.slot = slot, r.low = v->wLeftMotorSpeed, r.high = v->wRightMotorSpeed, r.frame = s->heartbeat;
        r.seq = n; // last: a reader that sees the new seq sees the whole entry
        return;
    }
    if (g_setState)
        g_setState(slot, v);
}

// ---------------------------------------------------------------- the SDL3 backend (pad_backend 1, WORK_QUEUE Q5 slice 1)
// SDL runs in the host on its own thread (modules\controller\src\host\sdlpad.cpp, HostApi v33); this side keeps it running while
// pad_backend is 1, reads its newest state into the XInput form every reader below uses (the focus gate, the virtual
// pad's merge, the dead zones, the hold times: unchanged), and logs its changes. XInput reads the pad while SDL starts,
// after it failed, with an older host and while the device-layer test pads are on (pad_backend.h ReadFromSdl).
bool Gamepad::ReadSdl(XINPUT_STATE& out, bool& ok, bool fallback)
{
    const bool hostHas = HostAtLeast(padbackend::kHostVersion) && g_host->SdlPad;
    if (!padbackend::WantSdl(padBackend, hostHas, fallback))
    {
        if (sdlOn_)
        {
            if (hostHas)
                g_host->SdlPad(0, nullptr);
            Log("pad backend: SDL3 stopped (pad_backend %d) - XInput reads the pad", padBackend);
        }
        if (padBackend == 1 && !hostHas && !sdlWarned_)
            Log("pad backend: pad_backend 1 needs host API v%u (this host: v%u) - XInput reads the pad",
                padbackend::kHostVersion, g_host ? g_host->version : 0);
        sdlWarned_ = padBackend == 1 && !hostHas;
        sdlOn_ = sdlSource_ = false;
        sdlStatusWas_ = sdlConnectedWas_ = 0;
        sdlVendor_ = 0;
        return false;
    }
    if (!sdlOn_)
        Log("pad backend: SDL3 starting (%s; SDL_Init runs on the host's own thread)",
            fallback ? "no XInput pad, a PlayStation or Nintendo pad in the HID list" : "pad_backend 1");
    sdlOn_ = true;
    const int status = g_host->SdlPad(fallback ? 2 : 1, &sdl_);
    if (status != sdlStatusWas_)
    {
        if (status == padbackend::kSdlRunning)
            Log("pad backend: SDL3 running (init %.1f ms, %d gamepad(s))", sdl_.initMs, sdl_.count);
        else if (status == padbackend::kSdlFailed)
            Log("pad backend: SDL3 failed (%s) - XInput reads the pad; pad_backend 0 then 1 retries", sdl_.error);
        sdlStatusWas_ = status;
    }
    const bool was = sdlSource_;
    sdlSource_ = padbackend::ReadFromSdl(padBackend, hostHas, DevActive(), status, fallback);
    if (sdlSource_ != was && !fallback)
        slots_ = padslots::Scan(); // the XInput slot scan starts fresh whenever XInput takes over again (the fallback's runs throughout)
    if (!sdlSource_)
    {
        sdlVendor_ = 0;
        return false;
    }
    if (sdl_.connected != sdlConnectedWas_)
    {
        if (sdl_.connected)
            Log("pad backend: SDL3 pad %s (type %d, %04X:%04X)", sdl_.name, sdl_.type, sdl_.vendor, sdl_.product);
        else
            Log("pad backend: SDL3 pad disconnected");
        sdlConnectedWas_ = sdl_.connected;
    }
    sdlVendor_ = sdl_.connected ? padbackend::VendorForType(sdl_.type, sdl_.vendor) : 0;
    ok = sdl_.connected != 0;
    if (ok)
    {
        out.dwPacketNumber = sdl_.seq;
        XINPUT_GAMEPAD& g = out.Gamepad;
        g.wButtons = static_cast<WORD>(sdl_.buttons & 0xFFFF);
        g.bLeftTrigger = static_cast<BYTE>(sdl_.lt);
        g.bRightTrigger = static_cast<BYTE>(sdl_.rt);
        g.sThumbLX = static_cast<SHORT>(sdl_.lx);
        g.sThumbLY = static_cast<SHORT>(sdl_.ly);
        g.sThumbRX = static_cast<SHORT>(sdl_.rx);
        g.sThumbRY = static_cast<SHORT>(sdl_.ry);
    }
    return true;
}

std::string Gamepad::BackendStatus() const
{
    if (padBackend == 0 && !sdlOn_) // (the fallback not started: no line)
        return {};
    char line[256];
    snprintf(line, sizeof(line), "pad_backend=%d source=%s sdl_status=%d sdl_init_ms=%.1f sdl_pads=%d sdl_connected=%d "
             "sdl_type=%d sdl_vendor=0x%04X sdl_product=0x%04X sdl_seq=%u dropped=%u sdl_name='%s'",
             padBackend, sdlSource_ ? "sdl" : "xinput", sdlStatusWas_, sdl_.initMs, sdl_.count, sdl_.connected, sdl_.type,
             sdl_.vendor, sdl_.product, sdl_.seq, sdl_.dropped, sdl_.name);
    return line;
}

void Gamepad::ReadReal(XINPUT_STATE& out, bool& ok, float dt)
{
    ok = false;
    if (padBackend == 1 && impulseOn_) // (one SDL, its hints set at its start: this one reads)
        Impulse(dt, false, true);
    if (padBackend == 1 && ReadSdl(out, ok, false))
        return;
    if (!g_getState && !DevActive())
        slots_ = padslots::Scan();
    else
    {
        padslots::Event ev, ev2;
        ok = slots_.Read([&out](int i) { return GetState(static_cast<DWORD>(i), &out) == ERROR_SUCCESS; }, dt, ev, ev2);
        for (padslots::Event e : {ev, ev2})
        {
            if (e == padslots::Event::Disconnected)
                Log("XInput: controller %d disconnected%s", slots_.Lost(), DevActive() ? " (test pad)" : "");
            else if (e == padslots::Event::Connected)
                Log("XInput: controller connected on slot %d%s", slots_.Index(), DevActive() ? " (test pad)" : "");
        }
    }
    if (padBackend == 1)
        return;
    if (!primary)
    {
        sdlOn_ = sdlSource_ = false; // (PadBackend followed the flight's reader to XInput: only that reader stops SDL)
        sdlStatusWas_ = sdlConnectedWas_ = 0;
        return;
    }
    // XInput read first, every frame it would have been: with no candidate in the HID list nothing below changes what is read
    const bool hostHas = HostAtLeast(padbackend::kHostVersion) && g_host->SdlPad;
    const bool want = fallback_.Want(sdlFallback, hostHas, sdlFallback && hostHas && Candidate(dt),
                                     sdlOn_ && sdlStatusWas_ == padbackend::kSdlRunning && !sdl_.connected, dt);
    if (!want && !sdlOn_)
    {
        handsOnSdl_ = xinputBeside_ = false;
        Filter(false);
        Impulse(dt, ok, false);
        return;
    }
    Impulse(dt, ok, true);
    XINPUT_STATE sdl = {};
    bool sdlOk = false;
    const bool answered = ReadSdl(sdl, sdlOk, want);
    bool onSdl = false;
    if (answered)
    {
        const auto input = [this](const XINPUT_GAMEPAD& g) {
            return padbackend::HasInput(g.wButtons, g.sThumbLX, g.sThumbLY, g.sThumbRX, g.sThumbRY, g.bLeftTrigger, g.bRightTrigger, stickDeadzone, triggerThreshold);
        };
        onSdl = hands_.OnSdl(sdlOk, sdlOk && input(sdl.Gamepad), ok, ok && input(out.Gamepad), dt);
    }
    if (onSdl != handsOnSdl_)
    {
        if (rumbling_) // the rumble stays with the hands: the pad they left stops
        {
            sdlSource_ = handsOnSdl_;
            Rumble(0, 0, 0);
        }
        Log("pad backend: the hands are on %s", onSdl ? "the pad SDL reads" : "XInput's pad");
    }
    handsOnSdl_ = sdlSource_ = onSdl;
    xinputBeside_ = answered && !onSdl && ok;
    Filter(answered && sdlOk && !IgnoreRealNow(true));
    if (onSdl)
        out = sdl, ok = true;
}

// An Xbox pad's four motors through SDL's WGI driver, XInput still reading (padbackend::ImpulseRoute). `fallback`: the fallback wants SDL, whose hints are set at its start,
// so this gives it way at once. A look every kImpulseLook, not a frame: the output needs no read.
void Gamepad::Impulse(float dt, bool xinputPad, bool fallback)
{
    const bool hostHas = HostAtLeast(41) && g_host->SdlPad && g_host->SdlPadTriggerRumble;
    static const bool background = EnvVar("FLVIBED_BACKGROUND") == "1"; // a test game leaves the machine's own pad alone
    if (!impulseOn_ && (fallback || !impulseWanted || !xinputPad)) // (no pad, no cost)
        return;
    impulseLook_ -= dt;
    if (impulseLook_ > 0 && !(fallback && impulseOn_))
        return;
    impulseLook_ = padbackend::kImpulseLook;
    hidTimerXbox_ -= padbackend::kImpulseLook;
    if (hidTimerXbox_ <= 0)
        hidTimerXbox_ = padbackend::kHidRescan, xboxSeen_ = HidListHasXbox(xboxSeen_);
    const bool up = impulseOn_ && sdl_.status == padbackend::kSdlRunning;
    const bool want = impulse_.Want(impulseWanted, hostHas, xinputPad && xboxSeen_, fallback, OnWine(), background || DevActive(), up && sdl_.connected, up,
                                    padbackend::kImpulseLook);
    if (!want)
    {
        if (impulseOn_)
        {
            if (impulseReady_)
            {
                g_host->SdlPadRumble(0, 0, 0);
                g_host->SdlPadTriggerRumble(0, 0, 0);
            }
            g_host->SdlPad(0, nullptr);
            sdl_ = {};
            Log("pad backend: SDL3's output for the Xbox pad stopped: XInput's two motors");
        }
        impulseOn_ = impulseReady_ = false;
        return;
    }
    if (!impulseOn_)
        Log("pad backend: SDL3 starting for the Xbox pad's trigger motors, XInput still reads it");
    impulseOn_ = true;
    const int status = g_host->SdlPad(padbackend::kSdlStartImpulse, &sdl_);
    const bool ready = status == padbackend::kSdlRunning && sdl_.connected && sdl_.count == 1;
    if (status == padbackend::kSdlFailed)
        impulse_.GiveUp();
    if (ready != impulseReady_)
        Log("pad backend: the Xbox pad's four motors %s", ready ? "through SDL's WGI driver" : "back on XInput");
    impulseReady_ = ready;
}

void Gamepad::RumbleTriggers(float left, float right, float seconds)
{
    if (!HostAtLeast(41) || !g_host->SdlPadTriggerRumble || (ignoreReal && (left > 0 || right > 0)))
        return;
    const u16 l = static_cast<u16>(Clampf(left, 0, 1) * 65535), r = static_cast<u16>(Clampf(right, 0, 1) * 65535);
    g_host->SdlPadTriggerRumble(l, r, (l || r) ? static_cast<u32>(Clampf(seconds, 0, 60) * 1000) + 100 : 0);
}

// A mapper's keys and mouse dropped by the host while SDL holds a pad and the game is in front (padbackend::DropInjected)
void Gamepad::Filter(bool on)
{
    if (on == filterOn_ || !HostAtLeast(38) || !g_host->SdlPadFilter)
        return;
    filterOn_ = on;
    g_host->SdlPadFilter(on, stickDeadzone, triggerThreshold);
}

u32 Gamepad::Vendor() const
{
    if (sdlSource_)
        return sdlVendor_;
    const ShmBlock* s = DevBlock();
    if (s && slots_.Index() >= 0)
        return s->devSlot[slots_.Index()].vendor;
    return xinputBeside_ ? padbackend::kVendorXbox : 0; // the HID list would name the Sony pad at rest beside it: the prompts follow the hands
}

static void Stick(i32 x, i32 y, float dz, float& ox, float& oy)
{
    float fx = Clampf(x / 32767.0f, -1, 1), fy = Clampf(y / 32767.0f, -1, 1);
    float mag = sqrtf(fx * fx + fy * fy);
    if (mag <= dz)
    {
        ox = oy = 0;
        return;
    }
    float scaled = (std::min(mag, 1.0f) - dz) / (1.0f - dz);
    ox = fx / mag * scaled;
    oy = fy / mag * scaled;
}

void Gamepad::Update(float dt)
{
    prev_ = cur_;

    XINPUT_STATE xs = {};
    bool ok = false;
    ReadReal(xs, ok, dt);
    realWas_ = realConnected_;
    realConnected_ = ok;

    i32 lx = 0, ly = 0, rx = 0, ry = 0;
    u32 buttons = 0;
    int lt = 0, rt = 0;
    {
        // the focus gate (focus_gate.h): the real pad's held inputs as bits, those held across the gate's opening
        // dropped until released
        constexpr u32 kStickL = 1u << 18, kStickR = 1u << 19;
        const XINPUT_GAMEPAD& g = xs.Gamepad;
        auto deflected = [this](i32 x, i32 y) {
            const float fx = x / 32767.0f, fy = y / 32767.0f;
            return fx * fx + fy * fy > stickDeadzone * stickDeadzone;
        };
        u32 down = 0;
        if (ok)
            down = g.wButtons | (g.bLeftTrigger / 255.0f > triggerThreshold ? PAD_LT : 0) |
                   (g.bRightTrigger / 255.0f > triggerThreshold ? PAD_RT : 0) |
                   (deflected(g.sThumbLX, g.sThumbLY) ? kStickL : 0) | (deflected(g.sThumbRX, g.sThumbRY) ? kStickR : 0);
        const u32 live = realHeld_.Filter(ignoreReal, down);
        buttons = g.wButtons & live & 0xFFFF;
        if (live & kStickL)
            lx = g.sThumbLX, ly = g.sThumbLY;
        if (live & kStickR)
            rx = g.sThumbRX, ry = g.sThumbRY;
        if (live & PAD_LT)
            lt = g.bLeftTrigger;
        if (live & PAD_RT)
            rt = g.bRightTrigger;
    }

    // Merge the scripted virtual pad (used for automated testing).
    bool virt = false;
    if (ShmBlock* s = shm::Get())
    {
        if (s->padActive)
        {
            virt = true;
            buttons |= s->padButtons & 0xFFFF;
            if (s->padLX || s->padLY)
            {
                lx = s->padLX;
                ly = s->padLY;
            }
            if (s->padRX || s->padRY)
            {
                rx = s->padRX;
                ry = s->padRY;
            }
            lt = std::max(lt, static_cast<int>(s->padTriggers & 0xFF));
            rt = std::max(rt, static_cast<int>((s->padTriggers >> 8) & 0xFF));
        }
    }

    cur_.connected = ok || virt;
    Stick(lx, ly, stickDeadzone, cur_.lx, cur_.ly);
    Stick(rx, ry, stickDeadzone, cur_.rx, cur_.ry);
    float flt = lt / 255.0f, frt = rt / 255.0f;
    cur_.lt = flt > triggerThreshold ? (flt - triggerThreshold) / (1 - triggerThreshold) : 0;
    cur_.rt = frt > triggerThreshold ? (frt - triggerThreshold) / (1 - triggerThreshold) : 0;
    if (cur_.lt > 0.25f)
        buttons |= PAD_LT;
    if (cur_.rt > 0.25f)
        buttons |= PAD_RT;
    cur_.buttons = buttons;

    for (int i = 0; i < 18; ++i)
        holdTime_[i] = (buttons & (1u << i)) ? holdTime_[i] + dt : 0;

    if (rumbling_)
    {
        rumbleTimer_ -= dt;
        if (rumbleTimer_ <= 0 || ignoreReal) // a pulse running when the game goes to the background stops at once
            Rumble(0, 0, 0);
    }
}

bool Gamepad::AnyInput() const
{
    return cur_.buttons || cur_.lx || cur_.ly || cur_.rx || cur_.ry || cur_.lt || cur_.rt;
}

static u32 SwapAB(u32 b)
{
    const u32 ab = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B;
    return (b & ~ab) | ((b & XINPUT_GAMEPAD_A) ? XINPUT_GAMEPAD_B : 0) | ((b & XINPUT_GAMEPAD_B) ? XINPUT_GAMEPAD_A : 0);
}

Gamepad Gamepad::SwappedAB() const
{
    static_assert(XINPUT_GAMEPAD_A == 1u << 12 && XINPUT_GAMEPAD_B == 1u << 13, "holdTime_ is indexed by the bit");
    Gamepad g = *this;
    g.cur_.buttons = SwapAB(cur_.buttons);
    g.prev_.buttons = SwapAB(prev_.buttons);
    std::swap(g.holdTime_[12], g.holdTime_[13]);
    return g;
}

float Gamepad::HeldTime(u32 b) const
{
    for (int i = 0; i < 18; ++i)
        if (b == (1u << i))
            return holdTime_[i];
    return 0;
}

bool Gamepad::IgnoreRealNow(bool focusOnly)
{
    const HWND fg = GetForegroundWindow();
    bool front = focusgate::Foreground(fg, fg ? GetAncestor(fg, GA_ROOTOWNER) : nullptr, fl::MainWindow());
    if (const ShmBlock* s = DevBlock())
        front = front || (s->devFlags & kDevForeground) != 0; // a background test of the open gate (shm.h)
    // the same for the SDL virtual test pads (shm.h v3; pad_backend 1, the host's debug `sdlpad`) while one is attached
    if (const ShmBlock* s = shm::Get(); s && s->version >= 3 && s->sdlAttached)
        front = front || (s->sdlFlags & kDevForeground) != 0;
    return focusgate::IgnoreReal(focusOnly, front);
}

void Gamepad::Wave(bool want)
{
    if (!HostAtLeast(41) || !g_host->PadWave)
        return;
    if (!want && !waveOpen_)
        return;
    const int state = g_host->PadWave(want ? 1 : 0); // (host\hapticwave.h: 2 running, under 0 failed)
    waveOpen_ = want;
    const bool running = want && state == 2;
    if (state < 0 && !waveWhy_[0])
    {
        g_host->PadWaveWhy(waveWhy_, sizeof(waveWhy_));
        Log("haptics: no waveform: %s", waveWhy_);
    }
    if (!want)
        waveWhy_[0] = 0;
    waveRunning_ = running;
}

void Gamepad::WaveFeed(const wavesynth::WaveFrame& f)
{
    if (waveRunning_)
        g_host->PadWaveFeed(&f);
}

void Gamepad::TriggerEffects(const u8* left, const u8* right)
{
    if (HostAtLeast(41) && g_host->SdlPadTriggers)
        g_host->SdlPadTriggers(left, right);
}

haptics::PadFacts Gamepad::Facts() const
{
    haptics::PadFacts f;
    f.connected = cur_.connected;
    f.sdl = sdlSource_ && sdl_.connected;
    f.type = sdl_.type;
    f.impulseRoute = impulseReady_ && !sdlSource_;
    f.waveRunning = waveRunning_;
    return f;
}

bool Gamepad::OutputIsReal() const
{
    if (DevActive())
        return false;
    const ShmBlock* s = shm::Get();
    return !(s && s->version >= 3 && s->sdlAttached);
}

void Gamepad::Rumble(float low, float high, float seconds)
{
    if (sdlSource_ || impulseReady_) // (with the trigger motors on SDL's WGI driver its two motors go there too: XInput's would fight them)
    {
        // the SDL backend's pad (sent by the host's SDL thread); the same gate and the same timer as XInput's
        if (ignoreReal && (low > 0 || high > 0))
            return;
        const u16 lo = static_cast<u16>(Clampf(low, 0, 1) * 65535), hi = static_cast<u16>(Clampf(high, 0, 1) * 65535);
        // SDL ends an effect itself after its duration; the timer below still sends the stop (a margin over it)
        const u32 ms = (lo || hi) ? static_cast<u32>(Clampf(seconds, 0, 60) * 1000) + 100 : 0;
        g_host->SdlPadRumble(lo, hi, ms);
        rumbling_ = seconds > 0;
        rumbleTimer_ = seconds;
        return;
    }
    if ((!g_setState && !DevActive()) || slots_.Index() < 0)
        return;
    if (ignoreReal && (low > 0 || high > 0))
        return; // the pad is not the game's now (focus_gate.h): no vibration in the background
    XINPUT_VIBRATION v;
    v.wLeftMotorSpeed = static_cast<WORD>(Clampf(low, 0, 1) * 65535);
    v.wRightMotorSpeed = static_cast<WORD>(Clampf(high, 0, 1) * 65535);
    SetState(static_cast<DWORD>(slots_.Index()), &v);
    rumbling_ = seconds > 0;
    rumbleTimer_ = seconds;
}
