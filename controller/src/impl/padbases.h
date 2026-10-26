// The pad in bases and in the NeuroNet windows (docs\UI_PLAN.md 6.4 "bases" / "dealers", 6.5; UI_SPEC.json screens
// cityscape, bar (+ offer), jobboard, news, commodity, equipment, shipdealer, missionlog, playerinfo, help, inventory;
// bindings.common rooms / dealers / menus). Phase D of the UI build (the D-BASES track, 25 Oct 2026). Everything ends
// in the game's own logic: the NavBar's hotspot records (room::Hotspots / uiclick::Click), the room characters' own
// hover / click methods (room::Hover / Talk), the windows' buttons by name (uiclick::FindVisible + Click), their scroll
// bars (uiclick::Scroll) - never cursor emulation. The game's own screens stay; FLvibed draws the dealers' quantity
// line, the offer word on the room characters' name card (uistrip's under ui_style 1, a label at the game's card under
// 0) and the prompt bar (promptbar.*) over them. Room characters: D-pad left / right walks them in screen order on the
// game's own hover (the game's highlight and card mark the focus), A talks (room::Talk).
//
// Ownership (controller.cpp's gates): Update decides per frame which pad buttons this code consumes (Owns); the
// generic D-pad focus (UpdateUiNav) and the free cursor (UpdateFreeCursor) skip those. The pad cursor option
// (pad_cursor, cursor_speed, cursor_magnet): the free cursor is the mechanism; CursorAllowed gates it, ShapeCursor adds
// the magnetism; a D-pad or bumper press "hides" it (focus carries on from where it is: the focus IS the game's cursor
// on a control), the stick shows it again - the arrow itself through the host's SetCursorHidden (HostApi v20), on the
// bases' and the menus' screens alike (NoteFrame / Overlay).
#pragma once
#include "common.h"

#include <istream>
#include <string>

class Gamepad;

namespace padbases
{
enum class Where
{
    Menu,    // the main menu / load screens (nothing here acts; ownership clears)
    Docked,  // on a base: rooms, the bar, the windows
    SpaceUi, // in space with a NeuroNet window (inventory, log, status, help; the nav map is padmap's)
};
// Once per frame, before UpdateUiNav / UpdateFreeCursor, while the pad is the device (and the controller is on).
void Update(float dt, const Gamepad& pad, Where where);
// Pad buttons (XINPUT_GAMEPAD_* masks) this code consumes this frame: the generic UI code leaves them alone. Includes
// padmap's while the nav map is open.
bool Owns(u32 mask);
// The left stick may drive the game's cursor here (pad_cursor: 0 = focus only).
bool CursorAllowed();
// A NeuroNet window is open in space (the nav map, the inventory, the log, the status, help) by its root control's
// visibility (cached 100 ms): the mode detector's second sign of Mode::SpaceUi - these windows hang under the HUD's
// own NeuroNet root, so "a window over the HUD roots" does not see them (25 Oct 2026).
bool SpaceWindowOpen();
// Magnetism for the free cursor's motion this frame (pixels): slows over a clickable element, pulls toward the
// nearest one in reach (cursor_magnet %).
void ShapeCursor(float& dx, float& dy);
// From OnRenderOverlay: the dealers' quantity line and, under ui_style 0, the offer label beside the game's own name
// card (then the prompt bar is promptbar::Draw's). (The bar's character strip is gone - ROOMS track 25 Oct 2026, the
// owner: "should not stay visible in a conversation"; D-pad left / right walks the characters in screen order on the
// game's own hover, the offer moves onto the name card.)
void Draw(void* device);
// The QUANTITY line's rectangle as Draw drew it this frame (pixels; false = none): the prompt lane's keep-out.
bool QuantityRect(float& x0, float& y0, float& x1, float& y1);
// The room character the game hovers now (the pad's focus or the pad cursor on it), while the pad's room layer shows
// (pad mode, a room, no conversation, no modal): its offer word - JOB, RUMOUR, BRIBE, INFO - or nullptr (nothing to
// offer, the story character, dealers, or no layer); HoveredIsStory: it is the story character (the yellow star).
// uistrip's name card (ui_style 1) and this module's label (ui_style 0) show them.
const wchar_t* HoveredOffer();
bool HoveredIsStory();
// A conversation in the room: a character talking to the player (+0x36C) or the game's room scene (NavBusy "scene").
// The room layer (focus hover, card, offer, room prompts) stands down while it runs.
bool InConversation();
// The game's arrow and the pad's room hover, one owner for every pad screen (PADFIX 25 Oct 2026). padmenus::Update
// calls NoteFrame once per pad frame, any mode (`menuScreen`: one of its menu screens is up - main menu, load, save,
// pause, confirm, death, the OSK / New Game prompt - so the arrow follows the pad cursor rule there too; false = the
// arrow shows unless this frame's Update hides it for a base screen). Overlay (from padmenus::Draw, every overlay
// pass whatever the device or mode) applies the frame's decision through the host (HostApi v20) - and when no pad
// frame ran (keyboard and mouse, the controller off, the autopilot flying, the options page) it stands down: the
// arrow shows and the room character the pad hovered is released (the game's own unhover).
void NoteFrame(bool menuScreen);
void Overlay();
// Debug: bases state | rooms | chars
std::string Command(std::istream& in);
void Shutdown();
} // namespace padbases
