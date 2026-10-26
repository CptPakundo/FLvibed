#include "chat.h"

#include "flexe.h"
#include "log.h"
#include "feed.h"
#include "display.h"
#include "game/render.h"

#include <vector>

namespace chat
{
// RDL node types (rich text buffer: {u32 type, u32 length, payload})
enum Node : u32
{
    TraNode = 1,
    TextNode = 2,
    ParagraphNode = 5, // a line break inside one entry (u16 0 payload; adoxa's Console BinaryRDL)
    StyleNode = 6
};
constexpr u16 kStyleDialog = 0x5C12;  // DATA\FONTS\rich_fonts.ini style (aqua, font 1)
constexpr u16 kStyleConsole = 0x73F7; // STYLE_CONSOLE (font 0: Arial Unicode MS 22, the smallest with a known id)

struct Buffer
{
    std::vector<u8> b;
    void Add(u32 type, const void* data, u32 len)
    {
        const size_t at = b.size();
        b.resize(at + 8 + len);
        memcpy(&b[at], &type, 4);
        memcpy(&b[at + 4], &len, 4);
        memcpy(&b[at + 8], data, len);
    }
    void Tra(u32 tra, u32 mask)
    {
        const u32 v[2] = {tra, mask};
        Add(TraNode, v, 8);
    }
    void Style(u16 style) { Add(StyleNode, &style, 2); }
    void Para()
    {
        const u16 zero = 0;
        Add(ParagraphNode, &zero, 2);
    }
    void Text(const std::wstring& s) { Add(TextNode, s.c_str(), static_cast<u32>((s.size() + 1) * 2)); }
};

// The chat window wraps only at its own, very wide limit (far past the chat panel: the eyes travel across half the
// screen; user, 19 Oct 2026): rows are broken here at the width of the lower-left HUD panels instead (they end 0.613 H
// from the left edge; the chat starts ~0.01 H in: 0.60 H), measured in the chat's own fonts at its on-screen size -
// the text Agency FB (rich_fonts.ini STYLE_DIALOG, em 0.0323 H), the name Arial Unicode MS (em ~0.021 H) (calibrated on
// a 4K screenshot, 19 Oct 2026) - at word boundaries; continuation rows go without the name.
struct Measure
{
    HDC dc = nullptr;
    HFONT text = nullptr, name = nullptr, plain = nullptr; // plain: the name font not bold (a faction)
    HGDIOBJ old = nullptr;
    Measure(int sh)
    {
        display::GameFonts();
        dc = CreateCompatibleDC(nullptr);
        text = CreateFontW(-static_cast<int>(sh * 0.0323f), 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                           CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Agency FB");
        name = CreateFontW(-static_cast<int>(sh * 0.021f), 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                           CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Arial Unicode MS");
        plain = CreateFontW(-static_cast<int>(sh * 0.021f), 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                            CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Arial Unicode MS");
        if (dc && text)
            old = SelectObject(dc, text);
    }
    ~Measure()
    {
        if (dc && old)
            SelectObject(dc, old);
        if (text)
            DeleteObject(text);
        if (name)
            DeleteObject(name);
        if (plain)
            DeleteObject(plain);
        if (dc)
            DeleteDC(dc);
    }
    // font: 0 the text, 1 the name (bold), 2 the name font not bold
    int Width(const std::wstring& s, int font = 0)
    {
        if (!dc)
            return static_cast<int>(s.size()) * 20;
        HFONT f = font == 1 ? name : font == 2 ? plain : nullptr;
        if (f)
            SelectObject(dc, f);
        SIZE sz = {};
        GetTextExtentPoint32W(dc, s.c_str(), static_cast<int>(s.size()), &sz);
        if (f)
            SelectObject(dc, text);
        return sz.cx;
    }
};

static bool EndsSentence(const std::wstring& w)
{
    const wchar_t c = w.empty() ? 0 : w.back();
    return c == L'.' || c == L'!' || c == L'?' || c == L'\x2026';
}

// A clause ends after a comma or a dash (a spaced en / em dash, or a word ending in one: "1 dash 2" joins as 1-2,
// a spoken dash as " – ").
static bool EndsClause(const std::wstring& w)
{
    const wchar_t c = w.empty() ? 0 : w.back();
    return c == L',' || c == L';' || c == L':' || c == L'\x2013' || c == L'\x2014';
}

// firstMax / secondMax: the room on the first two rows (a name, a faction before them), rowMax every row after.
// A sentence that would not fit in the rest of a row after the one before it starts a row of its own (user, 22 Oct
// 2026: "If a sentence ends and the next sentence would trigger word-wrap, just start that next sentence in a new line");
// the same after a comma or a dash for the clause that follows ("add commas and em dashes" - up to its next break).
static std::vector<std::wstring> WrapRows(Measure& m, const std::wstring& text, int firstMax, int rowMax,
                                          int secondMax = -1)
{
    std::vector<std::wstring> words;
    {
        std::wstring word;
        for (wchar_t c : text)
            if (c == L' ')
            {
                if (!word.empty())
                    words.push_back(word), word.clear();
            }
            else
                word += c;
        if (!word.empty())
            words.push_back(word);
    }
    std::vector<std::wstring> rows;
    std::wstring row;
    int budget = std::max(firstMax, rowMax / 4);
    auto newRow = [&](const std::wstring& w)
    {
        rows.push_back(row);
        row = w;
        budget = rows.size() == 1 && secondMax >= 0 ? std::max(secondMax, rowMax / 4) : rowMax;
    };
    for (size_t i = 0; i < words.size(); ++i)
    {
        const std::wstring& w = words[i];
        const bool sentence = i > 0 && EndsSentence(words[i - 1]), clause = i > 0 && EndsClause(words[i - 1]);
        if (!row.empty() && (sentence || clause))
        {
            // a new sentence (or clause): all of it on this row, or it begins the next - a sentence up to its end, a
            // clause up to its next comma, dash or the sentence's end
            std::wstring part = w;
            for (size_t j = i + 1;
                 j < words.size() && !EndsSentence(words[j - 1]) && (sentence || !EndsClause(words[j - 1])); ++j)
                part += L" " + words[j];
            if (m.Width(row + L" " + part) > budget)
            {
                newRow(w);
                continue;
            }
        }
        const std::wstring next = row.empty() ? w : row + L" " + w;
        if (!row.empty() && m.Width(next) > budget)
            newRow(w);
        else
            row = next;
    }
    if (!row.empty())
        rows.push_back(row);
    return rows;
}

void Print(const std::wstring& name, u32 rgb, const std::wstring& text)
{
    feed::Push("chat", "line", name, text);
    typedef void(__cdecl * FlushFn)(int, const void*, u32);
    const FlushFn flush = exe::ChatFlush.fn<FlushFn>();
    if (!flush)
        return;
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh <= 0)
        sh = 1080;
    Measure m(sh);
    // rows end just inside the panels' right edge (seen in game); with HUD 2 inside its chat band
    const int rowMax = static_cast<int>(display::ChatRowMax(sw > 0 ? sw : sh * 16 / 9, sh));
    // TRA: colour in the high 24 bits (0xBBGGRR00), style bits in the low byte (1 bold).
    auto tra = [](u32 c) { return (((c & 0xFF) << 16) | (c & 0xFF00) | ((c >> 16) & 0xFF)) << 8; };
    // A speaker named "name · faction" (open-world NPCs, display's speaker names; user, 21/22 Oct 2026, layout 2 of a
    // mockup of four: "doesn't go so horizontal as to endanger the center of the screen"): the name over the faction,
    // the text in a column beside them - "Name: text...", the faction with the second row, every further row in the
    // same column (aligned after "Name: " by spaces measured in the chat's fonts: the log has no tab stop). The first
    // two rows are one entry of the log ("these two lines should act like one line"). Other speakers: "Name: text".
    const size_t dot = name.find(L" \x00B7 ");
    // Narrow screens (4:3, 5:4; chat_layout_narrow 1): a header row - the name, then the faction beside it, dimmer -
    // and the dialogue under it at the full width (user, 22 Oct 2026: "narrow aspect ratios might make better use of the
    // layout where you have one row for name and faction, and under it you can have the dialogue itself"); the spacer,
    // the header and the first text row are one entry of the log.
    const bool narrow = sw > 0 && sw < sh * 1.42f;
    const int layout = narrow ? display::Get().chatLayoutNarrow : display::Get().chatLayoutWide;
    if (dot != std::wstring::npos && layout == 1)
    {
        const std::wstring who = name.substr(0, dot), faction = name.substr(dot + 3);
        const std::vector<std::wstring> rows = WrapRows(m, text, rowMax, rowMax);
        Buffer head;
        head.Style(kStyleConsole);
        head.Text(L" ");
        head.Para();
        head.Style(kStyleDialog);
        head.Tra(tra(rgb) | 1, 0xFFFFFFFF);
        head.Text(who);
        head.Style(kStyleDialog);
        const u32 dim =
            ((((rgb >> 16) & 0xFF) * 7 / 10) << 16) | ((((rgb >> 8) & 0xFF) * 7 / 10) << 8) | ((rgb & 0xFF) * 7 / 10);
        head.Tra(tra(dim), 0xFFFFFFFF);
        head.Text(L"  " + faction);
        if (!rows.empty())
        {
            head.Para();
            head.Style(kStyleDialog);
            head.Text(rows[0]);
        }
        flush(0, head.b.data(), static_cast<u32>(head.b.size()));
        for (size_t i = 1; i < rows.size(); ++i)
        {
            Buffer buf;
            buf.Style(kStyleDialog);
            buf.Text(rows[i]);
            flush(0, buf.b.data(), static_cast<u32>(buf.b.size()));
        }
        return;
    }
    if (dot != std::wstring::npos)
    {
        const std::wstring who = name.substr(0, dot), faction = name.substr(dot + 3);
        // the column: two text-font spaces after the wider of "Name:" (bold) and the faction (plain; one space left a
        // long faction touching the text, 22 Oct 2026); each row padded
        // with text-font spaces to it (measured as the game draws them: the colon in the name font, the spaces in the
        // text's)
        const int space = std::max(1, m.Width(L" ")), whoW = m.Width(who + L":", 1), factionW = m.Width(faction, 2);
        const int indent = std::max(whoW, factionW) + 2 * space;
        auto pad = [&](int from)
        { return std::wstring(static_cast<size_t>(std::max(1, (indent - from + space / 2) / space)), L' '); };
        const std::vector<std::wstring> rows = WrapRows(m, text, rowMax - indent, rowMax - indent);
        const std::wstring column = pad(0);
        // a short spacer row first, in the smallest chat style (user, 22 Oct 2026: "this layout does require some
        // vertical spacing between different chat lines")
        Buffer head;
        head.Style(kStyleConsole);
        head.Text(L" ");
        head.Para();
        head.Style(kStyleDialog);
        head.Tra(tra(rgb) | 1, 0xFFFFFFFF);
        head.Text(who + L":");
        head.Style(kStyleDialog);
        head.Text(pad(whoW) + (rows.empty() ? std::wstring() : rows[0]));
        head.Para();
        head.Style(kStyleDialog);
        // the faction in the name's colour, dimmed to 70 %, not bold
        const u32 dim =
            ((((rgb >> 16) & 0xFF) * 7 / 10) << 16) | ((((rgb >> 8) & 0xFF) * 7 / 10) << 8) | ((rgb & 0xFF) * 7 / 10);
        head.Tra(tra(dim), 0xFFFFFFFF);
        head.Text(faction);
        if (rows.size() > 1)
        {
            head.Style(kStyleDialog);
            head.Text(pad(factionW) + rows[1]);
        }
        flush(0, head.b.data(), static_cast<u32>(head.b.size()));
        for (size_t i = 2; i < rows.size(); ++i)
        {
            Buffer buf;
            buf.Style(kStyleDialog);
            buf.Text(column + rows[i]);
            flush(0, buf.b.data(), static_cast<u32>(buf.b.size()));
        }
        return;
    }
    const int nameW = name.empty() ? 0 : m.Width(name + L": ", 1);
    const std::vector<std::wstring> rows = WrapRows(m, text, rowMax - nameW, rowMax);
    for (size_t i = 0; i < rows.size(); ++i)
    {
        Buffer buf;
        buf.Style(kStyleDialog); // the game's dialog font for the whole line (name included)
        if (i == 0 && !name.empty())
        {
            buf.Tra(tra(rgb) | 1, 0xFFFFFFFF);
            buf.Text(name + L":");
        }
        buf.Style(kStyleDialog);
        buf.Text((i == 0 && !name.empty() ? L" " : L"") + rows[i]);
        flush(0, buf.b.data(), static_cast<u32>(buf.b.size()));
    }
}
} // namespace chat
