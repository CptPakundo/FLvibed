// Subtitle text (EXE\FLSubtitles\subtitles.tsv, built by asr/merge.py) indexed the way the game names
// its sounds: CreateID of the line id and of the voice nickname.
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace data
{
enum class Kind
{
    Story,  // cutscene / room scene line, shown as written
    Unique, // mission character line
    Chunk,  // generic piece joined into a sentence by the runtime
    Nn      // Neural Net message
};

struct Line
{
    std::string id, voice;
    Kind kind;
    float sec;
    std::wstring speaker; // display name, may be empty
    std::wstring text;
    u32 textFnv = 0;               // FNV-1a of the UTF-8 text field (word_times.tsv rows must match it)
    std::vector<float> wordStarts; // seconds from the clip's start, one per whitespace word of `text`; empty: untimed
    float speechEnd = 0;           // the last word's end (seconds)
};

// Freelancer's nickname hash (Common.dll CreateID).
u32 CreateID(const char* s);

bool Load(const std::string& path);
// Word timings from the review's forced alignment (word_times.tsv next to subtitles.tsv): attached to their lines when
// the row's text hash matches the line's text (a line edited since its alignment stays untimed). Call after Load.
void LoadWordTimes(const std::string& path);
size_t Count();
// Every line in file order.
const std::vector<Line>& Lines();
// The line for (voice, msg), else any voice's line for msg; null if unknown.
const Line* Find(u32 voice, u32 msg);
// Display name for a voice nickname hash from named lines (empty if none).
const std::wstring& VoiceName(u32 voice);

// One tab-separated line (no line end) into its fields: every field kept, empty ones too (a line of n tabs gives n + 1
// fields, a trailing tab a last empty one); no trimming. The readers of subtitles / movies / movie_frames /
// speaker_colors.tsv all use it.
std::vector<std::string> SplitTsv(const std::string& line);

std::wstring Widen(const std::string& utf8);
std::string Narrow(const std::wstring& w);
} // namespace data
