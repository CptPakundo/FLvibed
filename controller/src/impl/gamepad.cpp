#include "gamepad.h"

#include "host.h"
#include "log.h"
#include "../fl.h"
#include "../pad_backend.h"
#include "../shm.h"

typedef DWORD(WINAPI* XInputGetStateFn)(DWORD, XINPUT_STATE*);
typedef DWORD(WINAPI* XInputSetStateFn)(DWORD, XINPUT_VIBRATION*);

static XInputGetStateFn g_getState = nullptr;
static XInputSetStateFn g_setState = nullptr;

void Gamepad::Init()
{
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
// SDL runs in the host on its own thread (controller\src\host\sdlpad.cpp, HostApi v33); this side keeps it running while
// pad_backend is 1, reads its newest state into the XInput form every reader below uses (the focus gate, the virtual
// pad's merge, the dead zones, the hold times: unchanged), and logs its changes. XInput reads the pad while SDL starts,
// after it failed, with an older host and while the device-layer test pads are on (pad_backend.h ReadFromSdl).
bool Gamepad::ReadSdl(XINPUT_STATE& out, bool& ok)
{
    const bool hostHas = HostAtLeast(padbackend::kHostVersion) && g_host->SdlPad;
    if (!padbackend::WantSdl(padBackend, hostHas))
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
        Log("pad backend: SDL3 starting (pad_backend 1; SDL_Init runs on the host's own thread)");
    sdlOn_ = true;
    const int status = g_host->SdlPad(1, &sdl_);
    if (status != sdlStatusWas_)
    {
        if (status == padbackend::kSdlRunning)
            Log("pad backend: SDL3 running (init %.1f ms, %d gamepad(s))", sdl_.initMs, sdl_.count);
        else if (status == padbackend::kSdlFailed)
            Log("pad backend: SDL3 failed (%s) - XInput reads the pad; pad_backend 0 then 1 retries", sdl_.error);
        sdlStatusWas_ = status;
    }
    const bool was = sdlSource_;
    sdlSource_ = padbackend::ReadFromSdl(padBackend, hostHas, DevActive(), status);
    if (sdlSource_ != was)
        slots_ = padslots::Scan(); // the XInput slot scan starts fresh whenever XInput takes over again
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
    if (padBackend == 0 && !sdlOn_)
        return {};
    char line[256];
    snprintf(line, sizeof(line), "pad_backend=%d source=%s sdl_status=%d sdl_init_ms=%.1f sdl_pads=%d sdl_connected=%d "
             "sdl_type=%d sdl_vendor=0x%04X sdl_product=0x%04X sdl_seq=%u sdl_name='%s'",
             padBackend, sdlSource_ ? "sdl" : "xinput", sdlStatusWas_, sdl_.initMs, sdl_.count, sdl_.connected, sdl_.type,
             sdl_.vendor, sdl_.product, sdl_.seq, sdl_.name);
    return line;
}

void Gamepad::ReadReal(XINPUT_STATE& out, bool& ok, float dt)
{
    ok = false;
    if (ReadSdl(out, ok))
        return;
    if (!g_getState && !DevActive())
    {
        slots_ = padslots::Scan();
        return;
    }
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

u32 Gamepad::Vendor() const
{
    if (sdlSource_)
        return sdlVendor_;
    const ShmBlock* s = DevBlock();
    return s && slots_.Index() >= 0 ? s->devSlot[slots_.Index()].vendor : 0;
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

void Gamepad::Rumble(float low, float high, float seconds)
{
    if (sdlSource_)
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
