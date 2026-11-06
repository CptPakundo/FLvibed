// FLvibed - the game's own name card and where a character's card may stand (gamecard.h). Core: the pad's offer word follows the card on the classic
// HUD with the HUD module off, and uistrip's card stands by the same rules.
#include "ui/gamecard.h"

#include "flexe.h"
#include "game/baseinfo.h"
#include "game/deckmode.h"
#include "game/hostcalls.h"
#include "game/render.h"
#include "game/room.h"
#include "log.h"
#include "safemem.h"
#include "ui/options_page.h"
#include "ui/ui.h"
#include "ui/views.h"
#include "ui/winplates.h"

namespace gamecard
{
constexpr float kRefresh = 0.1f; // the readers at most 10 Hz


// The game's name card's top-left (pixels): the `ToolTip` control keeps its position as its frame's translation
// (+0x3C frame, its translation at +0x60, UI camera space; measured 25 Oct 2026 at 4K: (-0.0846, 0.0686, -1) = the drawn
// box's corner 1684, 889) - its +0xA8 is not a position in this class (+0xAC holds 0.625), which ui::ControlRect adds:
// the card went off the top of the screen (y -864) and was clamped over the room bar.
// Under ui_style 0 the per-frame hover code (0x444C09..) derives the position from the character's box each frame (x, y -
// 30 px) and re-places the ToolTip whenever that moves (0x4BE370, room camera sway: a single write held 0.5-1.3 s) - so
// NativeCard writes +0x60 every frame before the UI pass, which the ToolTip's draw projects (padbases' offer label follows
// its corner through this read).
bool TipCorner(u32 tip, float& x, float& y)
{
    float p[3] = {};
    __try
    {
        memcpy(p, reinterpret_cast<const void*>(tip + 0x60), sizeof(p));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return p[2] < 0 && ui::Project(p[0], p[1], p[2], x, y);
}

// FLvibed's own modals over the base: the FLVIBED options page (standalone or hosted) and the on-screen keyboard - the
// same set padbases treats as modal (OwnModalOpen). Cheap flags, so Draw checks them every frame too (no 0.1 s lag).
bool OwnModalOpen() { return options_page::IsOpen() || views::OskOpen(); }

// A modal menu over the base (pause, save / load, options, confirm, FLvibed's own): no card (UI_HUD2_EXTENDED.md 10).
bool ModalOpen()
{
    if (OwnModalOpen())
        return true;
    for (const baseinfo::Ctl& c : baseinfo::Visible())
        if (exe::VtPauseMenu.is(c.vt) || exe::VtGameLoadScreen.is(c.vt) || exe::VtPrefs.is(c.vt) || exe::VtConfirm.is(c.vt))
            return true;
    return false;
}

std::vector<void*> Characters()
{
    std::vector<void*> out;
    if (!hostcalls::g.GetRoomCharacters)
        return out;
    void* objs[128];
    const int n = hostcalls::g.GetRoomCharacters(objs, 128);
    out.assign(objs, objs + std::max(0, std::min(n, 128)));
    return out;
}

// The card's place from the hovered character's own box ("no character card hides the character
// themselves on-screen"): above the head, centred on the character, a small gap; else beside the character on the side
// facing the screen centre (then the other side, then under the feet) - never over the character's box, inside the
// 0.05 W / 0.05 H safe area, clear of the strip and of the windows' keep-outs (winplates) this frame (the room bar, its sub-row, LB / RB,
// the NeuroNet box, the windows' plates - all collected by its geometry pass before the overlay; treated as reaching up
// to the top edge, as the bar and the box do). No search: the box was read with the card (Read, 10 Hz).
// The rules for any card of w x h pixels and a character box (pixels): uistrip's own card (PlaceCard) and the game's own
// ToolTip under ui_style 0 (NativeCard). `strip`: uistrip's strip is a keep-out too.
// The other characters on screen (the card's end touched a neighbouring patron's head in the bar): among the
// places those hard rules allow - above the head (centred, slid sideways off a neighbour while still over the character's
// centre, lifted by up to 0.08 H), beside on either side, under the feet - the card takes the one that covers the least of
// them (a neighbour's head - the top 22 % of its box - weighs 20 times its body), then the least moved from the centred
// place above the head, the side places costing a quarter card more (the rules' order when nothing is covered).
static bool PlaceRules(float bx0, float by0, float bx1, float by1, float stripRight, float stripBottom, float w, float h, float W, float H, float& x,
                       float& y, const char*& placed, const std::vector<Box>* others);

bool PlaceBy(float bx0, float by0, float bx1, float by1, float stripRight, float stripBottom, float w, float h, float W, float H, float& x, float& y,
             const char*& placed, const std::vector<Box>* others)
{
    return PlaceRules(bx0, by0, bx1, by1, stripRight, stripBottom, w, h, W, H, x, y, placed, others);
}

// The on-screen boxes of the room's characters other than `self` (pixels; off-screen or empty ones left out).
std::vector<Box> OtherBoxes(const std::vector<void*>& chars, void* self, float W, float H)
{
    std::vector<Box> out;
    for (void* o : chars)
    {
        float bx = 0, by = 0, bw = 0, bh = 0;
        if (o == self || !baseinfo::CharacterScreenRect(o, bx, by, bw, bh) || bw <= 0 || bh <= 0 || bx >= W || by >= H ||
            bx + bw <= 0 || by + bh <= 0)
            continue;
        out.push_back({bx, by, bx + bw, by + bh});
    }
    return out;
}

static bool PlaceRules(float bx0, float by0, float bx1, float by1, float stripRight, float stripBottom, float w, float h, float W, float H, float& x,
                       float& y, const char*& placed, const std::vector<Box>* others)
{
    const float gap = H * 0.012f, sx0 = W * deckmode::SideW(), sy0 = H * deckmode::EdgeH(), sx1 = W - sx0, sy1 = H - sy0;
    // how much of the other characters the card covers (pixels squared, a half gap around each): heads 20x, bodies 1x
    auto covered = [&](float cx, float cy)
    {
        float s = 0;
        if (others)
            for (const Box& o : *others)
            {
                const float ox0 = o.x0 - gap * 0.5f, ox1 = o.x1 + gap * 0.5f, oy0 = o.y0 - gap * 0.5f, oy1 = o.y1 + gap * 0.5f;
                const float headY1 = o.y0 + (o.y1 - o.y0) * 0.22f + gap * 0.5f;
                const float ix = std::min(cx + w, ox1) - std::max(cx, ox0);
                if (ix <= 0)
                    continue;
                const float iy = std::min(cy + h, oy1) - std::max(cy, oy0);
                const float ih = std::min(cy + h, headY1) - std::max(cy, oy0);
                if (iy > 0)
                    s += ix * iy;
                if (ih > 0)
                    s += 19 * ix * ih;
            }
        return s;
    };
    auto overlaps = [](float ax0, float ay0, float ax1, float ay1, float cx0, float cy0, float cx1, float cy1)
    { return ax0 < cx1 && cx0 < ax1 && ay0 < cy1 && cy0 < ay1; };
    // the lowest edge (pixels) the card must stay under in its x range: the keep-outs reaching below `top`, the strip
    auto under = [&](float cx0, float cx1, float top)
    {
        float low = winplates::LowestKeepOut(cx0, cx1, top);
        if (stripRight > 0 && stripBottom > 0 && cx0 < stripRight && stripBottom * H > top)
            low = std::max(low, stripBottom * H);
        return low > top ? low + gap : 0.0f;
    };
    auto fits = [&](float cx, float cy)
    {
        return cx >= sx0 - 0.5f && cx + w <= sx1 + 0.5f && cy >= sy0 - 0.5f && cy + h <= sy1 + 0.5f &&
               !overlaps(cx, cy, cx + w, cy + h, bx0 - gap * 0.5f, by0 - gap * 0.5f, bx1 + gap * 0.5f, by1 + gap * 0.5f) &&
               under(cx, cx + w, cy) == 0;
    };
    // the candidates in the rules' order; the best = the least covered, then the least cost (the first on a tie)
    bool have = false;
    float best = 0;
    const float x1st = Clampf((bx0 + bx1 - w) / 2, sx0, sx1 - w), y1st = by0 - gap - h, mid = (bx0 + bx1) / 2;
    auto consider = [&](float cx, float cy, const char* how, float extra)
    {
        if (!fits(cx, cy))
            return;
        const float cost = covered(cx, cy) + (fabsf(cx - x1st) + fabsf(cy - y1st)) * h * 0.3f + extra;
        if (!have || cost < best)
            have = true, best = cost, x = cx, y = cy, placed = how;
    };
    // 1 above the head, centred (slid sideways into the safe area, still above the box); with neighbours also slid off
    // each of them (still over the character's centre) and lifted (up to 0.08 H), each at the first height that clears
    consider(x1st, y1st, "above", 0);
    if (have && best <= 0)
        return true; // (nothing covered, nothing moved: the common case without a search)
    if (others)
    {
        std::vector<float> xs{x1st}, ys{y1st};
        for (const Box& o : *others)
        {
            for (const float cx : {o.x1 + gap * 0.5f, o.x0 - gap * 0.5f - w})
                if (cx <= mid && cx + w >= mid)
                    xs.push_back(Clampf(cx, sx0, sx1 - w));
            const float ly = o.y0 - gap * 0.5f - h - 1; // above its head
            if (ly < y1st && ly >= y1st - H * 0.08f)
                ys.push_back(ly);
        }
        for (const float cx : xs)
            for (const float cy : ys)
                consider(cx, cy, "above", 0);
    }
    // 2 / 3 beside, head high (pushed down under a keep-out, kept inside the safe area), the side facing the centre first
    const bool rightFirst = mid < W / 2;
    for (int i = 0; i < 2; ++i)
    {
        const bool right = rightFirst == (i == 0);
        const float cx = right ? bx1 + gap : bx0 - gap - w;
        float cy = Clampf(by0, sy0, sy1 - h);
        const float down = under(cx, cx + w, cy);
        if (down > 0)
            cy = std::min(down, sy1 - h);
        consider(cx, cy, right ? "right" : "left", w * h * 0.25f * (i + 1));
    }
    // 4 under the feet
    consider(x1st, by1 + gap, "below", w * h * 0.75f);
    return have;
}

// 0x4BE370 -> 0x4BDEA0, which re-places it through 0x4BD880 / 0x4BDA70 -> vt[0x14] only when its text or the passed
// position changed) - and its draw (0x4BDFE0) projects +0x60 itself each frame (0x4BE0B2) with its size in pixels at
// +0xB4 / +0xB8 (the text extents, written by the draw; the box adds a few pixels of padding). So a write of +0x60 after
// the frame update and before the UI pass is what the draw uses, every frame, whatever the game re-placed meanwhile.
static Native g_native;
// (at most 10 Hz) the shown ToolTip and the hovered room character - none under a modal menu (the pause menu's button
// help is the same ToolTip) or while a conversation runs
void NativeFind()
{
    g_native.tip = 0, g_native.who = nullptr;
    baseinfo::Refresh();
    if (!baseinfo::Docked() || ModalOpen())
        return;
    const u32 tip = baseinfo::Find("ToolTip");
    if (!tip)
        return;
    for (void* o : Characters())
        if (baseinfo::CharacterTalking(o))
            return; // a conversation: the game's own place
    // the character whose hover code placed the card last: the ToolTip keeps the position it was passed at +0x13C (x, y,
    // z: UI camera space; 0x4BD8AA), which the hover code derives from the box's top-left (x, y - 30 px) - the nearest
    // hovered box wins (two characters can claim the hover: the cursor's and the pad's)
    float last[3] = {}, lx = 0, ly = 0;
    const bool haveLast = safemem::Read(tip + 0x13C, last, sizeof(last)) && ui::Project(last[0], last[1], last[2], lx, ly);
    float best = 1e30f;
    g_native.lastAt[0] = lx, g_native.lastAt[1] = ly, g_native.hovered = 0;
    for (void* o : Characters())
    {
        float bx = 0, by = 0, bw = 0, bh = 0;
        if (!baseinfo::CharacterHovered(o))
            continue;
        ++g_native.hovered;
        const float d = haveLast && baseinfo::CharacterScreenRect(o, bx, by, bw, bh) ? fabsf(bx - lx) + fabsf(by - 30 - ly) : 1e29f;
        if (d < best)
            best = d, g_native.tip = tip, g_native.who = o;
    }
}

// The game's card's place by the rules, clear of the other characters too (their boxes read per frame: a few reads each;
// apart from NativeCard, whose __try allows no objects to unwind).
bool PlaceNative(float bx0, float by0, float bx1, float by1, float w, float h, float W, float H, float& x, float& y)
{
    const std::vector<Box> others = OtherBoxes(Characters(), g_native.who, W, H);
    return PlaceBy(bx0, by0, bx1, by1, 0, 0, w, h, W, H, x, y, g_native.placed, &others);
}

void NativeCard(bool scene)
{
    ++g_native.frames;
    if (scene)
    {
        g_native.tip = 0, g_native.who = nullptr, g_native.placed = "-";
        return;
    }
    const double now = static_cast<double>(GetTickCount64()) / 1000.0;
    // the hover's start / end is found at most 10 Hz; a cached hover is re-checked each frame by two byte reads
    if (!g_native.tip || now >= g_native.nextFind)
    {
        if (now >= g_native.nextFind)
            g_native.nextFind = now + kRefresh, NativeFind();
        if (!g_native.tip)
            return;
    }
    const u32 tip = g_native.tip;
    u8 st = 0;
    float sz[2] = {}, p[3] = {}, bx = 0, by = 0, bw = 0, bh = 0;
    int sw = 0, sh = 0;
    if (!baseinfo::CharacterHovered(g_native.who) || !safemem::Read(tip + 0x6C, &st, 1) || !(st & 2) ||
        !safemem::Read(tip + 0xB4, sz, sizeof(sz)) || !safemem::Read(tip + 0x60, p, sizeof(p)) || sz[0] <= 0 ||
        sz[1] <= 0 || p[2] > -0.01f || !render::ScreenSize(sw, sh) || sh < 100 ||
        !baseinfo::CharacterScreenRect(g_native.who, bx, by, bw, bh))
    {
        g_native.placed = "-";
        return;
    }
    u32 vt = 0;
    if (!safemem::Read(tip, &vt, 4) || !exe::VtToolTip.is(vt))
    {
        if (!g_native.refused++)
            Log("gamecard: the game's ToolTip 0x%08X has vt 0x%08X, not the signed VtToolTip - its card stays where the game puts it",
                tip, vt);
        g_native.tip = 0;
        return;
    }
    const float W = static_cast<float>(sw), H = static_cast<float>(sh);
    // the box's padding around the text: at 4K the drawn box measured 114 x 65 px for +0xB4 / +0xB8 = 128 x 63 ("Trent"),
    // so the text extents plus this are never under-sized
    const float pad = H * 0.004f;
    const float w = sz[0] + 2 * pad, h = sz[1] + 2 * pad;
    float x = 0, y = 0;
    if (!PlaceNative(bx, by, bx + bw, by + bh, w, h, W, H, x, y))
    {
        g_native.placed = "game";
        return;
    }
    // pixels -> UI camera space at the card's own depth (the inverse of ui::Project: linear in x / y at a fixed z)
    float cx = 0, cy = 0, ux = 0, uy = 0;
    if (!ui::Project(0, 0, p[2], cx, cy) || !ui::Project(1, 1, p[2], ux, uy) || fabsf(ux - cx) < 1e-3f || fabsf(uy - cy) < 1e-3f)
        return;
    const float nx = (x - cx) / (ux - cx), ny = (y - cy) / (uy - cy);
    __try
    {
        reinterpret_cast<float*>(tip + 0x60)[0] = nx;
        reinterpret_cast<float*>(tip + 0x60)[1] = ny;
        ++g_native.writes;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_native.tip = 0;
        return;
    }
    g_native.x = x, g_native.y = y, g_native.w = w, g_native.h = h;
    g_native.box[0] = bx, g_native.box[1] = by, g_native.box[2] = bx + bw, g_native.box[3] = by + bh;
}

bool GameCardCorner(float& x, float& y)
{
    int sw = 0, sh = 0;
    const u32 tip = uiclick::FindVisible("ToolTip");
    return tip && render::ScreenSize(sw, sh) && TipCorner(tip, x, y) && x >= 0 && y >= 0 && x < sw && y < sh;
}

const Native& NativeState() { return g_native; }
} // namespace gamecard
