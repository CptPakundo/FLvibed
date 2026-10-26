#include "gamepad.h"

#include "log.h"
#include "../shm.h"

typedef DWORD(WINAPI* XInputGetStateFn)(DWORD, XINPUT_STATE*);
typedef DWORD(WINAPI* XInputSetStateFn)(DWORD, XINPUT_VIBRATION*);

static XInputGetStateFn g_getState = nullptr;
static XInputSetStateFn g_setState = nullptr;

void Gamepad::Init()
{
    const char* dlls[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
    for (const char* name : dlls)
    {
        HMODULE mod = LoadLibraryA(name);
        if (!mod)
            continue;
        g_getState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(mod, "XInputGetState"));
        g_setState = reinterpret_cast<XInputSetStateFn>(GetProcAddress(mod, "XInputSetState"));
        if (g_getState)
        {
            Log("XInput: using %s", name);
            return;
        }
    }
    Log("XInput: not available, only the virtual pad will work");
}

void Gamepad::ReadReal(XINPUT_STATE& out, bool& ok)
{
    ok = false;
    if (!g_getState)
        return;

    // Polling disconnected slots can stall on some systems, so only rescan occasionally.
    if (index_ >= 0)
    {
        if (g_getState(index_, &out) == ERROR_SUCCESS)
        {
            ok = true;
            return;
        }
        Log("XInput: controller %d disconnected", index_);
        index_ = -1;
        scanTimer_ = 0;
    }
    if (scanTimer_ > 0)
        return;
    scanTimer_ = 1.5f;
    for (int i = 0; i < XUSER_MAX_COUNT; ++i)
    {
        if (g_getState(i, &out) == ERROR_SUCCESS)
        {
            index_ = i;
            ok = true;
            Log("XInput: controller connected on slot %d", i);
            return;
        }
    }
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
    scanTimer_ -= dt;

    XINPUT_STATE xs = {};
    bool ok = false;
    ReadReal(xs, ok);
    realConnected_ = ok;

    i32 lx = 0, ly = 0, rx = 0, ry = 0;
    u32 buttons = 0;
    int lt = 0, rt = 0;
    if (ok && !ignoreReal)
    {
        buttons = xs.Gamepad.wButtons;
        lx = xs.Gamepad.sThumbLX;
        ly = xs.Gamepad.sThumbLY;
        rx = xs.Gamepad.sThumbRX;
        ry = xs.Gamepad.sThumbRY;
        lt = xs.Gamepad.bLeftTrigger;
        rt = xs.Gamepad.bRightTrigger;
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
        if (rumbleTimer_ <= 0)
            Rumble(0, 0, 0);
    }
}

bool Gamepad::AnyInput() const
{
    return cur_.buttons || cur_.lx || cur_.ly || cur_.rx || cur_.ry || cur_.lt || cur_.rt;
}

float Gamepad::HeldTime(u32 b) const
{
    for (int i = 0; i < 18; ++i)
        if (b == (1u << i))
            return holdTime_[i];
    return 0;
}

void Gamepad::Rumble(float low, float high, float seconds)
{
    if (!g_setState || index_ < 0)
        return;
    XINPUT_VIBRATION v;
    v.wLeftMotorSpeed = static_cast<WORD>(Clampf(low, 0, 1) * 65535);
    v.wRightMotorSpeed = static_cast<WORD>(Clampf(high, 0, 1) * 65535);
    g_setState(index_, &v);
    rumbling_ = seconds > 0;
    rumbleTimer_ = seconds;
}
