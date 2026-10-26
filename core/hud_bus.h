// FLvibed - the HUD bus: the runtime layout of FLvibed's own flight HUD (HUD 2, docs\UI_HUD2.md), shared between the
// module logics so they keep clear of each other. The controller logic draws HUD 2 and publishes, once per frame, whether
// it shows (and how far it has faded in) and the screen rectangles its parts take; the subtitles logic reads them to place
// its dialogue panel (with the comm portrait), its chat column, the objective banner and the off-screen arrows' keep-outs,
// and publishes back where its dialogue panel landed so HUD 2 puts its alert below it. One instance lives in FLvibed.dll
// (core\hud_bus.cpp, compiled plain beside core\flvibed.cpp, as the settings bus); the module hosts hand it to their logic
// (`GetHudBus`: controller HostApi v21, subtitles HostApi v8). Everything runs on the game's thread: no locking, plain
// copies. A reader that sees `frame` older than two frames treats HUD 2 as absent (the controller logic unloaded or
// stopped publishing): fails closed to today's layout.
#pragma once
#include "common.h"

// A rectangle in fractions of the back buffer (x by W, y by H); empty (x1 <= x0) = the part is not shown.
struct HudRect
{
    float x0, y0, x1, y1;
};

// What HUD 2 publishes (the controller logic, every frame it is in flight).
struct HudLayout
{
    u32 frame;     // the publisher's frame counter (the reader compares it with its own sightings: stale = absent)
    u8 active;     // 1 while HUD 2 replaces the game's HUD (the game's panels hidden), 0 = the game's HUD shows
    u8 kbm;        // 1 when HUD 2 is in its keyboard-and-mouse form (keycaps instead of pad glyphs)
    u8 alerts;     // 1 while HUD 2's alert cards are on (hud_alerts): the subtitles' Neural Net column leaves out the lines
                   // HUD 2 shows as alerts (shield failed / destroyed, hull breach, incoming missile / torpedo, cruise
                   // disrupted) - one place per event (25 Oct 2026; was reserved: older publishers write 0)
    u8 reserved;
    float alpha;   // 0..1: HUD 2's fade (the transition between the game's HUD and HUD 2 takes a few frames)
    float scale;   // hud_scale / 100
    HudRect nav;      // top-left: objective, maneuver chip, NeuroNet chip
    HudRect target;   // top-right: the target card (+ the threat list under it in combat)
    HudRect ship;     // bottom-left: the ship panel
    HudRect loadout;  // bottom-right: the loadout panel (the D-pad cross and the weapons)
    HudRect ring;     // the reticle ring with its numbers and alerts
    HudRect context;  // the context line under the player's ship
    HudRect alert;    // the transient alert card (empty when none shows)
    HudRect topFree;  // the band between nav and target where the dialogue panel may go (top-centre)
    HudRect chatFree; // left-middle: where the chat column may go (below nav, above the ship panel)
};

// What the subtitles logic publishes back (every frame it lays out in flight).
struct HudSubtitles
{
    u32 frame;
    HudRect dialogue; // the dialogue panel (with the comm portrait) as placed; empty = none shows
    HudRect chat;     // the chat column as placed; empty = none
};

// The pad's prompt lane on docked screens (bus version 2, 25 Oct 2026; docs\UI_HUD2_EXTENDED.md 13): the controller logic
// publishes, every frame its prompt bar has prompts on a docked non-modal screen while the pad is the device, the
// rectangle its pills take at the bottom (the row's band, or the column right of a window) - whether the pills show or
// have faded (stable per screen). The subtitles keep their lowest row above `rect.y0`. Stale (no publish within two of the
// reader's frames) or inUse 0 = no lane: today's placement.
struct HudLane
{
    u32 frame;     // the publisher's frame counter
    u8 inUse;      // 1: the lane below is in use on this screen
    u8 column;     // 0: the bottom row (the subtitles stay above it wherever they are); 1: a column right of a window
                   // (only a block reaching its x range keeps above it)
    u8 reserved[2];
    HudRect rect;  // the pills' area (fractions of the back buffer)
};

// The open windows' floor on a docked screen (bus version 3, 25 Oct 2026; docs\UI_HUD2_EXTENDED.md 13 rule 5): the
// controller logic publishes, every frame its prompt bar runs while the pad is the device, the lowest bottom edge of the
// keep-outs uiskin's geometry pass collected (every open window's plates - modal ones too -, the room bar, its sub-row, the
// NeuroNet box, the tabs) per vertical slice of the screen: slice i covers x in [i / count, (i + 1) / count) of W. The
// subtitles keep their block's top below it in their x range (never over a window's buttons). count 0 = nothing known.
constexpr int kHudFloorSlices = 32;
struct HudFloor
{
    u32 frame;
    u8 count;      // slices used (kHudFloorSlices), 0 = none
    u8 reserved[3];
    float bottom[kHudFloorSlices]; // fractions of H, 0 = nothing in that slice
};

// The subtitles' block on a docked screen (bus version 3): published by the subtitles logic every frame it places lines
// in a base, read by the prompt bar as a keep-out - the lane yields to the subtitles where they cannot sit between a
// window and the lane (the pills never under the subtitles). Stale (no publish within ~100 ms) = none.
struct HudDockedSubs
{
    u32 frame;
    HudRect rect; // fractions of the back buffer; empty = none
};

// FLUF's windows (bus version 4, 26 Oct 2026; docs\research\fluf.md "FLUF in play" B / E, core\fluf.h): the bus itself
// reads them (FLvibed.dll's one FLUF probe), lazily: at most once per FLUF ImGui frame, whoever asks first. Every
// rectangle is a keep-out for FLvibed's drawing (HUD 2, Extended HUD 2, the subtitles, the prompt lane): FLUF's ImGui
// draws over FLvibed's overlay and eats the mouse over its windows. present 0 = no FLUF (today's behaviour exactly);
// 1 = FLUF, co-residency only (an unknown build, ImGui's layout not the expected one, or `fluf_windows` 0): no windows,
// but `textInput` still says when FLUF's text box has the keys; 2 = aware.
constexpr int kHudFlufMax = 16;
enum HudFlufKind : u8
{
    FlufNone = 0,
    FlufPalette, // the command palette (the "search" element, top centre; text input)
    FlufWindow,  // the extended options menu / Player Status (one ImGui window "Player Status")
    FlufMap,     // FLUF's nav map (and its child windows), the mission briefing's map too
    FlufDebug,   // FLUF's developer windows: the Debug Window and its tools (`fluf_windows` 2 only)
    FlufToast,   // a notification (top left, no input)
    FlufOther,   // any other FLUF / FLUF-module window
};
struct HudFlufWindow
{
    HudRect rect; // fractions of the back buffer
    u8 kind;      // HudFlufKind
    u8 mouse;     // 1: FLUF takes the mouse over it (a background or FLUF's own window: its click rule)
    u8 reserved[2];
};
struct HudFluf
{
    u32 frame;     // FLUF's ImGui frame count of this snapshot (0 = none yet)
    u8 present;    // 0 no FLUF, 1 co-residency only, 2 aware (above)
    u8 textInput;  // FLUF's ImGui has the keyboard for text (io.WantTextInput: FLUF withholds every key from the game)
    u8 mouseOver;  // the game's cursor lies in a window FLUF takes the mouse over (its buttons / wheel go to FLUF)
    u8 count;      // windows below
    u8 kinds;      // bit (1 << kind) for every kind open (after the policy)
    u8 policy;     // `fluf_windows` as set (0 ignore, 1 player windows, 2 all)
    u8 palette;    // FLUF's command palette is open (whatever the policy: it takes every key like a chat box, the owner)
    u8 reserved[1];
    HudFlufWindow win[kHudFlufMax];
    char build[24];  // "nightly-2026-10-16", "2.0.0", "unknown", "" (none)
    char status[112]; // one line for the logs (what was detected, what is adapted, why not)
};

// Bus version 5 (26 Oct 2026, the owner's attended run): what each window of HudFluf::win is, by index - its ImGui name
// (logged: which window a palette command opened), whether it takes the player's input (FLUF's click rule and not
// ImGuiWindowFlags_NoMouseInputs: a pass-through overlay does not own the pad), whether ImGui's keyboard focus is on it
// and where its title bar's close button is (fractions of the back buffer; -1 = none: no title bar, no close button,
// collapsed). Read beside HudFluf (the same snapshot: `frame` equal).
struct HudFlufWindowExtra
{
    char name[40];
    float closeX, closeY;
    u8 input;   // takes the mouse (and so the pad's cursor)
    u8 focused; // ImGui's NavWindow (or a window inside it)
    u8 reserved[2];
};
struct HudFlufExtra
{
    u32 frame; // = HudFluf::frame of the same snapshot
    u8 count;  // = HudFluf::count
    u8 reserved[3];
    HudFlufWindowExtra win[kHudFlufMax];
};

struct HudBus
{
    u32 version; // 1; 2 adds the lane (PublishLane / ReadLane / LaneFrame) - read them only when version >= 2; 3 adds the
                 // floor and the docked subtitles (PublishFloor ... DockedSubsFrame) - only when version >= 3; 4 adds FLUF
                 // (ReadFluf, SetFlufPolicy) - only when version >= 4; 5 adds ReadFlufExtra
    void (*PublishLayout)(const HudLayout* layout);
    // Copies the last layout; false when none was ever published.
    bool (*ReadLayout)(HudLayout* out);
    void (*PublishSubtitles)(const HudSubtitles* subs);
    bool (*ReadSubtitles)(HudSubtitles* out);
    // A frame counter the bus keeps (incremented by the controller logic's Publish): readers use it to tell a fresh
    // layout from a stale one without their own counter.
    u32 (*Frame)();
    // version 2 (appended: a reader built against version 1 never sees them)
    void (*PublishLane)(const HudLane* lane);
    bool (*ReadLane)(HudLane* out); // false when none was ever published
    u32 (*LaneFrame)();             // incremented by every PublishLane: readers tell fresh from stale by it
    // version 3 (appended)
    void (*PublishFloor)(const HudFloor* floor);
    bool (*ReadFloor)(HudFloor* out); // false when none was ever published
    u32 (*FloorFrame)();              // incremented by every PublishFloor
    void (*PublishDockedSubs)(const HudDockedSubs* subs);
    bool (*ReadDockedSubs)(HudDockedSubs* out);
    u32 (*DockedSubsFrame)(); // incremented by every PublishDockedSubs
    // version 4 (appended)
    bool (*ReadFluf)(HudFluf* out); // false = no FLUF (out->present 0); refreshes at most once per FLUF ImGui frame
    void (*SetFlufPolicy)(int policy); // `fluf_windows` (the controller's registry; 2 until it says otherwise)
    // version 5 (appended)
    bool (*ReadFlufExtra)(HudFlufExtra* out); // the last snapshot's per-window extras (call after ReadFluf)
};
