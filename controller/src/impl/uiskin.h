// Extended HUD 2's skin over the game's non-flight windows (docs\UI_HUD2_EXTENDED.md 9-10; `ui_style` 1 = HUD 2,
// 0 = CLASSIC = the game's own look, untouched). The game's windows, rows, buttons, texts and flows stay: only their
// ART is taken away (state bit 1 on art children; the own mesh handle +0x84 = -1 on windows / buttons that draw their
// art themselves, after the draw slot was checked against the signed base draw) and HUD 2 glass is drawn under the UI
// pass in its place (plates from the window's own control rects, the modal scrim, the focus fill, header rules, tab
// plates, the room bar's slim glass and current-room underline, the NeuroNet box plate); titles are recoloured through
// the title text's own colour (+0x348). An unknown window stays native (fails closed); every write is restored when
// ui_style goes back to 0 (only the bytes still holding FLvibed's value: the game's own later writes win).
//
// The window registry: the open windows are found by their signed vtables when the host's UI generation counter moves
// (HostApi v23 UiGeneration: one count per control added / removed), on an older host by a cheap check at most every
// 500 ms; cached between. Per frame: the NavBar's frame / tab re-cleared (the game re-shows them every frame), a few
// cached reads (rects, the hovered control) and the quads.
#pragma once
#include "common.h"

#include <istream>
#include <string>

namespace uiskin
{
struct Context
{
    int style = 0;        // ui_style: 0 CLASSIC, 1 HUD 2
    bool pad = false;     // the pad is the device (the LB / RB glyphs at the room bar, the focus fill's source)
    bool enabled = true;  // the controller module is on
};
// Once per frame (OnFrame).
void Update(float dt, const Context& c);
// From OnRenderUnderHud, any mode: the registry refresh, the art hides / re-clears / restores, the glass.
void RenderUnder(void* device);
// A family's plate anchored at control `anchor` (e.g. "options", "OptionsBox"): its offsets from the anchor's centre in
// fractions of H (x0 y0 x1 y1) - the window frame the glass covers, measured once for both ui_style values. False when
// the family has no such plate.
bool PlateOffsets(const char* family, const char* anchor, float out[4]);
// The skin is on (ui_style 1 and the controller on): the prompt bar / options page take their HUD 2 look.
bool On();
// The prompt lane's geometry (docs\UI_HUD2_EXTENDED.md 13): collected every frame the pad is the device, under both
// ui_style values (under 0 from the same plate rectangles, with no drawing and no write; nothing while the keyboard and
// mouse are the device). Pixels.
// The NeuroNet box's bottom edge on a docked non-modal screen (0 = none): the last resort's top lane hangs under it.
float NeuroNetBottom();
// A docked screen with the room bar up and no modal menu (non-modal windows included).
bool DockedScreen();
// A modal menu (pause, save / load, Options) is shown.
bool ModalShown();
// The lowest bottom edge of a keep-out overlapping x0..x1 and reaching below `below` (0 = none).
float LowestKeepOut(float x0, float x1, float below);
// The left limit (pixels) of a pill row spanning y0..y1 that ends at `right`: right of everything the pills must not
// cover there this frame - the room bar, its sub-row (RoomControl icons) and LB / RB, the NeuroNet box, every window's
// plates (modal ones too; a plate-less window: its rect), the tabs, uistrip's strip - plus a gap; `right` when the band
// is blocked; 0.05 W at least.
float LaneLeft(float W, float H, float y0, float y1, float right);
// From the overlay pass (promptbar::Draw calls it first): what must sit over the game's own UI - the confirm dialog's
// focus (its native dimming box covers the under-pass glass). Nothing when the skin is off.
void DrawOver(void* device);
// A modal menu is shown now (pause, save / load, Options - the scrim's families - or the confirm dialog) while the skin
// is on (false under ui_style 0). From OnRenderUnderHud before the skin's own pass: the registry is brought current
// first (on the host's UI generation: a compare when nothing changed), then a few cached state reads - no search.
bool ModalOpen();
// Everything restored (hot reload, unload).
void Shutdown();
// Debug: uiskin state | windows | restore | rescan
std::string Command(std::istream& in);
} // namespace uiskin
