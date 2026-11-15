// The INFO page's text, composed without the game's window (audit R3; docs\NEURONET_PLAN.md "INFO", docs\FREELANCER_RE.md
// "The INFO page's composers"), and INFO's actions (the HUD 2 page framework, audit 70b 2.3). The game's NN_Help window
// builds its text once when it opens (0x474B90) and again when the target changes while it is open (0x4E4690 -> 0x4754A0).
// nnhelp_screen.cpp builds the same text without the window: the game's own appenders (an infocard, a plain string, the
// market) fill a RenderDisplayList the mod owns, in the order the window's builders call them, and Common.dll's
// BinaryRDLWriter writes it out for nnhelp_rules.h to read. The game's window is never written, read or shown by the page;
// ReadWindow reads it for the comparison test only (the `nnpage info` verb).
#pragma once
#include "nnhelp_model.h"
#include "action.h"

namespace infobody
{
// What the game's INFO window would show now, for the player's target `targetId` (0 none; fl::TargetId). Game thread only
// (the market shares the game's scratch buffers).
Body Compose(u32 targetId);
// The game's own NN_Help window's text (its title and its text control's list), for the comparison test; false when the
// window or its text control is not there.
bool ReadWindow(u32 helpRoot, Body& out);
// The player's visit map's byte for an object id (0x4C4DA0), -1 when the map holds none: the probe watches it across an
// open of the window (the game's INFO makes no knowledge reveal: docs\FREELANCER_RE.md "The INFO page's composers").
int VisitByte(u32 id);
// The object behind a target id as the card reads it (class, type bits, visit value): the probe's line.
std::wstring Probe(u32 id);
// Test only (the test switch, FLVIBED_TEST=1; `nnpage info card <ids>`): the page's next open shows infocard `ids` instead of
// the game's text (title "INFOCARD <ids>"), once; the open after it is the game's again. So a test can put a card on the page
// whose styles the game's INFO never shows (underline: only the system cards carry it; audit decision 56). False (nothing
// set) without the switch or for 0.
bool TestCard(u32 ids);
// One line per paragraph ("|" and its text), the title first: what `nnpage info` prints and the comparison test compares;
// `styles`: each styled paragraph followed by its style line (inforules::StyleLine).
std::wstring Dump(const Body& b, bool styles = false);

// The page's reads (the runtime calls them: pages\pagerun.cpp). The target the card is of: 0 docked or with none.
u32 Target();
// The page's stamp: the target (the game's window builds again when the target changes while it is open: 0x4E4690).
uint64_t Stamp();
// Compose into the model at the moments the window does: always on a fresh model (the open); later only for a new target
// the card takes (a cleared target, or one the card does not take, 0x4754A0's early exit, leaves it as it was). True when
// the text changed.
bool Read(Model& m);

// The window the page claims: NN_Help's class (exe::VtHelp), named for the runtime's lookups.
extern const hk::Sym* const kWindowClass;
constexpr const char* kWindowName = "NN_Help";

// INFO's actions: the runtime's only way to act for the page (pages\pagerun.cpp walks this table through screens::Runner).
enum Action
{
    kActShowOnMap = 1, // Y / the SHOW ON MAP chip: the NeuroNet's map tab, the game's own tab command
};
extern const screens::ActionDesc kActions[];
extern const int kActionCount;
} // namespace infobody
