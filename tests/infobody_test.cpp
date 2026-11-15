// The INFO page's text, the pure half (core\game\screens\nnhelp_rules.h, audit R3): BinaryRDLWriter's records read into
// paragraphs (TEXT, PARA with length 0, a blank paragraph, TRA's attributes under their mask, STYLE 0xF64 as a heading, a
// malformed record ends the read), the plain text the comparison test compares, and the layout of whole words to a width;
// the card's styles (its colour, the style line, attributes through the layout: audit decision 56).
#include "../core/game/screens/nnhelp_rules.h"

#include "test.h"

namespace
{
struct Writer
{
    std::vector<unsigned char> b;
    void Rec(u32 type, const void* p, u32 len)
    {
        const unsigned char* h = reinterpret_cast<const unsigned char*>(&type);
        b.insert(b.end(), h, h + 4);
        h = reinterpret_cast<const unsigned char*>(&len);
        b.insert(b.end(), h, h + 4);
        const unsigned char* q = static_cast<const unsigned char*>(p);
        b.insert(b.end(), q, q + len);
    }
    void Text(const std::wstring& s) { Rec(inforules::kText, s.c_str(), static_cast<u32>((s.size() + 1) * 2)); }
    void Para() { Rec(inforules::kPara, nullptr, 0); }
    void Style(u16 id) { Rec(inforules::kStyle, &id, 2); }
    void Tra(u32 a, u32 mask)
    {
        const u32 v[2] = {a, mask};
        Rec(inforules::kTra, v, 8);
    }
};
} // namespace

void Test_infobody()
{
    using namespace inforules;
    // ---- the space body: header, blank line, zone name, paragraph, zone info
    {
        Writer w;
        w.Style(kStyleBody);
        w.Text(L"New York System, Sector D4");
        w.Para();
        w.Para();
        w.Text(L"Badlands");
        w.Para();
        w.Text(L"A nebula.");
        const std::vector<Para> p = Parse(w.b.data(), w.b.size());
        CHECK(p.size() == 4);
        CHECK(PlainText(p) == L"New York System, Sector D4\n\nBadlands\nA nebula.");
        CHECK(p[1].Empty() && !p[0].runs[0].heading);
    }
    // ---- a trailing PARA leaves no empty paragraph; TRA bits under their mask; a heading style and back
    {
        Writer w;
        w.Style(kStyleHeading);
        w.Text(L"Ships For Sale");
        w.Para();
        w.Style(kStyleBody);
        w.Tra(kBold | kItalic, kBold | kItalic);
        w.Text(L"Patriot");
        w.Tra(0, kItalic);
        w.Text(L" $1,000");
        w.Para();
        const std::vector<Para> p = Parse(w.b.data(), w.b.size());
        CHECK(p.size() == 2);
        CHECK(p[0].runs[0].heading && p[0].Text() == L"Ships For Sale");
        CHECK(!p[1].runs[0].heading && p[1].runs[0].attrs == (kBold | kItalic) && p[1].runs[1].attrs == kBold);
        CHECK(PlainText(p) == L"Ships For Sale\nPatriot $1,000");
    }
    // ---- a record longer than the buffer ends the read; what was read stands
    {
        Writer w;
        w.Text(L"First");
        w.Para();
        w.Text(L"Second");
        std::vector<unsigned char> cut(w.b.begin(), w.b.end() - 4);
        const std::vector<Para> p = Parse(cut.data(), cut.size());
        CHECK(p.size() == 1 && PlainText(p) == L"First");
        CHECK(Parse(nullptr, 0).empty());
    }
    // ---- superscript digits as their plain digits; anything else 0
    CHECK(SuperDigit(0x00B2) == L'2' && SuperDigit(0x00B3) == L'3' && SuperDigit(0x00B9) == L'1');
    CHECK(SuperDigit(0x2070) == L'0' && SuperDigit(0x2074) == L'4' && SuperDigit(0x2079) == L'9');
    CHECK(SuperDigit(L'4') == 0 && SuperDigit(0x2071) == 0 && SuperDigit(0x207A) == 0);
    // ---- the layout: whole words, a blank paragraph a blank line, leading spaces kept, a wrap drops the space before
    // the word, a word too wide stands alone, runs keep their attributes
    {
        const auto width = [](const std::wstring& s) { return static_cast<float>(s.size()); };
        std::vector<Para> p(3);
        p[0].runs.push_back({L"one two three", 0, false});
        p[2].runs.push_back({L"    item", 0, false});
        std::vector<Line> l = Layout(p, 8, width);
        CHECK(l.size() == 4);
        CHECK(l[0].pieces[0].text == L"one two" && l[1].pieces[0].text == L"three" && l[1].pieces[0].x == 0);
        CHECK(l[2].pieces.empty() && l[3].pieces[0].text == L"    item");
        std::vector<Para> q(1);
        q[0].runs.push_back({L"a", 0, false});
        q[0].runs.push_back({L" bold", kBold, false});
        q[0].runs.push_back({L" enormousword", 0, false});
        l = Layout(q, 8, width);
        CHECK(l.size() == 2 && l[0].pieces.size() == 2 && l[0].pieces[1].attrs == kBold && l[0].pieces[1].x == 1);
        CHECK(l[1].pieces[0].text == L"enormousword");
    }
    // ---- the market's grid (the owner, 12 Nov 2026: aligned, a price column): the game's ragged indents become one indent,
    // the prices right-aligned in one column for every section; the words, the items and their order unchanged
    {
        const auto width = [](const std::wstring& s) { return static_cast<float>(s.size()); };
        auto para = [](const wchar_t* s, bool heading = false)
        {
            Para p;
            if (*s)
                p.runs.push_back({s, 0, heading});
            return p;
        };
        std::vector<Para> p = {para(L"A planet."),          para(L"Ships For Sale ", true), para(L"    Patriot "),
                               para(L"     Rhino "),        para(L"      "),                 para(L"Commodities Selling ", true),
                               para(L"    Boron $155"),     para(L"     Luxury Food $1,200"), para(L"      "),
                               para(L"Commodities Buying ", true), para(L"    Oxygen $5"),    para(L"      "),
                               para(L"    not an item")};
        const std::vector<int> role = MarketRoles(p);
        CHECK(role[0] == 0 && role[1] == 1 && role[2] == 2 && role[3] == 2 && role[4] == 0 && role[5] == 1 && role[6] == 2);
        CHECK(role[7] == 2 && role[9] == 1 && role[10] == 2 && role[12] == 0); // (indented, but after a closed section: the game's spacing)
        MarketItem m;
        CHECK(MarketItemOf(p[7], m) && m.name == L"Luxury Food" && m.price == L"$1,200");
        CHECK(MarketItemOf(p[2], m) && m.name == L"Patriot" && m.price.empty());
        CHECK(!MarketItemOf(p[0], m) && !MarketItemOf(p[4], m));
        std::vector<Para> q = {para(L"    Bad $ide")};
        CHECK(MarketItemOf(q[0], m) && m.name == L"Bad $ide" && m.price.empty()); // ("$" and no number: the name's)
        std::vector<Line> l = Layout(p, 40, width);
        CHECK(l.size() == p.size()); // (every paragraph one line here)
        // the names at one indent (four spaces), with no spaces of their own
        for (int i : {2, 3, 6, 7, 10})
            CHECK(l[i].item && l[i].para == i && l[i].pieces[0].x == 4 && l[i].pieces[0].text[0] != L' ');
        CHECK(l[2].pieces.size() == 1 && l[2].pieces[0].text == L"Patriot");
        // the prices right-aligned at indent + the widest priced name (11) + 4 + the widest price (6) = 25, in every section
        CHECK(l[6].pieces.size() == 2 && l[6].pieces[1].text == L"$155" && l[6].pieces[1].x + l[6].pieces[1].w == 25);
        CHECK(l[7].pieces[1].text == L"$1,200" && l[7].pieces[1].x + l[7].pieces[1].w == 25);
        CHECK(l[10].pieces[1].text == L"$5" && l[10].pieces[1].x + l[10].pieces[1].w == 25);
        // the rest as before: the heading at 0, the blank line its spaces, the closed section's indent kept
        CHECK(!l[1].item && l[1].pieces[0].x == 0 && l[12].pieces[0].text == L"    not an item" && !l[12].item);
        // the same words in the same order, paragraph for paragraph
        for (size_t i = 0; i < p.size(); ++i)
        {
            std::wstring drawn;
            for (const Line& li : l)
                if (li.para == static_cast<int>(i))
                    for (const Piece& pc : li.pieces)
                        drawn += L" " + pc.text;
            auto words = [](const std::wstring& s)
            {
                std::wstring o;
                bool sp = true;
                for (wchar_t c : s)
                    if (c == L' ')
                        sp = true;
                    else
                        o += (sp && !o.empty() ? L" " : L"") + std::wstring(1, c), sp = false;
                return o;
            };
            CHECK(words(drawn) == words(p[i].Text()));
        }
        // a narrow box: the column stops at the box's edge, a long name wraps before it, its price on its last line
        l = Layout(p, 20, width);
        CHECK(l[7].pieces[0].text == L"Luxury" && l[8].pieces[0].text == L"Food" && l[8].para == 7 && l[8].item);
        CHECK(l[8].pieces[1].text == L"$1,200" && l[8].pieces[1].x + l[8].pieces[1].w == 20);
    }
    // ---- the card's styles (audit decision 56): the colour from the TRA bits' 0xBBGGRR00, a card's "default" (colour 0 under
    // the mask) back to the style's colour; the style line the comparison test compares; a styled run keeps its attributes
    // through a wrap, piece by piece
    {
        const u32 red = 0x0000FF00; // #FF0000 as the writer stores it (COLORREF 0x0000FF << 8)
        CHECK(CardColour(red) == 0xFF0000 && CardColour(0x00FF0000 | kBold) == 0x00FF00 && CardColour(0xFF000000) == 0x0000FF);
        CHECK(CardColour(kBold | kItalic | kUnderline | (8u << 3)) == 0); // (bold, italic, underline, font: no colour)
        // a base card's red "CLASSIFIED" then the default colour (Li01_03 Missouri's shape, audit 84_), an italic name in prose
        Writer w;
        w.Tra(red | kBold, 0xFFFFFF00 | kBold);
        w.Text(L"CLASSIFIED");
        w.Tra(0, 0xFFFFFF00 | kBold);
        w.Text(L" The base is ");
        w.Tra(kItalic, kItalic);
        w.Text(L"Missouri");
        w.Tra(0, kItalic);
        w.Text(L".");
        w.Para();
        w.Text(L"Plain.");
        const std::vector<Para> p = Parse(w.b.data(), w.b.size());
        CHECK(p.size() == 2 && p[0].runs.size() == 4);
        CHECK(CardColour(p[0].runs[0].attrs) == 0xFF0000 && (p[0].runs[0].attrs & kBold));
        CHECK(p[0].runs[1].attrs == 0 && CardColour(p[0].runs[1].attrs) == 0); // ("default": the style's colour again)
        CHECK(p[0].runs[2].attrs == kItalic && p[0].runs[3].attrs == 0);
        CHECK(StyleLine(p[0]) == L"~ 10:0000FF01 13:00000000 8:00000002 1:00000000");
        CHECK(StyleLine(p[1]).empty()); // (a plain paragraph: no style line)
        Para h;
        h.runs.push_back({L"Ships ", 0, true});
        h.runs.push_back({L"For Sale", 0, true});
        CHECK(StyleLine(h) == L"~ 14:00000000h"); // (neighbours of equal attributes merged)
        // the layout: widths as plain (the kit's italic keeps the advances), each piece its run's attributes, a wrap inside
        // the italic run keeps it italic on the next line
        const auto width = [](const std::wstring& s) { return static_cast<float>(s.size()); };
        Para q;
        q.runs.push_back({L"See the ", 0, false});
        q.runs.push_back({L"Liberty Navy dreadnought", kItalic, false});
        q.runs.push_back({L" here.", 0, false});
        const std::vector<Line> l = Layout(std::vector<Para>{q}, 20, width);
        CHECK(l.size() == 2);
        if (l.size() == 2)
        {
            CHECK(l[0].pieces.size() == 2 && l[0].pieces[0].attrs == 0 && l[0].pieces[1].attrs == kItalic);
            CHECK(l[0].pieces[1].text == L"Liberty Navy" && l[0].pieces[1].x == 8);
            CHECK(l[1].pieces[0].text == L"dreadnought" && l[1].pieces[0].attrs == kItalic && l[1].pieces[0].x == 0);
            CHECK(l[1].pieces.size() == 2 && l[1].pieces[1].text == L" here." && l[1].pieces[1].attrs == 0);
        }
    }
}
