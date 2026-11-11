// What the pages over the game's windows share (nnpage.*, basepage.*): the holes the game's text shows through, the frame, the compare block. docs\NEURONET_PLAN.md.
#pragma once
#include "game/baseinfo.h"
#include "game/deckmode.h"
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
#include "ui/hud2_kit.h"
#include "ui/ui.h"
#include "ui/winplates.h"

#include <vector>

namespace nnkit
{
using namespace hud2kit;

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

// An EquipDesc's hardpoint, lower case ("" in the hold): its name through the pointer at +8 (baseinfo's WalkEquipment reads the same, node +0x10).
inline std::string DescHardpoint(u32 desc)
{
    char hp[33] = {};
    const u32 p = R32(desc + 8);
    if (!p || !safemem::Read(p, hp, 32))
        return std::string();
    return equiprules::Lower(hp);
}

// A list of the ship's items in its slots' fixed order (issue 52, the owner: a slot's place is part of the ship): every hardpoint of the filter where it stands, what is
// mounted there or an EMPTY row; then the rest (the hold, a hardpoint not read) in the game's order. `hpOf(row)` the row's hardpoint, `empty(i)` the EMPTY row of free[i];
// `free` comes back holding the empty slots in that order.
template <class R, class HpOf, class Empty>
std::vector<R> InSlotOrder(const std::vector<R>& rows, int filter, std::vector<equiprules::Hp>& free, HpOf hpOf, Empty empty)
{
    std::vector<R> out;
    std::vector<bool> used(rows.size(), false);
    free.clear();
    for (const equiprules::Hp& h : gamedata::ShipSlots(filter))
    {
        size_t at = rows.size();
        for (size_t i = 0; i < rows.size() && at == rows.size(); ++i)
            if (!used[i] && hpOf(rows[i]) == h.name)
                at = i;
        if (at < rows.size())
        {
            used[at] = true;
            out.push_back(rows[at]);
            continue;
        }
        free.push_back(h);
        out.push_back(empty(static_cast<int>(free.size()) - 1));
    }
    for (size_t i = 0; i < rows.size(); ++i)
        if (!used[i])
            out.push_back(rows[i]);
    return out;
}

// ---- the holes: the game's text shows through
struct Box
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool ok = false;
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
        x0 = ok ? std::min(x0, a) : a, y0 = ok ? std::min(y0, b) : b, x1 = ok ? std::max(x1, c) : c, y1 = ok ? std::max(y1, d) : d;
        ok = true;
    }
};
// The hole to cut for the game's text `ctl`: only once that text stands laid out where it stays (windowtext::TextReady). A newly opened window arrives at the
// game's place and is moved to the page's, and in those frames its title, X and icon passed through the hole (issue 62's residue): until then the hole stays
// shut, the page without its text. What a page lays out by the text (INFO's and LOG's columns) waits for the same frame: an element shows from the frame it
// is right, not before.
inline Box OpenHole(const Box& hole, u32 ctl) { return windowtext::TextReady(ctl) ? hole : Box(); }
inline void Ground(float x0, float y0, float x1, float y1, float alpha = 1)
{
    if (x1 > x0 && y1 > y0)
        Rect(x0, y0, x1 - x0, y1 - y0, Argb(0x060716, alpha));
}
// The page's ground, but for `hole`.
inline void Cover(float W, float H, const Box& hole, float alpha = 1)
{
    if (!hole.ok)
    {
        Ground(0, 0, W, H, alpha);
        return;
    }
    Ground(0, 0, W, hole.y0, alpha);
    Ground(0, hole.y1, W, H, alpha);
    Ground(0, hole.y0, hole.x0, hole.y1, alpha);
    Ground(hole.x1, hole.y0, W, hole.y1, alpha);
}

// Several holes in one ground: the game's 3D icons stand in them.
struct Holes
{
    float r[16][4];
    int n = 0;
    void Add(float x0, float y0, float x1, float y1)
    {
        if (n < 16)
            r[n][0] = x0, r[n][1] = y0, r[n][2] = x1, r[n][3] = y1, ++n;
    }
};
inline void GroundBut(float x0, float y0, float x1, float y1, const Holes& h)
{
    float ys[34];
    int m = 0;
    ys[m++] = y0, ys[m++] = y1;
    for (int i = 0; i < h.n; ++i)
        ys[m++] = std::max(y0, std::min(y1, h.r[i][1])), ys[m++] = std::max(y0, std::min(y1, h.r[i][3]));
    std::sort(ys, ys + m);
    for (int b = 0; b + 1 < m; ++b)
    {
        const float a = ys[b], c = ys[b + 1];
        if (c <= a)
            continue;
        int order[16], k = 0;
        for (int i = 0; i < h.n; ++i)
            if (h.r[i][1] <= a && h.r[i][3] >= c)
                order[k++] = i;
        std::sort(order, order + k, [&](int p, int q) { return h.r[p][0] < h.r[q][0]; });
        float x = x0;
        for (int i = 0; i < k; ++i)
        {
            Ground(x, a, h.r[order[i]][0], c);
            x = std::max(x, h.r[order[i]][2]);
        }
        Ground(x, a, x1, c);
    }
}

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
// A row's icon: where the game's mesh of that row stands, or off the page.
struct Icon
{
    float cx = 0, cy = 0;
    bool on = false;
};
// The five meshes of a list (`format` by 1..5 or 0..), the `nth` of that name on the screen.
inline void PlaceIcons(const char* format, int base, int count, int nth, const Icon* icons)
{
    for (int i = 0; i < count; ++i)
    {
        char name[40];
        sprintf_s(name, format, base + i);
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

// The list of a window whose rows show: the game's own scrolled to `first`, so its five meshes are the page's.
inline void SyncList(u32 window, int first)
{
    const int now = static_cast<int>(R32(window + kListFirst));
    if (first >= 0 && first != now)
        if (const u32 bar = R32(window + 0x3D0))
            uiclick::Scroll(bar, static_cast<float>(first - now), false, nullptr, nullptr);
}
// The same for the page's shown rows (their game rows, -1 none): the start whose five show the most of them (nnrules::BestWindowStart), so
// the fewest borrow a mesh (RowIcons).
inline void SyncList(u32 window, const int* gameRow, int n) { SyncList(window, nnrules::BestWindowStart(gameRow, n)); }

struct Frame
{
    float W, H, left, right, top, bottom, titleY;
};
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
inline void Title(const Frame& f, const std::wstring& word) { Text(f.left, f.titleY, word, 0.044f * f.H, Argb(kInk, 1)); }

inline std::wstring Money(long long v)
{
    std::wstring s = std::to_wstring(v < 0 ? -v : v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(static_cast<size_t>(i), L",");
    return (v < 0 ? L"-$" : L"$") + s;
}

// The mouse's chips stand in the prompts' lane, level with CLOSE.
inline float LaneY() { return deckmode::LaneTopH() * winplates::g_H; }
inline float LaneChipH() { return (deckmode::LaneBottomH() - deckmode::LaneTopH()) * winplates::g_H; }

// One chip at (x, y), its click; the x after it.
inline float ChipAt(float x, float y, float h, const std::wstring& word, skinchip::Action act, int arg, bool on = true)
{
    const float size = skinrules::kChipWord * h / skinrules::kChipLow;
    const navlayout::Rect r{x, y, x + TextWidth(word, size) + 0.03f * winplates::g_H, y + h};
    const int slot = on ? skinchip::Hot(r, act, arg) : -1;
    skinchip::Chip(r, word, size, !on ? skinchip::Look::Disabled : slot >= 0 && slot == skinchip::Hovered() ? skinchip::Look::Hover : skinchip::Look::Plain);
    return r.x1 + 0.012f * winplates::g_H;
}

// `text` in lines that fit `width`: its own newlines kept, by whole words.
inline std::vector<std::wstring> Wrap(const std::wstring& text, float size, float width)
{
    std::vector<std::wstring> lines;
    size_t i = 0;
    while (i <= text.size())
    {
        size_t e = text.find(L'\n', i);
        if (e == std::wstring::npos)
            e = text.size();
        std::wstring line;
        size_t w = i;
        while (w < e)
        {
            size_t sp = text.find(L' ', w);
            if (sp == std::wstring::npos || sp > e)
                sp = e;
            const std::wstring word = text.substr(w, sp - w);
            const std::wstring next = line.empty() ? word : line + L" " + word;
            if (!line.empty() && TextWidth(next, size) > width)
            {
                lines.push_back(line);
                line = word;
            }
            else
                line = next;
            w = sp + 1;
        }
        lines.push_back(line);
        i = e + 1;
    }
    return lines;
}

inline std::wstring Upper(std::wstring s)
{
    for (wchar_t& c : s)
        c = towupper(c);
    return s;
}

// An item against what the ship carries: a heading, then its numbers, each beside the mounted item's difference. The y after it.
inline float Compare(float x0, float x1, float y, u32 arch, bool mounted, float H)
{
    const equiprules::Spec* me = gamedata::Equip(arch);
    if (!me || me->stats.empty())
        return y;
    using namespace equiprules;
    // a mounted item compares only with the tagged one, where that sits on a hardpoint this one could go on (Shockwave, 7 Nov: with an empty hold the tag showed nothing)
    gamedata::Versus v = gamedata::Against(arch);
    if (mounted && (!v.tagged || v.arch == arch))
        v = gamedata::Versus();
    const Spec* other = v.arch ? gamedata::Equip(v.arch) : nullptr;
    std::wstring head;
    u32 rgb = kChrome;
    if (mounted && !(v.tagged && other))
        head = L"MOUNTED"; // (else VS ITS NAME (TAGGED) below: the row says MOUNTED, and the middle column is narrow at 1280x800)
    else if (v.hasSlot && !v.fits)
        head = L"NO HARDPOINT ON THIS SHIP TAKES IT", rgb = kAmber;
    else if (v.tagged && other) // (the player's choice first, a free hardpoint or not)
        head = L"VS " + Upper(hudinfo::EquipName(v.arch)) + L" (TAGGED)", rgb = kActive;
    else if (v.freeHp)
        head = L"FITS A FREE HARDPOINT", rgb = kFriend;
    else if (other)
        head = L"VS " + Upper(hudinfo::EquipName(v.arch));
    else
        head = L"NUMBERS";
    const std::wstring slot = SlotWord(SlotOf(me->type));
    const float sw = slot.empty() ? 0 : TextWidth(slot, 0.022f * H) + 0.012f * H;
    TextMarquee(x0, y + 0.014f * H, head, 0.026f * H, x1 - x0 - sw, Argb(rgb, 1));
    if (!slot.empty())
        Text(x1, y + 0.014f * H, slot, 0.022f * H, Argb(kChrome, 0.8f), Right);
    y += 0.034f * H;
    const float pitch = 0.032f * H, c1 = other ? x0 + (x1 - x0) * 0.68f : x1; // (no difference to show: the value stands at the column's edge)
    for (const Stat& s : me->stats)
    {
        Text(x0, y + pitch / 2, s.label, 0.024f * H, Argb(kChrome, 1));
        Text(c1, y + pitch / 2, ValueWord(s.value) + L" " + s.unit, 0.026f * H, Argb(kInk, 1), Right);
        float theirs = 0;
        if (other && Find(other->stats, s.label, theirs))
        {
            const Verdict j = Judge(s, theirs);
            Text(x1, y + pitch / 2, DeltaWord(s.value, theirs), 0.026f * H, Argb(j == Verdict::Better ? kFriend : j == Verdict::Worse ? kHostile : kChrome, 1), Right);
        }
        y += pitch;
    }
    return y + 0.008f * H;
}
} // namespace nnkit
