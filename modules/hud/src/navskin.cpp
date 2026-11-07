// The nav map's frame and words: navskin.h. The control names: docs\FREELANCER_RE.md "Nav map"; the table: modules\hud\docs\UI_HUD2_SKIN.md 6.1.
#include "navskin.h"

#include "flexe.h"
#include "game/deckmode.h"
#include "game/fl.h"
#include "game/hudsize.h"
#include "game/navview.h"
#include "game/nnwin.h"
#include "game/room.h"
#include "game/space.h"
#include "log.h"
#include "navmap_layout.h"
#include "navnew.h"
#include "safemem.h"
#include "skin_rules.h"
#include "skinchip.h"
#include "text.h"
#include "ui/hud2_kit.h"
#include "ui/hud_rows.h"
#include "ui/ui.h"
#include "windowtext.h"

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
constexpr int kOpenLayers = kDefCount + 1; // the mouse's chip that opens the layers' card
static bool g_layersOpen = false;
static_assert(kDefCount + 2 <= 32, "every chip fits the click box beside the strip");

struct Chip
{
    int id = -1;
    navlayout::Rect r;
    bool enabled = false, stepper = false;
};
static u32 g_root = 0;
static u32 g_ctl[kDefCount] = {};
static Chip g_chips[kDefCount + 2];
static int g_nChips = 0;
static int g_focus = -1;
static bool g_listOpen = false; // the pad's destination list stands
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
    // the game's info card on the selection (its framed box, title and words) stood over HUD 2's VIEW panel (the owner's frames, 7 Nov 2026: "This still needs
    // fixing"): hidden. The map still marks and labels the selection; HUD 2's own card is the map plan's P3 (docs\MAP_PLAN.md: +0x90C is no selection to read)
    for (const char* name : {"NavMapInfoCard", "NavMapInfoText", "NavMapInfoTitle"})
        if (const u32 c = Child(root, name))
            out.art[out.nArt++] = c;
    if (navnew::On()) // the full-screen map: HUD 2 draws the sectors, and the left panel names the system
    {
        for (const char* name : {"NavMapSystemName", "NavMapFilterTitle"}) // (the layer's own word: the panel's chip says it)
            if (const u32 c = Child(root, name))
                out.art[out.nArt++] = c;
        for (int i = 0; i < 8; ++i)
        {
            char n[32];
            sprintf_s(n, "NavMapGridLetter%c", 'A' + i);
            if (const u32 c = Child(root, n))
                out.art[out.nArt++] = c;
            sprintf_s(n, "NavMapGridNumber%d", i + 1);
            if (const u32 c = Child(root, n))
                out.art[out.nArt++] = c;
        }
    }
    if (!g_ctl[0] && !g_ctl[1]) // (a window with neither view button is not the map this table knows)
    {
        Fail("neither view button is there");
        return false;
    }
    g_root = root;
    return true;
}

void BeginPass() { g_nChips = 0; }

// The mission briefing's CONTINUE / ABORT MISSION: hiding the captions as art took their frames but not their words, so the game's CONTINUE stood at the map's foot under
// the panel's own chip (Shockwave, 7 Nov 2026). Their text goes blank through the text control's setter (message 0x1D) while the chips stand, and comes back after.
struct Blanked
{
    u32 ctl = 0;
    wchar_t saved[48] = {};
};
static Blanked g_blanked[2];
static bool SetCaption(u32 ctl, const wchar_t* text)
{
    __try
    {
        const u32 fn = Mem<u32>(Mem<u32>(ctl) + 0xA8);
        if (!exe::TextCtlSetProperty.is(fn))
            return false;
        reinterpret_cast<u32(__thiscall*)(u32, u32, const void*, u32)>(fn)(ctl, 0x1D, text, 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
static void BlankCaptions()
{
    static const char* const kNames[2] = {"NavMapContinueText", "NavMapAbortMissionText"};
    for (int i = 0; i < 2; ++i)
    {
        const u32 c = Child(g_root, kNames[i]);
        std::wstring now;
        if (!c || !TextOf(c, now) || now == L" ")
            continue;
        Blanked& b = g_blanked[i];
        b.ctl = c;
        wcsncpy_s(b.saved, now.c_str(), _TRUNCATE);
        SetCaption(c, L" ");
    }
}
static void RestoreCaptions()
{
    static const char* const kNames[2] = {"NavMapContinueText", "NavMapAbortMissionText"};
    for (int i = 0; i < 2; ++i)
    {
        Blanked& b = g_blanked[i];
        if (b.ctl && uiclick::Find(kNames[i]) == b.ctl) // (only the control that is still there)
            SetCaption(b.ctl, b.saved);
        b = Blanked();
    }
}

void Unbind()
{
    RestoreCaptions();
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
    const int slot = enabled ? skinchip::Hot(r, [](int i) { i == kOpenLayers ? void(g_layersOpen = !g_layersOpen) : i == kLayerLine ? StepLayer(+1) : Press(i); }, id) : -1;
    skinchip::Look look = !enabled && !current ? skinchip::Look::Disabled : current ? skinchip::Look::Current : skinchip::Look::Plain;
    if (slot >= 0 && slot == hover)
        look = skinchip::Look::Hover;
    if (g_focus == id)
        look = skinchip::Look::Focus;
    skinchip::Chip(r, word, size, look);
    if (g_nChips <= kDefCount + 1)
        g_chips[g_nChips++] = {id, r, enabled, stepper};
}

// The game's waypoint list on the map, read for HUD 2's own ROUTE section: NavMapMissionObjectiveText1..n (TextControls, "Player Waypoint 1\nNew York
// System\nSector D3"), the entries the game shows (its own shown bit: a page of three, its scroll bar beside). The game lays its boxes out under its
// own info card, over HUD 2's VIEW panel, and re-lays them each frame (a move did not hold, 7 Nov 2026): they are hidden (uiskin Reclear) and drawn here.
static void Waypoints(std::vector<std::vector<std::wstring>>& out)
{
    out.clear();
    for (int i = 1; i <= 8; ++i)
    {
        char name[40];
        sprintf_s(name, "NavMapMissionObjectiveText%d", i);
        const u32 c = Child(g_root, name);
        std::wstring s;
        if (!c || !Shown(c) || !TextOf(c, s) || s.empty())
            continue;
        std::vector<std::wstring> lines;
        size_t at = 0;
        for (size_t nl; (nl = s.find(L'\n', at)) != std::wstring::npos; at = nl + 1)
            if (nl > at)
                lines.push_back(s.substr(at, nl - at));
        if (at < s.size())
            lines.push_back(s.substr(at));
        if (lines.size() > 3)
            lines.resize(3);
        if (!lines.empty())
            out.push_back(std::move(lines));
    }
}


static bool g_show = false; // the under pass found the map up: the overlay draws
static Rc g_mapRect;

// A panel's plate eases to what it holds (kMoveSec's rule: a change of height travels): its height of the last frame is this frame's plate, and the content says the next.
static float g_panelH[3] = {};
static float g_cardAt[2] = {};
static bool g_cardUp = false, g_clickWas = false; // the mouse's card: where it opened
static unsigned long long g_panelTick = 0;

static void EasePanel(float& h, float target)
{
    const unsigned long long now = GetTickCount64();
    const float dt = std::min(0.05f, static_cast<float>(now - g_panelTick) / 1000.0f);
    h += (target - h) * (1 - std::exp(-dt / 0.05f));
    if (std::fabs(target - h) < 1)
        h = target;
}

// The full-screen map's furniture (docs\MAP_PLAN.md; the owner, 7 Nov 2026: "the two panels at the sides of the navmap ... need to be re-considered"): nothing frames the map. The system's name
// and one row of chips along the top, how the map moves as one dim line above the strip, and two cards that exist only while they have something to say.
static void DrawFull(const navlayout::MapOut& lay, int v, int layer, float size, int hover, bool pad)
{
    const float W = g_W, H = g_H, side = lay.side * W, right = W - side, gap = kChipGap * H, pad2 = kPanelPad * H;
    const float top = deckmode::EdgeH() * H, chipH = kChipH * H;
    auto shown = [&](int i) { return g_ctl[i] && Shown(g_ctl[i]); };
    // the title: the game's own word for the system on show
    std::wstring name;
    if (const u32 c = Child(g_root, "NavMapSystemName"))
        TextOf(c, name);
    const float titleSize = 0.044f * H;
    const float titleW = name.empty() ? 0 : TextWidth(name, titleSize);
    if (!name.empty())
        Text(side, top + chipH / 2, name, titleSize, Argb(kInk, 1));
    (void)titleW;
    // the mouse's way into the layers' card, where the pad has D-pad right
    if (!pad)
    {
        const float w = TextWidth(L"LAYERS", size) + 2 * pad2;
        DrawChip(kOpenLayers, {right - w, top, right, top + chipH}, L"LAYERS", size, true, g_layersOpen, false, hover);
    }
    // the left card: what the mission asks and the route
    int mission[4], nm = 0;
    for (int i = 0; i < kDefCount && nm < 4; ++i)
        if (kDefs[i].group == GMission && shown(i))
            mission[nm++] = i;
    const bool info = navnew::HasInfo();
    const float cardTop = top + chipH + 2 * gap + 0.012f * H;
    if (info || nm)
    {
        if (g_panelH[0] <= 0)
            g_panelH[0] = 0.15f * H;
        const Rc p = {side, cardTop, side + lay.panel * H, cardTop + g_panelH[0]};
        Plate(p.x0, p.y0, p.x1, p.y1, 0.012f * H, kTL | kBR, 0.92f, kChrome, 0.69f);
        float y = info ? navnew::DrawLeft(p, H, false) : p.y0 + pad2 - gap;
        for (int k = 0; k < nm; ++k)
        {
            Rc c[1];
            y = Grid(p, H, y + gap, 1, 1, c);
            DrawChip(mission[k], c[0], kDefs[mission[k]].word, size, NativeEnabled(g_ctl[mission[k]]), false, false, hover);
        }
        EasePanel(g_panelH[0], y + pad2 - p.y0);
        g_keep.push_back({p.x0, p.y0, p.x1, p.y1});
    }
    else
        g_panelH[0] = 0;
    // the right card: the selection, and on the mouse what it offers (the pad's are its prompts)
    int sel[8], nSel = 0;
    if (!pad)
        for (int i = 0; i < kDefCount && nSel < 8; ++i)
            if (kDefs[i].group == GSelect && shown(i))
                sel[nSel++] = i;
    const bool card = navnew::HasCard();
    if (card || nSel)
    {
        if (g_panelH[1] <= 0)
            g_panelH[1] = 0.15f * H;
        const float w = lay.panel * H;
        Rc p = {right - w, cardTop, right, cardTop + g_panelH[1]};
        // on the mouse the card opens where the click was, as the game's popup did: what it offers stands a short move away
        if (!pad)
        {
            const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
            const bool inside = mx >= g_cardAt[0] - 1 && mx <= g_cardAt[0] + w + 1 && my >= g_cardAt[1] - 1 && my <= g_cardAt[1] + g_panelH[1] + 1;
            const bool clicked = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (!g_cardUp || (clicked && !g_clickWas && !inside))
            {
                const float gapX = 0.03f * H;
                g_cardAt[0] = mx + gapX + w <= right ? mx + gapX : std::max(side, mx - gapX - w);
                g_cardAt[1] = my;
            }
            g_clickWas = clicked;
            const float top = std::max(cardTop, std::min(g_cardAt[1] - 0.02f * H, deckmode::LaneTopH() * H - 0.08f * H - g_panelH[1]));
            p = {g_cardAt[0], top, g_cardAt[0] + w, top + g_panelH[1]};
        }
        g_cardUp = true;
        Plate(p.x0, p.y0, p.x1, p.y1, 0.012f * H, kTL | kBR, 0.92f, kChrome, 0.69f);
        float y = navnew::DrawRight(p, H);
        y = y > p.y0 ? y : p.y0 + pad2 - gap;
        if (nSel)
        {
            Rc c[8];
            y = Grid(p, H, y + gap, nSel, 2, c);
            for (int i = 0; i < nSel; ++i)
                DrawChip(sel[i], c[i], Word(sel[i], v), size, NativeEnabled(g_ctl[sel[i]]), false, false, hover);
        }
        EasePanel(g_panelH[1], y + pad2 - p.y0);
        g_keep.push_back({p.x0, p.y0, p.x1, p.y1});
    }
    else
        g_panelH[1] = 0, g_cardUp = false;
    // the layers' card, under the selection's: it comes when asked for (the pad's D-pad right, the mouse's LAYERS) and goes with the asking. Its chips always answer the pad's walk
    {
        const bool open = pad ? g_focus >= 0 : g_layersOpen;
        const float w = lay.panel * H, head = kSectionH * H, y0 = cardTop + (pad && (card || nSel) ? g_panelH[1] + gap : 0);
        const Rc p = {right - w, y0, right, y0 + std::max(g_panelH[2], 0.1f * H)};
        if (open)
            Plate(p.x0, p.y0, p.x1, p.y1, 0.012f * H, kTL | kBR, 0.92f, kChrome, 0.69f);
        auto chip = [&](int id, const wchar_t* word, const Rc& r, bool enabled, bool current)
        {
            if (open)
                DrawChip(id, r, word, size, enabled, current, false, hover);
            else if (g_nChips <= kDefCount + 1)
                g_chips[g_nChips++] = {id, r, enabled, false};
        };
        float y = p.y0 + pad2;
        if (open)
            skinchip::Section(p.x0 + pad2, y + head / 2, L"VIEW");
        y += head;
        Rc c[12];
        y = Grid(p, H, y, 2, 2, c);
        for (int i = 0; i < 2; ++i)
            chip(i, kDefs[i].word, c[i], shown(i) && NativeEnabled(g_ctl[i]) && !Current(i, v, layer), Current(i, v, layer));
        y += gap;
        if (open)
            skinchip::Section(p.x0 + pad2, y + head / 2, L"LAYERS");
        y += head;
        int ids[12], nt = 0;
        for (int i = 0; i < kDefCount && nt < 12; ++i)
            if ((kDefs[i].group == GLayer || kDefs[i].group == GFilter) && shown(i))
                ids[nt++] = i;
        const bool odd = nt % 2 != 0;
        float bottom = Grid(p, H, y, nt - (odd ? 1 : 0), 2, c);
        if (odd)
        {
            Rc last[1];
            bottom = Grid(p, H, nt > 1 ? bottom + gap : y, 1, 1, last);
            c[nt - 1] = last[0];
        }
        for (int i = 0; i < nt; ++i)
            chip(ids[i], kDefs[ids[i]].word, c[i], NativeEnabled(g_ctl[ids[i]]), Current(ids[i], v, layer));
        EasePanel(g_panelH[2], bottom + pad2 - p.y0);
        if (open)
            g_keep.push_back({p.x0, p.y0, p.x1, p.y1});
    }
    // the bottom's left half, the mirror of the prompts: how to move about the map, in two lines; what does nothing now dims, and a line that does not fit loses its last words first
    if (pad)
    {
        // (only what a press does right now, the rest closes up: with the layers' card or the list open the D-pad is theirs, the list's A too, so the cursor has no use; zoom and pan go on: the owner, 7 Nov 2026)
        const bool system = v != 0, zoomed = system && !navview::AtWidest(), inSpace = space::PlayerShipId() != 0, map = !g_listOpen, free = map && g_focus < 0;
        struct Row
        {
            glyphs::Id a, b;
            const wchar_t* word;
            bool lit;
        };
        // (a stick and its click share a glyph, so the words tell them apart, MOVE and CLICK, each stick's two side by side: the owner, 7 Nov 2026)
        const Row upper[5] = {{glyphs::LS, glyphs::COUNT, L"MOVE CURSOR", map},
                              {glyphs::L3, glyphs::COUNT, L"CLICK FIND ME", system && inSpace},
                              {glyphs::RS, glyphs::COUNT, L"MOVE MAP", zoomed},
                              {glyphs::R3, glyphs::COUNT, system ? L"CLICK SIRIUS MAP" : L"CLICK BACK TO SYSTEM", true},
                              {glyphs::DU, glyphs::DD, inSpace ? L"NEXT DESTINATION" : L"NEXT OBJECT", system && free}};
        const Row lower[3] = {{glyphs::LT, glyphs::RT, system ? L"ZOOM" : L"OPEN SYSTEM", true},
                              {glyphs::DL, glyphs::COUNT, L"DESTINATIONS", inSpace && free},
                              {glyphs::DR, glyphs::COUNT, L"LAYERS", free}};
        const float gh = 0.030f * H, limit = W * 0.408f; // (the lower line shares the lane with the prompts; the upper has the screen's width)
        float upperEnd = side;
        auto line = [&](const Row* rows, int count, float y, float room)
        {
            float hx = side;
            for (int i = 0; i < count; ++i)
            {
                const Row& r = rows[i];
                if (!r.lit)
                    continue;
                const float a = 0.95f, wa = GlyphWidth(r.a, gh), wb = r.b != glyphs::COUNT ? GlyphWidth(r.b, gh) + 0.006f * H : 0;
                const float tw = TextWidth(r.word, TextFloor());
                if (hx + wa + wb + 0.010f * H + tw > room)
                    break;
                Glyph(r.a, hx + wa / 2, y, gh, Argb(kChrome, a));
                hx += wa + 0.006f * H;
                if (wb > 0)
                    Glyph(r.b, hx + (wb - 0.006f * H) / 2, y, gh, Argb(kChrome, a)), hx += wb;
                Text(hx + 0.004f * H, y, r.word, TextFloor(), Argb(kInk, a));
                hx += 0.004f * H + tw + 0.020f * H;
            }
            upperEnd = std::max(upperEnd, hx);
        };
        const float laneMid = (deckmode::LaneTopH() + deckmode::LaneBottomH()) / 2 * H;
        line(upper, 5, laneMid - 0.042f * H, right);
        // (the prompts' second row, when they need one, stands beside what this line leaves)
        g_keep.push_back({side, deckmode::LaneTopH() * H - 0.06f * H, upperEnd, deckmode::LaneTopH() * H - 0.001f * H});
        line(lower, 3, laneMid, limit);
        // (this holds the lane's left, as the tabs did: the prompts stand at the right edge as its mirror, promptbar.cpp CentreLane)
        g_keep.push_back({side, deckmode::LaneTopH() * H, limit, deckmode::LaneBottomH() * H});
    }
    else
    {
        // the mouse's line: what the buttons do, and the same dim rule (HUD 2 is the mouse's too, the owner, 7 Nov 2026). What a selection offers stands on its card
        const bool system = v != 0, zoomed = system && !navview::AtWidest();
        const struct
        {
            const wchar_t *key, *word;
            bool lit;
        } rows[5] = {{L"LEFT CLICK", system ? L"SELECT" : L"SELECT SYSTEM", true},
                     {L"RIGHT CLICK", L"WAYPOINT", system},
                     {L"WHEEL", L"ZOOM", system},
                     {L"HOLD AND DRAG", L"MOVE MAP", zoomed},
                     {L"HOVER", L"NAME", true}};
        float hx = side;
        const float y = (deckmode::LaneTopH() + deckmode::LaneBottomH()) / 2 * H, s = TextFloor();
        for (const auto& r : rows)
        {
            if (!r.lit)
                continue;
            const float a = 0.95f;
            Text(hx, y, r.key, s, Argb(kChrome, a));
            hx += TextWidth(r.key, s) + 0.010f * H;
            Text(hx, y, r.word, s, Argb(kInk, a));
            hx += TextWidth(r.word, s) + 0.030f * H;
        }
    }
}

void Draw(const Win* map, bool pad)
{
    g_pad = pad;
    g_show = false;
    if (!map || !g_root || map->root != g_root || !Shown(map->root))
        return;
    const float W = g_W, H = g_H;
    float view[4] = {};
    if (!safemem::Read(g_root + kViewX, &view[0], 8) || !safemem::Read(g_root + kViewW, &view[2], 8) || !(view[2] > 8) || !(view[3] > 8) || view[2] > W * 2 || view[3] > H * 2)
    {
        Fail("its viewport does not read");
        return;
    }
    g_mapRect = {view[0], view[1], view[0] + view[2], view[1] + view[3]};
    navnew::Frame(g_root, g_mapRect, pad);
    BlankCaptions();
    g_show = true;
}

// Over the game's UI, because the map's viewport now runs under the panels: the marks, then the panels on them.
void DrawOver()
{
    if (!g_show || !g_root)
        return;
    const bool pad = g_pad;
    const Rc mapRect = g_mapRect;
    const float W = g_W, H = g_H;
    const navlayout::MapOut lay = navnew::Grown() ? navlayout::Solve(W, H, deckmode::SideW()) : navlayout::Around(W, H, deckmode::SideW(), mapRect);
    if (lay.panel < kColumnMin)
        return; // (no room beside the map in this slide's frame: the words wait for it to settle)
    const int v = View(), layer = Layer();
    // (the popup stands, by the zoom button every popup shows: Set Waypoint is not in the popup of an object the click hit, the owner's Newark, 7 Nov 2026)
    navnew::SetSelected((Child(g_root, "NavMapOutZoom") && Shown(Child(g_root, "NavMapOutZoom"))) || (Child(g_root, "NavMapInZoom") && Shown(Child(g_root, "NavMapInZoom"))));
    navnew::DrawOverMap(mapRect);
    const float size = kWordH * H * hudsize::g_baseK * (deckmode::g_on ? 1.2f : 1.0f);
    const int hover = skinchip::Hovered();
    if (navnew::Grown())
    {
        // the mission briefing's MISSION BRIEFING (the game's MissionBriefingText) stood on the map's letter row (Shockwave, 7 Nov 2026): on the top line, with
        // the system's name and LAYERS
        windowtext::StandText("MissionBriefingText", 0.041f * H);
        DrawFull(lay, v, layer, size, hover, pad);
        g_panelTick = GetTickCount64();
        return;
    }
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
        // the route: the game's waypoint list in HUD 2's words, under VIEW (the owner, 7 Nov 2026: "HUD 2's own list"; the game's boxes, which it lays out
        // under its own info card over this panel, are hidden: Waypoints())
        std::vector<std::vector<std::wstring>> route;
        Waypoints(route);
        const float wpLine = size * 1.15f, wpGap = kChipGap * H;
        float routeH = route.empty() ? 0 : head;
        for (const auto& e : route)
            routeH += static_cast<float>(e.size()) * wpLine + wpGap;
        const float height = head + viewRows * (kChipH + kChipGap) * H + n * (kChipH + kChipGap) * H + routeH + 2 * kPanelPad * H;
        if (g_panelH[0] <= 0)
            g_panelH[0] = height;
        Rc p = panel(lay.left, g_panelH[0]);
        p.y1 = p.y0 + g_panelH[0];
        Plate(p.x0, p.y0, p.x1, p.y1, 0.012f * H, kTL | kBR, 0.92f, kChrome, 0.69f);
        float y = p.y0 + kPanelPad * H;
        skinchip::Section(p.x0 + kPanelPad * H, y + head / 2, L"VIEW");
        y += head;
        Rc cell[2];
        y = Grid(p, H, y, 2, cols, cell);
        for (int i = 0; i < 2; ++i)
            DrawChip(i, cell[i], kDefs[i].word, size, shown(i) && NativeEnabled(g_ctl[i]) && !Current(i, v, layer), Current(i, v, layer), false, hover);
        for (int k = 0; k < n; ++k)
        {
            Rc c[1];
            y = Grid(p, H, y + kChipGap * H, 1, 1, c);
            DrawChip(ids[k], c[0], kDefs[ids[k]].word, size, NativeEnabled(g_ctl[ids[k]]), false, false, hover);
        }
        if (!route.empty())
        {
            y += kChipGap * H;
            const float x = p.x0 + kPanelPad * H, w = (p.x1 - p.x0) - 2 * kPanelPad * H;
            skinchip::Section(x, y + head / 2, L"ROUTE");
            y += head;
            for (const auto& e : route)
            {
                for (size_t l = 0; l < e.size(); ++l, y += wpLine)
                    TextFit(x, y + wpLine / 2, e[l], l == 0 ? size : size * 0.85f, w, Argb(l == 0 ? kInk : kDim, 1));
                y += wpGap;
            }
        }
        EasePanel(g_panelH[0], y + kPanelPad * H - p.y0);
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
        const float height = (nSel + 1) / 2 * pitch + (layerRows + filterRows) * pitch + head + 2 * kPanelPad * H;
        if (g_panelH[1] <= 0)
            g_panelH[1] = height;
        Rc p = panel(lay.right, g_panelH[1]);
        p.y1 = p.y0 + g_panelH[1];
        Plate(p.x0, p.y0, p.x1, p.y1, 0.012f * H, kTL | kBR, 0.92f, kChrome, 0.69f);
        float y = p.y0 + kPanelPad * H;
        Rc c[12];
        // (an odd last tile takes the whole row: nothing stands with a hole beside it)
        auto tiles = [&](float at, int count)
        {
            const bool odd = count % 2 != 0;
            float bottom = Grid(p, H, at, count - (odd ? 1 : 0), 2, c);
            if (odd)
            {
                Rc last[1];
                bottom = Grid(p, H, count > 1 ? bottom + kChipGap * H : at, 1, 1, last);
                c[count - 1] = last[0];
            }
            return bottom;
        };
        y = nSel ? tiles(y, nSel) + kChipGap * H : y;
        for (int i = 0; i < nSel; ++i)
            DrawChip(sel[i], c[i], Word(sel[i], v), size, NativeEnabled(g_ctl[sel[i]]), false, false, hover);
        skinchip::Section(p.x0 + kPanelPad * H, y + head / 2, L"LAYERS");
        y += head;
        // the layers and the filters in one block of tiles: nothing stands alone with a hole beside it, and an odd last tile takes the row (one line for the layers on the pad: 6.1)
        struct Tile
        {
            int id;
            const wchar_t* word;
            bool enabled, current, stepper;
        } t[12];
        int nt = 0;
        if (pad)
            t[nt++] = {kLayerLine, CurrentLayerWord(layer), nLay > 0, false, true};
        else
            for (int i = 0; i < nLay; ++i)
                t[nt++] = {lay2[i], kDefs[lay2[i]].word, NativeEnabled(g_ctl[lay2[i]]), Current(lay2[i], v, layer), false};
        for (int i = 0; i < nFil; ++i)
            t[nt++] = {fil[i], kDefs[fil[i]].word, NativeEnabled(g_ctl[fil[i]]), Current(fil[i], v, layer), false};
        y = tiles(y, nt);
        for (int i = 0; i < nt; ++i)
            DrawChip(t[i].id, c[i], t[i].word, size, t[i].enabled, t[i].current, t[i].stepper, hover);
        EasePanel(g_panelH[1], y + kPanelPad * H - p.y0);
        g_keep.push_back({p.x0, p.y0, p.x1, p.y1});
    }
    g_panelTick = GetTickCount64();
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
void ListOpen(bool on) { g_listOpen = on; }

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "click") // the map's own click at the cursor (`mouse x y` first): a test's left button, no real mouse needed
        return g_root && uiclick::MapClick(g_root) ? "navskin: clicked" : "navskin: no map";
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
