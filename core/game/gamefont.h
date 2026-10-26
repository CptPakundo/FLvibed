// FLvibed - the game's own fonts, resolved once per module part so FLvibed's text shares the game's typefaces: the label
// face (Agency FB in every known build: HUD labels, the objective banner, the Options screen's rows) and the body face
// (Arial Unicode MS: lists, tooltips, infocards; the widest character coverage). Moved out of subtitles\src\impl\
// display.cpp on 25 Oct 2026 (the UI build) so the options page and the pad HUD share it; compiled into each module
// part that uses it (core\game).
//
// Resolution (fix-text, 25 Oct 2026; the vanilla 1.1 run found Segoe UI everywhere): the faces' names come from the
// game's DATA\FONTS\fonts.ini [TrueType] (nickname HudSmall = the label face, ListText = the body face; the names above
// when absent), the file read as text (HDE) or binary BINI (vanilla); then each face from, in order:
//   1. the game's font files - fonts.ini [FontFiles] path = ... loaded for this process only (FR_PRIVATE), as the game
//      itself loads them (HDE ships the TTFs there),
//   2. the same face installed in Windows (vanilla's installer puts Agency FB into Windows\Fonts, its DATA\FONTS has none),
//   3. the fallback, Segoe UI.
// One log line names the source of each face.
#pragma once
#include "common.h"

namespace gamefont
{
// Resolves the faces once; true when the label face resolved (game files or Windows), false = the Segoe UI fallback.
bool Load();
// The label face (Agency FB when it resolves, else Segoe UI) and its weight the game's labels read best with.
const wchar_t* LabelFace();
int LabelWeight();
// The body face (Arial Unicode MS when it resolves, else Segoe UI).
const wchar_t* TextFace();
int TextWeight();
// Where a face came from: 1 the game's DATA\FONTS files, 2 Windows' installed fonts, 0 the fallback.
int LabelSource();
int TextSource();
} // namespace gamefont
