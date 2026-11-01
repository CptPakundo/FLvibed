// Subtitles for the scene mixes (gf_sounds.ini mix_*: 19 room scenes whose dialogue is one recorded track, e.g. the
// landing at Manhattan, Lonnigan on the pad). Their lines: EXE\FLSubtitles\mixes.tsv (data/mixes.tsv: mix, start, end,
// speaker, text on the mix's own clock), word starts from word_times.tsv rows with voice "mix". The track starts when the
// game starts the mix's sound (PlayArchetype, OnSoundStart) and runs on the wall clock; each line is shown when its
// window starts, like any scene line (letterboxed placement, word-timed pages). The track stops when its last line is
// over or the scene's letterbox goes (the scene ended or was skipped: its lines are retired at once). Pure parts:
// mix_cues.h.
#pragma once
#include "common.h"

#include <string>

namespace mixcues
{
// mixes.tsv + the "mix" rows of word_times.tsv next to it; false when there is no mixes.tsv (none shown, no error).
bool Load(const std::string& path);
// A sound archetype started (its nickname id): true when it is a scene mix with lines (its track starts).
bool OnSound(u32 id);
// Each frame: the lines that are due, the end of the track.
void Tick();
// The game left for the menu (or `clear`): the track stops.
void Stop();
// Debug `state`: lines loaded, the track playing and where.
std::string Describe();
// Every loaded line's speaker and text (the font's characters: display::SeedGlyphs).
std::wstring Text();
} // namespace mixcues
