// FLvibed - the LOG page's view (the HUD 2 page framework, audit 70b F4a; nnpage.cpp's DrawLog moved unchanged): the page's
// ground round the game's journal (its hole: the text itself is the game's), the two tabs, then in the left column RIGHT
// NOW (the objective), the time left, where the player is, the ship and what stands near. Draws its model
// (core\game\screens\mlog_model.h) and what its shell hands in (the journal's hole) and nothing of the game: the tabs and
// SHOW ON MAP are hit boxes, handed on by the runtime. Declared in mlog_page.h.
#include "../page_kit.h"
#include "../page_view.h"
#include "game/screens/mlog_model.h"

namespace mlogpage
{
using namespace pagekit;
using namespace mlogscreen; // (the actions' ids)

void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out)
{
    const mlogscreen::Model& m = *static_cast<const mlogscreen::Model*>(model);
    const Frame f = {in.W, in.H, in.x0, in.x1, in.top, in.bottom, in.titleY};
    const float H = f.H;
    Box hole;
    if (in.textOk)
        hole.AddRect(in.text[0], in.text[1], in.text[2], in.text[3]);
    const Box open = in.textReady ? hole : Box();
    Cover(f.W, H, open);
    Title(f, L"NEURAL NET LOG");
    float tx = f.left;
    const float size = 0.030f * H, tabH = 0.046f * H;
    for (int i = 0; i < 2; ++i)
    {
        const wchar_t* word = i == 0 ? L"MISSION" : L"JOURNAL";
        const float w = TextWidth(word, size) + 0.024f * H;
        const navlayout::Rect r{tx, f.top, tx + w, f.top + tabH};
        const int hit = HitAt(out, r.x0, r.y0, r.x1, r.y1, kActTab, i == 0 ? 1 : 0);
        skinchip::Tab(r, word, size, (i == 0) == m.mission, hit >= 0 && hit == in.hover);
        tx += w + 0.008f * H;
    }
    if (!in.pad)
    {
        if (!m.objective.empty())
            ChipAt(in, out, 0.425f * f.W, in.laneY, in.laneH, L"SHOW ON MAP", kActShowOnMap, 0);
        Text(f.left, in.laneMid, L"WHEEL SCROLLS THE TEXT", 0.026f * H, Argb(kChrome, 1));
    }
    st.first = m.journal; // (the test verb `page` prints the journal's line: the text and its scroll are the game's)
    // (the column comes with the text, in the frame it stands laid out where it stays: before, it followed it from the game's place)
    if (hole.ok && !open.ok)
        return;
    const float x1 = hole.ok ? hole.x0 - 0.03f * H : 0.34f * f.W, inner = 0.016f * H;
    float y = f.top + tabH + 0.02f * H;
    const float cardH = m.objective.empty() ? 0.10f * H : 0.21f * H;
    skinkit::Plate(f.left, y, x1, y + cardH, kTL | kBR);
    Text(f.left + inner, y + 0.03f * H, L"RIGHT NOW", 0.026f * H, Argb(kActive, 1));
    if (m.objective.empty())
        Text(f.left + inner, y + 0.066f * H, L"No objective", 0.030f * H, Argb(kDim, 1));
    else
        skinkit::Body(f.left + inner, y + 0.056f * H, x1 - f.left - 2 * inner, m.objective, 4);
    y += cardH + 0.03f * H;
    if (m.timer > 0)
    {
        wchar_t b[32];
        swprintf_s(b, L"%d:%02d", static_cast<int>(m.timer) / 60, static_cast<int>(m.timer) % 60);
        y = Fact(f, y, x1, L"TIME LEFT", b);
    }
    y = HereFacts(f, y, x1, m.here);
    y = Fact(f, y, x1, L"SHIP", m.ship);
    PlaceList(f, y, f.left, x1, L"NEAR YOU", m.places, [](const screens::Place&) { return kInk; });
}
} // namespace mlogpage
