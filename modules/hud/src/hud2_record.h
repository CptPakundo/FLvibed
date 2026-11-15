// HUD 2's frame recorder (audit 71b B4b; the test verb `hud2 bench record`): the next n live frames as the design bench
// replays them - per frame one state file (hud2_state_io.h StateDoc, flstate/0: Update's input and the state Step took,
// Render's input and the state Draw took, the under pass's, what each half asked of the host, the host's answers each
// half used as a tape) and one display list (core\ui\hud2_kit_fldl.h, fldl/0: the under and over passes as the kit
// submitted them, with their cut marks), tied together by fingerprints (the list's hash and its inputs' hash in the
// record, the inputs' hash in the list, each font set's fingerprint, each layer's kitdump hash). Host code, outside the
// game-free core: it wraps the game's FrameHost and copies the kit's open passes. Off (the default), hud2.cpp tests one
// flag per Update / Render and nothing of this file runs; on, the frames are drawn exactly as without it (the wrappers
// return the game's own answers) and only the writing costs time.
#pragma once
#include "hud2_frame.h"

#include <iosfwd>
#include <string>

namespace hud2rec
{
extern bool g_on; // a recording is armed (frames left): hud2.cpp's one test per frame half
inline bool On() { return g_on; }

// `hud2 bench record [<n> <folder> [<name>]]`: arms n frames (1..200) into <folder> (made), files
// <name>_<W>x<H>_f<k>.flstate.json and .fldl.json (<name>: the sim's or "live", then _pad / _kbm); 0 stops; bare: the
// state (left, written, skipped, failed, the last file, the last error).
std::string Command(std::istream& in, const std::string& defaultName);
// Stops a recording (HUD 2 gives its things back): a frame half recorded is dropped.
void Stop(const char* why);

// The game's host with each answer also put on the tape of the half being recorded (Step's, Draw's).
hud2::FrameHost Host(const hud2::FrameHost& game);
enum class Half
{
    None,
    Step,
    Draw
};
void Tape(Half half);

// Update, a live frame: the input and the state Step takes, what StepStart decided; then what Step asked.
void BeforeStep(const hud2::FrameIn& in, const hud2::FrameState& st, hud2::Phase phase, const hud2::StartOut& start);
void AfterStep(const hud2::StepOut& out);
// RenderUnderHud: the under pass (drawn: called with its pass open, before End).
void Under(bool drawn, int sw, int sh, int px, const bool back[2], const hud2::FrameState& st);
// Render: the input and the state Draw takes, the pass's size and text sizes; then the over pass, before End; then the
// frame's files written.
void BeforeDraw(const hud2::DrawIn& in, const hud2::FrameState& st, int sw, int sh, int askPx, int drawPx, bool iconsReady);
void BeforeEnd();
void FrameDone();
} // namespace hud2rec
