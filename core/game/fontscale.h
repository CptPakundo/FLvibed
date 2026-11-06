// FLvibed - the game's own text, larger ("very legible and readable in all windows, all HUD elements,
// all modals - EVERYTHING"). The game keeps two font tables, both from DATA\FONTS: fonts.ini (a fraction of the screen's height
// per nickname, every text control) and rich_fonts.ini (pixels at 768 lines per id, the rich text: infocards, job and news
// text). docs\FREELANCER_RE.md "The game's text size is its font's". Pure: the files' reading and the sizes; gametext.cpp writes.
// The sizes always come from the files, so applying twice, or after a hot reload, never compounds.
#pragma once
#include <cstdlib>
#include <string>
#include <vector>

namespace fontscale
{
struct Fixed
{
    std::string nickname;
    float height = 0;
    std::string face;
};
struct Rich
{
    int id = 0;
    std::string face;
    int size = 0;
};

// What is already large stays: a heading or a button has no need of it and is what collides first (130 %, 1280x800: INVENTORY ran
// into the credits under it, ACCEPT JOB out of its button; the title card's font is 100).
constexpr float kFixedKeep = 0.04f;
constexpr int kRichKeep = 60;

inline float Scaled(float height, int percent) { return height >= kFixedKeep ? height : height * percent / 100.0f; }
inline int Scaled(int size, int percent) { return size >= kRichKeep ? size : (size * percent + 50) / 100; }

inline std::string Trim(const std::string& s)
{
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
// "key = value" on a line; false for anything else (a comment, a section)
inline bool KeyValue(const std::string& line, std::string& key, std::string& value)
{
    const size_t eq = line.find('=');
    if (eq == std::string::npos || line[0] == ';')
        return false;
    key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
    return !key.empty();
}
template <class Fn> inline void Lines(const std::string& text, Fn fn)
{
    size_t at = 0;
    while (at <= text.size())
    {
        size_t end = text.find('\n', at);
        if (end == std::string::npos)
            end = text.size();
        fn(Trim(text.substr(at, end - at)));
        at = end + 1;
    }
}

// fonts.ini: each [TrueType]'s nickname and fixed_height
inline std::vector<Fixed> ParseFixed(const std::string& text)
{
    std::vector<Fixed> out;
    std::string nick, face;
    Lines(text, [&](const std::string& line)
          {
              std::string k, v;
              if (!line.empty() && line[0] == '[')
                  nick.clear(), face.clear();
              else if (KeyValue(line, k, v) && k == "nickname")
                  nick = v;
              else if (k == "font")
                  face = v;
              else if (k == "fixed_height" && !nick.empty())
                  out.push_back({nick, static_cast<float>(atof(v.c_str())), face});
          });
    return out;
}

// rich_fonts.ini: "font = id, face, size" lines of its [TrueType] section ([Style] sections say font = id alone)
inline std::vector<Rich> ParseRich(const std::string& text)
{
    std::vector<Rich> out;
    bool trueType = false;
    Lines(text, [&](const std::string& line)
          {
              std::string k, v;
              if (!line.empty() && line[0] == '[')
                  trueType = line == "[TrueType]";
              else if (trueType && KeyValue(line, k, v) && k == "font")
              {
                  const size_t a = v.find(','), b = a == std::string::npos ? a : v.find(',', a + 1);
                  if (b != std::string::npos)
                      out.push_back({atoi(v.c_str()), Trim(v.substr(a + 1, b - a - 1)), atoi(v.c_str() + b + 1)});
              }
          });
    return out;
}
} // namespace fontscale
