// The lines over the movies Freelancer plays (startup intro): EXE\FLSubtitles\movies.tsv, timed from the movie start, drawn with the
// software renderer (same font, layout and style) over core's window (core\game\moviescreen.h). The path is logged per movie.
#pragma once
#include "common.h"
#include "settings.h"

#include <string>

namespace movie
{
bool Load(const std::string& path);
// The lines join core's window (at Init).
void Attach();
// Movie.dll PlayMovie(hwnd, ?, path) / UpdateMovie() (once per movie loop pass) / StopMovie().
void OnStart(HWND hwnd, const char* path);
void OnUpdate();
void OnStop();
// The logic goes (a reload, an unload): a movie keeps playing, without its lines, and nothing of this code stays behind: the sampler and
// (moviescreen) the windows, their classes.
void Shutdown();
// debug `moviesample on|off`: the frame sampler for the lines' backing box (off: no frame reads; the audio A / B)
std::string SetSampling(bool on);
// Every loaded line's speaker and text, concatenated (the characters the font must hold; display::SeedGlyphs).
std::wstring Text();
} // namespace movie
