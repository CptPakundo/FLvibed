// FLvibed - the INFO page's view (the HUD 2 page framework, audit 70b; moved from nnpage.cpp's DrawInfoText unchanged):
// the text column, the card's title for a target (the game's window shows it as its title), then the paragraphs in the
// card's styles (italic, underline, its colour: audit decision 56), a scroll bar when they are longer than the column.
// Draws its model (core\game\screens\nnhelp_model.h) and nothing of the game.
// Declared in info_page.h (a view includes only the draw kits and its model).
#include "../page_kit.h"
#include "../page_view.h"
#include "game/screens/nnhelp_model.h"

namespace infopage
{
using namespace hud2kit;

static std::vector<inforules::Line> s_lines; // the text laid out for PageState::wrapW

const std::vector<inforules::Line>& Lines() { return s_lines; }

void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out)
{
    const infobody::Body& info = static_cast<const infobody::Model*>(model)->body;
    const float H = in.H, size = 0.025f * H, pitch = 0.031f * H;
    const float x0 = in.x0, x1 = in.x1, top = in.top, foot = in.bottom;
    float y = top;
    if ((info.kase == infobody::Case::Target || info.kase == infobody::Case::TargetGone) && !info.title.empty())
    {
        TextFit(x0, y + 0.018f * H, info.title, 0.032f * H, x1 - x0, Argb(kChrome, 1));
        y += 0.05f * H;
    }
    const float w = x1 - x0 - 0.016f * H;
    if (w != st.wrapW)
    {
        s_lines = inforules::Layout(info.paras, w, [size](const std::wstring& s) { return TextWidth(s, size); });
        st.wrapW = w;
    }
    const int count = static_cast<int>(s_lines.size());
    st.room = std::max(1, static_cast<int>((foot - y) / pitch));
    st.scrolls = count > st.room;
    st.first = std::max(0, std::min(st.first, count - st.room));
    int shown = 0;
    for (; shown < st.room && st.first + shown < count; ++shown)
        for (const inforules::Piece& p : s_lines[st.first + shown].pieces)
        {
            // the card's own colour where it sets one (audit decision 56: HDE's cards set only red), else by role: the market's
            // headings in the page's heading colour, a bold run full ink, the rest as the pages' text; the card's italic and
            // underline as the game draws them, a bold run emboldened once more since the body is already the bold face (the
            // owner, 13 Nov 2026: "Synthetic emphasis"; hud2_kit.h kStyleItalic / kStyleUnderline / kStyleBold: widths
            // unchanged, so the layout holds)
            const u32 own = inforules::CardColour(p.attrs);
            const float alpha = p.heading || (p.attrs & inforules::kBold) ? 1.0f : 0.9f;
            const u32 argb = Argb(own ? own : p.heading ? kChrome : kInk, alpha);
            const u32 style = ((p.attrs & inforules::kItalic) ? kStyleItalic : 0) | ((p.attrs & inforules::kUnderline) ? kStyleUnderline : 0) |
                              ((p.attrs & inforules::kBold) ? kStyleBold : 0);
            // superscript digits (the planets' masses) small and raised: the page's font has no glyph for them
            const float yy = y + pitch * (shown + 0.5f), clip = x1 - 0.016f * H;
            float x = x0 + p.x;
            size_t i = 0;
            while (i < p.text.size())
            {
                const bool sup = inforules::SuperDigit(p.text[i]) != 0;
                size_t j = i;
                std::wstring seg;
                for (; j < p.text.size() && (inforules::SuperDigit(p.text[j]) != 0) == sup; ++j)
                    seg += sup ? inforules::SuperDigit(p.text[j]) : p.text[j];
                const float s = sup ? 0.62f * size : size;
                // (an italic superscript leans about its own raised baseline: moved right by the line's lean at that height)
                const float lean = sup && (style & kStyleItalic) ? kItalicSlant * 0.28f * size : 0.0f;
                TextClipped(x + lean, sup ? yy - 0.28f * size : yy, seg, s, x0, clip, argb, style);
                x += TextWidth(seg, s);
                i = j;
            }
        }
    if (count > st.room)
        skinkit::ScrollBar(x1 - 0.004f * H, y, foot, static_cast<float>(st.first) / count, static_cast<float>(st.room) / count);
    out.used = y - top + shown * pitch;
}
} // namespace infopage
