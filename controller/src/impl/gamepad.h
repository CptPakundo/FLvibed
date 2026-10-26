#pragma once
#include "common.h"

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

    void Rumble(float low, float high, float seconds);
    int ActiveIndex() const { return index_; }
    bool HasRealController() const { return realConnected_; }

    float stickDeadzone = 0.24f;
    float triggerThreshold = 0.12f;
    bool ignoreReal = false; // drop the physical pad's input (game out of focus); the virtual pad still works

  private:
    void ReadReal(XINPUT_STATE& out, bool& ok);

    PadState cur_, prev_;
    float holdTime_[18] = {};
    int index_ = -1;
    bool realConnected_ = false;
    float scanTimer_ = 0;
    float rumbleTimer_ = 0;
    bool rumbling_ = false;
};
