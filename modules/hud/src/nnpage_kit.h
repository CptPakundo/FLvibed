// What the pages over the game's windows share (nnpage.*, basepage.*): the holes round the game's text, the frame's place, the game's icons in the rows,
// the compare block's numbers: the half that reads and works the game. The draw-only half, included here, is pages\page_kit.h. docs\NEURONET_PLAN.md.
#pragma once
#include "game/baseinfo.h"
#include "game/deckmode.h"
#include "game/screens/compare_screen.h"
#include "game/screens/parked.h"
#include "game/screens/slot_order.h"
#include "game/equip_rules.h"
#include "game/gamedata.h"
#include "game/hudinfo.h"
#include "flexe.h"
#include "skin_rules.h"
#include "navmap_layout.h"
#include "nnpage_rules.h"
#include "nnstrip.h"
#include "safemem.h"
#include "skinchip.h"
#include "skinkit.h"
#include "windowtext.h"
#include "pages/page_kit.h"
#include "ui/hud2_kit.h"
#include "ui/ui.h"
#include "ui/winplates.h"

#include <vector>

namespace nnkit
{
using namespace hud2kit;
// the draw-only half (pages\page_kit.h): the ground, the frame, words, the compare block; Box and ChipAt are this file's own, on the kit's
using pagekit::CompareBlock;
using pagekit::CompareRow;
using pagekit::Cover;
using pagekit::DrawCompare;
using pagekit::Frame;
using pagekit::Ground;
using pagekit::GroundBut;
using pagekit::Holes;
using pagekit::Money;
using pagekit::Title;
using pagekit::Upper;
using pagekit::Wrap;

inline u32 R32(u32 a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}
inline float RF(u32 a)
{
    float v = 0;
    safemem::Read(a, &v, 4);
    return v;
}
inline void W32(u32 a, u32 v)
{
    for (int i = 0; i < 4; ++i)
        safemem::Write(a + i, static_cast<u8>(v >> (8 * i)));
}

// An item's hardpoint, the ship's items in its slots' order, the game's list window moved over the shown rows: core's since
// audit 70b F3b (core\game\screens\slot_order.h, parked.h: the dealer's screen reads and moves them); their names here as before.
using screens::DescHardpoint;
using screens::InSlotOrder;
using screens::SyncList;

// ---- the holes: the game's text shows through (the ground and its holes are drawn by the kit: pages\page_kit.h)
// A hole round the game's controls: each added control's rectangle on the screen.
struct Box : pagekit::Box
{
    void Add(u32 ctl, float grow = 0) { Add(ctl, grow, grow); }
    // grown by `grow` but at the top, by `top` there (less than 0: cut in)
    void Add(u32 ctl, float grow, float top)
    {
        ui::Item it;
        if (!ctl || !ui::ControlRect(ctl, it))
            return;
        float a = it.x - it.hw - grow, b = it.y - it.hh - top, c = it.x + it.hw + grow, d = it.y + it.hh + grow;
        if (exe::VtText.is(R32(ctl))) // (a rich text answers its top left; its box is in pixels, its bar outside on the right)
            a = it.x - grow, b = it.y - top, c = it.x + static_cast<float>(R32(ctl + 0x358)) + grow * 3, d = it.y + static_cast<float>(R32(ctl + 0x35C)) + grow;
        else if (it.hw <= 1)
            return;
        AddRect(a, b, c, d);
    }
};
// The hole to cut for the game's text `ctl`: only once that text stands laid out where it stays (windowtext::TextReady). A newly opened window arrives at the
// game's place and is moved to the page's, and in those frames its title, X and icon passed through the hole (issue 62's residue): until then the hole stays
// shut, the page without its text. What a page lays out by the text (INFO's and LOG's columns) waits for the same frame: an element shows from the frame it
// is right, not before.
inline Box OpenHole(const Box& hole, u32 ctl) { return windowtext::TextReady(ctl) ? hole : Box(); }

// A control of the game's, centred on (cx, cy): its local position through its position property, as windowtext moves a text. A window off the screen keeps it where the page wants it.
using SetProp = void(__thiscall*)(u32, int, const float*, int);
// (a guard, not a rule: a window the pad closed this frame is gone before the page draws, 7 Nov 2026: the callers hold a fresh list, this stays for what still slips through)
inline bool SetPosition(u32 ctl, const float* pos)
{
    __try
    {
        reinterpret_cast<SetProp>(R32(R32(ctl) + 0xA8))(ctl, 6, pos, 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
inline void PlaceAt(u32 ctl, float cx, float cy)
{
    ui::Item it;
    float pos[3] = {};
    if (!ctl || !ui::ControlRect(ctl, it) || !safemem::Read(ctl + 0xA8, pos, sizeof(pos)))
        return;
    if (fabsf(it.x - cx) < 1.5f && fabsf(it.y - cy) < 1.5f)
        return;
    const float H = winplates::g_H;
    pos[0] += (cx - it.x) / H, pos[1] += (it.y - cy) / H;
    SetPosition(ctl, pos);
}
// A row's icon: where the game's mesh of that row stands, or off the page (pages\page_view.h: a view records its icons).
using page::Icon;
// The five meshes of a list (`format` by 1..5 or 0..; a format with no number: that one control), the `nth` of that name on the screen.
inline void PlaceIcons(const char* format, int base, int count, int nth, const Icon* icons)
{
    for (int i = 0; i < count; ++i)
    {
        char name[40];
        if (strchr(format, '%'))
            sprintf_s(name, format, base + i);
        else
            strcpy_s(name, format);
        PlaceAt(baseinfo::Find(name, nth), icons[i].on ? icons[i].cx : -winplates::g_W, icons[i].on ? icons[i].cy : 0);
    }
}
// ---- the rows' item models (docs\FREELANCER_RE.md "The list rows' item models")
// A list window (NN_Inventory, NN_Dealer and its partner) holds five image controls, ItemMesh1..5, and its row fill (0x47C740) gives mesh k the icon of
// the game's row (window start + k) every frame. HUD 2 orders its rows by hardpoint (InSlotOrder), so a shown row's game row can lie outside the game's five
// (issue 58): the pages move the game's window to cover the most shown rows (SyncList), and a row it still misses borrows a mesh the game is not showing on
// the page, sent its item's icon the way the row fill sends it (RowIcons, ListIcons).
constexpr u32 kListSelected = 0x3A4; // the selected row, -1 none
constexpr u32 kListFirst = 0x3C4;    // the game's first shown row: its window of five starts here
constexpr u32 kListMeshes = 0x44C;   // ItemMesh1..5 (0x47B410), mesh k at +4k
constexpr int kListMeshCount = nnrules::kMeshes;
constexpr u32 kImageModelHandle = 0x84; // an image control's model in the engine, -1 none
constexpr u32 kImageModelName = 0xD0;   // the file its model was loaded from (property 0xB copies it out)
// image control properties (vt[0xA8], exe::ImageSetProperty), in the row fill's order: model, flags, selected
constexpr int kImageModel = 4;    // (const char* file, 0): loads that model; the file already loaded returns at once (0x59FBD0 compares the name)
constexpr int kImageFlags = 0x19; // (0x10000, 0x10000): what the row fill sends after a model
constexpr int kImageSelected = 0xC; // (bool, 0): whether the model's row is the selected one (+0x347)

// A good's icon file (goods.ini item_icon: GoodInfo +0x78), null none.
inline const char* ItemIconFile(u32 arch)
{
    using FindGood = const void*(__cdecl*)(u32);
    static const FindGood find = [] {
        const HMODULE common = GetModuleHandleA("common.dll");
        return common ? reinterpret_cast<FindGood>(GetProcAddress(common, "?find_by_archetype@GoodList@@YAPBUGoodInfo@@I@Z")) : nullptr;
    }();
    if (!find || !arch)
        return nullptr;
    const u32 good = static_cast<u32>(reinterpret_cast<uintptr_t>(find(arch)));
    return good ? reinterpret_cast<const char*>(static_cast<uintptr_t>(R32(good + 0x78))) : nullptr;
}
// One property to an image control, only through the image control's own handler.
inline bool ImageProperty(u32 ctl, int id, u32 value, u32 extra)
{
    __try
    {
        const u32 fn = *reinterpret_cast<const u32*>(static_cast<uintptr_t>(*reinterpret_cast<const u32*>(static_cast<uintptr_t>(ctl)) + 0xA8));
        if (!exe::ImageSetProperty.is(fn))
            return false;
        reinterpret_cast<int(__thiscall*)(u32, int, u32, u32)>(fn)(ctl, id, value, extra);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
// Whether the image control holds the model of `file` now.
inline bool ImageHolds(u32 ctl, const char* file)
{
    char now[128] = {};
    return R32(ctl + kImageModelHandle) != 0xFFFFFFFF && safemem::Read(ctl + kImageModelName, now, sizeof(now) - 1) && _stricmp(now, file) == 0;
}
inline int g_iconSends = 0, g_iconLoads = 0, g_iconCatchUps = 0, g_iconHidden = 0; // (debug, `nnpage`: rows given a borrowed mesh; model loads among them; own meshes caught up; rows left without an icon)

// A lent mesh keeps its row's model without a load a frame (issue 58's cost: up to 0.55 ms a frame at rest). The game's fill sends mesh k its own row's
// model every frame, and 0x59FBD0 returns at once when the control already holds a model (+0x84) of that name (+0xD0); a different name drops the model and
// loads the fill's, and the page's send then loads the lent one again. So a lent model stands under the name of the model the fill sends that mesh: the fill
// finds "its" model and leaves it. The true name goes back (the fill then loads its own) the first frame the mesh is not lent for that model any more.
// A mesh whose own row the fill blanks (0x17: an EMPTY hardpoint's row) cannot keep anything: it is lent last (nnrules::AssignMeshes `keeps`).
struct LentModel
{
    u32 window = 0, mesh = 0, handle = 0xFFFFFFFF;
    const char* file = nullptr; // the lent model's file (a GoodInfo's item_icon: the game's own string)
    const char* under = nullptr; // the name it stands under: the fill's model for that mesh
    unsigned seen = 0;           // the last frame a page lent it (LentSweep)
    // Still standing: the same model under the same name. A fill that sent another model replaced both (its new model can come back under the old
    // handle: the engine reuses them, seen 11 Nov 2026), and its name is then not `under`.
    bool Holds() const { return file && R32(mesh + kImageModelHandle) == handle && handle != 0xFFFFFFFF && ImageHolds(mesh, under); }
};
inline LentModel g_lent[2 * nnrules::kMeshes]; // (two windows: the dealer's pair)
inline unsigned g_lentFrame = 1;inline LentModel* LentOn(u32 window, u32 mesh)
{
    for (LentModel& l : g_lent)
        if (l.file && l.window == window && l.mesh == mesh)
            return &l;
    return nullptr;
}
// The control's model's name, as the game's own copy loop writes it (0x59FBD0)
inline bool WriteModelName(u32 ctl, const char* file)
{
    const size_t n = strlen(file);
    if (n >= 120)
        return false;
    for (size_t i = 0; i <= n; ++i)
        if (!safemem::Write(ctl + kImageModelName + static_cast<u32>(i), static_cast<u8>(file[i])))
            return false;
    return true;
}
// What the mesh shows now: a lent model while its handle is the one it was lent under, else the model its name says.
inline bool MeshShows(u32 window, u32 mesh, const char* file)
{
    if (const LentModel* l = LentOn(window, mesh))
        if (l->Holds())
            return _stricmp(l->file, file) == 0;
    return ImageHolds(mesh, file);
}
// The true name back on a lent mesh (the next fill loads its own model) and the record gone.
inline void GiveBack(LentModel& l)
{
    if (l.Holds())
        WriteModelName(l.mesh, l.file);
    l = LentModel();
}

// One shown row of a page's list: the game's row (-1: none, an EMPTY slot), its item, where its icon stands.
struct IconRow
{
    int gameRow = -1;
    u32 arch = 0;
    float cx = 0, cy = 0;
};
// The model the game's fill sends a row of the list: its item's icon (null: none, or a row the fill blanks). `all`: every row of the list.
inline const char* FillFile(int gameRow, const IconRow* all, int allN)
{
    for (int i = 0; i < allN; ++i)
        if (all[i].gameRow == gameRow && gameRow >= 0)
        {
            const char* f = ItemIconFile(all[i].arch);
            return f && *f ? f : nullptr;
        }
    return nullptr;
}
// Gives every shown row that has an item an icon: the game's own mesh where its row is inside the game's window, else one the game's window shows
// nothing of on the page (preferring one that shows the row's model already: no load; then one the fill does not blank). The mesh of each row comes
// back in `meshOf` (-1 none), `icons` by mesh. With `send` the meshes are sent their rows' items: only between the game's row fill and its UI pass,
// which draws the meshes (the pages' Under: the fill runs every frame and would undo a model sent later, before it is ever drawn); a lent model stands
// under the fill's name for its mesh (LentModel), and a row whose mesh does not show its model after the sends gets no icon: never another row's.
// `all` (every row of the list, for the fill's own models) may be null without `send`.
inline void RowIcons(u32 window, const IconRow* rows, int n, Icon* icons, int* meshOf, bool send, const IconRow* all = nullptr, int allN = 0)
{
    n = std::min(n, kListMeshCount);
    const int first = static_cast<int>(R32(window + kListFirst)), selected = static_cast<int>(R32(window + kListSelected));
    u32 mesh[kListMeshCount];
    const char* fill[kListMeshCount]; // what the fill sends mesh k
    for (int k = 0; k < kListMeshCount; ++k)
        mesh[k] = R32(window + kListMeshes + 4 * k), fill[k] = FillFile(first + k, all, allN);
    int gameRow[kListMeshCount];
    bool wants[kListMeshCount], borrowed[kListMeshCount];
    const char* file[kListMeshCount];
    for (int i = 0; i < n; ++i)
    {
        gameRow[i] = rows[i].gameRow;
        file[i] = rows[i].gameRow >= 0 ? ItemIconFile(rows[i].arch) : nullptr;
        wants[i] = file[i] && *file[i];
    }
    nnrules::AssignMeshes(first, gameRow, wants, n, [&](int i, int k) { return mesh[k] && MeshShows(window, mesh[k], file[i]); }, [&](int k) { return fill[k] != nullptr; },
                          meshOf, borrowed);
    if (send)
        for (LentModel& l : g_lent) // (a lent mesh not lent for that model this frame: the true name back first, so its own row's check below sees what it shows)
        {
            if (!l.file || l.window != window)
                continue;
            bool still = false;
            for (int i = 0; i < n && !still; ++i)
                still = meshOf[i] >= 0 && borrowed[i] && mesh[meshOf[i]] == l.mesh && _stricmp(file[i], l.file) == 0;
            if (!still)
                GiveBack(l);
        }
    for (int i = 0; i < n; ++i)
    {
        const int k = meshOf[i];
        if (k < 0 || !mesh[k])
        {
            meshOf[i] = -1;
            continue;
        }
        icons[k] = {rows[i].cx, rows[i].cy, true};
        if (!send || !wants[i])
            continue;
        // the window's own mesh only when it does not show its row's model yet; a lent one keeps its model and gets its row's selected flag
        const bool load = !MeshShows(window, mesh[k], file[i]);
        if (load || borrowed[i])
        {
            perf::Scope ps("nn.RowModel");
            if (load && ImageProperty(mesh[k], kImageModel, static_cast<u32>(reinterpret_cast<uintptr_t>(file[i])), 0))
                ImageProperty(mesh[k], kImageFlags, 0x10000, 0x10000), (borrowed[i] ? ++g_iconLoads : ++g_iconCatchUps);
            if (borrowed[i])
            {
                ImageProperty(mesh[k], kImageSelected, rows[i].gameRow == selected, 0), ++g_iconSends;
                // under the fill's name for this mesh: the fill leaves it (a fill that blanks the mesh, or sends this model, needs nothing)
                LentModel* l = LentOn(window, mesh[k]);
                if (fill[k] && _stricmp(fill[k], file[i]) != 0 && ImageHolds(mesh[k], file[i]) && WriteModelName(mesh[k], fill[k]))
                {
                    if (!l)
                        for (LentModel& e : g_lent)
                            if (!l && (!e.file || e.seen + 2 < g_lentFrame))
                                l = &e;
                    if (l)
                        *l = {window, mesh[k], R32(mesh[k] + kImageModelHandle), file[i], fill[k], g_lentFrame};
                    else
                        WriteModelName(mesh[k], file[i]); // (no record left: no disguise)
                }
                else if (l && l->Holds())
                    l->seen = g_lentFrame;
                else if (l)
                    *l = LentModel();
            }
        }
        // what the UI pass will draw: the row's own model, or no icon
        if (!MeshShows(window, mesh[k], file[i]))
            icons[k].on = false, meshOf[i] = -1, ++g_iconHidden;
    }
}
// Once a frame after every page's sends: a lent mesh no page lent this frame gets its true name back while its window stands (else the record goes:
// the window and its controls are gone). Also when the pages stop (`all`).
inline void LentSweep(bool all = false)
{
    for (LentModel& l : g_lent)
    {
        if (!l.file || (!all && l.seen == g_lentFrame))
            continue;
        const u32 w = l.window;
        if (w && (uiclick::FindVisible("NN_Inventory") == w || uiclick::FindVisible("NN_Dealer") == w || R32(uiclick::FindVisible("NN_Dealer") + 0x360) == w))
            GiveBack(l);
        else
            l = LentModel();
    }
    ++g_lentFrame;
}
// A list as its page drew it, for its Under to place the meshes and send their models before the next UI pass, from the page's rows as they stand
// then: a step or a scroll after the page's last Draw is in the icons of the frame that shows it (the release check of issue 58: the icons stood at the
// old rows for 1 to 2 frames on each step). The page clears it when the list goes.
struct ListIcons
{
    u32 window = 0;
    std::vector<IconRow> all;            // every row of the list (game row, item)
    const nnrules::List* list = nullptr; // the page's list: which rows show; none: `all` are the shown rows
    float cx = 0, y = 0, pitch = 0;      // shown row i's icon at (cx, y + (i + 0.5) pitch)
    int meshOf[kListMeshCount] = {}, n = 0, sentFirst = -1; // as the last Send gave them: shown row i's mesh, -1 none; the list's first row then
    IconRow shown[kListMeshCount];
    std::vector<IconRow> fills; // every row of the list, when `all` is not (the fill's own models: LentModel)
    void Set(u32 w, const IconRow* r, int count, std::vector<IconRow>&& every) // a fixed set of rows (the jettison panel's one)
    {
        window = w, list = nullptr, all.assign(r, r + std::min(count, kListMeshCount)), fills = std::move(every);
    }
    void SetList(u32 w, std::vector<IconRow>&& rows, const nnrules::List* l, float iconX, float top, float rowPitch)
    {
        window = w, all = std::move(rows), fills.clear(), list = l, cx = iconX, y = top, pitch = rowPitch;
    }
    // The meshes of the shown rows as Under left them this frame, null when it has not sent for this window and first row.
    const int* MeshesFor(u32 w, int first) const { return window == w && sentFirst == first && n > 0 ? meshOf : nullptr; }
    // `now`: the window the page holds this frame; another (or none) sends nothing
    void Send(u32 now)
    {
        n = 0, sentFirst = -1;
        if (!window || window != now)
            return;
        const int from = list ? list->first : 0;
        for (; n < kListMeshCount && from + n < static_cast<int>(all.size()) && (!list || n < list->rows); ++n)
        {
            shown[n] = all[from + n];
            if (list)
                shown[n].cx = cx, shown[n].cy = y + (n + 0.5f) * pitch;
        }
        Icon icons[kListMeshCount];
        const std::vector<IconRow>& every = fills.empty() ? all : fills;
        RowIcons(window, shown, n, icons, meshOf, true, every.data(), static_cast<int>(every.size()));
        for (int k = 0; k < kListMeshCount; ++k)
            if (const u32 m = R32(window + kListMeshes + 4 * k))
                PlaceAt(m, icons[k].on ? icons[k].cx : -winplates::g_W, icons[k].on ? icons[k].cy : 0);
        sentFirst = from;
    }
};

// This screen's page frame (pages\page_kit.h Frame): the deck's side columns, the title on the strip's line.
inline Frame Bounds()
{
    Frame f;
    f.W = winplates::g_W, f.H = winplates::g_H;
    f.left = deckmode::SideW() * f.W, f.right = f.W - f.left;
    float r[4] = {};
    f.titleY = nnstrip::Rect(r) ? (r[1] + r[3]) / 2 : 0.07f * f.H;
    f.top = 0.135f * f.H, f.bottom = 0.865f * f.H;
    return f;
}

// A page stands this frame: what the pad's prompt lane must know of it (core\ui\winplates.h). The pad's B CLOSE stands at
// the right edge, where the page's prompts end: a mark at the lane's left keeps the row there (as on the map); `content`
// = the page's content box stands (not in the gap between two windows' slides): its bottom edge is the floor a second row
// of prompts keeps under.
inline void PageLane(const Frame& f, bool content = true)
{
    const float lane = deckmode::LaneTopH() * f.H;
    winplates::g_keep.push_back({f.left, lane, f.left + 0.02f * f.H, lane + 0.02f * f.H});
    if (content)
        winplates::g_pageFloor = f.bottom;
}

// The mouse's chips stand in the prompts' lane, level with CLOSE.
inline float LaneY() { return deckmode::LaneTopH() * winplates::g_H; }
inline float LaneChipH() { return (deckmode::LaneBottomH() - deckmode::LaneTopH()) * winplates::g_H; }

// One chip at (x, y), its click to the skin's click box; the x after it. The kit's chip, its box handed on at once.
inline float ChipAt(float x, float y, float h, const std::wstring& word, skinchip::Action act, int arg, bool on = true)
{
    page::ViewIn in{winplates::g_W, winplates::g_H, 0, 0, 0, 0};
    in.hover = skinchip::Hovered() >= 0 && skinchip::Hovered() == skinchip::Count() ? 0 : -1; // (the box's next rectangle is this chip's)
    page::ViewOut out;
    const float next = pagekit::ChipAt(in, out, x, y, h, word, 0, arg, on);
    if (out.nHits > 0)
        skinchip::Hot({out.hits[0].x0, out.hits[0].y0, out.hits[0].x1, out.hits[0].y1}, act, arg);
    return next;
}

// What the compare block shows of an item against what the ship carries (drawn by pages\page_kit.h DrawCompare).
// (the read is core's since audit 70b F3a, so a screen's model can hold the block: core\game\screens\compare_screen.cpp)
inline pagekit::CompareBlock ReadCompare(u32 arch, bool mounted) { return screens::ReadCompare(arch, mounted); }
// An item against what the ship carries, read and drawn. The y after it.
inline float Compare(float x0, float x1, float y, u32 arch, bool mounted, float H) { return DrawCompare(x0, x1, y, ReadCompare(arch, mounted), H); }
} // namespace nnkit
