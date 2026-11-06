// Gamepad class and PadState: buttons, sticks, triggers as buttons, edges, hold times, backend and focus gate, rumble.
#pragma once
#include "common.h"
#include "../api.h"
#include "../pad_backend.h"
#include "../wave_synth.h"
#include "focus_gate.h"
#include "haptics_rules.h"
#include "haptics_render.h"
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
    // Both triggers' effects (11 bytes each, haptics::StanceBytes), through the host's SDL thread; nothing on an older host or with no SDL running.
    void TriggerEffects(const u8* left, const u8* right);
    // An Xbox pad's trigger motors, as Rumble.
    void RumbleTriggers(float left, float right, float seconds);
    // The grips' stream: `want` (the mode, a DualSense on USB, a real pad in a foreground game) opens or closes it; its state decides Facts().waveRunning. A failure stands
    // until `want` goes false. The reason of the last failure, "" none.
    void Wave(bool want);
    void WaveFeed(const wavesynth::WaveFrame& f);
    const char* WaveWhy() const { return waveWhy_; }
    bool Wired() const { return sdl_.link == 1; }
    bool impulseWanted = false; // `trigger_effects` on: an Xbox pad's four motors may go through SDL
    // Grows whenever SDL's open pad changes: what was sent to the last one is gone.
    u32 PadSerial() const { return sdl_.padSerial; }
    // What haptics_render.h needs to know of the pad in use.
    haptics::PadFacts Facts() const;
    // The output goes to a physical pad, not a test pad of the device layer or of SDL.
    bool OutputIsReal() const;
    int ActiveIndex() const { return slots_.Index(); }
    // A pad answers on an XInput slot (a real one, or a device-layer test pad: DevActive).
    bool HasRealController() const { return realConnected_; }
    // The real pad stopped answering this frame, or the game lost the focus: what went up was not released by a hand.
    bool ReadLost() const { return realWas_ && (!realConnected_ || ignoreReal); }
    // The device-layer test pads serve the XInput slots (modules\controller\src\shm.h v2, IDEAS T36).
    static bool DevActive();
    // The pad in use's USB vendor id when its device layer knows it (a test pad; the SDL backend: the vendor of SDL's
    // gamepad type, pad_backend.h VendorForType), else 0 (glyphs.cpp reads the HID list).
    u32 Vendor() const;
    // The SDL backend reads the pad this frame (pad_backend 1 and SDL running: pad_backend.h ReadFromSdl).
    bool FromSdl() const { return sdlSource_; }
    // One line for `state` while pad_backend is not 0 or SDL still runs ("" otherwise: the default's output unchanged).
    std::string BackendStatus() const;
    void ForceCandidate(int answer) { candidateForced_ = answer; } // debug: a test on a machine with no such pad, or with one

    float stickDeadzone = 0.24f;
    float triggerThreshold = 0.12f;
    // pad_backend: 0 XInput (default), 1 SDL3 (WORK_QUEUE Q5 slice 1; modules\controller\src\pad_backend.h, host sdlpad.*)
    int padBackend = 0;
    bool primary = false; // the flight's reader: only it runs the fallback (PadBackend tells the others)
    bool sdlFallback = true; // `pad_sdl_fallback`: read PlayStation and Switch pads natively; off = the player asks for Steam Input or a mapper
    bool HandsOnFallback() const { return primary && handsOnSdl_ && padBackend != 1; }
    // Drop the physical pad's input (the game in the background, focus_gate.h); the virtual pad still works. An input
    // held when the gate opens stays dropped until it is released.
    bool ignoreReal = false;
    // The one gate's question for this frame (focus_gate.h): `focus_only` and the foreground window.
    static bool IgnoreRealNow(bool focusOnly);

  private:
    void ReadReal(XINPUT_STATE& out, bool& ok, float dt);
    bool ReadSdl(XINPUT_STATE& out, bool& ok, bool fallback); // true = the SDL backend answered for this frame (ok: a pad)
    bool Candidate(float dt);                                 // the HID list holds a pad for the fallback (rescanned at kHidRescan)
    void Filter(bool on);
    void Impulse(float dt, bool xinputPad, bool fallback);    // SDL's output for an Xbox pad, kept up or stopped

    focusgate::Held realHeld_;
    // the SDL backend (pad_backend 1)
    bool sdlOn_ = false, sdlSource_ = false, sdlWarned_ = false;
    padbackend::Fallback fallback_;
    padbackend::Hands hands_;
    padbackend::ImpulseRoute impulse_;
    bool waveOpen_ = false, waveRunning_ = false;
    char waveWhy_[128] = {};
    bool impulseOn_ = false, impulseReady_ = false, xboxSeen_ = false;
    float impulseLook_ = 0, hidTimerXbox_ = 0;
    bool handsOnSdl_ = false, xinputBeside_ = false; // the fallback's reader this frame; XInput's pad read while SDL holds a pad at rest
    bool filterOn_ = false;
    bool candidate_ = false;
    float hidTimer_ = 0;
    int candidateForced_ = -1; // debug `padfallback`: -1 the HID list, else that answer
    int sdlStatusWas_ = 0, sdlConnectedWas_ = 0;
    u32 sdlVendor_ = 0;
    SdlPadState sdl_ = {};
    padslots::Scan slots_; // the slot in use and the rescan (pad_slots.h)

    PadState cur_, prev_;
    float holdTime_[18] = {};
    bool realConnected_ = false;
    bool realWas_ = false; // ...in the frame before
    float rumbleTimer_ = 0;
    bool rumbling_ = false;
};
