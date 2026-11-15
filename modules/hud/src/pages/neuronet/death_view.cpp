// FLvibed - the death page's view (the HUD 2 page framework, audit 70b F4a; nnpage.cpp's DrawDeath moved unchanged): the page's
// ground round the game's journal (its hole, handed in by the shell), the title, and with the mouse a chip on each of the
// game's three buttons, lit while the mouse is on it (the game's button takes the click: no hit box). Draws its model
// (core\game\screens\mlog_model.h DeathModel) and nothing of the game. Declared in death_page.h.
#include "../page_kit.h"
#include "../page_view.h"
#include "game/screens/mlog_model.h"

namespace deathpage
{
using namespace pagekit;

void View(const page::ViewIn& in, const void* model, page::PageState&, page::ViewOut&)
{
    const mlogscreen::DeathModel& m = *static_cast<const mlogscreen::DeathModel*>(model);
    const Frame f = {in.W, in.H, in.x0, in.x1, in.top, in.bottom, in.titleY};
    const float H = f.H;
    Box hole;
    if (in.textOk)
        hole.AddRect(in.text[0], in.text[1], in.text[2], in.text[3]);
    Cover(f.W, H, in.textReady ? hole : Box());
    Title(f, L"NEURAL NET LOG");
    if (in.pad)
        return;
    for (const mlogscreen::DeathButton& b : m.buttons)
    {
        if (!b.ok)
            continue;
        const navlayout::Rect r{b.x0, b.y0, b.x1, b.y1};
        const bool over = m.mouseX >= r.x0 && m.mouseX <= r.x1 && m.mouseY >= r.y0 && m.mouseY <= r.y1;
        Ground(r.x0, r.y0, r.x1, r.y1); // (the game's art and caption under the chip)
        skinchip::Chip(r, b.word, std::min(0.030f * H, (r.y1 - r.y0) * 0.6f), over ? skinchip::Look::Hover : skinchip::Look::Plain);
    }
}
} // namespace deathpage
