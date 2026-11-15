// The page runtime: pagerun.h. The park and the model's moments are nnpage.cpp's INFO as it was (audit 70b F2a moved them
// here unchanged): the park is the window's own position setter (basepage's Away does the same for the dealers), so the
// window's calls do not care where it stands.
#include "pagerun.h"

#include "pagelist.h"
#include "../nnpage_kit.h"
#include "../skinchip.h"
#include "game/baseinfo.h"
#include "game/nnwin.h"
#include "game/room.h"
#include "game/screens/parked.h"
#include "game/screens/screen_rules.h"
#include "flexe.h"
#include "hook.h"
#include "safemem.h"
#include "text.h"
#include "ui/glyphs.h"
#include "ui/hud2_kit.h"
#include "ui/ui.h"
#include "ui/winplates.h"

#include <sstream>

namespace pagerun
{
// What the runtime keeps per listed page (by its index in pagelist::All()).
struct Slot
{
    void* model = nullptr; // the model's storage (ModelOps::size), made on the first open
    bool built = false;    // Init ran and Done has not
    page::PageState state;
    pageinput::RepeatState repeat; // a held scroll's part line
    uint64_t stamp = 0;
    int reads = 0;  // the model's reads that changed it, for the verb
    int shell = 0;  // the shell that keeps it (pagerun.h kShell*)
};
static Slot g_slots[pagelist::List::kMax];
static int g_open = -1;  // the standing page's index, -1 none
static u32 g_root = 0;   // the window it stands over
static bool g_pad = false; // the pad's form this frame (Frame), for a click's Hit hook
static page::Holes g_holes; // the holes the last view left in its ground (Draw), drawn in under the game's pass (GroundUnder)
static screens::Runner g_runner;

struct ParkedWindow
{
    int page = -1; // whose window
    u32 root = 0;
    float pos[3] = {};
};
static ParkedWindow g_parked; // the window parked and where the game had put it

static u32 R32(u32 a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}
static int IndexOf(const char* id)
{
    const pagelist::List& l = pagelist::All();
    for (int i = 0; id && i < l.Count(); ++i)
        if (!strcmp(l.At(i).core->id, id))
            return i;
    return -1;
}
static const page::PageDesc* Desc(int i)
{
    if (i < 0 || i >= pagelist::All().Count() || pagelist::All().At(i).core->kind != page::Kind::Page)
        return nullptr; // (the runtime runs full pages so far; the other kinds come with their pages)
    return static_cast<const page::PageDesc*>(pagelist::All().At(i).desc);
}

// ---- the park
// Off the screen by the window's own setter; once per window: a parked window is left alone. A Root page's window has the
// controls' setter; an Away page's may have the inventory's own (NN_Inventory: basepage.cpp's Away took both, audit 70b F3a).
static void Park(int page, u32 root, page::Park how, bool partner = false)
{
    ui::Item it;
    float pos[3] = {};
    if (!root || !ui::ControlRect(root, it) || it.x > 1.5f * winplates::g_W || !safemem::Read(root + 0x60, pos, sizeof(pos)))
        return;
    const u32 set = R32(R32(root) + 0x14);
    const bool control = exe::ControlSetPosition.is(set);
    if (!control && !(how == page::Park::Away && exe::InventorySetPosition.is(set)))
        return;
    if (!partner && g_parked.root != root) // (a partner is never given back: nothing to remember of it)
        g_parked.root = root, g_parked.page = page, memcpy(g_parked.pos, pos, sizeof(pos));
    pos[0] += 4;
    hk::Call<void(__thiscall*)(u32, const float*)>(control ? exe::ControlSetPosition : exe::InventorySetPosition, root, pos);
}
// The page went while its window stays (another page, the interface switched to the classic HUD with the page open): the
// window back where the game had it. `stillOpen`: the window the game shows by that name now (a closed one is not found).
// An Away page's window is never given back: it closes by its own close button (page.h Park::Away).
static void Unpark(u32 stillOpen)
{
    const page::PageDesc* d = Desc(g_parked.page);
    const bool away = d && d->core.park == page::Park::Away;
    if (!away && g_parked.root && stillOpen == g_parked.root && exe::ControlSetPosition.is(R32(R32(stillOpen) + 0x14)))
        hk::Call<void(__thiscall*)(u32, const float*)>(exe::ControlSetPosition, stillOpen, g_parked.pos);
    g_parked = ParkedWindow();
}
static void UnparkByName()
{
    const page::PageDesc* d = Desc(g_parked.page);
    Unpark(d && d->core.claim.name && d->core.park != page::Park::Away ? uiclick::FindVisible(d->core.claim.name) : 0);
}

// ---- the model
static void Open(int i, const page::PageDesc& d, Slot& s)
{
    if (!s.model)
        s.model = ::operator new(d.model.size);
    if (s.built && d.model.Done)
        d.model.Done(s.model);
    d.model.Init(s.model);
    s.built = true;
}
// The model read again: when its text changed, the page starts at its top and lays it out again.
static void Read(const page::PageDesc& d, Slot& s, u32 root)
{
    if (!d.model.Read(s.model, root))
        return;
    ++s.reads;
    s.state.wrapW = -1, s.state.first = 0, s.repeat = pageinput::RepeatState();
}

// ---- the entries
u32 Claim(const char* id)
{
    const page::PageDesc* d = Desc(IndexOf(id));
    if (!d || !d->core.claim.vt)
        return 0;
    hk::Sym* vt = const_cast<hk::Sym*>(d->core.claim.vt); // (Sym::is is not const: the description holds it as a value)
    for (const winplates::Win& w : winplates::g_wins)
        if (vt->is(w.vt) && winplates::Shown(w.root) && (!d->core.claim.Is || d->core.claim.Is(w.root)))
            return w.root;
    return 0;
}

void Keep(const char* id, u32 root, int shell)
{
    const int i = IndexOf(id);
    if (i >= 0)
        g_slots[i].shell = shell;
    // (each shell answers for its own pages: the other shell's page, standing or parked, is not this one's to release)
    if (g_parked.root && g_slots[g_parked.page].shell == shell && (i < 0 || i != g_parked.page || root != g_parked.root))
        UnparkByName();
    if ((g_open < 0 || g_slots[g_open].shell == shell) && (i < 0 || i != g_open))
        g_open = -1, g_root = 0, g_runner.Drop();
}

void Frame(const char* id, u32 root, bool opened, int wheel, float dt, bool pad, u32 partner)
{
    const int i = IndexOf(id);
    const page::PageDesc* d = Desc(i);
    if (!d)
        return;
    g_pad = pad;
    Slot& s = g_slots[i];
    if (opened || !s.built || i != g_open || root != g_root)
    {
        Open(i, *d, s);
        Read(*d, s, root);
        s.stamp = d->model.Stamp ? d->model.Stamp() : 0;
        g_runner.Drop();
    }
    else if (d->model.Stamp)
    {
        const uint64_t now = d->model.Stamp();
        if (now != s.stamp)
            Read(*d, s, root);
        s.stamp = now;
    }
    g_open = i, g_root = root;
    if ((d->core.park == page::Park::Root || d->core.park == page::Park::Away) && !room::NNWindowBusy()) // (after the open slide, which places the window itself)
    {
        Park(i, root, d->core.park);
        if (partner && d->core.park == page::Park::Away)
            Park(i, partner, d->core.park, true);
    }
    if (d->core.input && d->core.input->Tick)
        d->core.input->Tick(s.model, root, wheel, dt, pad);
    else if (d->core.input && d->core.input->Wheel)
        d->core.input->Wheel(s.model, wheel);
    else
        s.state.first -= wheel;
    const screens::RunEnv env = {nnwin::Multiplayer(), root, nullptr};
    const screens::Outcome o = g_runner.Tick(d->core.actions, d->core.nActions, s.model, dt);
    if (o.result == screens::Result::TimedOut)
        Log("page: %s action %d: no answer, cleared", d->core.id, o.action);
    g_runner.FrameEnd(d->core.actions, d->core.nActions, s.model, env);
}

static void Run(const page::PageDesc& d, Slot& s, int action, int arg);

// A click on one of the view's boxes (the skin's click box calls it with the box's action and argument in one int): the
// page's action through the runner, or the page's own Hit hook (a row's pick). Nothing when no page stands any more.
static void HitClick(int code)
{
    const page::PageDesc* d = Desc(g_open);
    if (!d || !g_slots[g_open].built)
        return;
    const int action = code >> 16, arg = code & 0xFFFF;
    for (int a = 0; a < d->core.nActions; ++a)
        if (d->core.actions[a].id == action)
        {
            Run(*d, g_slots[g_open], action, arg);
            return;
        }
    if (d->core.input && d->core.input->Hit)
        d->core.input->Hit(g_slots[g_open].model, action, arg, g_pad);
}
static void HitNothing(int) {} // (a box that only keeps the mouse off what lies under it: the card's)

// ---- the game lists (page_view.h GameList): their meshes, holes and models, as basepage's dealer did them (audit 70b F3b)
using nnkit::Icon;
using nnkit::IconRow;
using nnkit::kListMeshCount;
using nnkit::ListIcons;
using nnkit::PlaceIcons;
using nnkit::RowIcons;
static ListIcons g_lists[page::ViewOut::kLists];          // each list as the last view drew it, its models sent in IconsUnder
static const unsigned* g_listWindow[page::ViewOut::kLists]; // the model's window of each: the one it holds when they are sent
static bool g_listOnRoot[page::ViewOut::kLists];             // the list is the page's own window: it sends while that page still stands over it
static int g_nLists = 0;

// Which of the two names' ItemMesh1 is this list's: the one in its window; none (0) when only the partner's stands (this list has
// no rows: what is there is the other's, not to be moved away); else by their places on the screen, `right` the right one.
static int MeshNth(u32 window, u32 partner, bool right)
{
    for (int n = 1; n <= 2; ++n)
        if (screens::InWindow(baseinfo::Find("ItemMesh1", n), window))
            return n;
    for (int n = 1; n <= 2; ++n)
        if (screens::InWindow(baseinfo::Find("ItemMesh1", n), partner))
            return 0;
    ui::Item a, b;
    const u32 one = baseinfo::Find("ItemMesh1", 1), two = baseinfo::Find("ItemMesh1", 2);
    if (!one || !two || !ui::ControlRect(one, a) || !ui::ControlRect(two, b))
        return 1;
    const int rightOne = a.x > b.x ? 1 : 2;
    return right ? rightOne : 3 - rightOne;
}

// One list after the view: its shown rows' meshes (no send yet: that is IconsUnder's), a hole round each row that got one, the
// list kept for IconsUnder, the meshes moved to their rows.
static void WorkList(int k, const page::GameList& gl, page::ViewOut& out)
{
    const u32 window = gl.window ? *gl.window : 0, partner = gl.partner ? *gl.partner : 0;
    if (gl.atSend) // (the INVENTORY's form: only kept for IconsUnder, which gives, places and sends; the view cut its holes)
    {
        std::vector<IconRow> every(gl.rows.size());
        for (size_t i = 0; i < gl.rows.size(); ++i)
            every[i] = {gl.rows[i].gameRow, gl.rows[i].arch, 0, 0};
        if (gl.fixed > 0)
        {
            IconRow fixed[kListMeshCount];
            const int n = std::min(gl.fixed, kListMeshCount);
            for (int i = 0; i < n; ++i)
                fixed[i] = {gl.fixedRows[i].row.gameRow, gl.fixedRows[i].row.arch, gl.fixedRows[i].cx, gl.fixedRows[i].cy};
            g_lists[k].Set(window, fixed, n, std::move(every));
        }
        else if (gl.list)
            g_lists[k].SetList(window, std::move(every), gl.list, gl.x0 + gl.cell / 2, gl.top, gl.pitch);
        g_listWindow[k] = gl.window, g_listOnRoot[k] = window && window == g_root;
        return;
    }
    const screens::List& l = *gl.list;
    IconRow shown[kListMeshCount];
    int n = 0;
    for (; n < kListMeshCount && n < l.rows && l.first + n < l.count && l.first + n < static_cast<int>(gl.rows.size()); ++n)
        shown[n] = {gl.rows[l.first + n].gameRow, gl.rows[l.first + n].arch, gl.x0 + gl.cell / 2, gl.top + n * gl.pitch + gl.pitch / 2};
    Icon icons[kListMeshCount];
    int meshOf[kListMeshCount] = {};
    RowIcons(window, shown, n, icons, meshOf, false);
    for (int i = 0; i < n; ++i)
        if (meshOf[i] >= 0)
        {
            const float top = gl.top + i * gl.pitch;
            out.holes.Add(gl.x0 + 1, top + (gl.pitch - gl.cell) / 2, gl.x0 + gl.cell - 1, top + (gl.pitch + gl.cell) / 2);
        }
    std::vector<IconRow> every(gl.rows.size()); // (Under places and sends from the list as it stands then: issue 58's step lag)
    for (size_t i = 0; i < gl.rows.size(); ++i)
        every[i] = {gl.rows[i].gameRow, gl.rows[i].arch, 0, 0};
    g_lists[k].SetList(window, std::move(every), gl.list, gl.x0 + gl.cell / 2, gl.top, gl.pitch);
    g_listWindow[k] = gl.window, g_listOnRoot[k] = window && window == g_root;
    if (const int nth = MeshNth(window, partner, gl.right))
        PlaceIcons("ItemMesh%d", 1, 5, nth, icons);
}

void DrawStart(const page::Holes& holes)
{
    for (ListIcons& l : g_lists)
        l = ListIcons();
    g_nLists = 0;
    g_holes = holes;
}

// The page's own window's list is checked against the window the page stands over now (none, or another page's, in the frame
// another docked page took its place: no send, as basepage's Send(WindowOf) had it); its partner's against the model's.
void IconsUnder()
{
    for (int k = 0; k < g_nLists; ++k)
        g_lists[k].Send(g_listOnRoot[k] ? g_root : g_listWindow[k] ? *g_listWindow[k] : 0);
}

void DropLists()
{
    for (ListIcons& l : g_lists)
        l = ListIcons();
    g_nLists = 0;
}

int ListShown(int k, int* gameRows, int max)
{
    if (k < 0 || k >= page::ViewOut::kLists)
        return 0;
    int n = 0;
    for (; n < g_lists[k].n && n < max; ++n)
        gameRows[n] = g_lists[k].shown[n].gameRow;
    return n;
}

pageinput::HoldState& PurchaseHold()
{
    static pageinput::HoldState hold;
    return hold;
}

page::ViewOut Draw(const page::ViewIn& in)
{
    page::ViewOut out;
    const page::PageDesc* d = Desc(g_open);
    if (d && d->view && g_slots[g_open].built)
    {
        // the boxes go to the skin's click box after the view, in its order, so a box's slot is the first free one plus its
        // index: the hovered slot (the last frame's) names the box the view lights (audit 97_ F0 note 2)
        page::ViewIn vin = in;
        const int first = skinchip::Count(), hovered = skinchip::Hovered();
        vin.hover = hovered >= first ? hovered - first : -1;
        vin.holes = g_holes;
        static_assert(sizeof(vin.sent) / sizeof(vin.sent[0]) == page::ViewOut::kLists && screens::kMeshes == kListMeshCount, "a list's sends");
        for (int k = 0; k < page::ViewOut::kLists; ++k)
        {
            page::ViewIn::Sent& s = vin.sent[k];
            s.window = g_lists[k].window, s.first = g_lists[k].sentFirst, s.n = g_lists[k].n;
            for (int i = 0; i < kListMeshCount; ++i)
                s.meshOf[i] = g_lists[k].meshOf[i];
        }
        if (d->core.input && d->core.input->Prepare)
            d->core.input->Prepare(g_slots[g_open].model, g_root);
        d->view(vin, g_slots[g_open].model, g_slots[g_open].state, out);
        for (int i = 0; i < out.nHits; ++i)
        {
            const page::Hit& h = out.hits[i];
            skinchip::Hot({h.x0, h.y0, h.x1, h.y1}, h.action < 0 ? HitNothing : HitClick, h.action < 0 ? 0 : (h.action << 16) | (h.arg & 0xFFFF));
        }
        g_nLists = 0;
        for (int i = 0; i < out.nLists && i < page::ViewOut::kLists; ++i)
            if (out.lists[i].list || (out.lists[i].atSend && out.lists[i].fixed > 0))
                WorkList(g_nLists++, out.lists[i], out);
        for (int k = g_nLists; k < page::ViewOut::kLists; ++k)
            g_lists[k] = ListIcons(); // (a list this view did not draw sends nothing, and its last send is forgotten)
        for (int i = 0; i < out.nIconSets; ++i)
        {
            const page::IconSet& set = out.iconSets[i];
            if (set.format)
                nnkit::PlaceIcons(set.format, set.base, set.count, set.nth, set.icons);
        }
        g_holes = out.holes;
    }
    return out;
}

void GroundUnder()
{
    for (int i = 0; i < g_holes.n; ++i)
        pagekit::GroundBut(g_holes.r[i][0], g_holes.r[i][1], g_holes.r[i][2], g_holes.r[i][3], page::Holes());
}

// The pad button's glyph for a prompt.
static int GlyphOf(unsigned pad)
{
    using namespace glyphs;
    const struct
    {
        unsigned bit;
        int glyph;
    } kMap[] = {{XINPUT_GAMEPAD_A, A}, {XINPUT_GAMEPAD_B, B}, {XINPUT_GAMEPAD_X, X}, {XINPUT_GAMEPAD_Y, Y},
                {XINPUT_GAMEPAD_DPAD_UP, DU}, {XINPUT_GAMEPAD_DPAD_DOWN, DD}, {XINPUT_GAMEPAD_DPAD_LEFT, DL},
                {XINPUT_GAMEPAD_DPAD_RIGHT, DR}};
    for (const auto& k : kMap)
        if (pad & k.bit)
            return k.glyph;
    return -1;
}

int Prompts(views::PagePrompt* out, int max)
{
    const page::PageDesc* d = Desc(g_open);
    if (!d)
        return 0;
    const Slot& s = g_slots[g_open];
    if (d->core.input && d->core.input->Prompts)
        return s.built ? d->core.input->Prompts(s.model, out, max) : 0;
    int n = 0;
    auto add = [&](int id, const wchar_t* word, int id2 = -1)
    {
        if (n < max)
            out[n++] = {id, id2, word, false};
    };
    if (d->core.scroll && s.state.scrolls) // (a prompt only where its press does something: the text longer than the page)
        add(glyphs::DU, L"SCROLL", glyphs::DD);
    int shown[promptlane::kMax];
    const int k = page::Prompts(d->core, d->core.pad, d->core.nPad, s.model, 0, shown);
    for (int j = 0; j < k; ++j)
        add(GlyphOf(d->core.pad[shown[j]].pad), d->core.pad[shown[j]].word);
    return n;
}

// One press of an action, through the runner; a disagreement with the game's window is logged. Whether it ran.
static bool RunOutcome(const page::PageDesc& d, Slot& s, int action, int arg)
{
    for (int a = 0; a < d.core.nActions; ++a)
        if (d.core.actions[a].id == action)
        {
            const screens::RunEnv env = {nnwin::Multiplayer(), g_root, nullptr};
            const screens::Outcome o = g_runner.Run(d.core.actions[a], s.model, arg, env);
            if (o.result == screens::Result::Shadow)
            {
                char line[160];
                screens::ShadowLine(line, sizeof(line), d.core.id, d.core.actions[a].name, g_runner.LastShadow());
                Log("%s", line);
            }
            return o.result == screens::Result::Done || o.result == screens::Result::Pending || o.result == screens::Result::Deferred;
        }
    return false;
}
static void Run(const page::PageDesc& d, Slot& s, int action, int arg) { RunOutcome(d, s, action, arg); }

bool Act(int action, int arg)
{
    const page::PageDesc* d = Desc(g_open);
    return d && g_slots[g_open].built && RunOutcome(*d, g_slots[g_open], action, arg);
}

unsigned Pad(unsigned pressed, unsigned held, float dt)
{
    const page::PageDesc* d = Desc(g_open);
    if (!d)
        return 0;
    Slot& s = g_slots[g_open];
    if (d->core.input && d->core.input->Pad)
        return s.built ? d->core.input->Pad(s.model, pressed, held, dt) | d->core.padTakes : 0;
    unsigned taken = d->core.padTakes;
    for (int b = 0; b < d->core.nPad; ++b)
    {
        taken |= d->core.pad[b].pad;
        if ((pressed & d->core.pad[b].pad) && d->core.pad[b].action >= 0)
            Run(*d, s, d->core.pad[b].action, 0);
    }
    if (d->core.scroll)
    {
        // a press a line, a held D-pad or right stick by the page's repeat
        taken |= XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN;
        s.state.first += pageinput::ScrollLines(*d->core.scroll, s.repeat, (pressed & XINPUT_GAMEPAD_DPAD_DOWN) != 0, (pressed & XINPUT_GAMEPAD_DPAD_UP) != 0,
                                                (held & (XINPUT_GAMEPAD_DPAD_DOWN | views::kPagePadRsDown)) != 0,
                                                (held & (XINPUT_GAMEPAD_DPAD_UP | views::kPagePadRsUp)) != 0, dt);
    }
    return taken;
}

void Shutdown()
{
    if (g_parked.root)
        UnparkByName(); // (the module going with a page open: the game's window back on the screen)
    g_parked = ParkedWindow();
    g_runner.Drop();
    g_open = -1, g_root = 0;
    DrawStart(page::Holes()); // (no list sends, no hole stays)
    for (int i = 0; i < pagelist::List::kMax; ++i)
    {
        Slot& s = g_slots[i];
        const page::PageDesc* d = Desc(i);
        if (s.built && d && d->model.Done)
            d->model.Done(s.model);
        ::operator delete(s.model);
        const page::PageState kept = s.state; // (the page's state outlives the module's going, as it always did)
        s = Slot();
        s.state = kept;
    }
}

bool Standing() { return pagelist::Legacy().Standing(); }
bool Covers(int fam) { return pagelist::Legacy().Covers(fam); }
bool ControlsOffScreen() { return pagelist::Legacy().OffScreen(); }
void DrawUnder() { pagelist::Legacy().Under(); }

// The test verb `page texts` (audit 72_: the base pages' net compares what a page drew, not only its frames): while armed,
// the kit records the pages' over-pass (hud2kit::Record, the layout lint's recorder; the pages draw in the skin's pass,
// never inside HUD 2's flight pass where the lint records, so the two never share it). Off by default: one bool per frame.
static bool g_textsArmed = false;
static std::vector<hud2kit::Drawn> g_textsRec, g_textsLast; // the pass being recorded, the last whole one
static u32 g_textsTick = 0;                                 // when the last whole pass ended (0: none since arming)
void DrawOver()
{
    if (g_textsArmed)
        g_textsRec.clear(), hud2kit::Record(&g_textsRec);
    pagelist::Legacy().Draw();
    if (g_textsArmed)
        hud2kit::Record(nullptr), g_textsLast.swap(g_textsRec), g_textsTick = GetTickCount();
}

// The last recorded pass's texts and keycaps, in drawn order: kind, box (px, rounded), em (px), the string. Alpha is left
// out (a fade's moment is not the page's content); a text drawn fully transparent is not recorded by the kit at all.
static std::string Texts(std::istream& in)
{
    std::string v;
    in >> v;
    if (v == "off")
    {
        g_textsArmed = false, g_textsTick = 0;
        g_textsRec.clear(), g_textsLast.clear();
        return "page texts off";
    }
    if (!g_textsArmed)
    {
        g_textsArmed = true, g_textsTick = 0;
        return "page texts: recording armed (every pages' pass records its texts until `page texts off`); run `page texts` again for them";
    }
    if (!g_textsTick)
        return "page texts: n/a: no pages' pass drawn since arming";
    std::ostringstream o;
    int n = 0;
    for (const hud2kit::Drawn& d : g_textsLast)
        n += d.kind == 'T' || d.kind == 'K';
    o << "page texts n=" << n << " age_ms=" << (GetTickCount() - g_textsTick);
    for (const hud2kit::Drawn& d : g_textsLast)
        if (d.kind == 'T' || d.kind == 'K')
            o << "\n" << d.kind << " " << static_cast<int>(d.x0 + 0.5f) << "," << static_cast<int>(d.y0 + 0.5f) << "-" << static_cast<int>(d.x1 + 0.5f)
              << "," << static_cast<int>(d.y1 + 0.5f) << " em=" << static_cast<int>(d.size + 0.5f) << " '" << text::Utf8(d.text) << "'";
    return o.str();
}
void DrawAfter() { pagelist::Legacy().After(); }

bool Parked() { return g_parked.root != 0; }
const page::PageState* State(const char* id)
{
    const int i = IndexOf(id);
    return i >= 0 && g_slots[i].built ? &g_slots[i].state : nullptr;
}
const void* Model(const char* id)
{
    const int i = IndexOf(id);
    return i >= 0 && g_slots[i].built ? g_slots[i].model : nullptr;
}

// ---- the verb
struct Fields : page::FieldVisitor
{
    std::ostringstream o;
    void Field(const char* name, double value) override { o << " " << name << "=" << value; }
    void Text(const char* name, const wchar_t* value) override { o << " " << name << "=\"" << text::Utf8(value ? value : L"") << "\""; }
};

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    const page::PageDesc* d = Desc(g_open);
    if (what == "press")
    {
        std::string b;
        in >> b;
        const struct
        {
            const char* n;
            unsigned m;
        } kMap[] = {{"a", XINPUT_GAMEPAD_A}, {"b", XINPUT_GAMEPAD_B}, {"x", XINPUT_GAMEPAD_X}, {"y", XINPUT_GAMEPAD_Y}, {"up", XINPUT_GAMEPAD_DPAD_UP}, {"down", XINPUT_GAMEPAD_DPAD_DOWN}, {"left", XINPUT_GAMEPAD_DPAD_LEFT}, {"right", XINPUT_GAMEPAD_DPAD_RIGHT}};
        for (const auto& k : kMap)
            if (b == k.n)
                return "page: took 0x" + std::to_string(Pad(k.m, 0, 0));
        return "page press <a|b|x|y|up|down|left|right>";
    }
    if (what == "texts")
        return Texts(in);
    if (what == "model")
    {
        if (!d || !d->model.Visit)
            return "page: none";
        Fields f;
        d->model.Visit(g_slots[g_open].model, f);
        return "page model " + std::string(d->core.id) + f.o.str();
    }
    if (what == "game")
    {
        if (!d || !d->model.Game)
            return "page: none";
        std::wostringstream w;
        d->model.Game(g_root, w);
        return "page game " + std::string(d->core.id) + "\n" + text::Utf8(w.str());
    }
    if (!what.empty() && what != "state")
        return "page [state] | model | game | texts [off] | press <a|b|x|y|up|down|left|right>";
    std::ostringstream o;
    o << "page id=" << (d ? d->core.id : "-") << " root=0x" << std::hex << g_root << std::dec << " parked=" << Parked();
    if (d)
    {
        const Slot& s = g_slots[g_open];
        o << " first=" << s.state.first << " room=" << s.state.room << " scrolls=" << s.state.scrolls << " wrap=" << static_cast<int>(s.state.wrapW)
          << " reads=" << s.reads << " stamp=0x" << std::hex << s.stamp << std::dec << " windowFields=" << d->model.windowFields;
        for (int a = 0; a < d->core.nActions; ++a)
        {
            const screens::ActionDesc& x = d->core.actions[a];
            const int may = x.May ? x.May(g_slots[g_open].model, 0) : 0;
            const int says = x.WindowSays && g_root ? x.WindowSays(g_root, 0) : 0;
            const screens::RunEnv env = {nnwin::Multiplayer(), g_root, nullptr};
            o << " act." << x.name << "=may:" << may << ",window:" << says << ",route:" << static_cast<int>(screens::RouteFor(x, env));
        }
    }
    o << " pending=" << g_runner.Action() << " shadows=" << g_runner.Shadows() << " registered=" << pagelist::All().Count()
      << " refused=" << pagelist::All().Refused();
    return o.str();
}
} // namespace pagerun
