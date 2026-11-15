// The INFO page's text (audit R3, docs\NEURONET_PLAN.md "INFO"): the pure half. nnhelp_screen.cpp lets the game's own appenders
// fill a RenderDisplayList the mod owns, in the order the game's NN_Help window fills its own (docs\FREELANCER_RE.md "The
// INFO page's composers"), and Common.dll's BinaryRDLWriter writes it out as records. This file reads those records into
// paragraphs of runs and lays them out into lines for a width. No game call, no system headers: tests\infobody_test.cpp
// runs the same code.
//
// The records (BinaryRDLWriter::write_buffer, read in Common.dll 12 Nov 2026): u32 type, u32 length, the payload, the next
// record at +8+length. 1 TRA (u32 attributes, u32 mask: bit 0 bold, 1 italic, 2 underline, 3-7 font, 8-31 colour: a
// COLORREF shifted up, 0xBBGGRR00), 2 TEXT (UTF-16, a NUL at the end), 4 POSITION, 5 PARA (a new paragraph; written with
// length 0), 6 STYLE (u16 id). JUST, PUSH and POP are not written. A card's `color="default"` is written as colour 0 under a
// set mask (the writer drops the default flag: audit 84_), so colour 0 is the style's own colour.
#pragma once
#include "common.h"

#include <algorithm>
#include <utility>

namespace inforules
{
constexpr u32 kTra = 1, kText = 2, kPara = 5, kStyle = 6;
constexpr u16 kStyleHeading = 0xF64; // STYLE_NN_BOLD: the market's headings ("Ships For Sale" ...)
constexpr u16 kStyleBody = 0x9CA4;   // STYLE_NN: everything else
constexpr u32 kBold = 1, kItalic = 2, kUnderline = 4;

struct Run
{
    std::wstring text;
    u32 attrs = 0;        // TRA bits as they stood (bold, italic, underline, font, colour)
    bool heading = false; // under STYLE 0xF64
};
struct Para
{
    std::vector<Run> runs;
    bool Empty() const
    {
        for (const Run& r : runs)
            if (!r.text.empty())
                return false;
        return true;
    }
    std::wstring Text() const
    {
        std::wstring s;
        for (const Run& r : runs)
            s += r.text;
        return s;
    }
};

// The records into paragraphs: a PARA closes the paragraph in hand (an empty one too: "\n\n" is a blank line). A malformed
// record ends the read (what was read stands).
inline std::vector<Para> Parse(const unsigned char* buf, size_t size)
{
    std::vector<Para> out(1);
    u32 attrs = 0;
    bool heading = false;
    size_t at = 0;
    while (at + 8 <= size)
    {
        u32 type, len;
        memcpy(&type, buf + at, 4);
        memcpy(&len, buf + at + 4, 4);
        if (len > size - at - 8)
            break;
        const unsigned char* p = buf + at + 8;
        if (type == kTra && len >= 8)
        {
            u32 a, mask;
            memcpy(&a, p, 4);
            memcpy(&mask, p + 4, 4);
            attrs = (attrs & ~mask) | (a & mask);
        }
        else if (type == kText)
        {
            std::wstring s;
            for (size_t i = 0; i + 1 < len; i += 2)
            {
                wchar_t c;
                memcpy(&c, p + i, 2);
                if (!c)
                    break;
                s += c;
            }
            if (!s.empty())
            {
                Run r;
                r.text = s, r.attrs = attrs, r.heading = heading;
                out.back().runs.push_back(r);
            }
        }
        else if (type == kPara)
            out.emplace_back();
        else if (type == kStyle && len >= 2)
        {
            u16 id;
            memcpy(&id, p, 2);
            heading = id == kStyleHeading;
        }
        at += 8 + len;
    }
    // (a list ends in an empty paragraph after its last PARA: nothing to show there)
    while (!out.empty() && out.back().Empty())
        out.pop_back();
    return out;
}

// The paragraphs as plain text, one paragraph a line: what the comparison test compares.
inline std::wstring PlainText(const std::vector<Para>& paras)
{
    std::wstring s;
    for (size_t i = 0; i < paras.size(); ++i)
        s += (i ? L"\n" : L"") + paras[i].Text();
    return s;
}

// A run's own colour as 0xRRGGBB (from the TRA bits' 0xBBGGRR00), 0 for none: the style's colour (a card's "default", and
// the text before any colour). No card sets black, which would not show on HUD 2's glass either. In HDE 0.7.1's cards the
// one colour set is #FF0000 (bases' "CLASSIFIED", the zones' warnings: audit 84_ "The styles in the game's data").
inline u32 CardColour(u32 attrs)
{
    const u32 bgr = attrs >> 8;
    return ((bgr & 0xFF) << 16) | (bgr & 0xFF00) | ((bgr >> 16) & 0xFF);
}

// A paragraph's styles for the comparison test (`nnpage info`): its runs, neighbours of equal attributes merged, each as
// " <characters>:<attributes, 8 hex digits>" with "h" for the heading style, after a "~"; empty when every run is plain
// (attributes 0, not a heading), so a plain card's dump reads as before.
inline std::wstring StyleLine(const Para& p)
{
    std::wstring s;
    bool styled = false;
    size_t n = 0;
    u32 attrs = 0;
    bool heading = false;
    auto flush = [&]()
    {
        if (!n)
            return;
        wchar_t b[32];
        swprintf(b, 32, L" %u:%08X%ls", static_cast<unsigned>(n), attrs, heading ? L"h" : L"");
        s += b;
    };
    for (const Run& r : p.runs)
    {
        if (r.text.empty())
            continue;
        styled |= r.attrs != 0 || r.heading;
        if (n && (r.attrs != attrs || r.heading != heading))
            flush(), n = 0;
        attrs = r.attrs, heading = r.heading, n += r.text.size();
    }
    flush();
    return styled ? L"~" + s : std::wstring();
}

// A superscript digit (the planets' "10\x00B2\x2074 kg": U+00B9, U+00B2, U+00B3, U+2070, U+2074..U+2079) as its plain
// digit, 0 for any other character: the page's font has none of U+2070..U+2079, so it draws them small and raised.
inline wchar_t SuperDigit(wchar_t c)
{
    if (c == 0x00B9)
        return L'1';
    if (c == 0x00B2 || c == 0x00B3)
        return static_cast<wchar_t>(L'2' + (c - 0x00B2));
    if (c == 0x2070)
        return L'0';
    if (c >= 0x2074 && c <= 0x2079)
        return static_cast<wchar_t>(L'4' + (c - 0x2074));
    return 0;
}

// ---- the market's lists (the owner, 12 Nov 2026: "aligned, price column"). The game writes each section as a heading
// paragraph (STYLE_NN_BOLD: "Ships For Sale ") and then one paragraph per item, indented with spaces - four before the
// first, five before the rest ("    Boron $155", "     Consumer Goods $66") - the price, where the section has one, after
// the item's last space; a paragraph of spaces closes the section. HUD 2 lays them out in its own grid: every name at the
// same indent under its heading, the prices right-aligned in one column shared by every section. The words, the items and
// their order stay the game's: only the spaces around them become places.
struct MarketItem
{
    std::wstring name, price; // price empty: the section has none (ships, equipment)
};
inline bool Blank(const std::wstring& s) { return s.find_first_not_of(L' ') == std::wstring::npos; }
// A heading paragraph: text, and every run of it under the heading style.
inline bool HeadingPara(const Para& p)
{
    bool any = false;
    for (const Run& r : p.runs)
        if (!r.text.empty())
        {
            if (!r.heading)
                return false;
            any |= !Blank(r.text);
        }
    return any;
}
// An item paragraph's name and price: false when it is no item (no leading space, only spaces, or runs of mixed attributes:
// those keep the game's own spacing, nothing of their look lost).
inline bool MarketItemOf(const Para& p, MarketItem& out)
{
    const std::wstring s = p.Text();
    if (s.empty() || s[0] != L' ' || Blank(s))
        return false;
    bool first = true;
    u32 attrs = 0;
    for (const Run& r : p.runs)
        if (!r.text.empty())
        {
            if (!first && (r.attrs != attrs || r.heading))
                return false;
            attrs = r.attrs, first = false;
        }
    const size_t a = s.find_first_not_of(L' '), b = s.find_last_not_of(L' ');
    const std::wstring t = s.substr(a, b - a + 1);
    out = MarketItem();
    out.name = t;
    const size_t sp = t.find_last_of(L' ');
    if (sp == std::wstring::npos || sp + 2 >= t.size() || t[sp + 1] != L'$')
        return true;
    bool digit = false;
    for (size_t i = sp + 2; i < t.size(); ++i)
    {
        if (t[i] >= L'0' && t[i] <= L'9')
            digit = true;
        else if (t[i] != L',' && t[i] != L'.')
            return true; // ("$" followed by something else: part of the name)
    }
    if (!digit)
        return true;
    out.price = t.substr(sp + 1);
    out.name = t.substr(0, t.find_last_not_of(L' ', sp) + 1);
    return true;
}
// What each paragraph is: 1 a heading, 2 a market item (one that follows a heading or another item), 0 the rest.
inline std::vector<int> MarketRoles(const std::vector<Para>& paras)
{
    std::vector<int> role(paras.size(), 0);
    bool in = false;
    MarketItem m;
    for (size_t i = 0; i < paras.size(); ++i)
        if (HeadingPara(paras[i]))
            role[i] = 1, in = true;
        else if (in && MarketItemOf(paras[i], m))
            role[i] = 2;
        else
            in = false;
    return role;
}

// ---- the layout: whole words to a width, run by run, so a bold word stays bold where it wraps
struct Piece
{
    std::wstring text;
    float x = 0;
    float w = 0; // its width as measured
    u32 attrs = 0;
    bool heading = false;
};
struct Line
{
    std::vector<Piece> pieces;
    int para = -1;     // the paragraph it shows (a wrapped paragraph: several lines)
    bool item = false; // a market item's line: its name at the indent, its price (a piece starting with '$') in the column
};
// `text` in tokens (the spaces before a word and the word) to lines of `room` from x = 0: a wrap drops the spaces before the
// word, a word too wide stands alone. Each line's text and width.
template <class Width>
inline std::vector<std::pair<std::wstring, float>> WrapTokens(const std::wstring& text, float room, const Width& width)
{
    std::vector<std::pair<std::wstring, float>> out(1);
    size_t i = 0;
    while (i < text.size())
    {
        size_t j = i;
        while (j < text.size() && text[j] == L' ')
            ++j;
        while (j < text.size() && text[j] != L' ')
            ++j;
        std::wstring tok = text.substr(i, j - i);
        float w = width(tok);
        if (!out.back().first.empty() && out.back().second + w > room && !Blank(tok))
        {
            out.emplace_back();
            tok.erase(0, tok.find_first_not_of(L' '));
            w = width(tok);
        }
        out.back().first += tok, out.back().second += w;
        i = j;
    }
    return out;
}
// `width` measures a string at the page's text size. An empty paragraph is a blank line; a word wider than the box stands
// alone on its line (cut by the drawing, never lost). Leading spaces of a paragraph are kept, but for a market's items (above).
template <class Width> std::vector<Line> Layout(const std::vector<Para>& paras, float maxW, const Width& width)
{
    std::vector<Line> lines;
    // the market's grid: the names at the game's first indent (four spaces), the prices' column right of the widest priced
    // name by the same four spaces, never past the box
    const std::vector<int> role = MarketRoles(paras);
    const float indent = width(L"    "), gap = indent;
    float nameW = 0, priceW = 0;
    MarketItem m;
    for (size_t i = 0; i < paras.size(); ++i)
        if (role[i] == 2 && MarketItemOf(paras[i], m) && !m.price.empty())
            nameW = std::max(nameW, width(m.name)), priceW = std::max(priceW, width(m.price));
    const float column = std::min(maxW, indent + nameW + gap + priceW); // the prices' right edge
    for (size_t pi = 0; pi < paras.size(); ++pi)
    {
        const Para& para = paras[pi];
        if (role[pi] == 2 && MarketItemOf(para, m))
        {
            u32 attrs = 0;
            for (const Run& r : para.runs)
                if (!r.text.empty())
                {
                    attrs = r.attrs;
                    break;
                }
            const float pw = m.price.empty() ? 0.0f : width(m.price);
            // (a name too long for its room before the column wraps there, the price on its last line)
            const float room = m.price.empty() ? maxW - indent : std::max(column - pw - gap - indent, 1.0f);
            for (const auto& l : WrapTokens(m.name, room, width))
            {
                lines.emplace_back();
                Line& line = lines.back();
                line.para = static_cast<int>(pi), line.item = true;
                Piece pc;
                pc.text = l.first, pc.x = indent, pc.w = l.second, pc.attrs = attrs;
                line.pieces.push_back(pc);
            }
            if (!m.price.empty())
            {
                Piece pc;
                pc.text = m.price, pc.x = column - pw, pc.w = pw, pc.attrs = attrs;
                lines.back().pieces.push_back(pc);
            }
            continue;
        }
        lines.emplace_back();
        lines.back().para = static_cast<int>(pi);
        float x = 0;
        bool lineStart = true;
        for (const Run& r : para.runs)
        {
            size_t i = 0;
            while (i < r.text.size())
            {
                // a token: the spaces before a word and the word
                size_t j = i;
                while (j < r.text.size() && r.text[j] == L' ')
                    ++j;
                while (j < r.text.size() && r.text[j] != L' ')
                    ++j;
                std::wstring tok = r.text.substr(i, j - i);
                float w = width(tok);
                if (!lineStart && x + w > maxW && tok.find_first_not_of(L' ') != std::wstring::npos)
                {
                    lines.emplace_back(); // wrap: the spaces before the word go
                    lines.back().para = static_cast<int>(pi);
                    tok.erase(0, tok.find_first_not_of(L' '));
                    w = width(tok), x = 0;
                }
                Line& line = lines.back();
                if (!line.pieces.empty() && line.pieces.back().attrs == r.attrs && line.pieces.back().heading == r.heading)
                    line.pieces.back().text += tok, line.pieces.back().w += w;
                else
                {
                    Piece pc;
                    pc.text = tok, pc.x = x, pc.w = w, pc.attrs = r.attrs, pc.heading = r.heading;
                    line.pieces.push_back(pc);
                }
                x += w, lineStart = false, i = j;
            }
        }
    }
    return lines;
}
} // namespace inforules
