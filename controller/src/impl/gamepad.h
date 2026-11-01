#pragma once
#include "common.h"
#include "../api.h"
#include "focus_gate.h"
#include "pad_slots.h"

#include <Xinput.h>

// Digital "buttons" for the analog triggers, stored above the 16 XInput bits.
constexpr u32 PAD_LT = 1u << 16;
constexpr u32 PAD_RT = 1u << 17;

struct PadState
{
    bool connected = false;
    u32 buttons = 0;      // XINPUT_GAMEPAD_* | PAD_LT | PAD_RT
    float lx = 0, ly = 0; // -1..1 after deadzone, +y = up
    float rx = 0, ry = 0;
    float lt = 0, rt = 0; // 0..1 after threshold
};

class Gamepad
{
  public:
    void Init();
    void Update(float dt);

    const PadState& Cur() const { return cur_; }
    bool Down(u32 b) const { return (cur_.buttons & b) != 0; }
    bool Pressed(u32 b) const { return (cur_.buttons & b) && !(prev_.buttons & b); }
    bool Released(u32 b) const { return !(cur_.buttons & b) && (prev_.buttons & b); }
    // True if any button/stick/trigger is active this frame.
    bool AnyInput() const;
    // Seconds the button has been held (0 if up).
    float HeldTime(u32 b) const;

    // The same pad with A and B exchanged (cur, prev and hold times): the menus' view when confirm_button is 1 (the right
    // button accepts; glyphs::Menu, device.h ConfirmMask). Flight reads the positional pad.
    Gamepad SwappedAB() const;

    void Rumble(float low, float high, float seconds);
    int ActiveIndex() const { return slots_.Index(); }
    // A pad answers on an XInput slot (a real one, or a device-layer test pad: DevActive).
    bool HasRealController() const { return realConnected_; }
    // The device-layer test pads serve the XInput slots (controller\src\shm.h v2, IDEAS T36).
    static bool DevActive();
    // The pad in use's USB vendor id when its device layer knows it (a test pad; the SDL backend: the vendor of SDL's
    // gamepad type, pad_backend.h VendorForType), else 0 (glyphs.cpp reads the HID list).
    u32 Vendor() const;
    // The SDL backend reads the pad this frame (pad_backend 1 and SDL running: pad_backend.h ReadFromSdl).
    bool FromSdl() const { return sdlSource_; }
    // One line for `state` while pad_backend is not 0 or SDL still runs ("" otherwise: the default's output unchanged).
    std::string BackendStatus() const;

    float stickDeadzone = 0.24f;
    float triggerThreshold = 0.12f;
    // pad_backend: 0 XInput (default), 1 SDL3 (WORK_QUEUE Q5 slice 1; controller\src\pad_backend.h, host sdlpad.*)
    int padBackend = 0;
    // Drop the physical pad's input (the game in the background, focus_gate.h); the virtual pad still works. An input
    // held when the gate opens stays dropped until it is released.
    bool ignoreReal = false;
    // The one gate's question for this frame (focus_gate.h): `focus_only` and the foreground window.
    static bool IgnoreRealNow(bool focusOnly);

  private:
    void ReadReal(XINPUT_STATE& out, bool& ok, float dt);
    bool ReadSdl(XINPUT_STATE& out, bool& ok); // true = the SDL backend answered for this frame (ok: a pad)

    focusgate::Held realHeld_;
    // the SDL backend (pad_backend 1)
    bool sdlOn_ = false, sdlSource_ = false, sdlWarned_ = false;
    int sdlStatusWas_ = 0, sdlConnectedWas_ = 0;
    u32 sdlVendor_ = 0;
    SdlPadState sdl_ = {};
    padslots::Scan slots_; // the slot in use and the rescan (pad_slots.h)

    PadState cur_, prev_;
    float holdTime_[18] = {};
    bool realConnected_ = false;
    float rumbleTimer_ = 0;
    bool rumbling_ = false;
};
