// FLvibed - what the Subtitles and Movies modules share of a movie: which one it is, and the window over its video. Movie.dll (2003,
// DirectShow) plays through the renderer's own child window while the game's frame loop and its Present stop, so Direct3D drawing is not
// seen. What is drawn over a movie goes through one 32-bit DIB of the game window's client size, shown by a child window of the game
// window above the renderer's: its window region = the margins around the fitted picture (the Movies module's rectangle) + the lines
// (the Subtitles module's Content). Whatever captures the game window shows it. Fallback when that child cannot be made: a transparent,
// click-through layered window owned by the game window. Neither module is needed for the other: null Content draws no lines, no
// picture rectangle means no margins. docs\DISPLAY_PLAN.md "Movies".
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace moviescreen
{
struct Info
{
    std::string id;     // the movie's stem, the intro recognised by its duration and size, not its name
    int w = 0, h = 0;   // the encoded frame (0: unknown)
    long long bytes = 0;
    double seconds = 0; // the header's play duration
};

// What Content drew this frame: per line its rectangle and backing (0 none .. 1 full), a key of everything drawn (unchanged = the same
// picture), whether a transition runs (redrawn at the display's rate).
struct Painted
{
    const std::vector<RECT>* rects = nullptr;
    const std::vector<float>* box = nullptr;
    unsigned long long key = 0;
    bool animating = false;
};

// The lines over the movie, from the Subtitles module. Lines: on the movie clock (`t` seconds), every tick. Paint: into premultiplied
// BGRA pixels; `picture` is the fitted picture in those pixels (empty: not fitted), `translucent` whether the target blends.
struct Content
{
    void (*Lines)(double t);
    void (*Paint)(u32* pixels, int w, int h, const RECT& picture, bool translucent, Painted& out);
};
inline Content content = {};

double Seconds(); // a monotonic clock
// The movie starts. Both modules call it for the same event: the second call is a no-op.
void Begin(HWND hwnd, const char* path);
// The movie ended: the windows go. The second call is a no-op.
void End();
// The logic goes: End, and the window classes, whose procedures are this logic's code.
void Shutdown();
bool Playing();
const Info& Current();
double Clock();                         // seconds since Begin
HWND Game();
HWND Video();                           // the renderer's window, found when it exists
void SetPicture(const RECT& dest, const RECT& src); // in the video window's pixels; in native video pixels
const RECT& Source();                   // the part of the video shown (empty: all of it)
void Pass();                            // one pass of the movie loop: counted, then a Tick
void Tick();                            // the lines, then the picture at its rate
bool Animating();
double RefreshSeconds();                // the display's frame time
const char* PathName();                 // "child", "overlay", "none"
u32 Passes();
u32 Redraws();
u32 Paints();
std::string GapReport();                // the frame times of the transitions drawn in this movie
} // namespace moviescreen
