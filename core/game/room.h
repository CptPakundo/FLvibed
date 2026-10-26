// Base-room characters (bar patrons, dealers, story characters; class vtable 0x5CB930) handled natively,
// without moving the cursor onto them: what each offers and talking to it, through the class's own
// virtual methods (vt[10] 0x445A90 hover-enter sets the cursor naming the offer, vt[11] 0x445BE0
// hover-leave, vt[12] 0x445C10 click). The live list comes from the module's host (its ctor/dtor detours),
// handed over with SetCharacterSource. Shared by every module (core/game), compiled into each module.
// Verified in game 17 Oct 2026 (Manhattan bar: 7 characters, Jun'ko Zane = talk_story, her M01 scene).
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace room
{
// The host's live room characters: fills out[0..max) and returns the count (HostApi GetRoomCharacters).
typedef int (*CharacterSource)(void** out, int max);
void SetCharacterSource(CharacterSource source);

struct Character
{
    void* obj;
    std::string offer; // talk_story, talk_mission, talk_rumor, talk_bribe, talk_info, talk_blowoff,
                       // talk_commodity_dealer, talk_ship_dealer, talk_equip_dealer
};

std::vector<Character> List();
// Shows the game's hover feedback for a character (its cursor) without talking; Unhover undoes it.
bool Hover(void* obj);
bool Unhover(void* obj);
// Talks to a character, as a click on it would.
bool Talk(void* obj);

// The room's hotspots and the UI buttons the game made for them. RoomControlN / ButtonControlN are numbered
// per room, not by function, so tools and the pad go through these records (the room UI builder 0x43F2E0
// keeps them in a list on the NavBar control: docs/FREELANCER_RE.md "Room hotspots -> controls").
struct Hotspot
{
    std::string label;   // room switch ("Bar", "Trader", "Equipment", "ShipDealer", "Cityscape", "Deck", ...)
                         // or, for actions, the hotspot name
    std::string hotspot; // IDS_HOTSPOT_BAR, IDS_HOTSPOT_NEWSVENDOR, IDS_NN_REPAIR_YOUR_SHIP, ...
    std::string control; // RoomControlN / ButtonControlN
    u32 behaviour;       // 1 room switch, 2 news, 3 mission vendor, 6 dealer front desk, 7 ship dealer, 12 repair
    u32 ctl;             // the FlControl (uiclick::Click)
};
std::vector<Hotspot> Hotspots();
// Whether the NavBar acts on a room button now. Its click handler 0x43EE80 (message 0x26) swallows the click
// while any of these is set: global byte [0x67DCC8], NavBar +0x32C / +0x34C sub-objects busy (byte +0x18, read
// by 0x55E280), current room info byte +0x1338 (0x44C4A0). Empty = ready, else the busy gate's name.
// Also busy ("nn-window") while NNWindowBusy().
std::string NavBusy();
// Whether the docked NeuroNet's window manager (exe::NNManager) runs one of its two transitions - the open slide of a
// dealer / job board / news / ship dealer / repair window (~0.5 s after its controls show) or the close slide. Its
// window close (exe::NNWindowClose: the close button, the pad's B, Escape) does nothing then: the press is lost, not
// queued (verified 26 Oct 2026, HDE 0.7.1 and vanilla 1.1, ui_style 0 and 1). False when it cannot tell (the symbols
// unresolved: then callers act at once, as before).
bool NNWindowBusy();
// NNWindowBusy() with no window open any more (+0x3A8 cleared): a close's slide, the frames in which the game hides the
// room bar and the NeuroNet box for the room's camera move while their controls still read shown (an open's slide
// runs with the window set). False when it cannot tell.
bool NNWindowClosing();
// Clicks the hotspot whose label equals `want` (case-insensitive), else whose hotspot name contains it
// ("bar", "cityscape", "news", "repair"). Fills `what` with the control clicked or the reason it failed.
bool Goto(const std::string& want, std::string* what);

// The modules' shared debug commands for native game interaction (same in every module's debug channel):
//   click <ControlName>   hotspots   goto <label>   talkers   talk <index | offer part>
// Returns false if `cmd` is not one of them.
bool Command(const std::string& cmd, const std::string& rest, std::string* reply);
} // namespace room

namespace uiclick
{
// Native click on a UI button (no cursor): replays what the button mouse handlers do on release
// (click sound; owner window message 0x27 (/0x26) for the button, or the button's own vt[0xB4]). Supports
// text/OK buttons (vt[0x4C] 0x5A4380: 0x5E614C, 0x5E622C) and icon buttons (0x55B950: 0x5E15F4: HUD, NN menu,
// maneuvers, base rooms); false for others. Verified 17 Oct 2026 (nav map open/close in flight) and on
// the M01 "Basics of Combat" tutorial in flight (closed it; the mouse was captured).
u32 FindVisible(const char* name);
// Research: every visible control whose name contains `part` (debug `uinames [part]`), one per line.
std::string VisibleNames(const char* part);
bool Click(u32 ctl);
// Native scroll of a scroll bar (vt[0x4C] = exe::ScrollBarClick, e.g. the dealer's Inventory_Scroll): moves the
// position by `delta` (in its units: list rows; `pages` = whole pages instead), as the bar's own page click does
// (clamp, layout, then its list + window messages). Returns the new position, -1 when not a scroll bar. pos/min/max
// out (any may be null) report the state after the move.
float Scroll(u32 ctl, float delta, bool pages, float* min, float* max);
} // namespace uiclick
