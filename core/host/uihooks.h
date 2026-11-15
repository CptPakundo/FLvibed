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
// text gate or neither: TargetMuteReady says whether the mute can hold. Idempotent; an unsigned symbol leaves the game's drawing as it is. HUD 2's alone:
// installed at its first selection (router::InstallHud2Hooks), never at the start.
void InstallTargetMute();
void SetTargetMute(bool on);
bool TargetMuteReady();
// The nav map's political and patrol zones (exe::NavMapZoneBuild, issue 61): while HUD 2's map asks (SetQuietZones every frame; an ask holds 500 ms), the filter ALL
// builds every zone (the game's ALL keeps only the neutral ones), each in its dominant relation's colour (core\game\zonemix.h), neutral triangles first, and every
// zone's outer edges are kept for HUD 2 to outline (MapZoneEdges: 7 floats a segment, x, y, z of each end as the builder left them, then the D3DCOLOR's bits;
// navview::ProjectZone places them). Flags kZoneCells: on the political layer HUD 2's influence cells stand in for the fill and the outline. The single filters
// stay the game's own. HUD 2's alone: installed at its first selection. Idempotent.
constexpr int kZoneCells = 1;
void InstallMapZones();
void SetQuietZones(int flags);
int MapZoneEdges(float* xz, int maxSegments);
// The political zones the last HUD 2 build drew (their ids: CreateID of the nickname), the player's reputation vibe the build read (0 none yet), the game's
// palette by relation (3 D3DCOLORs: friendly, neutral, hostile). Returns the ids' count.
int MapZoneIds(u32* ids, int maxIds, int* vibe, u32* colours);
void TraceKeys(bool on);                       // debug `trace keys`
bool TraceKeys();
} // namespace uihooks
