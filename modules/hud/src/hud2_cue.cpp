// The weapon group cue drawn (modules\hud\docs\UI_HUD2_GROUP_CUE.md 0; the rules: hud2_cue.h). Cells are shapes, not plates: a plate inside the sightline zone fails
// the lint, and this row adds nothing to the boxed budget.
#include "hud2_impl.h"
#include "hud2_cue.h"

#include <cmath>

// (ASCII only: the build reads the sources as cp1252)
namespace hud2
{
using namespace hud2kit;
namespace hc = hud2cue;

namespace
{
constexpr u32 kCell = 0x0A0C20, kDigit = 0x5B8CAA; // the dark cell; a group's digit, chrome at 70 %

void Cell(float x, float y, float w, float h, u32 argb)
{
    const float c = w * 0.18f, x0 = x - w / 2, y0 = y - h / 2, x1 = x + w / 2, y1 = y + h / 2;
    const float p[12] = {x0 + c, y0, x1, y0, x1, y1 - c, x1 - c, y1, x0, y1, x0, y0 + c};
    Poly(p, 6, argb);
}

void CellEdge(float x, float y, float w, float h, float lw, u32 argb)
{
    const float c = w * 0.18f, x0 = x - w / 2, y0 = y - h / 2, x1 = x + w / 2, y1 = y + h / 2;
    const float p[12] = {x0 + c, y0, x1, y0, x1, y1 - c, x1 - c, y1, x0, y1, x0, y0 + c};
    Outline(p, 6, lw, argb);
}
} // namespace

void DrawCue(const Model& m, const Layout& L, float a, int motion)
{
    if (!m.cueShow || m.nCue <= 0)
        return;
    const float H = L.H, k = L.k, s = H * k, w = hc::kCellW * s, h = hc::kCellH * s, T = 0.024f * s;
    const float y = L.cy - L.R - hc::kAboveRing * s;
    float xs[hc::kMaxMarks] = {};
    hc::Layout(m.cueDigits, m.nCue - m.cueDigits, s, xs);
    const float breath = hc::Breath(GetTickCount() / 1000.0f, motion);
    const float fadeUntilLit = m.presetDrawn == hud2preset::Focus && !m.cueAnyLit ? 0.45f : 1.0f;
    RecordTag("ring.groups");
    const RecUnit unit; // (one row of cells, 0.003 apart by design: the lint's gap is for separate elements, and the row cannot widen, UI_HUD2_GROUP_CUE.md)
    for (int i = 0; i < m.nCue && i < hc::kMaxMarks; ++i)
    {
        const Model::CueMark& c = m.cue[i];
        const float x = L.cx + xs[i];
        SetAlpha(a * fadeUntilLit);
        const u32 lit = c.selected ? kActive : kChrome;
        // the lock's glow first, under the cell, in the same breath as its tick
        if (c.locked)
        {
            const float g = 0.35f + 0.65f * breath;
            for (const float d : {0.006f, 0.0035f})
                Cell(x, y, w + 2 * d * s, h + 2 * d * s, Argb(kChrome, 0.22f * g));
        }
        Cell(x, y, w, h, Argb(kCell, 0.78f));
        if (c.litA > 0.01f && !c.amber)
            Cell(x, y, w, h, Argb(lit, c.litA));
        if (c.selected)
            CellEdge(x, y, w, h, std::max(1.0f, 0.0012f * H), Argb(kActive, 1));
        const u32 ink = c.amber ? kAmber : c.litA > 0.5f ? kNavy : c.selected ? kActive : kDigit;
        const wchar_t label[2] = {c.ch, 0};
        Text(x, y, label, T, Argb(ink, 1), Center, 0, false);
        if (c.amber) // a slash: no rounds or no way to fire, not colour alone
            Line(x - w * 0.5f, y + h * 0.5f, x + w * 0.5f, y - h * 0.5f, std::max(1.5f, 0.0022f * H), Argb(kAmber, 1));
        if (c.locked) // the inverted tick above the mark, point down, bouncing with the glow
        {
            const float t = 0.45f * w, up = hc::kTickTravel * s * breath, ty = y - h / 2 - 0.0035f * s - up;
            Tri(x - t, ty - t * 0.8f, x + t, ty - t * 0.8f, x, ty, Argb(kInk, 1));
        }
        if (c.reload > 0.001f) // the refire, under M and T only
        {
            const float bh = std::max(1.0f, hc::kBarH * H);
            Rect(x - w / 2, y + h / 2 + 0.001f * s, w * c.reload, bh, Argb(kChrome, 1));
        }
    }
    SetAlpha(a);
}
} // namespace hud2
