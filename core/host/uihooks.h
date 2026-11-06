// FLvibed - the UI hooks two modules share: the game's key handler (the maneuver lock's answer, the HUD's wake cues, the key trace) and the UI generation counter
// (one count per control added or removed: the window registry, core\ui\winplates.*, rescans on it). Installed once, by the controller's host part or by
// core's page host when the controller is off and the HUD runs.
#pragma once
#include "common.h"

namespace uihooks
{
// Idempotent. onKeyCmd sees a key command after the game handled it, `down` = its keymap state.
void Install(void (*onKeyCmd)(int cmd, bool down));
// 1 + adds + removes: grows with every change of the control tree; 0 = the detours are not installed (the registry falls back to its poll).
u32 Generation();
void GenerationReply(char* reply, size_t len); // the `uigen` verb
// The game's damage indicator's hits on the player ship (the HUD's hit cues, the autopilot). Idempotent.
void InstallHits();
u32 PlayerHit(float pos[3], u32* kind, u32* tick); // sequence, 0 = none yet
// The game's target label and bars left out of its indicator draw while HUD 2's targeting display stands (UI_HUD2_SKIN.md 6.10). Both the vertex gate and the
// text gate or neither: TargetMuteReady says whether the mute can hold. Idempotent; an unsigned symbol leaves the game's drawing as it is.
void InstallTargetMute();
void SetTargetMute(bool on);
bool TargetMuteReady();
void TraceKeys(bool on);                        // debug `trace keys`
bool TraceKeys();
} // namespace uihooks
