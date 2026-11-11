// The pad as core's UI sees it (core\ui\views.h): this module's Gamepad readers behind the row kit, and the prompt bar.
#include "device.h"
#include "device_view.h"

#include "controller.h"
#include "gamepad.h"
#include "newgame.h"
#include "osk.h"
#include "padbases.h"
#include "wheel.h"
#include "promptbar.h"
#include "ui/views.h"

namespace device_view
{
namespace
{
Gamepad* Pad(void* p) { return static_cast<Gamepad*>(p); }
const Gamepad* Pad(const void* p) { return static_cast<const Gamepad*>(p); }

void* NewPad()
{
    Gamepad* p = new Gamepad();
    p->Init();
    return p;
}

void Poll(void* pad, float dt, uikit::PadRead& in)
{
    Gamepad& p = *Pad(pad);
    p.ignoreReal = Gamepad::IgnoreRealNow(ctl::FocusOnly()); // the flight pad's gate and setting (focus_gate.h)
    p.stickDeadzone = ctl::StickDeadzone(); // the player's setting, as in flight (it kept the default: 29 Oct 2026)
    p.padBackend = ctl::PadBackend(); // and the backend (Q5 slice 2: with pad_backend 1 this reader stayed on XInput)
    p.Update(dt);
    const PadState& s = p.Cur();
    in.anyPad = p.AnyInput();
    in.accept = p.Pressed(device::ConfirmMask()); // A, or B with confirm_button 1
    in.back = p.Pressed(device::CancelMask());
    in.tabPrev = p.Pressed(XINPUT_GAMEPAD_LEFT_SHOULDER);
    in.tabNext = p.Pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    in.pageUp = p.Pressed(PAD_LT);
    in.pageDown = p.Pressed(PAD_RT);
    // D-pad and the left stick as four held directions
    in.dir[0] = p.Down(XINPUT_GAMEPAD_DPAD_UP) || s.ly > 0.6f;
    in.dir[1] = p.Down(XINPUT_GAMEPAD_DPAD_DOWN) || s.ly < -0.6f;
    in.dir[2] = p.Down(XINPUT_GAMEPAD_DPAD_LEFT) || s.lx < -0.6f;
    in.dir[3] = p.Down(XINPUT_GAMEPAD_DPAD_RIGHT) || s.lx > 0.6f;
}

void Extra(const void* pad, uikit::Input::PadExtra& e)
{
    const Gamepad& p = *Pad(pad);
    const PadState& s = p.Cur();
    e.connected = s.connected;
    e.x = p.Pressed(XINPUT_GAMEPAD_X), e.y = p.Pressed(XINPUT_GAMEPAD_Y);
    e.menu = p.Pressed(XINPUT_GAMEPAD_START), e.view = p.Pressed(XINPUT_GAMEPAD_BACK);
    e.lb = p.Pressed(XINPUT_GAMEPAD_LEFT_SHOULDER), e.rb = p.Pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    e.l3 = p.Pressed(XINPUT_GAMEPAD_LEFT_THUMB), e.r3 = p.Pressed(XINPUT_GAMEPAD_RIGHT_THUMB);
    e.xDown = p.Down(XINPUT_GAMEPAD_X), e.yDown = p.Down(XINPUT_GAMEPAD_Y);
    e.lbDown = p.Down(XINPUT_GAMEPAD_LEFT_SHOULDER), e.rbDown = p.Down(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    e.aDown = p.Down(device::ConfirmMask()), e.bDown = p.Down(device::CancelMask()); // accept / back (confirm_button)
    e.menuDown = p.Down(XINPUT_GAMEPAD_START), e.viewDown = p.Down(XINPUT_GAMEPAD_BACK);
    e.xHeld = p.HeldTime(XINPUT_GAMEPAD_X);
    e.lx = s.lx, e.ly = s.ly, e.rx = s.rx, e.ry = s.ry, e.lt = s.lt, e.rt = s.rt;
}

bool Connected(const void* pad) { return Pad(pad)->Cur().connected; }
bool PadActive() { return device::Pad(); }

} // namespace

void Publish()
{
    views::device = {NewPad, Poll, Extra, Connected, padbases::NoteFrame, promptbar::Set, promptbar::Top, PadActive, promptbar::LaneWanted, osk::IsOpen, nullptr /* RearView: the wheel has none since GitHub issue 65 */, promptbar::Visible, padbases::HoveredOffer, padbases::HoveredIsStory, promptbar::Plates};
}

void Withdraw()
{
    views::device = {};
}
} // namespace device_view
