// The game's windows' text by role - see windowtext.h, core\game\roletext.h and docs\FREELANCER_RE.md "The game's text size is its
// font's". One table row per window says which texts play which role. A text control takes another font the way the game's own init
// does (0x58A110): the text object's +0x1C, each laid out line's vt[8], the layout's +0x28 cleared. Rich text reads one table for the
// whole game, so the window's size goes in while the window is open and comes out when it closes; open text is laid out again by the
// game once its two dirty bytes are set. Fails closed: a font or class not as expected leaves that text as the game drew it.
#include "windowtext.h"
#include "basepage.h"

#include "flexe.h"
#include "game/deckmode.h"
#include "game/roletext.h"
#include "gametext.h"
#include "skin_rules.h"
#include "hook.h"
#include "log.h"
#include "game/room.h"
#include "game/fl.h"
#include "safemem.h"
#include "ui/ui.h"
#include "ui/winplates.h"
#include "ui/hud2_kit.h"

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
// A name stays at its base: its length is what the list holds, which is not known until it scrolls into view, so a name's top would change the column's font under the
// player's eyes, and at 800 lines the next rung is also visibly bolder (GitHub issue 22). Numbers are short: they climb.
constexpr float kPriceTop = 0.045f, kNameTop = 0;
constexpr float kPriceFit = 0.039f; // what a row's price stands at, under the game's own size: bigger, it crossed the row's line and crowded the word over it (the owner, 6 Nov 2026)
constexpr float kTraderPriceTop = 0.0435f; // the commodity trader's rows carry the FAIR / GOOD word over the price (uistrip)
constexpr float kDealNumberTop = 0.0435f, kDealLabelTop = 0.0385f, kHelpTop = 0.035f;
// One size for every item of an opening: a size per item read as text changing at random (GitHub issue 22). At 125 a long heading wraps across the plate's rule, which no
// height shows ("SM-a Rampart Heavy Fighter Molecular Shield", frame z1_gear). A description too long for it scrolls in its box.
constexpr int kRichSize = 115;
constexpr float kRichLine = 0.03625f; // a rich line's pitch at that size, fractions of H (29 px at 800 lines)
constexpr float kBoxGap = 0.02f;   // fractions of H between the description and the stats
constexpr float kBarRoom = 0.03f;  // the game's scroll bar stands right of a box: the wide box ends this short of the plate's edge
constexpr int kPlaceRest = 4;      // frames a window stands still before its boxes change
constexpr float kStatsW = 0.335f;  // the stats' labels, fractions of H: the longest ("Shield Damage Per Shot:") on one line, for a wrapped label moved every value after it a row off its own
constexpr float kRowFloor = 0.024f; // a row's text gives way this far, fractions of H, sooner than it runs into its neighbour or out of its row (the owner, 6 Nov 2026)

// Where a text's grown height goes: 0 down (its top stays), 0.5 both ways, 1 up (its foot stays)
struct TextRole
{
    const char* name; // a printf form when count > 0
    int first, count;
    float top, anchor;
    bool under = false; // the top holds under the game's size too: two lines the game stands closer than its font
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
    float drop;              // the window stands this much lower, fractions of H (move the map, not the subtitles)
    int frame;               // R8: 1 the window stands on the one frame, 2 only with g_inventoryFrame
    float boxFoot = 0;       // the box grows to its text up to here, fractions of H, in whole lines: a box cut through a line showed half of it
    float ruleTop = 0;       // the box starts this far under the window's top, fractions of H: under the title's rule, which ran through its first line
    float footOff = 0;       // ... and ends this far off the window's foot, in whole lines
    float grow = 0;          // the window stands this much taller than the game's, fractions of H: there is the screen for it (the owner, 6 Nov 2026)
    float shift = 0;         // the window stands this far right of the screen's middle, fractions of H: a NeuroNet page has its list or card at the left (nnpage.*)
    const char* rightEdge = nullptr; // ... or this control of it ends at the page's right edge, on any aspect (shift until it shows): at 16:10 a shift in H left the list beside it 200 px
    bool fit = false;                // the window's foot and its OK button follow its box, which grows to its text (boxFoot): FitFoot
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
    {"TransferItemName", "TransferItemQuantity", "TransferItemPrice", 0, 0, kNameTop, kPriceTop, 0.068f},
};
const TextRole kShipTexts[] = {{"PlayerCash", 0, 0, kHelpTop, 0.5f}};
const RowRole kShipRows[] = {
    {"ShipTraderShipName%d", "ShipTraderShipClass%d", "ShipTraderShipPrice%d", 0, 3, kNameTop, kPriceTop, 0.065f},
    {"ShipTraderCurrentShipName", "ShipTraderCurrentShipClass", "ShipTraderCurrentShipPrice", 0, 0, kNameTop, kPriceTop, 0.065f},
};
// (a job row's and the chosen job's Dest and Title lines stand 0.0255 H apart, and the game's font ran one into the other)
constexpr float kJobLineTop = 0.035f; // (PlaceFramed spreads a row's two lines over its frame, which makes this room)
const TextRole kJobTexts[] = {
    {"MissionVendorSummaryReward%d", 0, 5, 0.04f, 0.5f},
    {"MissionVendorSummaryDest%d", 0, 5, kJobLineTop, 0, true},
    {"MissionVendorSummaryTitle%d", 0, 5, kJobLineTop, 0, true},
    {"MissionVendorMissionDest", 0, 0, kJobLineTop, 0, true},
    {"MissionVendorMissionTitle", 0, 0, kJobLineTop, 0, true},
    {"MissionVendorMissionReward", 0, 0, 0.05f, 0}, // (uistrip's type word goes under it by TextHeight)
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

constexpr float kTextWindowGrow = 0.15f; // the plate from under the room's tab to over the prompts' lane
// The box's top under the window's: a small gap under the rule, which stands higher with the title for more text (winplates' 0.056). It ends on a whole line
constexpr float kTextWindowTop = 0.056f;
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
     "ShipTraderCancelButton", 0.436f, 0.7295f, 0, 0, nullptr}, // (from the X where it stands now: StandShipHead)
    {"NN_MissionVendor", nullptr, kJobTexts, 7, nullptr, 0, {"MissionVendorMissionFullText", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr, 0, 0, 0.888f},
    {"NN_News", nullptr, kNewsTexts, 2, nullptr, 0, {"NewsStoryText", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr},
    {"NN_Inventory", nullptr, kInventoryTexts, 1, kInventoryRows, 1, {"InfoCard_Info", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0.62f, nullptr, 0, 2, 0, 0, 0, 0.40f, 0.31f, "InfoCard_Info"},
    {"NeuroNetStoryStar", nullptr, nullptr, 0, nullptr, 0, {"StoryStarHistoryText", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr, 0, 1, 0, kTextWindowTop, -0.089f, 0.36f, 0.26f, "StoryStarHistoryText"},
    {"NN_Help", nullptr, nullptr, 0, nullptr, 0, {"PlayerInfoText", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr, 0, 1, 0, kTextWindowTop, -0.015f, 0.36f, 0.26f, "PlayerInfoText"},
    {"ScriptSequence_OfferDialog", nullptr, nullptr, 0, nullptr, 0, {"OfferDialogOfferText", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr},
    {"NN_PlayerInfo", nullptr, kPlayerTexts, 1, nullptr, 0, {"FactionInfoCard", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0.62f, nullptr, 0, 1, 0, 0, 0, 0.46f, 0.38f, "FactionInfoCard"},
    {"NN_LoadSave", nullptr, kSaveTexts, 4, nullptr, 0, {nullptr, nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr},
    // the nav map: its toolbar lay under the top panel of subtitles (UI_HUD2.md 5.34) and there is room under the map
    {"NeuroNetNavMap", nullptr, nullptr, 0, nullptr, 0, {nullptr, nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr, 0.05f},
    // the story / tutorial popup (pub::Player::PopUpDialog, M01's boxes): at 1280x800's larger text its box took a scroll bar (the owner, 7 Nov 2026).
    // Last, so a NeuroNet window open under it keeps its own
    {"NeuroNetMissionMessage", nullptr, nullptr, 0, nullptr, 0, {"MissionMessageText", nullptr, nullptr, nullptr},
     nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, nullptr, 0, 0, 0.80f, 0, 0, 0, 0, nullptr, true},
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

// A row of a list: two lines in a frame the game draws. The game's offsets are for its own fonts: a larger price crossed the frame's foot (the owner, 6 Nov 2026). So each line is
// placed by the frame as it stands: the first under its top, the others on its foot. One rule for every list.
struct FramedRole
{
    const char *frame, *top, *low1, *low2; // printf forms when count > 0
    int first, count;
};
const FramedRole kFramed[] = {
    {"ItemView%02d", "ItemName%02d", "ItemQuantity%02d", "ItemPrice%02d", 1, 5},
    {"TransferItemView", "TransferItemName", "TransferItemQuantity", "TransferItemPrice", 0, 0},
    {"ShipTraderShipNameFrame%d", "ShipTraderShipName%d", "ShipTraderShipClass%d", "ShipTraderShipPrice%d", 0, 3},
    {"ShipTraderCurrentShipFrame", "ShipTraderCurrentShipName", "ShipTraderCurrentShipClass", "ShipTraderCurrentShipPrice", 0, 0},
    {"MissionVendorSummaryBox%d", "MissionVendorSummaryDest%d", "MissionVendorSummaryTitle%d", nullptr, 0, 5},
    {"PlayerInfoFactionFrame%d", "PlayerInfoFactionName%d", nullptr, nullptr, 0, 6},
};
struct Framed
{
    u32 frame = 0, top = 0, low1 = 0, low2 = 0;
};
constexpr float kFramePad = 0.003f; // between a line's ink and its frame, fractions of H
constexpr float kInkTop = 0.10f, kInkFoot = 0.90f; // where a font's ink starts and ends in its line

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
    int nameRungs = 0; // rungs over the base, one fixed size for the whole opening (R1, GitHub issue 22)
    int overflow = 0;  // names wider than the row at that size, read as they came into view: the game has no marquee, a probe counts them
    float secondBase = 0; // the second line's own height
    int secondRung = -1;  // its rung, -1 not read yet
};
struct Single
{
    u32 ctl = 0;
    float top = 0, anchor = 0;
    int rung = -1; // down only while the window is open
    bool under = false;
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
std::vector<Rung> g_ladder;
std::map<u32, float> g_heights; // font -> height, every font of the ladder
std::map<u32, Base> g_base;
std::vector<Column> g_columns;
std::vector<Single> g_singles;
std::vector<Single> g_loose; // every other text of the window: no role, but it stays in its box
int g_looseSteps = 0;
std::vector<Framed> g_framed;
u32 g_welcomeText = 0, g_welcomeHelp = 0, g_shipTitle = 0, g_shipClose = 0, g_viewName = 0, g_viewClass = 0;
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

// The dealer's quantity block, its button with it: it rises into the slider's room while the game hides the slider (the owner, 6 Nov 2026: no empty room), and goes back with it.
struct Lifted
{
    u32 ctl;
    float y;
};
std::vector<Lifted> g_lifted;
float g_liftNow = 0;
u32 g_jobReward = 0;
constexpr float kRewardLeft = 0.0375f; // the job's reward ends on ACCEPT JOB's right edge, as the type word under it: it stood past it (fractions of H)
constexpr float kStackNudge = 0.018f; // with the slider showing: the slack over the block, so the line at its foot stands clear of the plate's edge
const char* const kLiftNames[] = {"QuantityText", "QuantityEdit", "QuantityPrice", "TransferPriceText", "QuantityTotal", "QuantityTotalText", "PlusMinus", "ShipBuyProblem"};

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
void Apply(u32 ctl, const Rung& r, float anchor, bool under = false)
{
    const auto it = g_base.find(ctl);
    if (it == g_base.end() || !it->second.font)
        return;
    Base& b = it->second;
    const u32 font = fabsf(r.height - b.height) <= 0.0001f || (!under && r.height < b.height) ? b.font : r.font; // (`under`: a text that would run into its neighbour goes below the game's size)
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
    g_loose.clear();
    g_boxes.clear();
    g_facts = g_stats = g_values = g_wide = g_anchor = 0;
    for (int c = 0; c < s.rowCount; ++c)
    {
        Column& col = g_columns[static_cast<size_t>(c)];
        col.topName = s.rows[c].topName, col.topPrice = s.rows[c].topPrice, col.pitch = s.rows[c].pitch;
        // (a size the player raised already grew the names: the rung is for the game's own size)
        col.nameRungs = gametext::Applied() == 100 ? skinrules::NameRungs(static_cast<float>(fl::ScreenHeight()), col.pitch) : 0;
        if (s.wordless && c == 0 && !uiclick::FindVisible(s.wordless))
            col.topPrice = kTraderPriceTop;
    }
    std::map<std::string, Slot> slots;
    Slots(s, slots);
    std::map<std::string, u32> named[3]; // by side

    const u32 head = Mem<u32>(exe::UiControls);
    u32 node = head ? R32(head + 4) : 0;
    for (int guard = 0; node && node != head && guard < 5000; ++guard, node = R32(node + 4))
    {
        const u32 ctl = R32(node + 8);
        if (!ctl)
            continue;
        char n[40] = {}; // (ui::ControlName keeps one buffer: Side asks it again)
        strncpy_s(n, ui::ControlName(ctl), 32);
        if (const int sd = Side(ctl, s))
            named[sd][n] = ctl;
        if (strcmp(n, "MissionVendorMissionReward") == 0)
            g_jobReward = ctl;
        if (strcmp(s.window, "NN_Dealer") == 0)
            for (const char* l : kLiftNames)
                if (strcmp(l, n) == 0)
                {
                    float pos[3] = {};
                    if (safemem::Read(ctl + kPos, pos, sizeof(pos)))
                        g_lifted.push_back({ctl, pos[1]});
                }
        const auto slot = slots.find(n);
        bool box = false;
        for (const char* b : s.boxes)
            box = box || (b && strcmp(b, n) == 0);
        const bool anchor = s.anchor && strcmp(s.anchor, n) == 0;
        if (slot == slots.end() && !box && !anchor)
        {
            if (IsText(ctl) && Side(ctl, s) && !winplates::IsTitle(n))
                g_loose.push_back({ctl, 0, 0, -1});
            continue;
        }
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
            g_singles.push_back({ctl, at.role->top, at.role->anchor, -1, at.role->under});
            continue;
        }
        RowCtl& r = g_columns[static_cast<size_t>(at.column)].rows[side * 100 + at.row];
        (at.kind == Kind::Name ? r.name : at.kind == Kind::Second ? r.second : r.price) = ctl;
    }
    g_framed.clear();
    const auto welcome = [&](const char* name) -> u32
    {
        const auto it = named[1].find(name);
        return it == named[1].end() ? 0 : it->second;
    };
    g_welcomeText = welcome("WelcomeText"), g_welcomeHelp = welcome("WelcomeHelp");
    g_shipTitle = welcome("ShipTraderTitle"), g_shipClose = welcome("ShipTraderCancelButton"), g_viewName = welcome("ShipTraderViewName"), g_viewClass = welcome("ShipTraderViewClass");
    for (const auto& side : named)
        for (const FramedRole& r : kFramed)
            for (int i = 0; i < (r.count ? r.count : 1); ++i)
            {
                const auto at = [&](const char* form) -> u32
                {
                    if (!form)
                        return 0;
                    const auto it = side.find(r.count ? Named(form, r.first + i) : std::string(form));
                    return it == side.end() ? 0 : it->second;
                };
                Framed f;
                f.frame = at(r.frame), f.top = at(r.top), f.low1 = at(r.low1), f.low2 = at(r.low2);
                if (f.frame && f.top)
                    g_framed.push_back(f);
            }
    for (const Column& c : g_columns)
        for (const auto& kv : c.rows)
            for (u32 ctl : {kv.second.name, kv.second.second, kv.second.price})
                Remember(ctl);
    for (const Single& t : g_singles)
        Remember(t.ctl);
    for (const Single& t : g_loose)
        Remember(t.ctl);
    const auto reward = g_base.find(g_jobReward);
    if (reward != g_base.end())
    {
        reward->second.pos[0] -= kRewardLeft;
        SetPos(g_jobReward, reward->second.pos);
    }
}

// A row name the game set (a TextControl) shortened through its own setter (flexe TextCtlSetProperty, message 0x1D: a written buffer does not redraw - the
// load screen's lesson, 7 Nov 2026). Kept: the trailing "(...)", the class a buyer reads; cut: the middle, marked "...". The game sets a row's text anew when
// the list scrolls: that text is cut again. A cut is re-judged only 3 frames later, once the game has laid the new text out.
bool SetRowText(u32 ctl, const wchar_t* text)
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
struct Cut
{
    std::wstring set, from; // what was set, and the game's text it stands for: the game sets its rows' texts anew every frame
    int wait = 0;
    float room = 0, size = 0; // the marquee's width and text height, pixels
};
std::map<u32, Cut> g_cuts;
int g_cutCount = 0, g_cutCalls = 0;
void CutName(u32 ctl, float width, float room, float size)
{
    std::wstring now;
    ++g_cutCalls;
    if (!winplates::TextOf(ctl, now) || now.size() < 6 || !(width > 0))
        return;
    Cut& k = g_cuts[ctl];
    if ((now == k.set || now == k.from) && k.wait > 0)
    {
        --k.wait;
        return;
    }
    if (now != k.set)
        k.from = now;
    // the owner, 7 Nov 2026: "Instead of clipping, I hope it marquees": the game's text is blanked (its layout kept) and HUD 2 draws the whole name over
    // the row as its marquee (DrawMarquees: hud2kit::TextMarquee - the cut form held, a smooth scroll to the end, held, back)
    if (now == L" ")
        return;
    k.from = now, k.set = L" ", k.room = room, k.size = size;
    k.wait = 3;
    if (SetRowText(ctl, k.set.c_str()))
        ++g_cutCount;
}

} // namespace

// Before the UI pass draws (hud_module UnderBegin): every cut row whose text the game set back to the full name gets its cut again, so the cut is what draws
void ApplyCuts()
{
    for (auto it = g_cuts.begin(); it != g_cuts.end();)
    {
        std::wstring now;
        if (!winplates::TextOf(it->first, now))
        {
            it = g_cuts.erase(it);
            continue;
        }
        if (!it->second.set.empty() && now == it->second.from)
            SetRowText(it->first, it->second.set.c_str());
        ++it;
    }
}

// In HUD 2's draw pass (uiskin): each blanked row name drawn whole as a marquee where the game's text stands (its anchor: the line's top left), in the
// caption's own colour (TextControl +0x348, flexe TextCtlSetProperty's note), at the line's height
void DrawMarquees()
{
    for (const auto& kv : g_cuts)
    {
        const Cut& k = kv.second;
        std::wstring now;
        ui::Item it;
        if (k.set != L" " || k.from.empty() || !(k.size > 0) || !Shown(kv.first) || !winplates::TextOf(kv.first, now) || now != L" " ||
            !ui::ControlRect(kv.first, it))
            continue;
        u8 c[4] = {};
        safemem::Read(kv.first + 0x348, c, 4); // (bytes R, G, B, A: 87 C3 E0 a row's blue, E2 F0 FC the selected row's)
        const u32 argb = (c[0] | c[1] | c[2]) ? (0xFF000000u | (u32(c[0]) << 16) | (u32(c[1]) << 8) | c[2]) : hud2kit::Argb(hud2kit::kChrome, 1);
        // the line's middle: under its row frame's top as Place puts the game's names (kFramePad, the ink from kInkTop of the line), else the anchor
        float y = it.y + k.size * 0.5f;
        for (const Framed& f : g_framed)
        {
            ui::Item fr;
            if (f.top == kv.first && ui::ControlRect(f.frame, fr) && fr.hh > 0.5f)
                y = fr.y - fr.hh + kFramePad * static_cast<float>(fl::ScreenHeight()) + k.size * (0.5f - kInkTop);
        }
        // (the kit's size is its glyphs', the game's height its line's: the ink's share of it, kInkTop..kInkFoot)
        hud2kit::TextMarquee(it.x, y, k.from, k.size * (kInkFoot - kInkTop), k.room, argb, hud2kit::Left, 0, false);
    }
}

namespace
{
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
        const float secondNow = second.valid ? HeightOf(second.font) : 0;
        if (second.valid && c.secondBase <= 0)
            c.secondBase = g_base[r.second].height;
        rows.push_back({name.width * nameBase / nameNow, second.valid && secondNow > 0 ? second.width * c.secondBase / secondNow : 0, price.width * priceBase / priceNow});
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
    // (the frame says the room, where there is one: the two controls' boxes said more than it holds, and a name ran out of it)
    for (const auto& kv : c.rows)
        for (const Framed& f : g_framed)
            if (f.top == kv.second.name)
            {
                ui::Item fr;
                if (Shown(f.frame) && ui::ControlRect(f.frame, fr) && fr.hw > 0.5f)
                    room.width = std::min(room.width, fr.hw * 2 - 0.02f * screenH);
            }
    room.height = c.pitch * screenH;
    room.gap = 0.01f * screenH;
    room.screenH = screenH;
    if (!c.started)
    {
        c.base = {roletext::RungAt(g_ladder, room.nameBase), roletext::RungAt(g_ladder, room.priceBase)};
        c.cap = {c.topName > 0 ? roletext::RungAt(g_ladder, c.topName) : roletext::RungAt(g_ladder, room.nameBase), c.topPrice > 0 ? roletext::RungAt(g_ladder, c.topPrice) : roletext::RungAt(g_ladder, room.priceBase)};
        c.cap.name = std::max(c.cap.name, c.base.name), c.cap.price = std::max(c.cap.price, c.base.price); // (a top under the game's size: the commodity rows' sat just under it, and these rows went unmanaged)
        if (c.base.name < 0 || c.base.price < 0)
            return;
        c.started = true;
    }
    roletext::Pick p = roletext::PickRows(rows, room, g_ladder, c.base, c.cap);
    p.price = std::min(p.price, std::max(0, roletext::RungAt(g_ladder, kPriceFit)));
    if (c.nameRungs != 0)
    {
        // the name's rung stands whatever scrolls into view; the price gives way to it, for the row's height is what cannot grow
        p.name = std::clamp(c.base.name + c.nameRungs, 0, static_cast<int>(g_ladder.size()) - 1);
        while (p.price > c.base.price && (g_ladder[static_cast<size_t>(p.name)].height + g_ladder[static_cast<size_t>(p.price)].height) * roletext::kInk > c.pitch)
            --p.price;
    }
    // Nothing in a row runs into its neighbour or out of its row. A name wider than its room takes the column down, and it stays down for this opening: one step when the long
    // name first shows, not a size per item (GitHub issue 22). So does the second line against the price, the price first
    const int floorRung = std::max(0, roletext::RungAt(g_ladder, kRowFloor));
    const float H1 = 1.0f / room.nameBase, nameRoom = room.width;
    const auto nameOver = [&](int rung)
    {
        int over = 0;
        for (const roletext::Row& r : rows)
            over += r.name * g_ladder[static_cast<size_t>(rung)].height * H1 * roletext::kWidthMargin > nameRoom ? 1 : 0;
        return over;
    };
    // (tried 7 Nov 2026 on the owner's word, "smaller is pointless with a marquee": without this step the game's own rows broke at full size - long names
    // over their prices, the transfer panel's too - so the column still steps down to the floor, and the marquee takes what is still too wide there)
    while (p.name > floorRung && nameOver(p.name) > 0)
        --p.name;
    c.nameRungs = p.name - c.base.name;
    c.overflow = nameOver(p.name);
    // at the floor and still wider than the row (Shockwave at 1280x800, 7 Nov 2026: "Adv. Rampart H. F. Shield (Class 2)" ran into the scroll bar): the
    // text-fit rule's last step, the name cut - its "(Class N)" kept (CutName)
    if (c.overflow > 0 && p.name <= floorRung)
        for (const auto& kv : c.rows)
        {
            const Now n = Read(kv.second.name);
            if (n.shown && n.valid && n.width * roletext::kWidthMargin > nameRoom) // (the margin the column's own test uses)
                CutName(kv.second.name, n.width * roletext::kWidthMargin, nameRoom, HeightOf(n.font) * screenH); // (a font's height: a share of H)
        }
    if (c.secondBase > 0)
    {
        if (c.secondRung < 0)
            c.secondRung = roletext::RungAt(g_ladder, c.secondBase);
        const auto collides = [&](int second, int price)
        {
            for (const roletext::Row& r : rows)
                if (r.price > 0 && r.second > 0 &&
                    (r.second * g_ladder[static_cast<size_t>(second)].height / c.secondBase + r.price * g_ladder[static_cast<size_t>(price)].height / room.priceBase) * roletext::kWidthMargin + room.gap > room.width)
                    return true;
            return false;
        };
        const int was = c.secondRung;
        while (c.secondRung >= 0 && p.price > c.base.price && collides(c.secondRung, p.price))
            --p.price;
        while (c.secondRung > floorRung && collides(c.secondRung, p.price))
            --c.secondRung;
        if (c.secondRung != was && c.secondRung >= 0)
            for (const auto& kv : c.rows)
                Apply(kv.second.second, g_ladder[static_cast<size_t>(c.secondRung)], 0, true);
    }
    if (p.name == c.pick.name && p.price == c.pick.price)
        return;
    c.pick = p, c.cap = p;
    for (const auto& kv : c.rows)
    {
        Apply(kv.second.name, g_ladder[static_cast<size_t>(p.name)], 0, true);
        Apply(kv.second.price, g_ladder[static_cast<size_t>(p.price)], 1, true);
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
            while (now > 0 && wrap > 0 && s.rung > 0 && (s.under || g_ladder[static_cast<size_t>(s.rung)].height > base->second.height + 0.0001f) &&
                   n.width / now * g_ladder[static_cast<size_t>(s.rung)].height * roletext::kWidthMargin > wrap)
                --s.rung;
        }
        Apply(s.ctl, g_ladder[static_cast<size_t>(s.rung)], s.anchor, s.under);
    }
}

void PlaceFramed(float screenH)
{
    enum Where
    {
        Top,
        Foot,
        Middle,
        Exact, // y0 is the line's top
    };
    const auto empty = [&](u32 ctl)
    {
        const Now n = Read(ctl);
        return !n.shown || !n.valid || n.lines < 1 || n.width < 0.012f * screenH; // (an empty row's name is one space)
    };
    const auto put = [&](u32 ctl, float y0, float y1, Where where)
    {
        const Now n = Read(ctl);
        ui::Item it;
        float pos[3] = {};
        if (!n.shown || !n.valid || n.lines < 1 || !ui::ControlRect(ctl, it) || !safemem::Read(ctl + kPos, pos, sizeof(pos)))
            return;
        const float line = n.height / static_cast<float>(n.lines);
        const float want = where == Exact ? y0 : where == Foot ? y1 - kFramePad * screenH - line * kInkFoot : where == Top ? y0 + kFramePad * screenH - line * kInkTop : (y0 + y1) / 2 - n.height / 2;
        if (fabsf(it.y - want) < 1.0f)
            return;
        const float dy = (it.y - want) / screenH;
        pos[1] += dy;
        SetPos(ctl, pos);
        const auto base = g_base.find(ctl);
        if (base != g_base.end())
            base->second.pos[1] += dy; // (a later font keeps the place)
    };
    for (const Framed& f : g_framed)
    {
        ui::Item fr;
        if (!Shown(f.frame) || !ui::ControlRect(f.frame, fr) || fr.hh <= 0.5f)
            continue;
        const float y0 = fr.y - fr.hh, y1 = fr.y + fr.hh;
        // a name at the smallest size that still wants the frame's width: it starts nearer the frame's left, sooner than past its right
        const Now name = Read(f.top);
        const auto nameBase = g_base.find(f.top);
        if (name.shown && name.valid && nameBase != g_base.end())
        {
            float pos[3] = {};
            const float margin = 0.0125f * screenH;
            const float over = std::clamp(name.width + margin + 2 - fr.hw * 2, 0.0f, margin - 2);
            if (safemem::Read(f.top + kPos, pos, sizeof(pos)) && fabsf(pos[0] - (nameBase->second.pos[0] - over / screenH)) * screenH >= 1.0f)
            {
                pos[0] = nameBase->second.pos[0] - over / screenH;
                SetPos(f.top, pos);
            }
        }
        const bool noTop = empty(f.top), noLow = (!f.low1 || empty(f.low1)) && (!f.low2 || empty(f.low2));
        put(f.top, y0, y1, noLow ? Middle : Top);
        put(f.low1, y0, y1, noTop ? Middle : Foot);
        put(f.low2, y0, y1, noTop ? Middle : Foot);
    }
    // the welcome text under its title as it stands: the title takes two lines or three by the base's name
    const Now title = Read(g_welcomeText);
    ui::Item t;
    if (title.shown && title.valid && title.lines > 0 && ui::ControlRect(g_welcomeText, t))
        put(g_welcomeHelp, t.y + title.height + 0.024f * screenH, 0, Exact);
}

// A text with no role that the game wrote wider than its box (a long name, another language): it runs over its neighbour or out of its panel, so it steps down, a few rungs at most, and
// stays down for the opening. It is the same whatever its alignment: a text no wider than its box cannot leave it. A text that wraps grows instead, and its panel with it (winfit.h).
void Loose()
{
    constexpr int kMostSteps = 3;
    const int floorRung = std::max(0, roletext::RungAt(g_ladder, kRowFloor));
    for (Single& s : g_loose)
    {
        const Now n = Read(s.ctl);
        if (!n.shown || !n.valid || n.lines != 1)
            continue;
        const u32 obj = R32(s.ctl + kObj);
        u8 wraps = 0;
        safemem::Read(obj + 0x30, &wraps, 1);
        const float now = HeightOf(n.font), wrap = static_cast<float>(R32(obj + kWrapPx));
        if (wraps || now <= 0 || wrap <= 0)
            continue;
        const int first = roletext::RungAt(g_ladder, g_base[s.ctl].height);
        if (s.rung < 0)
            s.rung = first;
        const int was = s.rung;
        while (s.rung > floorRung && s.rung > first - kMostSteps && n.width / now * g_ladder[static_cast<size_t>(s.rung)].height * roletext::kWidthMargin > wrap)
            --s.rung;
        if (s.rung != was && s.rung >= 0)
        {
            ++g_looseSteps;
            Apply(s.ctl, g_ladder[static_cast<size_t>(s.rung)], 0, true);
        }
    }
}

// The boxes take the plate's room: the full height, the description the width up to the stats. By absolute targets, so a window that
// comes back as it was asks for nothing. True when a box changed: its pixels are new only after the next draw.
// A box's height in the setter's units for this many pixels: by what the height it has came to, for the width's scale is not the height's (a whole number of lines came out a part of one over).
// The pixels are new only after a draw: asked once, it waits for them, or it would correct a correction
static struct
{
    u32 box;
    float units, pixels;
} g_askedHeight = {};
static float ToPixels(u32 box, float units, float pixels, float want)
{
    if (g_askedHeight.box == box && fabsf(g_askedHeight.units - units) < 0.002f && g_askedHeight.pixels == pixels)
        return units;
    if (pixels <= 0 || fabsf(pixels - want) <= 1.0f)
        return units;
    const float next = units * want / pixels;
    g_askedHeight = {box, next, pixels};
    return next;
}

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
    // the trade words at the ship's final stage have no heading: they stand even in the plate, not hard under its top
    const bool bare = g_wide && !(IsRich(g_stats) && Shown(g_stats)) && uiclick::FindVisible("BackToShipDealer");
    // (the panel under the row takes the room the row leaves: its boxes go with it, beyond a pixel's noise)
    float shift = s.anchor ? winplates::FitBoxShift(strcmp(s.window, "NN_Dealer") == 0 ? winplates::kFamDealer : winplates::kFamShipDealer) : 0;
    shift = fabsf(shift) < 0.004f * screenH ? 0 : shift;
    ui::Item rootIt;
    const bool rooted = s.ruleTop > 0 && ui::ControlRect(g_window, rootIt) && rootIt.hh > 0.5f;
    const float top = anchored ? anchor.y + (s.boxTop + (bare ? 0.016f : 0.0f)) * screenH + shift : rooted ? rootIt.y - rootIt.hh + s.ruleTop * screenH : 0;
    const float bottom = anchored ? anchor.y + s.boxBottom * screenH : rooted ? rootIt.y + rootIt.hh + (s.footOff + s.grow) * screenH : 0;
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
        if (anchored || rooted)
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
        if (box == g_stats && IsRich(g_values) && Shown(g_values))
        {
            // the labels' width, from the values' left edge: the description gives up what they take
            const float left = static_cast<float>(static_cast<int>(R32(g_values + kLeft))) - kStatsW * screenH;
            const float dx = left - static_cast<float>(static_cast<int>(R32(box + kLeft)));
            if (fabsf(dx) >= 1.5f)
            {
                pos[0] += dx / k;
                SetPos(box, pos);
                changed = true;
            }
            wantW = kStatsW * screenH / k;
        }
        if (box == g_facts && IsRich(g_stats) && Shown(g_stats))
            wantW = (static_cast<float>(static_cast<int>(R32(g_stats + kLeft)) - static_cast<int>(R32(box + kLeft))) - (kBoxGap + kBarRoom) * screenH) / k; // (its scroll bar stands right of it: on the labels without the room)
        if (box == g_wide && anchored)
            wantW = (anchor.x + (s.plateRight - kBarRoom) * screenH - static_cast<float>(static_cast<int>(R32(box + kLeft)))) / k;
        if (anchored)
            wantH = (bottom - top) / k;
        else if (rooted)
        {
            const float line = kRichLine * screenH * static_cast<float>(g_rich) / kRichSize;
            wantH = ToPixels(box, h, static_cast<float>(R32(box + kBoxH)), floorf((bottom - top) / line) * line);
        }
        else if (s.boxHeight > 0)
            wantH = s.boxHeight * screenH / k;
        else if (s.boxFoot > 0)
        {
            const float line = kRichLine * screenH * static_cast<float>(g_rich) / kRichSize, text = static_cast<float>(R32(box + kTextH));
            const float lines = floorf((s.boxFoot * screenH - static_cast<float>(static_cast<int>(R32(box + kTop)))) / line);
            if (text > 0 && lines >= 2)
                wantH = ToPixels(box, h, static_cast<float>(R32(box + kBoxH)), std::min(text, lines * line) + 2);
        }
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
    // (a window with a stats table stays at kRichSize whatever the window text does: the table is sized for it, and at 125 % a label wrapped at 800 lines, which put every
    // number beside the wrong label and laid two bars over the values. tests\docked_text.ps1 LABELS)
    const int chosen = g_stats ? kRichSize : std::max(kRichSize, gametext::Applied());
    gametext::RichSet(chosen);
    g_richFits = 1;
    for (u32 box : boxes)
        Relay(box);
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

// R8: the window's top left on the one frame, once it has stopped sliding, through its own signed setter. A correction by measure, as the UI's units are not quite
// pixels: a few tries, then it stands where it stands. No hook on the slide yet (0x55E120 goes with the strip's work), so the window arrives at the game's place first.
bool g_inventoryFrame = true; // (the window keeps its rows' hit boxes as the status and log do: tests\\docked_text.ps1)
void Place(const Spec& s)
{
    const u32 set = R32(R32(g_window) + 0x14);
    if ((s.frame == 2 && !g_inventoryFrame) || g_dropTries >= 3 || !(exe::ControlSetPosition.is(set) || exe::InventorySetPosition.is(set)))
        return;
    float pos[3] = {};
    ui::Item it;
    if (!safemem::Read(g_window + 0x60, pos, sizeof(pos)) || !ui::ControlRect(g_window, it) || it.hw <= 0.5f)
        return;
    u32 key = 0, kx = 0;
    memcpy(&key, &pos[1], 4);
    memcpy(&kx, &pos[0], 4);
    key = key * 31 + kx;
    g_dropRest = key == g_dropKey ? g_dropRest + 1 : 0;
    g_dropKey = key;
    if (g_dropRest < kPlaceRest)
        return;
    const float H = static_cast<float>(fl::ScreenHeight()), W = static_cast<float>(fl::ScreenWidth());
    // centred between what stands around it (the owner, 6 Nov 2026): on the screen's middle, and between the strip's tabs and the prompts' row; a window that grows downwards stands higher by half of it
    float dx = (W / 2 + s.shift * H - it.x) / H;
    ui::Item edge;
    if (s.rightEdge && ui::ControlRect(uiclick::FindVisible(s.rightEdge), edge) && edge.hw > 0.5f)
        dx = ((1 - deckmode::SideW()) * W - (edge.x + edge.hw)) / H;
    const float dy = (it.y - H * (skinrules::kFrameCentreY - s.grow / 2)) / H;
    if (fabsf(dx) < 0.002f && fabsf(dy) < 0.002f)
        return;
    pos[0] += dx, pos[1] += dy;
    ++g_dropTries;
    g_dropRest = 0;
    hk::Call<void(__thiscall*)(u32, const float*)>(exe::ControlSetPosition.is(set) ? exe::ControlSetPosition : exe::InventorySetPosition, g_window, pos);
}

// A button to a height on the screen, through the position property as a text is. Checked: it must stay on its column. A button that leaves it goes back and is left alone for the
// opening: every plate of a window can hang on one (the ship dealer's X went 1000 px left through vt[0x14], whose position is not the local one, and took the panels with it)
bool g_moveRefused = false;
void MoveButton(u32 ctl, float y, float H)
{
    ui::Item it, after;
    float pos[3] = {};
    if (g_moveRefused || !ctl || !ui::ControlRect(ctl, it) || fabsf(it.y - y) < 1.0f || !safemem::Read(ctl + kPos, pos, sizeof(pos)))
        return;
    const float was = pos[1];
    pos[1] += (it.y - y) / H;
    if (!SetPos(ctl, pos))
        return;
    if (!ui::ControlRect(ctl, after) || fabsf(after.x - it.x) > 2.0f)
    {
        pos[1] = was;
        SetPos(ctl, pos);
        g_moveRefused = true;
        Log("windowtext: a button left its column when moved: left where the game has it");
    }
}

// The ship dealer's head: its side panels start at their headings, so the X goes on the headings' line and the ship's name under the panels' top. By where they stand, as StretchFoot
void StandShipHead(float H)
{
    constexpr float kTopAir = 0.018f, kViewAir = 0.012f;
    const Now title = Read(g_shipTitle), name = Read(g_viewName), kind = Read(g_viewClass);
    ui::Item t, v;
    if (!title.shown || !title.valid || title.lines < 1 || !ui::ControlRect(g_shipTitle, t))
        return;
    MoveButton(g_shipClose, t.y + title.height / 2, H);
    const float top = t.y - kTopAir * H + kViewAir * H;
    const auto stand = [&](u32 ctl, const Now& n, float want)
    {
        float p[3] = {};
        if (n.shown && n.valid && ui::ControlRect(ctl, v) && fabsf(v.y - want) >= 1.0f && safemem::Read(ctl + kPos, p, sizeof(p)))
        {
            p[1] += (v.y - want) / H;
            SetPos(ctl, p);
        }
    };
    stand(g_viewName, name, top);
    stand(g_viewClass, kind, top + name.height);
}

// The log's two buttons at the window's foot, taller as it is: by where they stand, so a window the game keeps across openings is never moved twice
void StretchFoot(const Spec& s, float H)
{
    constexpr float kButtonFoot = 0.0515f, kCaptionUp = 0.022f;
    static const char* const kPairs[][2] = {{"MissionButton", "MissionText"}, {"LogButton", "LogText"}};
    ui::Item root;
    if (s.grow <= 0 || !ui::ControlRect(g_window, root) || root.hh <= 0.5f)
        return;
    // the title at the window's top: the rule under it stands higher now
    const u32 title = winplates::Child(g_window, strcmp(s.window, "NN_Help") == 0 ? "PlayerInfoTitle" : "StoryStarTitle");
    ui::Item t;
    float tp[3] = {};
    const float titleTop = root.y - root.hh;
    if (title && ui::ControlRect(title, t) && fabsf(t.y - titleTop) >= 1.0f && safemem::Read(title + kPos, tp, sizeof(tp)))
    {
        tp[1] += (t.y - titleTop) / H;
        SetPos(title, tp);
    }
    if (strcmp(s.window, "NeuroNetStoryStar") != 0)
        return;
    const float want = root.y + root.hh + (s.grow - kButtonFoot) * H;
    for (const auto& pair : kPairs)
    {
        const u32 button = winplates::Child(g_window, pair[0]), text = winplates::Child(g_window, pair[1]);
        ui::Item it;
        float pos[3] = {};
        MoveButton(button, want, H);
        if (text && ui::ControlRect(text, it) && fabsf(it.y - (want - kCaptionUp * H)) >= 1.0f && safemem::Read(text + kPos, pos, sizeof(pos)))
        {
            pos[1] += (it.y - (want - kCaptionUp * H)) / H;
            SetPos(text, pos);
        }
    }
    // the death / mission-failed screen's buttons stand where the log's two would (the owner, 7 Nov 2026: they stood at the game's height, over the
    // taller window's text). The game adds them to the log for that screen only, each with its caption, a text of its own named after it
    // (LoadAutosaveButton / LoadAutosaveText, read 7 Nov 2026; the multiplayer respawn's caption by the same rule, not read): the caption keeps its place on the button
    static const char* const kDeath[][2] = {{"LoadAutosaveButton", "LoadAutosaveText"}, {"MainMenuButton", "MainMenuText"},
                                            {"LoadSavegameButton", "LoadSavegameText"}, {"RespawnButtonButton", "RespawnButtonText"}};
    for (const auto& pair : kDeath)
    {
        const u32 button = uiclick::FindVisible(pair[0]), text = uiclick::FindVisible(pair[1]);
        ui::Item b, it;
        float pos[3] = {};
        if (!button || !ui::ControlRect(button, b))
            continue;
        const float on = text && ui::ControlRect(text, it) ? it.y - b.y : 0; // the caption's place on its button, before the move
        MoveButton(button, want, H);
        if (text && ui::ControlRect(text, it) && fabsf(it.y - (want + on)) >= 1.0f && safemem::Read(text + kPos, pos, sizeof(pos)))
        {
            pos[1] += (it.y - (want + on)) / H;
            SetPos(text, pos);
        }
    }
}

// A `fit` window (the story popup): its box grows or shrinks to its text (PlaceBoxes, boxFoot); the plate's foot, the OK button and its caption go as much.
// From what stood at the opening, so a box the game keeps between two popups is measured once
float g_fitBox0 = -1, g_fitButton = 0, g_fitCaption = 0;
float FitFoot(float H)
{
    const u32 box = g_boxes.empty() ? 0 : g_boxes.front();
    const u32 button = uiclick::FindVisible("MMOkButton"), caption = uiclick::FindVisible("MMOkText");
    ui::Item b, c;
    if (!box || !IsRich(box) || !Shown(box) || !button || !ui::ControlRect(button, b))
        return 0;
    const float now = static_cast<float>(R32(box + kBoxH));
    if (g_fitBox0 < 0)
    {
        if (now <= 0)
            return 0;
        g_fitBox0 = now, g_fitButton = b.y, g_fitCaption = caption && ui::ControlRect(caption, c) ? c.y : 0;
    }
    const float grow = now - g_fitBox0;
    MoveButton(button, g_fitButton + grow, H);
    float pos[3] = {};
    if (g_fitCaption > 0 && ui::ControlRect(caption, c) && fabsf(c.y - (g_fitCaption + grow)) >= 1.0f && safemem::Read(caption + kPos, pos, sizeof(pos)))
    {
        pos[1] += (c.y - (g_fitCaption + grow)) / H;
        SetPos(caption, pos);
    }
    return grow / H;
}

void Lift(float want)
{
    if (want == g_liftNow)
        return;
    g_liftNow = want;
    for (const Lifted& l : g_lifted)
    {
        float pos[3] = {};
        if (!safemem::Read(l.ctl + kPos, pos, sizeof(pos)))
            continue;
        pos[1] = l.y + want;
        SetPos(l.ctl, pos);
        const auto it = g_base.find(l.ctl);
        if (it != g_base.end())
            it->second.pos[1] = pos[1];
    }
}

void Closed()
{
    g_lifted.clear();
    g_framed.clear();
    g_moveRefused = false;
    g_liftNow = 0;
    g_jobReward = 0;
    g_dropRest = g_dropTries = 0;
    const int base = gametext::Applied();
    if (g_rich != base)
        gametext::RichSet(base);
    g_rich = base;
    g_richKey = 0;
    winplates::SetRootGrow(0);
    g_fitBox0 = -1;
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
        Lift(0);
        if (const auto reward = g_base.find(g_jobReward); reward != g_base.end())
            reward->second.pos[0] += kRewardLeft;
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
    if (basepage::Hides()) // (a page is the window, its controls stand off the screen: nothing here to place)
        return;
    if (!g_spec)
    {
        // every frame, as an open window's own check above: every 12th left the game's text at its own size and place for 12 frames, then it popped
        // (the owner: "frame-perfect", 7 Nov 2026)
        u32 window = 0;
        const Spec* spec = Find(window);
        if (!spec)
            return;
        static int ladderAt = 0; // (a size that took effect at the main menu changed every font's height)
        if (g_ladder.empty() || ladderAt != gametext::Applied())
            BuildLadder(), ladderAt = gametext::Applied();
        g_spec = spec, g_window = window;
        g_base.clear();
        Collect(*spec);
    }
    if (g_ladder.empty())
        return;
    const float screenH = static_cast<float>(fl::ScreenHeight());
    if (g_spec->drop > 0)
        Drop(*g_spec);
    else if (g_spec->frame)
        Place(*g_spec);
    for (Column& c : g_columns)
        Rows(c, screenH);
    Singles();
    Loose();
    PlaceFramed(screenH);
    StretchFoot(*g_spec, screenH);
    if (g_shipClose)
        StandShipHead(screenH);
    winplates::SetRootGrow(g_spec->fit ? FitFoot(screenH) : g_spec->grow);
    if (!g_lifted.empty())
        Lift(uiclick::FindVisible("QuantitySlider") ? kStackNudge : winplates::kStackLift);
    if (PlaceBoxes(*g_spec, screenH))
        g_richKey = 0; // fitted after the next draw has the new pixels
    else
        FitRich();
}

void SetInventoryFrame(bool on) { g_inventoryFrame = on; }

// A text off the screen while HUD 2 says it: the dealer shows its refusal again every frame, so hiding it by its state does nothing. By its position, which the game leaves alone
static u32 g_parked = 0;
static float g_parkedPos[3] = {};
void Park(u32 ctl)
{
    if (ctl == g_parked)
        return;
    if (g_parked && IsText(g_parked) && strcmp(ui::ControlName(g_parked), "ErrorHelp") == 0)
        SetPos(g_parked, g_parkedPos);
    g_parked = 0;
    float pos[3] = {};
    if (ctl && IsText(ctl) && safemem::Read(ctl + kPos, pos, sizeof(pos)))
    {
        memcpy(g_parkedPos, pos, sizeof(pos));
        pos[0] -= 8.0f;
        if (SetPos(ctl, pos))
            g_parked = ctl;
    }
}

float TextWidthPx(u32 ctl) { return IsText(ctl) && Shown(ctl) ? static_cast<float>(R32(R32(ctl + kObj) + kExtW)) : 0.0f; }

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

// Every text out of its frame or box, for the walk: nothing is judged by eye
std::string Overflows()
{
    std::ostringstream o;
    const float H = static_cast<float>(fl::ScreenHeight());
    for (const Framed& f : g_framed)
    {
        ui::Item fr;
        if (!Shown(f.frame) || !ui::ControlRect(f.frame, fr) || fr.hh <= 0.5f)
            continue;
        for (u32 ctl : {f.top, f.low1, f.low2})
        {
            const Now n = Read(ctl);
            ui::Item it;
            if (!n.shown || !n.valid || n.lines < 1 || !ui::ControlRect(ctl, it))
                continue;
            const float line = n.height / static_cast<float>(n.lines);
            if (it.y + line * kInkTop < fr.y - fr.hh - 1 || it.y + line * kInkFoot > fr.y + fr.hh + 1 || n.width > fr.hw * 2)
                o << "over frame " << ui::ControlName(ctl) << " y " << it.y << " h " << n.height << " w " << n.width << " frame " << fr.y - fr.hh << ".." << fr.y + fr.hh << " wide " << fr.hw * 2 << "\n";
        }
    }
    const auto box = [&](const std::vector<Single>& list)
    {
        for (const Single& s : list)
        {
            const Now n = Read(s.ctl);
            const u32 obj = R32(s.ctl + kObj);
            u8 wraps = 0;
            safemem::Read(obj + 0x30, &wraps, 1);
            const u32 wrap = R32(obj + kWrapPx);
            if (n.shown && n.valid && n.lines == 1 && !wraps && wrap && n.width > static_cast<float>(wrap))
                o << "over box " << ui::ControlName(s.ctl) << " w " << n.width << " box " << wrap << "\n";
        }
    };
    box(g_singles);
    box(g_loose);
    return o.str();
}

// A label that wraps puts every number beside the wrong label, and stats over their box scroll apart from their values. Read at rest: the boxes grow after a first fit.
static bool LabelsApart()
{
    if (!IsRich(g_stats) || !IsRich(g_values) || !Shown(g_stats) || !Shown(g_values))
        return false;
    return R32(g_stats + kTextH) != R32(g_values + kTextH) || R32(g_stats + kTextH) > R32(g_stats + kBoxH);
}

std::string Status()
{
    std::ostringstream o;
    o << "windowtext window=" << (g_spec ? g_spec->window : "none") << " ladder=" << g_ladder.size() << " fonts=" << g_heights.size() << " columns";
    for (const Column& c : g_columns)
        o << " " << c.pick.name << "/" << c.pick.price << " over=" << c.overflow;
    o << " inventoryFrame=" << g_inventoryFrame << " cuts=" << g_cutCount << "/" << g_cutCalls;
    // (debug: each row name's font, to tell a face the row ladder does not know)
    o << " rowfonts:";
    const std::vector<gametext::Font> fonts = gametext::Fonts();
    for (const Framed& f : g_framed)
    {
        const Now n = Read(f.top);
        if (!n.shown)
            continue;
        std::string face = "?";
        for (const gametext::Font& g : fonts)
            if (g.entry == n.font)
                face = g.nickname + "/" + g.face;
        o << " " << ui::ControlName(f.top) << "=" << face;
    }
    o << " loose=" << g_loose.size() << "/" << g_looseSteps << " singles=" << g_singles.size() << " rich=" << g_rich << " tries=" << g_richFits << " ms=" << g_richMs << " labelsOff=" << (LabelsApart() ? 1 : 0);
    for (u32 box : g_boxes)
        if (IsRich(box))
        {
            float w = 0;
            safemem::Read(box + kSizeW, &w, 4);
            float barPos = -1;
            if (const u32 bar = R32(box + kBar))
                safemem::Read(bar + 0x3C8, &barPos, 4);
            o << "\n  " << ui::ControlName(box) << " shown=" << Shown(box) << " w=" << w << " box " << R32(box + kBoxW) << " x " << R32(box + kBoxH)
              << " text " << R32(box + kTextH) << " bar " << (R32(box + 0x3F0) ? 1 : 0) << " at " << barPos << " top " << static_cast<int>(R32(box + kTop));
        }
    return o.str();
}
} // namespace windowtext
