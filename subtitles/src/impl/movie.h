// Subtitles for the movies Freelancer plays (startup intro). Movie.dll (2003, DirectShow) plays through the video
// renderer's own child window inside the game window while the game's frame loop (and its Direct3D Present) stops, so
// Direct3D drawing is not seen: the lines are drawn with the software renderer (same font, layout and style) into a
// child window of the game window above the renderer's (a window region = the subtitles' rectangles + the margins
// around the fitted picture), so whatever captures the game window (window capture, the taskbar thumbnail, PrintWindow)
// shows them. Fallback when that child cannot be made: a transparent, click-through layered window owned by the game
// window. The path is logged per movie (docs\DISPLAY_PLAN.md "Movies").
// Lines: EXE\FLSubtitles\movies.tsv (data/movies.tsv), timed from the movie start.
#pragma once
#include "common.h"
#include "settings.h"

#include <string>

namespace movie
{
bool Load(const std::string& path);
// [Subtitles] movie_intro (the UI build's MOVIES row): tells the host which intro plays (HostApi v7 SetIntroOverride:
// 1 HD fl_intro.wmv, 2 the original 2003 fl_intro_vanilla.wmv, 3 the full Bonus DVD fl_intro_full.wmv, 0 as installed).
// A value naming a file this install lacks is reset to 0 and logged (only the files present are offered). Called after
// the settings load and whenever the row changes (OnSettingApplied: the bus, `set`, `config load`).
void ApplyIntro();
void OnSettingApplied(const settings::Def* d);
// Movie.dll PlayMovie(hwnd, ?, path) / UpdateMovie() (once per movie loop pass) / StopMovie().
void OnStart(HWND hwnd, const char* path);
void OnUpdate();
void OnStop();
// The movie playing, the drawing path, the movie loop's rate and the redraws / paints since it started (debug `state`).
std::string Describe();
// debug `moviesample on|off`: the frame sampler for the lines' backing box (off: no frame reads; the audio A / B)
std::string SetSampling(bool on);
// Every loaded line's speaker and text, concatenated (the characters the font must hold; display::SeedGlyphs).
std::wstring Text();
} // namespace movie
