// FLvibed - the INVENTORY page's description (audit 70b F4 unit b2; nnpage.cpp's INVENTORY as it was): it stands over the
// game's NN_Inventory window, which stays on the screen (the item's card, the game's own, shows through the page's hole; its
// five meshes stand in the page's rows); its model is the ship's items in the window's filter (core\game\screens\
// inventory_screen.*). The jettison panel is a state of this page, not a dialog: its view draws the panel while the game's
// stands. Its input is its own (page.h PageInput): the D-pad walks the items (up / down) and the filters (left / right), A
// mounts or unmounts (docked), Y jettisons (a single item asks twice), X opens BEST PRICES on a commodity, RS's click tags
// the item to compare with; the wheel scrolls the list, or the jettison's count while its panel stands; a click takes a row.
// The game's selection follows the focus (inventory_screen.cpp SyncNow); its buttons go through the actions' table.
#include "inventory_page.h"

#include "../pagelist.h"
#include "../pagerun.h"
#include "game/screens/inventory_screen.h"
#include "ui/glyphs.h"

namespace inventorypage
{
using namespace inventoryscreen; // (the actions' ids)
using inventoryscreen::Model;

static void Init(void* m) { inventoryscreen::Open(*new (m) Model()); }
static void Done(void* m) { static_cast<Model*>(m)->~Model(); }
static bool Read(void* m, u32 root) // (its rows are read in its frame, on their own clock: Tick)
{
    static_cast<Model*>(m)->root = root;
    return false;
}
static void Visit(const void* m, page::FieldVisitor& v)
{
    const Model& s = *static_cast<const Model*>(m);
    v.Field("items", static_cast<double>(s.k->items.size()));
    v.Field("sel", s.k->list.sel);
    v.Field("first", s.k->list.first);
    v.Field("filter", s.filter);
    v.Field("syncPending", s.k->syncPending);
    v.Field("jettison", s.jettison);
    v.Field("guns", s.k->load.guns);
}

static int Prompts(const void* model, views::PagePrompt* out, int max)
{
    using namespace glyphs;
    const Model& m = *static_cast<const Model*>(model);
    const Kept& k = *m.k;
    int n = 0;
    auto add = [&](int id, const wchar_t* word, int id2 = -1)
    {
        if (n < max)
            out[n++] = {id, id2, word, false};
    };
    add(DL, L"FILTER", DR);
    if (k.list.count > 1)
        add(DU, L"ITEM", DD);
    if (CanMount(m.root))
        add(A, k.items[k.list.sel].mounted ? L"UNMOUNT" : L"MOUNT");
    if (JettisonShown())
        add(Y, k.jettisonArmed ? L"AGAIN TO JETTISON" : L"JETTISON");
    if (TradeOffered(m.root))
        add(X, L"BEST PRICES");
    add(RS, L"SCROLL CARD");
    if (CanTag()) // (last: a full lane drops it, the card says it too)
        add(R3, Tagged() ? L"CLICK UNTAG" : L"CLICK COMPARE WITH");
    return n;
}

static unsigned Pad(void* model, unsigned pressed, unsigned, float)
{
    Model& m = *static_cast<Model*>(model);
    Kept& k = *m.k;
    const int filter = FilterOf(m.root);
    if (pressed & ~static_cast<unsigned>(XINPUT_GAMEPAD_Y))
        k.jettisonArmed = 0;
    // (the game's selection follows in the frame when due: inventory_screen.cpp SyncDue)
    if ((pressed & XINPUT_GAMEPAD_DPAD_UP) && k.list.Step(-1))
        k.stepAge = 0, k.syncPending = true;
    if ((pressed & XINPUT_GAMEPAD_DPAD_DOWN) && k.list.Step(1))
        k.stepAge = 0, k.syncPending = true;
    if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)
        pagerun::Act(kActFilter, (filter + 4) % 5);
    if (pressed & XINPUT_GAMEPAD_DPAD_RIGHT)
        pagerun::Act(kActFilter, (filter + 1) % 5);
    if (pressed & XINPUT_GAMEPAD_A)
        pagerun::Act(kActMount, 0);
    if (pressed & XINPUT_GAMEPAD_Y)
        pagerun::Act(kActJettison, 0);
    if (pressed & XINPUT_GAMEPAD_X)
        pagerun::Act(kActRoutes, 0);
    if (pressed & XINPUT_GAMEPAD_RIGHT_THUMB)
        ToggleTag();
    return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y |
           XINPUT_GAMEPAD_RIGHT_THUMB;
}

// The page's frame (the runtime's, once a frame after the read): its reads in the old order, then the wheel - the jettison's
// count while its panel stands, else the list.
static void Tick(void* model, u32 root, int notches, float dt, bool pad)
{
    Model& m = *static_cast<Model*>(model);
    m.pad = pad;
    inventoryscreen::Update(m, root, dt);
    if (notches && JettisonOpen())
        pagerun::Act(kActSlide, notches);
    else if (notches)
        m.k->list.Scroll(-notches);
}

// A click on a row takes the focus (a second click on it is its A: the game's selection follows at once); the tag's chip is
// RS's click; the jettison's -, + and MAX move its count.
static void Hit(void* model, int action, int arg, bool)
{
    Kept& k = *static_cast<Model*>(model)->k;
    switch (action)
    {
    case kHitRow:
        k.list.sel = arg, k.list.Fit();
        k.jettisonArmed = 0;
        pagerun::Act(kActSelect, 0);
        break;
    case kHitTag: ToggleTag(); break;
    case kHitJetLess: pagerun::Act(kActSlide, -1); break;
    case kHitJetMore: pagerun::Act(kActSlide, 1); break;
    case kHitJetMax: pagerun::Act(kActSlide, 100000); break;
    default: break;
    }
}

static void Prepare(void* model, u32 root) { inventoryscreen::Prepare(*static_cast<Model*>(model), root); }

static const page::PageInput kInput = {Prompts, Pad, nullptr, Hit, Prepare, Tick};

const page::PageDesc kInventory = {
    {kId, "neuronet", page::Kind::Page, {inventoryscreen::kWindowClass, inventoryscreen::kWindowName, nullptr, nullptr}, page::Park::Hosted,
     page::Pause::World, nullptr, 0, nullptr, 0, inventoryscreen::kActions, inventoryscreen::kActionCount, nullptr, nullptr, 0, &kInput},
    {sizeof(Model), Init, Read, nullptr, Visit, nullptr, 0, Done},
    View,
};
PAGE_REGISTER(kInventory);
} // namespace inventorypage
