// The native HUD switch (docs\UI_HUD2.md 5.1 / 5.2): while HUD 2 shows, the game's own flight HUD panels are hidden by
// their root controls' state byte (FlControl +0x6C bits 0|1 = visible; docs\FREELANCER_RE.md "UI"), and put back
// exactly when it goes. The roots (verified 25 Oct 2026, HDE 0.7.1, instance 6, SaveC705: every one a top-level control,
// parent 0): HUD_Maneuvers (the maneuver bar with the cruise box and FLvibed's autopilot button), NN_MainButton (the
// NeuroNet box), HUD_Target (the target panel), HUD_ContactList (the contact list and its filters), HUD_Status (the
// status / weapon panel), HUD_Core (the gauges). Found ONCE through a named child (ManeuverButton0, NNNavmapButtonRoot,
// TargetCloseButton, AllFilterButton, HudRepairButton, HUD_Core) and its +4 parent chain, cached; revalidated by the
// cached vtable every frame (one guarded 4-byte read) and the name every 2 s, rediscovered when the HUD is rebuilt
// (Invalidate: a load, a launch, entering space). Per frame only a compare and a byte write per root. The game re-shows
// HUD_Maneuvers and HUD_ContactList itself each frame (measured): their bytes are cleared again each frame. Kept native:
// the comm window, the objective banner, the target indicators (brackets, arrows, lead, the target's bars), the cursor /
// reticle (the damage arcs, the chat window, the mouse-over box and the missile warning go too since the fix round of
// 25 Oct 2026: SetKeyboardForm below).
// The game's own text lines that HUD 2 shows itself (polish, 25 Oct 2026): the cruise engine's line under the reticle
// ("CRUISE ENGINE CHARGING - WEAPONS OFFLINE - n%") = the top-level control HUD_CruiseProgress, a seventh root found by
// its own name - kept native while the cruise engine is disrupted ("Cruise Disrupted - n", the seconds until it can
// charge, which HUD 2's 3 s alert does not show); the system arrival line (NN_ArrivalText, one control per showing,
// exe::ArrivalText) only on SetHideArrival(true). Not native (FLvibed's own): the red "SHIELD FAILED" / "HULL BREACH"
// boxes are the subtitles module's Neural Net column.
#pragma once
#include "common.h"

#include <string>

namespace hudvis
{
// Once per frame from OnRenderUnderHud (before the UI pass draws the controls): hide the panels (true) or restore what
// was hidden (false; a no-op when nothing is hidden).
void Apply(bool hide);
// Whether the switch also hides the game's system arrival line ("New York System, Liberty Space.", typed top left)
// while it hides the panels. Default false: HUD 2's place line shows the system and zone after a jump only, not the
// house ("Liberty Space") and not at a launch / load; HUD 2 sets it once it shows the same line. One guarded read a
// frame (the line's control pointer, 0 almost always).
void SetHideArrival(bool hide);
// The fix round (25 Oct 2026, the round 3 critique): also hidden while HUD 2 shows - the chat window MsgWin (the
// subtitles draw the chatter in HUD 2's band; kept while the chat input CTID is open), the damage arcs
// HUD_DamageIndicator (HUD 2's own hit flash) and, in the pad form only, the mouse-over label Rollover (it sat on the
// ring's speed: the cursor is the aim point). The form: true = keyboard and mouse (the label kept).
void SetKeyboardForm(bool kbm);
// The game's hover help (the ToolTip box, the Rollover label) only for an intentional hover (owner, 25 Oct 2026): false =
// each Apply hides a shown one the game's own way before anything draws, in both HUD styles (hud2::RenderUnderHud
// decides per frame: in flight only after a real mouse move within the last seconds; elsewhere always allowed).
void SetHoverHelp(bool allowed);
// Extended HUD 2's modal menus in space (ui_style 1: pause, save / load, Options, the confirm dialog - docs\UI_HUD2_EXTENDED.md
// 10, the MODAL track of 25 Oct 2026): while set, Apply hides every flight root whatever its keep rule and whatever
// HUD 2 wants (the classic HUD too), the arrival line with them, and draws no 3D view - only the menus' own Rollover /
// ToolTip stay as the game wants them (the pause menu's button help is the ToolTip). Cleared: the next Apply restores
// what HUD 2 does not hide itself. Set by HUD 2's gate (hud2::SetMenuOver) before Apply, per frame (a flag write).
void SetMenuOver(bool on);
// The game's own 3D views without their panels, both drawn by the target panel's own view draw (exe::TargetViewDraw, the
// same on every build): which 0 = the player's ship (the status panel's watched object), 1 = the target (the target
// panel's). Called each frame while HUD 2 wants the view
// (a request holds one frame); the rectangle in screen pixels (centre, half extents). The next Apply (OnRenderUnderHud)
// draws the view there - under the HUD's controls and under HUD 2's overlay - when its panel is hidden by the switch and
// its draw is the one known; else nothing (fails closed). The game's rectangle is put back when the request stops.
void SetViewRect(int which, float cx, float cy, float halfW, float halfH, bool show);
// The view was drawn by the last Apply (its draw known, the panel hidden, no fault): HUD 2 opens its plate's window
// for it then, else draws the flat icon.
bool ViewShown(int which);
// The space id of the object the target panel's view is built for (0 none): a view for another object is stale.
u32 TargetViewObject();
// The target panel shown again (its state bits as the game wants them) until the next Apply, which hides it before the
// UI pass draws: the game's own update builds the view for a new target only while the panel is shown.
void PulseTargetPanel();
// The HUD was (or may have been) rebuilt: the cached roots are dropped (restored first when still valid).
void Invalidate();
// Roots currently hidden by the switch.
int Hidden();
std::string Status();
// Debug: the control tree under a cached root (by its label: maneuvers, neuronet, target, contactlist, status, gauges).
std::string Tree(const std::string& label);
} // namespace hudvis
