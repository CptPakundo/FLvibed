// The game's windows' text by role - see windowtext.h, core\game\roletext.h and docs\FREELANCER_RE.md "The game's text size is its
// font's". One table row per window says which texts play which role. A text control takes another font the way the game's own init
// does (0x58A110): the text object's +0x1C, each laid out line's vt[8], the layout's +0x28 cleared. Rich text reads one table for the
// whole game, so the window's size goes in while the window is open and comes out when it closes; open text is laid out again by the
// game once its two dirty bytes are set. Fails closed: a font or class not as expected leaves that text as the game drew it.
#include "windowtext.h"

#include "flexe.h"
#include "game/roletext.h"
#include "gametext.h"
#include "hook.h"
#include "log.h"
#include "game/room.h"
#include "../fl.h"
#include "safemem.h"
#include "ui.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace windowtext
{
namespace
{
using roletext::Rung;

// Text control (exe::VtTitleText)
constexpr u32 kObj = 0x334, kWrapW = 0x32C, kState = 0x6C, kPos = 0xA8;
// its text object
constexpr u32 kLines = 0x10, kLinesEnd = 0x14, kFont = 0x1C, kExtW = 0x20, kExtH = 0x24, kLaidOut = 0x28, kWrapPx = 0x2C;
// RichTextWin (exe::VtText)
constexpr u32 kSizeW = 0x330, kSizeH = 0x334, kLeft = 0x348, kTop = 0x34C, kBoxW = 0x358, kBoxH = 0x35C, kTextH = 0x380, kDirty = 0x400;
constexpr u32 kBar = 0x3F0, kLayoutPos = 0x47C, kNodes = 0x48C, kCount = 0x490;
constexpr u32 kBarCopy = 0x3DC; // the scroll bar's own copy of its pose (0x595F90)

// The roles' tops, fractions of the screen's height. Numbers climb most, labels less; what no table names is a heading or a button
// and stays.
constexpr float kPriceTop = 0.045f, kNameTop = 0.0385f, kTransferNameTop = 0.04f;
constexpr float kTraderPriceTop = 0.0435f; // the commodity trader's rows carry the FAIR / GOOD word over the price (uistrip)
constexpr float kDealNumberTop = 0.0435f, kDealLabelTop = 0.0385f, kHelpTop = 0.035f;
// Largest first. 115 is the top: at 125 a long heading wraps across the plate's rule, which no height shows ("SM-a Rampart Heavy
// Fighter Molecular Shield", frame z1_gear), and one top keeps most items at one size.
constexpr int kRichSteps[] = {115, 110, 105, 100};
constexpr int kRichScrolled = 115; // text that scrolls at any size has no bar to keep away: the size most text ends at, not a jump
constexpr float kBoxGap = 0.02f;   // fractions of H between the description and the stats
constexpr float kBarRoom = 0.03f;  // the game's scroll bar stands right of a box: the wide box ends this short of the plate's edge
constexpr int kPlaceRest = 4;      // frames a window stands still before its boxes change

// Where a text's grown height goes: 0 down (its top stays), 0.5 both ways, 1 up (its foot stays)
struct TextRole
{
    const char* name; // a printf form when count > 0
    int first, count;
    float top, anchor;
};
struct RowRole
{
    const char *name, *second, *price; // printf forms when count > 0
    int first, count;
    float topName, topPrice, pitch;
};
struct Spec
{
    const char* window;
    const char* partner; // a second window of the same screen
    const TextRole* texts;
    int textCount;
    const RowRole* rows;
    int rowCount;
    const char* boxes[4]; // rich text; the first two of a dealer: description, stats; a window's one text alone otherwise
    const char *facts, *stats, *values, *wide;
    const char* anchor;      // the boxes' targets are from this control's centre, fractions of H (as uiskin's plates are)
    float boxTop, boxBottom; // 0 = the game's
    float plateRight;        // for `wide`
    float boxHeight;         // the boxes' height, fractions of H, when no anchor says it (0 = the game's)
    const char* wordless;    // the control that says the rows have no word over the price
    float drop;              // the window stands this much lower, fractions of H (the owner: move the map, not the subtitles)
};

const TextRole kDealerTexts[] = {
    {"TransferPriceText", 0, 0, kDealNumberTop, 1},
    {"QuantityTotalText", 0, 0, kDealNumberTop, 1},
    {"QuantityEdit", 0, 0, kDealNumberTop, 1},
    {"QuantityPrompt", 0, 0, kDealLabelTop, 1},
    {"QuantityText", 0, 0, kDealLabelTop, 1},
    {"QuantityPrice", 0, 0, kDealLabelTop, 1},
    {"QuantityTotal", 0, 0, kDealLabelTop, 1},
    {"ErrorHelp", 0, 0, kHelpTop, 0},
    {"WelcomeHelp", 0, 0, kHelpTop, 0},
    {"CargoLabel", 0, 0, kHelpTop, 0},
};
const RowRole kDealerRows[] = {
    {"ItemName%02d", "ItemQuantity%02d", "ItemPrice%02d", 1, 5, kNameTop, kPriceTop, 0.068f},
    {"TransferItemName", "TransferItemQuantity", "TransferItemPrice", 0, 0, kTransferNameTop, kPriceTop, 0.068f},
};
const TextRole kShipTexts[] = {{"PlayerCash", 0, 0, kHelpTop, 0.5f}};
const RowRole kShipRows[] = {
    {"ShipTraderShipName%d", "ShipTraderShipClass%d", "ShipTraderShipPrice%d", 0, 3, kNameTop, kPriceTop, 0.065f},
    {"ShipTraderCurrentShipName", "ShipTraderCurrentShipClass", "ShipTraderCurrentShipPrice", 0, 0, kNameTop, kPriceTop, 0.065f},
};
// (a job row's and the chosen job's Dest and Title lines stand 18 px apart at 800 lines: no room for more, they stay)
const TextRole kJobTexts[] = {
    {"MissionVendorSummaryReward%d", 0, 5, 0.04f, 0.5f},
    {"MissionVendorMissionReward", 0, 0, 0.04f, 0}, // (uistrip's type word goes under it by TextHeight)
    {"MissionVendorMissionReject", 0, 0, kHelpTop, 0},
};
const TextRole kNewsTexts[] = {
    {"NewsHeadlineHeadline%d", 0, 5, 0.035f, 0.5f},
    {"NewsStoryHeadline", 0, 0, 0.035f, 0.5f},
};
const TextRole kInventoryTexts[] = {{"CargoLabel", 0, 0, kHelpTop, 0}};
const RowRole kInventoryRows[] = {{"ItemName%02d", "ItemQuantity%02d", nullptr, 1, 5, kNameTop, 0, 0.068f}};
const TextRole kPlayerTexts[] = {{"PlayerInfoStat%d", 0, 12, 0.0385f, 0.5f}};
const TextRole kSaveTexts[] = {
    {"LoadSave_InfoLocationData", 0, 0, kHelpTop, 0},
    {"LoadSave_InfoCreditsData", 0, 0, kHelpTop, 0},
    {"LoadSave_InfoRankData", 0, 0, kHelpTop, 0},
    {"LoadSave_InfoShipData", 0, 0, kHelpTop, 0},
};

// The dealer first: its partner NN_Inventory is also a window of its own in the NeuroNet
const Spec kSpecs[] = {
    // the boxes: the plate's 0.480 .. 0.779 (uiskin's "dealer" family) less a little room at its head; 7 stats at 115 % need 234 px at
    // 800 lines, the game's 214 would take a bar
    {"NN_Dealer", "NN_Inventory", kDealerTexts, 10, kDealerRows, 2,
     {"InfoCard_Facts", "InfoCard_Stats", "InfoCard_Values", "InfoCard_Info"},
     "InfoCard_Facts", "InfoCard_Stats", "InfoCard_Values", "InfoCard_Info",
     "Inventory_Close", 0.484f, 0.7795f, -0.060f, 0, "Filter_Weapons"},
    {"NN_ShipTrader", nullptr, kShipTexts, 1, kShipRows, 2,
     {"ShipInfoCard_Facts", "ShipInfoCard_Stats", "ShipInfoCard_Values", nullptr},
     "ShipInfoCard_Facts", "ShipInfoCard_Stats", "ShipInfoCard_Values", nullptr,
     "ShipTraderCancelButton", 0.486f, 0.7795f, 0, 0, nullptr},
    {"NN_MissionVendor", nullptr, kJobTexts, 3, nullptr, 0, {"MissionVendorMissionFullText", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr},
    {"NN_News", nullptr, kNewsTexts, 2, nullptr, 0, {"NewsStoryText", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr},
    {"NN_Inventory", nullptr, kInventoryTexts, 1, kInventoryRows, 1, {"InfoCard_Info", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0.40f, nullptr},
    {"NeuroNetStoryStar", nullptr, nullptr, 0, nullptr, 0, {"StoryStarHistoryText", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr},
    {"NN_Help", nullptr, nullptr, 0, nullptr, 0, {"PlayerInfoText", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0.475f, nullptr},
    {"ScriptSequence_OfferDialog", nullptr, nullptr, 0, nullptr, 0, {"OfferDialogOfferText", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr},
    {"NN_PlayerInfo", nullptr, kPlayerTexts, 1, nullptr, 0, {nullptr, nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr},
    {"NN_LoadSave", nullptr, kSaveTexts, 4, nullptr, 0, {nullptr, nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr},
    // the nav map: its toolbar lay under the top panel of subtitles (UI_HUD2.md 5.34) and there is room under the map
    {"NeuroNetNavMap", nullptr, nullptr, 0, nullptr, 0, {nullptr, nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr, 0.05f},
};
constexpr int kSpecCount = static_cast<int>(sizeof(kSpecs) / sizeof(kSpecs[0]));

u32 R32(u32 a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}
template <class T> bool W(u32 a, T v)
{
    __try
    {
        *reinterpret_cast<volatile T*>(a) = v;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
bool Shown(u32 ctl)
{
    u8 st = 0;
    return safemem::Read(ctl + kState, &st, 1) && (st & 3) == 3;
}
bool IsText(u32 ctl) { return ctl && (exe::VtTitleText.is(R32(ctl)) || exe::VtTextEdit.is(R32(ctl))); }
bool IsRich(u32 ctl) { return ctl && exe::VtText.is(R32(ctl)); }

// A text as it stands. Valid only when laid out since its last change (a hidden text never is).
struct Now
{
    bool shown = false, valid = false;
    float width = 0, height = 0;
    int lines = 0;
    u32 font = 0;
};
Now Read(u32 ctl)
{
    Now n;
    if (!IsText(ctl) || !Shown(ctl))
        return n;
    n.shown = true;
    const u32 obj = R32(ctl + kObj);
    const u32 a = R32(obj + kLines), b = R32(obj + kLinesEnd);
    n.lines = a && b >= a ? static_cast<int>((b - a) >> 3) : 0;
    n.font = R32(obj + kFont);
    n.valid = R32(obj + kLaidOut) != 0;
    if (n.lines > 0)
        n.width = static_cast<float>(R32(obj + kExtW)), n.height = static_cast<float>(R32(obj + kExtH));
    return n;
}

using LineSetFont = void(__thiscall*)(u32 self, u32 font);
bool CallLine(u32 line, u32 font)
{
    __try
    {
        reinterpret_cast<LineSetFont>(Mem<u32>(Mem<u32>(line) + 8))(line, font);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
// The game's own change of a text's font
bool SetFont(u32 ctl, u32 font)
{
    if (!IsText(ctl) || !font)
        return false;
    const u32 obj = R32(ctl + kObj);
    if (!obj || R32(obj + kFont) == font)
        return obj != 0;
    if (!W<u32>(obj + kFont, font))
        return false;
    const u32 a = R32(obj + kLines), b = R32(obj + kLinesEnd);
    for (u32 p = a; a && p < b && p < a + 8 * 64; p += 8)
        if (const u32 line = R32(p))
            CallLine(line, font);
    return W<u32>(obj + kLaidOut, 0);
}
using SetProp = u32(__thiscall*)(u32 self, u32 id, const float* value, u32 extra);
bool SetPos(u32 ctl, const float* pos)
{
    __try
    {
        reinterpret_cast<SetProp>(Mem<u32>(Mem<u32>(ctl) + 0xA8))(ctl, 6, pos, 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// What a control held before this module touched it
struct Base
{
    u32 font = 0;
    float height = 0;
    float pos[3] = {};
    u32 applied = 0; // the font last set
};

struct RowCtl
{
    u32 name = 0, second = 0, price = 0;
};
struct Column
{
    std::map<int, RowCtl> rows;
    float topName = 0, topPrice = 0, pitch = 0;
    roletext::Pick pick{-1, -1}, base{-1, -1}, cap{-1, -1};
    bool started = false;
};
struct Single
{
    u32 ctl = 0;
    float top = 0, anchor = 0;
    int rung = -1; // down only while the window is open
};
enum class Kind
{
    Name,
    Second,
    Price,
    Text,
};
struct Slot
{
    Kind kind = Kind::Text;
    int column = 0, row = 0;
    const TextRole* role = nullptr;
};

const Spec* g_spec = nullptr;
u32 g_window = 0;
int g_scan = 0;
std::vector<Rung> g_ladder;
std::map<u32, float> g_heights; // font -> height, every font of the ladder
std::map<u32, Base> g_base;
std::vector<Column> g_columns;
std::vector<Single> g_singles;
std::vector<u32> g_boxes;
u32 g_facts = 0, g_stats = 0, g_values = 0, g_wide = 0, g_anchor = 0;
int g_rich = 100;
u32 g_richKey = 0;
u32 g_placeKey = 0; // where the boxes stand: the window is still sliding in while it changes
int g_placeRest = 0;
int g_richFits = 0;
double g_richMs = 0;

float HeightOf(u32 font)
{
    const auto it = g_heights.find(font);
    return it == g_heights.end() ? 0 : it->second;
}

void Remember(u32 ctl)
{
    if (!IsText(ctl))
        return;
    Base& b = g_base[ctl];
    b.font = R32(R32(ctl + kObj) + kFont);
    b.height = HeightOf(b.font);
    safemem::Read(ctl + kPos, b.pos, sizeof(b.pos));
}

// The new font, the text's grown height going where its role says: a font grows down from the text's top.
void Apply(u32 ctl, const Rung& r, float anchor)
{
    const auto it = g_base.find(ctl);
    if (it == g_base.end() || !it->second.font)
        return;
    Base& b = it->second;
    const u32 font = r.height <= b.height + 0.0001f ? b.font : r.font;
    if (b.applied == font || (!b.applied && font == b.font))
        return;
    if (!SetFont(ctl, font))
        return;
    b.applied = font;
    float pos[3] = {b.pos[0], b.pos[1], b.pos[2]};
    if (font != b.font)
        pos[1] += (r.height - b.height) * roletext::kInk * anchor;
    SetPos(ctl, pos);
}

void PutBack(u32 ctl, Base& b)
{
    if (!b.applied || b.applied == b.font)
        return;
    if (SetFont(ctl, b.font))
        SetPos(ctl, b.pos);
    b.applied = 0;
}

// 1 the window's, 2 its partner's, 0 neither's
int Side(u32 ctl, const Spec& s)
{
    for (int guard = 0; ctl && guard < 12; ++guard)
    {
        const char* n = ui::ControlName(ctl);
        if (strcmp(n, s.window) == 0)
            return 1;
        if (s.partner && strcmp(n, s.partner) == 0)
            return 2;
        ctl = R32(ctl + 4);
    }
    return 0;
}

std::string Named(const char* form, int i)
{
    char buf[48];
    sprintf_s(buf, form, i);
    return buf;
}

void Slots(const Spec& s, std::map<std::string, Slot>& out)
{
    for (int c = 0; c < s.rowCount; ++c)
    {
        const RowRole& r = s.rows[c];
        for (int i = 0; i < (r.count ? r.count : 1); ++i)
        {
            const int n = r.first + i;
            const auto put = [&](const char* form, Kind k)
            {
                if (!form)
                    return;
                Slot slot;
                slot.kind = k, slot.column = c, slot.row = i;
                out[r.count ? Named(form, n) : std::string(form)] = slot;
            };
            put(r.name, Kind::Name);
            put(r.second, Kind::Second);
            put(r.price, Kind::Price);
        }
    }
    for (int t = 0; t < s.textCount; ++t)
    {
        const TextRole& r = s.texts[t];
        for (int i = 0; i < (r.count ? r.count : 1); ++i)
        {
            Slot slot;
            slot.role = &r;
            out[r.count ? Named(r.name, r.first + i) : std::string(r.name)] = slot;
        }
    }
}

void Collect(const Spec& s)
{
    g_columns.assign(static_cast<size_t>(s.rowCount), Column());
    g_singles.clear();
    g_boxes.clear();
    g_facts = g_stats = g_values = g_wide = g_anchor = 0;
    for (int c = 0; c < s.rowCount; ++c)
    {
        Column& col = g_columns[static_cast<size_t>(c)];
        col.topName = s.rows[c].topName, col.topPrice = s.rows[c].topPrice, col.pitch = s.rows[c].pitch;
        if (s.wordless && c == 0 && !uiclick::FindVisible(s.wordless))
            col.topPrice = kTraderPriceTop;
    }
    std::map<std::string, Slot> slots;
    Slots(s, slots);

    const u32 head = Mem<u32>(exe::UiControls);
    u32 node = head ? R32(head + 4) : 0;
    for (int guard = 0; node && node != head && guard < 5000; ++guard, node = R32(node + 4))
    {
        const u32 ctl = R32(node + 8);
        if (!ctl)
            continue;
        char n[40] = {}; // (ui::ControlName keeps one buffer: Side asks it again)
        strncpy_s(n, ui::ControlName(ctl), 32);
        const auto slot = slots.find(n);
        bool box = false;
        for (const char* b : s.boxes)
            box = box || (b && strcmp(b, n) == 0);
        const bool anchor = s.anchor && strcmp(s.anchor, n) == 0;
        if (slot == slots.end() && !box && !anchor)
            continue;
        const int side = Side(ctl, s);
        if (!side)
            continue;
        if (anchor && !g_anchor)
            g_anchor = ctl;
        if (box)
        {
            g_boxes.push_back(ctl);
            g_facts = s.facts && strcmp(s.facts, n) == 0 ? ctl : g_facts;
            g_stats = s.stats && strcmp(s.stats, n) == 0 ? ctl : g_stats;
            g_values = s.values && strcmp(s.values, n) == 0 ? ctl : g_values;
            g_wide = s.wide && strcmp(s.wide, n) == 0 ? ctl : g_wide;
        }
        if (slot == slots.end())
            continue;
        const Slot& at = slot->second;
        if (at.role)
        {
            g_singles.push_back({ctl, at.role->top, at.role->anchor, -1});
            continue;
        }
        RowCtl& r = g_columns[static_cast<size_t>(at.column)].rows[side * 100 + at.row];
        (at.kind == Kind::Name ? r.name : at.kind == Kind::Second ? r.second : r.price) = ctl;
    }
    for (const Column& c : g_columns)
        for (const auto& kv : c.rows)
            for (u32 ctl : {kv.second.name, kv.second.second, kv.second.price})
                Remember(ctl);
    for (const Single& t : g_singles)
        Remember(t.ctl);
}

// One column: measured at its base fonts, one size for all its rows; down at once when a text needs it, up only at the next opening
void Rows(Column& c, float screenH)
{
    roletext::Room room;
    std::vector<roletext::Row> rows;
    for (const auto& kv : c.rows)
    {
        const RowCtl& r = kv.second;
        const Now name = Read(r.name), second = Read(r.second), price = Read(r.price);
        if (!name.shown)
            continue;
        if (!name.valid || (price.shown && !price.valid))
            continue; // not laid out: a row with nothing in it, or a font set this frame
        const float nameBase = g_base[r.name].height, nameNow = HeightOf(name.font);
        const float priceBase = r.price ? g_base[r.price].height : nameBase, priceNow = price.shown ? HeightOf(price.font) : priceBase;
        if (nameBase <= 0 || priceBase <= 0 || nameNow <= 0 || priceNow <= 0)
            return;
        room.nameBase = nameBase, room.priceBase = priceBase;
        rows.push_back({name.width * nameBase / nameNow, second.valid ? second.width : 0, price.width * priceBase / priceNow});
        if (room.width <= 0)
        {
            // from the name's left to the price's right, by their boxes (pixels through the name's own wrap: its width in both units)
            const float wrapPx = static_cast<float>(R32(R32(r.name + kObj) + kWrapPx));
            float ww = 0, pw = 0, npos[3] = {}, ppos[3] = {};
            safemem::Read(r.name + kWrapW, &ww, 4);
            safemem::Read(r.name + kPos, npos, sizeof(npos));
            room.width = wrapPx;
            if (r.price)
            {
                safemem::Read(r.price + kWrapW, &pw, 4);
                safemem::Read(r.price + kPos, ppos, sizeof(ppos));
                const float k = ww > 0 ? wrapPx / ww : 0;
                room.width = ((ppos[0] + pw / 2) - (npos[0] - ww / 2)) * k;
            }
        }
    }
    if (rows.empty() || room.width <= 0)
        return;
    room.height = c.pitch * screenH;
    room.gap = 0.01f * screenH;
    room.screenH = screenH;
    if (!c.started)
    {
        c.base = {roletext::RungAt(g_ladder, room.nameBase), roletext::RungAt(g_ladder, room.priceBase)};
        c.cap = {roletext::RungAt(g_ladder, c.topName), c.topPrice > 0 ? roletext::RungAt(g_ladder, c.topPrice) : roletext::RungAt(g_ladder, room.priceBase)};
        if (c.base.name < 0 || c.base.price < 0 || c.cap.name < c.base.name || c.cap.price < c.base.price)
            return;
        c.started = true;
    }
    const roletext::Pick p = roletext::PickRows(rows, room, g_ladder, c.base, c.cap);
    if (p.name == c.pick.name && p.price == c.pick.price)
        return;
    c.pick = p, c.cap = p;
    for (const auto& kv : c.rows)
    {
        Apply(kv.second.name, g_ladder[static_cast<size_t>(p.name)], 0);
        Apply(kv.second.price, g_ladder[static_cast<size_t>(p.price)], 1);
    }
}

// A text alone: its role's top while it stays inside its wrap width, or it wraps by design (the refusal, the welcome text)
void Singles()
{
    for (Single& s : g_singles)
    {
        const auto base = g_base.find(s.ctl);
        if (base == g_base.end() || !base->second.font)
            continue;
        if (s.rung < 0)
            s.rung = roletext::RungAt(g_ladder, s.top);
        if (s.rung < 0)
            continue;
        const Now n = Read(s.ctl);
        const u32 obj = R32(s.ctl + kObj);
        u8 wraps = 0;
        safemem::Read(obj + 0x30, &wraps, 1);
        if (n.shown && n.valid && n.lines > 0 && !wraps)
        {
            const float now = HeightOf(n.font), wrap = static_cast<float>(R32(obj + kWrapPx));
            while (now > 0 && wrap > 0 && s.rung > 0 && g_ladder[static_cast<size_t>(s.rung)].height > base->second.height + 0.0001f &&
                   n.width / now * g_ladder[static_cast<size_t>(s.rung)].height * roletext::kWidthMargin > wrap)
                --s.rung;
        }
        Apply(s.ctl, g_ladder[static_cast<size_t>(s.rung)], s.anchor);
    }
}

// The boxes take the plate's room: the full height, the description the width up to the stats. By absolute targets, so a window that
// comes back as it was asks for nothing. True when a box changed: its pixels are new only after the next draw.
bool PlaceBoxes(const Spec& s, float screenH)
{
    if (!exe::RichSetSize.ok())
        return false;
    ui::Item anchor;
    const bool anchored = s.anchor && g_anchor && ui::ControlRect(g_anchor, anchor);
    if (s.anchor && !anchored)
        return false;
    // The game's setter places the box's scroll bar from the window's pose of the moment: during the slide-in it stays there, out of
    // its plate (frame w1_nn_inventory). So nothing moves until the window has stood still.
    u32 place = 0;
    for (u32 box : g_boxes)
        place = place * 31 + R32(box + kLeft) * 7 + R32(box + kTop);
    g_placeRest = place == g_placeKey ? g_placeRest + 1 : 0;
    g_placeKey = place;
    if (g_placeRest < kPlaceRest)
        return false;
    const float top = anchored ? anchor.y + s.boxTop * screenH : 0, bottom = anchored ? anchor.y + s.boxBottom * screenH : 0;
    bool changed = false;
    for (u32 box : g_boxes)
    {
        if (!IsRich(box) || !Shown(box))
            continue;
        float w = 0, h = 0, pos[3] = {};
        safemem::Read(box + kSizeW, &w, 4);
        safemem::Read(box + kSizeH, &h, 4);
        safemem::Read(box + kLayoutPos, pos, sizeof(pos));
        const int px = static_cast<int>(R32(box + kBoxW));
        if (w <= 0 || h <= 0 || px <= 0)
            continue; // not drawn yet
        const float k = static_cast<float>(px) / w;
        if (anchored)
        {
            const float up = static_cast<float>(static_cast<int>(R32(box + kTop))) - top;
            if (fabsf(up) >= 1.5f)
            {
                pos[1] += up / k;
                SetPos(box, pos);
                changed = true;
            }
        }
        float wantW = w, wantH = h;
        if (box == g_facts && IsRich(g_stats) && Shown(g_stats))
            wantW = (static_cast<float>(static_cast<int>(R32(g_stats + kLeft)) - static_cast<int>(R32(box + kLeft))) - kBoxGap * screenH) / k;
        if (box == g_wide && anchored)
            wantW = (anchor.x + (s.plateRight - kBarRoom) * screenH - static_cast<float>(static_cast<int>(R32(box + kLeft)))) / k;
        if (anchored)
            wantH = (bottom - top) / k;
        else if (s.boxHeight > 0)
            wantH = s.boxHeight * screenH / k;
        if (fabsf(wantW - w) > 0.002f || fabsf(wantH - h) > 0.002f)
        {
            // The setter sets the scroll bar's pose from the box's width, and a bar that holds that width already lands a width too
            // far right (the NeuroNet's: 290 px, frame w2_nn_inventory). Taller only: the bar goes back to where it stood.
            const u32 bar = R32(box + kBar);
            float barPos[3] = {}, barCopy[12] = {};
            const bool keepBar = bar && fabsf(wantW - w) <= 0.002f && exe::ControlSetPosition.ok() && safemem::Read(bar + 0x60, barPos, sizeof(barPos)) &&
                                 safemem::Read(bar + kBarCopy, barCopy, sizeof(barCopy));
            hk::Call<void(__thiscall*)(u32, float, float)>(exe::RichSetSize, box, wantW, wantH);
            if (keepBar)
            {
                hk::Call<void(__thiscall*)(u32, const float*)>(exe::ControlSetPosition, bar, barPos);
                for (int i = 0; i < 12; ++i)
                    W<float>(bar + kBarCopy + static_cast<u32>(i) * 4, barCopy[i]);
            }
            changed = true;
        }
    }
    return changed;
}

using RelayFn = u32(__thiscall*)(u32 self);
bool Relay(u32 box)
{
    __try
    {
        hk::Call<RelayFn>(exe::RichLayout, box);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// What the boxes hold now: a new filling of any of them changes it
u32 RichKey()
{
    u32 key = 0;
    for (u32 box : g_boxes)
        if (IsRich(box) && Shown(box))
        {
            const u32 list = R32(box + kNodes);
            key = key * 31 + R32(box + kCount) + (list ? R32(list) : 0);
        }
    return key;
}

bool FarOver(const std::vector<u32>& boxes)
{
    for (u32 box : boxes)
        if (R32(box + kTextH) > R32(box + kBoxH) * 3 / 2)
            return true;
    return false;
}

void FitRich()
{
    const u32 key = RichKey();
    if (!key || key == g_richKey || !exe::RichLayout.ok())
        return;
    for (u32 box : g_boxes)
        if (IsRich(box) && Shown(box) && !R32(box + kBoxW))
            return; // not drawn yet: it has no size to fit
    LARGE_INTEGER t0, t1, f;
    QueryPerformanceCounter(&t0);
    std::vector<u32> boxes;
    for (u32 box : g_boxes)
        if (IsRich(box) && Shown(box))
            boxes.push_back(box);
    // A try costs a rebuild of the rich fonts (~4 ms): from the last size, up while it fits, else down
    const bool pair = IsRich(g_stats) && IsRich(g_values) && Shown(g_stats) && Shown(g_values);
    const auto fits = [&](int step)
    {
        gametext::RichSet(step);
        ++g_richFits;
        std::vector<roletext::Box> fit;
        bool ok = true;
        for (u32 box : boxes)
        {
            ok = ok && Relay(box);
            fit.push_back({static_cast<int>(R32(box + kTextH)), static_cast<int>(R32(box + kBoxH))});
        }
        return ok && roletext::RichFits(fit, pair ? static_cast<int>(R32(g_stats + kTextH)) : 0, pair ? static_cast<int>(R32(g_values + kTextH)) : 0);
    };
    constexpr int kSteps = static_cast<int>(sizeof(kRichSteps) / sizeof(kRichSteps[0]));
    int at = 0;
    while (at < kSteps - 1 && kRichSteps[at] > g_rich)
        ++at;
    g_richFits = 0;
    int chosen = 0;
    if (fits(kRichSteps[at]))
    {
        chosen = kRichSteps[at];
        for (int i = at - 1; i >= 0 && fits(kRichSteps[i]); --i)
            chosen = kRichSteps[i];
    }
    else if (!FarOver(boxes)) // (text far over its box scrolls at every size: no tries)
    {
        for (int i = at + 1; i < kSteps && !chosen; ++i)
            if (fits(kRichSteps[i]))
                chosen = kRichSteps[i];
    }
    if (!chosen)
        chosen = kRichScrolled;
    gametext::RichSet(chosen);
    g_rich = chosen;
    for (u32 box : boxes)
    {
        Relay(box);
        W<u8>(box + kDirty, 1);
        W<u8>(box + kDirty + 1, 1);
    }
    g_richKey = RichKey();
    QueryPerformanceCounter(&t1);
    QueryPerformanceFrequency(&f);
    g_richMs = static_cast<double>(t1.QuadPart - t0.QuadPart) * 1000.0 / static_cast<double>(f.QuadPart);
}

// A window that stands lower than the game puts it: once it has stopped sliding, through its own position setter. The native height is
// kept across openings, so a window that comes back where it was left is not moved twice.
float g_dropNative = 0;
bool g_dropKnown = false;
u32 g_dropKey = 0;
int g_dropRest = 0, g_dropTries = 0;
void Drop(const Spec& s)
{
    float pos[3] = {};
    if (!safemem::Read(g_window + 0x60, pos, sizeof(pos)) || g_dropTries >= 3 || !exe::NavMapSetPosition.is(R32(R32(g_window) + 0x14)))
        return;
    u32 key = 0;
    memcpy(&key, &pos[1], 4);
    u32 kx = 0;
    memcpy(&kx, &pos[0], 4);
    key = key * 31 + kx;
    g_dropRest = key == g_dropKey ? g_dropRest + 1 : 0;
    g_dropKey = key;
    if (g_dropRest < kPlaceRest)
        return;
    if (!g_dropKnown)
        g_dropNative = pos[1], g_dropKnown = true;
    if (fabsf(pos[1] - g_dropNative) > 0.002f)
        return; // not where the game puts it: done, or a place of the game's own
    pos[1] = g_dropNative - s.drop;
    ++g_dropTries;
    hk::Call<void(__thiscall*)(u32, const float*)>(exe::NavMapSetPosition, g_window, pos);
}

void Closed()
{
    g_dropRest = g_dropTries = 0;
    if (g_rich != 100)
        gametext::RichSet(100);
    g_rich = 100;
    g_richKey = 0;
    g_spec = nullptr; // (the window's controls are gone: the game builds new ones at each opening, often at the same addresses)
    g_base.clear();
    g_boxes.clear();
}

void BuildLadder()
{
    g_heights.clear();
    std::vector<Rung> agency;
    for (const gametext::Font& f : gametext::Fonts())
    {
        g_heights[f.entry] = f.height;
        if (f.face == "Agency FB")
            agency.push_back({f.height, f.entry});
    }
    g_ladder = roletext::Ladder(agency);
}

// One walk of the game's control list, a few times a second while no window is open
const Spec* Find(u32& window)
{
    int best = kSpecCount;
    window = 0;
    const u32 head = Mem<u32>(exe::UiControls);
    u32 node = head ? R32(head + 4) : 0;
    for (int guard = 0; node && node != head && guard < 5000; ++guard, node = R32(node + 4))
    {
        const u32 ctl = R32(node + 8);
        u8 st = 0;
        if (!ctl || !safemem::Read(ctl + kState, &st, 1) || !(st & 1))
            continue;
        const char* n = ui::ControlName(ctl);
        if (n[0] != 'N' && n[0] != 'S')
            continue;
        for (int i = 0; i < best; ++i)
            if (strcmp(n, kSpecs[i].window) == 0)
            {
                best = i, window = ctl;
                break;
            }
    }
    return best < kSpecCount ? &kSpecs[best] : nullptr;
}
} // namespace

void Frame(bool on)
{
    if (!on)
    {
        for (auto& kv : g_base)
            if (IsText(kv.first))
                PutBack(kv.first, kv.second);
        if (g_spec)
            Closed();
        return;
    }
    if (g_spec)
    {
        u32 window = 0;
        const Spec* now = Find(window);
        if (now != g_spec || window != g_window)
            Closed();
    }
    if (!g_spec)
    {
        if (++g_scan < 12)
            return;
        g_scan = 0;
        u32 window = 0;
        const Spec* spec = Find(window);
        if (!spec)
            return;
        if (g_ladder.empty())
            BuildLadder();
        g_spec = spec, g_window = window;
        g_base.clear();
        Collect(*spec);
    }
    if (g_ladder.empty())
        return;
    const float screenH = static_cast<float>(fl::ScreenHeight());
    if (g_spec->drop > 0)
        Drop(*g_spec);
    for (Column& c : g_columns)
        Rows(c, screenH);
    Singles();
    if (PlaceBoxes(*g_spec, screenH))
        g_richKey = 0; // fitted after the next draw has the new pixels
    else
        FitRich();
}

float TextHeight(u32 ctl)
{
    const Now n = Read(ctl);
    return n.lines > 0 ? n.height : 0;
}

std::string Texts()
{
    if (g_ladder.empty())
        BuildLadder();
    std::map<u32, std::string> nick;
    for (const gametext::Font& f : gametext::Fonts())
        nick[f.entry] = f.nickname;
    std::ostringstream o;
    const u32 head = Mem<u32>(exe::UiControls);
    u32 node = head ? R32(head + 4) : 0;
    for (int guard = 0; node && node != head && guard < 5000; ++guard, node = R32(node + 4))
    {
        const u32 ctl = R32(node + 8);
        if (!ctl || !Shown(ctl) || !(IsText(ctl) || IsRich(ctl)))
            continue;
        ui::Item it;
        const bool rect = ui::ControlRect(ctl, it);
        const u32 parent = R32(ctl + 4);
        std::string up = parent ? ui::ControlName(parent) : "";
        o << ui::ControlName(ctl) << " < " << up << " x=" << (rect ? it.x : -1) << " y=" << (rect ? it.y : -1);
        if (IsRich(ctl))
            o << " rich " << R32(ctl + kBoxW) << " x " << R32(ctl + kBoxH) << " text " << R32(ctl + kTextH) << " bar " << (R32(ctl + 0x3F0) ? 1 : 0) << "\n";
        else
        {
            const u32 obj = R32(ctl + kObj);
            const Now n = Read(ctl);
            o << " font " << nick[n.font] << " ext " << n.width << " x " << n.height << " lines " << n.lines << " wrap " << R32(obj + kWrapPx) << "\n";
        }
    }
    return o.str();
}

std::string Status()
{
    std::ostringstream o;
    o << "windowtext window=" << (g_spec ? g_spec->window : "none") << " ladder=" << g_ladder.size() << " fonts=" << g_heights.size() << " columns";
    for (const Column& c : g_columns)
        o << " " << c.pick.name << "/" << c.pick.price;
    o << " singles=" << g_singles.size() << " rich=" << g_rich << " tries=" << g_richFits << " ms=" << g_richMs;
    for (u32 box : g_boxes)
        if (IsRich(box))
        {
            float w = 0;
            safemem::Read(box + kSizeW, &w, 4);
            o << "\n  " << ui::ControlName(box) << " shown=" << Shown(box) << " w=" << w << " box " << R32(box + kBoxW) << " x " << R32(box + kBoxH)
              << " text " << R32(box + kTextH) << " bar " << (R32(box + 0x3F0) ? 1 : 0);
        }
    return o.str();
}
} // namespace windowtext
