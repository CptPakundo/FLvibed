// The extras that run under both interfaces (the owner's decision of 12 Nov 2026: kept, optional, on the classic HUD too): the shield and hull
// cue, the reticle cue, game_text_size and the idle cursor's hide. Part of the HUD module (with it off they are off, as always), but no file of
// this folder includes HUD 2's: a change to HUD 2 cannot reach them. Their places in core's frame are views::extras (core\ui\views.h),
// published at the module's Attach and called through core's door (core\ui\hud_door.h).
#pragma once
#include "common.h"

namespace extras
{
// What the idle cursor's hide asks of HUD 2, which decides when its flight HUD rests (hudwake's CONTEXTUAL fade). HUD 2 sets it while it is selected
// (the door's Enter to its Leave); unset, the classic HUD's rule: free flight with only the HUD up, no in-flight scene (the owner's decision 62).
struct CursorFeed
{
    bool (*eligible)();      // HUD 2 shows and rests: the cursor may idle-hide
    void (*hidden)(bool on); // what the hide did, for HUD 2's state line
    bool (*cover)();         // a NeuroNet tab is on its way with no window up: the flight cursor goes under the cover
};
void SetCursorFeed(const CursorFeed& feed);

// At the UI pass's entry, every frame on both interfaces: the game's hover help hidden in flight unless the mouse moved in the last 3 s (UI_HUD2.md "the
// hover help"). `space`: free flight, no NeuroNet window on its way (the classic HUD's door passes it with the pad only); `menuOver`: a modal menu over
// the flight keeps the game's own.
void HoverHelp(bool space, bool menuOver);

void Attach(); // publishes views::extras
void Detach();
void Shutdown();
void Fault(int phase);
// The verb `cue ...`; false: not it.
bool Command(const std::string& op, std::istream& in, std::string& reply);
// `gametext [rich <percent>]`: the size as applied (gametext.h); rich: that table alone, now (research). The module's entry reads the verb's two
// words and hands it the ones that are not HUD 2's window text.
std::string GameTextVerb(const std::string& what, int pct);
} // namespace extras
