// The nav map's frame and words: navskin.h. The control names: docs\FREELANCER_RE.md "Nav map"; the table: modules\hud\docs\UI_HUD2_SKIN.md 6.1.
#include "navskin.h"

#include "flexe.h"
#include "game/deckmode.h"
#include "game/hudsize.h"
#include "game/nnwin.h"
#include "game/room.h"
#include "log.h"
#include "navmap_layout.h"
#include "navnew.h"
#include "safemem.h"
#include "skin_rules.h"
#include "skinchip.h"
#include "text.h"
#include "ui/hud2_kit.h"
#include "ui/hud_rows.h"

#include <sstream>

namespace navskin
{
using namespace hud2kit;
using namespace winplates;
using navlayout::kChipGap;
using navlayout::kChipH;
using navlayout::kPanelPad;
using navlayout::kSectionH;
using navlayout::kColumnMin;
using navlayout::kPanelGap;
using navlayout::kWordH;
using navlayout::Grid;
using navlayout::PanelTop;
using Rc = navlayout::Rect;

// NeuroNetNavMap's fields (07_navmap_facts.md): the viewport in pixels (x, y from the top, width, height) and the view, 0 universe, 1 system, 2 sector.
constexpr u32 kViewX = 0x424, kViewY = 0x428, kViewW = 0x430, kViewH = 0x434, kView = 0x7D0;
constexpr u32 kLeftWindow = 0xBB8, kRightWindow = 0xBBC;

enum Group
{
    GView,
    GMission,
    GSelect,
    GLayer,
    GFilter
};
struct Def
{
    const char* native;   // the button the chip calls
    const char* caption;  // its own caption, hidden with it (null: an icon)
    const wchar_t* word;
    const wchar_t* universeWord; // in the universe view (null: the same)
    Group group;
    int mode; // a layer: exe::NavMapMode's value (-1 none)
};
// Words for icons, by the table of 6.1. A map's button stands where the game shows it, disabled where the game disables it.
const Def kDefs[] = {
    {"NavMapShipButton", nullptr, L"SYSTEM", nullptr, GView, -1},
    {"NavMapUniverseButton", nullptr, L"SIRIUS", nullptr, GView, -1},
    {"NavMapContinueButton", "NavMapContinueText", L"CONTINUE", nullptr, GMission, -1},
    {"NavMapAbortMissionButton", "NavMapAbortMissionText", L"ABORT MISSION", nullptr, GMission, -1},
    {"NavMapSetWaypointButton", nullptr, L"SET WAYPOINT", nullptr, GSelect, -1},
    {"NavMapBestPathButton", nullptr, L"BEST PATH", nullptr, GSelect, -1},
    {"NavMapInZoom", nullptr, L"ZOOM IN", L"OPEN SYSTEM", GSelect, -1},
    {"NavMapOutZoom", nullptr, L"ZOOM OUT", nullptr, GSelect, -1},
    {"NavMapClearWaypointButton", nullptr, L"CLEAR", nullptr, GSelect, -1},
    {"NavMapClearAllWaypointsButton", "NavMapClearAllWaypointsText", L"CLEAR ALL", nullptr, GSelect, -1},
    {"NavMapSelectObjectButton", nullptr, L"SELECT", nullptr, GSelect, -1},
    {"NavMapJumpGateButton", nullptr, L"JUMP GATE", nullptr, GSelect, -1},
    {"NavMapPhysicalFilter", nullptr, L"PHYSICAL", nullptr, GLayer, 0},
    {"NavMapPoliticalFilter", nullptr, L"POLITICAL", nullptr, GLayer, 1},
    {"NavMapPatrolFilter", nullptr, L"PATROLS", nullptr, GLayer, 2},
    {"NavMapmineableFilter", nullptr, L"MINING", nullptr, GLayer, 5},
    {"NavMapBaseListFilter", nullptr, L"BASES", nullptr, GLayer, 7},
    {"NavMapLegendFilter", nullptr, L"LEGEND", nullptr, GLayer, 4},
    {"NavMapMissionFilter", nullptr, L"MISSION", nullptr, GLayer, 3},
    {"NavMapLabelButton", nullptr, L"LABELS", nullptr, GFilter, -1},
    {"NavMapAllFilter", nullptr, L"ALL", nullptr, GFilter, -1},
    {"NavMapFriendlyFilter", nullptr, L"FRIENDLY", nullptr, GFilter, -1},
    {"NavMapHostileFilter", nullptr, L"HOSTILE", nullptr, GFilter, -1},
};
constexpr int kDefCount = static_cast<int>(sizeof(kDefs) / sizeof(kDefs[0]));
constexpr int kLayerLine = kDefCount; // the pad's one line of layers
static_assert(kDefCount + 1 <= 32, "every chip fits the click box beside the strip");

struct Chip
{
    int id = -1;
    navlayout::Rect r;
    bool enabled = false, stepper = false;
};
static u32 g_root = 0;
static u32 g_ctl[kDefCount] = {};
static Chip g_chips[kDefCount + 1];
static int g_nChips = 0;
static int g_focus = -1;
static bool g_failed = false, g_restoreOwed = false, g_nativeAsked = false;
static bool g_pad = false;

bool NativeAsked() { return g_nativeAsked || skinrules::NativeListed(hudrows::g.skinNative, "navmap"); }

static void Fail(const char* why)
{
    if (g_failed)
        return;
    g_failed = g_restoreOwed = true;
    g_root = 0;
    Log("navskin: the map goes back to the game's for this session: %s", why);
}

bool TakeFailure()
{
    const bool owed = g_restoreOwed;
    g_restoreOwed = false;
    return owed;
}

bool Bind(u32 root, Hides& out)
{
    if (g_failed || NativeAsked() || !nnwin::Ready()) // (the chips' close and switch go through the manager: without it the old map keeps its own)
        return false;
    u32 side[2] = {};
    if (!R32(root + kLeftWindow, side[0]) || !R32(root + kRightWindow, side[1]) || !side[0] || !side[1] ||
        !(NameIs(side[0], "NavMapLeftWindow") || NameIs(side[0], "NavMapRightWindow")) || !(NameIs(side[1], "NavMapLeftWindow") || NameIs(side[1], "NavMapRightWindow")))
    {
        Fail("its side windows are not where the build says");
        return false;
    }
    out.nArt = 0;
    out.art[out.nArt++] = side[0];
    out.art[out.nArt++] = side[1];
    for (int i = 0; i < kDefCount; ++i)
    {
        g_ctl[i] = Child(root, kDefs[i].native);
        if (g_ctl[i])
            out.art[out.nArt++] = g_ctl[i];
        if (kDefs[i].caption)
            if (const u32 c = Child(root, kDefs[i].caption))
                out.art[out.nArt++] = c;
    }
    if (const u32 c = Child(root, "NavMapCloseButton")) // (the lane's CLOSE, the pad's B)
        out.art[out.nArt++] = c;
    if (!g_ctl[0] && !g_ctl[1]) // (a window with neither view button is not the map this table knows)
    {
        Fail("neither view button is there");
        return false;
    }
    g_root = root;
    return true;
}

void BeginPass() { g_nChips = 0; }

void Unbind()
{
    navnew::Closed();
    g_root = 0, g_nChips = 0, g_focus = -1;
    memset(g_ctl, 0, sizeof(g_ctl));
}

static bool NativeEnabled(u32 ctl)
{
    if (!Enabled(ctl))
        return false;
    u32 vt = 0;
    u8 flags = 0;
    // (the icon button's mouse handler takes nothing unless this bit is set: 0x55B956)
    if (R32(ctl, vt) && exe::VtIconButton.is(vt) && R8(ctl + 0x331, flags))
        return (flags & 2) != 0;
    return true;
}

static int View()
{
    u32 v = 1;
    R32(g_root + kView, v);
    return static_cast<int>(v);
}

static int Layer()
{
    if (!exe::NavMapMode.ok())
        return -1;
    int m = -1;
    safemem::Read(static_cast<u32>(exe::NavMapMode.get()), &m, 4);
    return m;
}

static bool LabelsOn()
{
    if (!exe::NavMapLabelsOn.ok())
        return false;
    u8 on = 0;
    safemem::Read(static_cast<u32>(exe::NavMapLabelsOn.get()), &on, 1);
    return on != 0;
}

static const wchar_t* Word(int id, int view) { return view == 0 && kDefs[id].universeWord ? kDefs[id].universeWord : kDefs[id].word; }

static bool Current(int id, int view, int layer)
{
    const Def& d = kDefs[id];
    if (d.group == GLayer)
        return d.mode == layer;
    if (id == 0)
        return view != 0;
    if (id == 1)
        return view == 0;
    return strcmp(d.native, "NavMapLabelButton") == 0 && LabelsOn();
}

// Presses the native button by its own action, if it still is that button and the game has it enabled.
static void Press(int id)
{
    if (id < 0 || id >= kDefCount || !g_root)
        return;
    const u32 c = g_ctl[id];
    if (c && NameIs(c, kDefs[id].native) && Shown(c) && NativeEnabled(c))
        uiclick::Click(c);
}

// The layer next to the one in force among those the game offers: the pad's one line.
static void StepLayer(int dir)
{
    int ids[8], n = 0, at = -1;
    const int layer = Layer();
    for (int i = 0; i < kDefCount && n < 8; ++i)
        if (kDefs[i].group == GLayer && g_ctl[i] && Shown(g_ctl[i]) && NativeEnabled(g_ctl[i]))
        {
            if (kDefs[i].mode == layer)
                at = n;
            ids[n++] = i;
        }
    if (n)
        Press(ids[((at < 0 ? 0 : at) + (dir < 0 ? n - 1 : 1)) % n]);
}

static const wchar_t* CurrentLayerWord(int layer)
{
    for (const Def& d : kDefs)
        if (d.group == GLayer && d.mode == layer)
            return d.word;
    return L"PHYSICAL";
}

static void DrawChip(int id, const navlayout::Rect& r, const wchar_t* word, float size, bool enabled, bool current, bool stepper, int hover)
{
    const int slot = enabled ? skinchip::Hot(r, [](int i) { i == kLayerLine ? StepLayer(+1) : Press(i); }, id) : -1;
    skinchip::Look look = !enabled && !current ? skinchip::Look::Disabled : current ? skinchip::Look::Current : skinchip::Look::Plain;
    if (slot >= 0 && slot == hover)
        look = skinchip::Look::Hover;
    if (g_focus == id)
        look = skinchip::Look::Focus;
    skinchip::Chip(r, word, size, look);
    if (g_nChips <= kDefCount)
        g_chips[g_nChips++] = {id, r, enabled, stepper};
}

void Draw(const Win* map, bool pad)
{
    g_pad = pad;
    if (!map || !g_root || map->root != g_root || !Shown(map->root))
        return;
    const float W = g_W, H = g_H;
    float view[4] = {};
    if (!safemem::Read(g_root + kViewX, &view[0], 8) || !safemem::Read(g_root + kViewW, &view[2], 8) || !(view[2] > 8) || !(view[3] > 8) || view[2] > W * 2 || view[3] > H * 2)
    {
        Fail("its viewport does not read");
        return;
    }
    const Rc mapRect = {view[0], view[1], view[0] + view[2], view[1] + view[3]};
    navnew::Frame(g_root, mapRect);
    const navlayout::MapOut lay = navnew::Grown() ? navlayout::Solve(W, H, deckmode::SideW()) : navlayout::Around(W, H, deckmode::SideW(), mapRect);
    if (lay.panel < kColumnMin)
        return; // (no room beside the map in this slide's frame: the words wait for it to settle)
    const int v = View(), layer = Layer();
    const float size = kWordH * H * hudsize::g_baseK * (deckmode::g_on ? 1.2f : 1.0f);
    const int hover = skinchip::Hovered();
    const float limit = deckmode::LaneTopH() * H - kPanelGap * H;
    auto shown = [&](int i) { return g_ctl[i] && Shown(g_ctl[i]); };
    auto panel = [&](const Rc& p, float height) { return Rc{p.x0, PanelTop(p.y0, height, limit), p.x1, 0}; };

    // the left panel: the view, then what the mission asks
    {
        int ids[4], n = 0;
        for (int i = 0; i < kDefCount; ++i)
            if (kDefs[i].group == GMission && shown(i))
                ids[n++] = i;
        const int cols = lay.column ? 1 : 2;
        const int viewRows = (2 + cols - 1) / cols;
        const float head = kSectionH * H;
        const float height = navnew::LeftHeight(lay.left, H) + head + viewRows * (kChipH + kChipGap) * H + n * (kChipH + kChipGap) * H + 2 * kPanelPad * H;
        Rc p = panel(lay.left, height);
        p.y1 = p.y0 + height;
        Plate(p.x0, p.y0, p.x1, p.y1, 0.012f * H, kTL | kBR, 0.85f, kChrome, 0.69f);
        float y = navnew::DrawLeft(p, H) + kPanelPad * H;
        skinchip::Section(p.x0 + kPanelPad * H, y + head / 2, L"VIEW");
        y += head;
        Rc cell[2];
        y = Grid(p, H, y, 2, cols, cell) + kChipGap * H;
        for (int i = 0; i < 2; ++i)
            DrawChip(i, cell[i], kDefs[i].word, size, shown(i) && NativeEnabled(g_ctl[i]) && !Current(i, v, layer), Current(i, v, layer), false, hover);
        for (int k = 0; k < n; ++k)
        {
            Rc c[1];
            y = Grid(p, H, y, 1, 1, c) + kChipGap * H;
            DrawChip(ids[k], c[0], kDefs[ids[k]].word, size, NativeEnabled(g_ctl[ids[k]]), false, false, hover);
        }
        g_keep.push_back({p.x0, p.y0, p.x1, p.y1});
    }

    // the right panel: what the selection offers, then the layers
    {
        int sel[8], nSel = 0, lay2[8], nLay = 0, fil[4], nFil = 0;
        for (int i = 0; i < kDefCount; ++i)
            if (shown(i))
            {
                if (kDefs[i].group == GSelect && nSel < 8)
                    sel[nSel++] = i;
                else if (kDefs[i].group == GLayer && nLay < 8)
                    lay2[nLay++] = i;
                else if (kDefs[i].group == GFilter && nFil < 4)
                    fil[nFil++] = i;
            }
        const float pitch = (kChipH + kChipGap) * H, head = kSectionH * H;
        const int layerRows = pad ? 1 : (nLay + 1) / 2, filterRows = (nFil + 1) / 2;
        const float cardH = navnew::RightHeight(lay.right, H);
        const float height = cardH + (nSel + 1) / 2 * pitch + (layerRows + filterRows) * pitch + head + 2 * kPanelPad * H;
        Rc p = panel(lay.right, height);
        p.y1 = p.y0 + height;
        Plate(p.x0, p.y0, p.x1, p.y1, 0.012f * H, kTL | kBR, 0.85f, kChrome, 0.69f);
        float y = navnew::DrawRight(p, H) + kPanelPad * H;
        Rc c[8];
        y = Grid(p, H, y, nSel, 2, c) + (nSel ? kChipGap * H : 0);
        for (int i = 0; i < nSel; ++i)
            DrawChip(sel[i], c[i], Word(sel[i], v), size, NativeEnabled(g_ctl[sel[i]]), false, false, hover);
        skinchip::Section(p.x0 + kPanelPad * H, y + head / 2, L"LAYERS");
        y += head;
        if (pad) // (one line on the pad, tiles on the mouse: 6.1)
        {
            y = Grid(p, H, y, 1, 1, c) + kChipGap * H;
            DrawChip(kLayerLine, c[0], CurrentLayerWord(layer), size, nLay > 0, false, true, hover);
        }
        else
        {
            y = Grid(p, H, y, nLay, 2, c) + kChipGap * H;
            for (int i = 0; i < nLay; ++i)
                DrawChip(lay2[i], c[i], kDefs[lay2[i]].word, size, NativeEnabled(g_ctl[lay2[i]]), Current(lay2[i], v, layer), false, hover);
        }
        y = Grid(p, H, y, nFil, 2, c);
        for (int i = 0; i < nFil; ++i)
            DrawChip(fil[i], c[i], kDefs[fil[i]].word, size, NativeEnabled(g_ctl[fil[i]]), Current(fil[i], v, layer), false, hover);
        g_keep.push_back({p.x0, p.y0, p.x1, p.y1});
    }
    navnew::DrawOverMap(mapRect);
}

int Chips(views::MapChip* out, int max)
{
    const int n = std::min(max, g_nChips);
    for (int i = 0; i < n; ++i)
    {
        const Chip& c = g_chips[i];
        out[i] = {c.r.x0, c.r.y0, c.r.x1, c.r.y1, c.id, c.enabled, c.stepper};
    }
    return n;
}

void ChipPress(int id)
{
    if (id == kLayerLine)
        StepLayer(+1);
    else
        Press(id);
}

void ChipStep(int id, int dir)
{
    if (id == kLayerLine)
        StepLayer(dir);
}

void ChipFocus(int id) { g_focus = id; }

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "native") // native on|off: the row `skin_native = navmap` will set; it takes effect at the map's next open
    {
        std::string v;
        in >> v;
        g_nativeAsked = v == "on";
        return "navskin native=" + std::to_string(g_nativeAsked);
    }
    if (what == "press") // press <id>: a chip's action
    {
        int id = -1;
        in >> id;
        ChipPress(id);
        return "navskin pressed " + std::to_string(id);
    }
    std::ostringstream o;
    o << "navskin bound=" << (g_root != 0) << " failed=" << g_failed << " native=" << g_nativeAsked << " chips=" << g_nChips << " focus=" << g_focus << " layer=" << Layer();
    if (g_root)
        o << " view=" << View() << " labels=" << LabelsOn();
    for (int i = 0; i < g_nChips; ++i)
    {
        const Chip& c = g_chips[i];
        char b[120];
        sprintf_s(b, "\n  %2d %-30s %.0f,%.0f - %.0f,%.0f%s", c.id, c.id < kDefCount ? kDefs[c.id].native : "(the layers' line)", c.r.x0, c.r.y0, c.r.x1, c.r.y1, c.enabled ? "" : " disabled");
        o << b;
    }
    o << "\n" << navnew::Status();
    return o.str();
}
} // namespace navskin
