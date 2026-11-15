// The docked windows as pages: basepage.h. The plan and the facts: docs\BASE_ROOMS_PLAN.md. The pages leave this file for
// the page runtime (audit 70b F3: pages\dealers\, pages\neuronet\; repair, BEST PRICES, the dealer in its three modes, the
// ship dealer, the job board and the news have left); it is their shell, as nnpage.cpp is
// INFO's: it decides which docked window stands (KindNow), calls pagerun's Keep / Frame / Draw / Prompts / Pad for them,
// places the job board's and the news's text round their panel, closes the window when HUD 2 leaves, and prints their
// `basepage` lines.
#include "basepage.h"
#include "uistrip.h"
#include "nnstrip.h"

#include "game/baseinfo.h"
#include "game/events.h"
#include "game/hostcalls.h"
#include "game/room.h"
#include "game/screens/board_screen.h"
#include "game/screens/market_screen.h"
#include "game/screens/parked.h"
#include "game/screens/prices_screen.h"
#include "game/screens/repair_screen.h"
#include "game/screens/shiptrade_screen.h"
#include "hudhost.h"
#include "nnpage_kit.h"
#include "nnpage_rules.h"
#include "pages/dealers/market_page.h"
#include "pages/dealers/prices_page.h"
#include "pages/dealers/repair_page.h"
#include "pages/dealers/shiptrade_page.h"
#include "pages/neuronet/board_page.h"
#include "pages/pagelist.h"
#include "pages/pagerun.h"
#include "text.h"
#include "ui/hud2_kit.h"
#include "ui/hud_rows.h"

#include <sstream>
#include <unordered_map>

namespace basepage
{
using namespace nnkit;
using nnrules::List;

enum class Kind
{
    None,
    Commodity,
    Equipment,
    Ships,
    Jobs,
    News,
    Repair,
    Final,  // the ship dealer's final stage: NN_Dealer in its mode 2
    Prices, // the game's commodity list by base
};
static Kind g_kind = Kind::None;
static u32 g_dealer = 0; // the window of the page standing
static bool g_pad = false;

constexpr u32 kDealerKind = 0x978; // NN_Dealer's kind (docs\research\neuronet-windows-facts.md): 1 commodity, 3 equipment, 2 the final stage

static bool g_pagesOn = true; // HUD 2 as it stood when no page was up

// (the game's window stands off the screen under the page: the runtime parks it by its own setter, pagerun.cpp Park)

// ---- the close: the window's own close button (HUD 2 leaving over any docked page; the pages' own CLOSE is their action)
static void PressClose()
{
    screens::PressClose(g_kind == Kind::Ships ? shiptradescreen::kCloseButton : g_kind == Kind::Jobs ? boardscreen::kJobsClose : g_kind == Kind::News ? boardscreen::kNewsClose : g_kind == Kind::Repair ? repairscreen::kCloseButton : g_kind == Kind::Prices ? pricesscreen::kCloseButton : marketscreen::kCloseButton);
}

// ---- the page
static Holes g_holes; // this frame's and the last's: the ground goes first, the rows say where (the runtime's pages share them: pagerun::DrawStart)
using pagekit::Heading;            // (the docked pages' heading: the draw kit's, shared with the moved pages' views)

// The page's ground starts under the strip and the room's words: a strip of three lines (1280x800, some rooms) stood cut by a fixed line. The fixed line stays the least,
// so the game's own views, which stand where the game has them, keep their place.
static float PageTop(float H) { return std::max(0.165f * H, std::max(uistrip::StripBottom() * H, nnstrip::RoomFoot()) + 0.006f * H); }

// ---- the pages on the runtime (audit 70b F3): repair (pages\dealers\repair_*), BEST PRICES (pages\dealers\prices_*), the
// dealer in its three modes (pages\dealers\market_*), the ship dealer (pages\dealers\shiptrade_*), the job board and the news
// (pages\neuronet\board_*)
static bool Market(Kind k) { return k == Kind::Commodity || k == Kind::Equipment || k == Kind::Final; }
static bool Board(Kind k) { return k == Kind::Jobs || k == Kind::News; }
static const char* RuntimeId(Kind k)
{
    return k == Kind::Repair ? repairpage::kId : k == Kind::Prices ? pricespage::kId : Market(k) ? marketpage::kId : k == Kind::Ships ? shiptradepage::kId : k == Kind::Jobs ? boardpage::kJobsId : k == Kind::News ? boardpage::kNewsId : nullptr;
}
// What the runtime's page is given of the screen: the frame, the ground's top under the strip, the lane, the pad's form. The
// ground's holes are the docked pages' one set (pagerun::DrawStart hands them over; the view's come back).
static void DrawRuntime(const Frame& f)
{
    page::ViewIn in{f.W, f.H, f.left, f.right, f.top, f.bottom};
    in.titleY = f.titleY, in.pageTop = PageTop(f.H), in.pad = g_pad;
    in.laneY = LaneY(), in.laneH = LaneChipH(), in.laneMid = (deckmode::LaneTopH() + deckmode::LaneBottomH()) / 2 * f.H;
    g_holes = pagerun::Draw(in).holes;
}

// ---- the job board and the news: a panel in the room, the room showing beside it (pages\neuronet\board_*). The game's window
// stays (these two answer no move of their own, and their picture stands by it) and does the work; the shell reflows its text
// into the panel's column (where the HUD module puts the panel: under the strip) before the page reads it, and moves what could
// be clicked blind or read through a hole off the screen.
static void BoardPlace(const Frame& f)
{
    const bool jobs = g_kind == Kind::Jobs;
    const int mode = jobs ? boardscreen::kJobs : boardscreen::kNews;
    const boardscreen::Panel p = boardscreen::PanelOf(f.W, f.H, PageTop(f.H), jobs);
    const u32 box = uiclick::FindVisible(boardscreen::TextName(mode));
    // (a rich text keeps its own place: size +0x330, pixels +0x358, left and top +0x348, its layout position +0x47C: windowtext's FitBoxes)
    const float w = RF(box + 0x330);
    const int px = box ? static_cast<int>(R32(box + 0x358)) : 0;
    // The text's bar where the game puts it (left of the text: drawn left of the list, audit 109_ 2g) - a bar it makes again (a job's text overflows after one
    // that fitted) and at times one it keeps from job to job: the size setter poses it as it posed the first, off the page (FREELANCER_RE "A bar so made stands
    // where the game puts it"; PlaceAt below does not move a scroll bar)
    ui::Item barAt;
    const u32 bar0 = box ? R32(box + 0x3F0) : 0;
    const bool barAstray = bar0 && ui::ControlRect(bar0, barAt) && barAt.x < p.tx0;
    if (box && w > 0 && px > 0 && exe::RichSetSize.ok())
    {
        const float k = static_cast<float>(px) / w, wantW = p.tx1 - p.tx0, wantH = p.ty1 - p.ty0;
        float pos[3] = {};
        safemem::Read(box + 0x47C, pos, sizeof(pos));
        const float dx = p.tx0 - static_cast<float>(static_cast<int>(R32(box + 0x348))), dy = static_cast<float>(static_cast<int>(R32(box + 0x34C))) - p.ty0;
        if (fabsf(dx) >= 1.5f || fabsf(dy) >= 1.5f)
        {
            pos[0] += dx / k, pos[1] += dy / k;
            SetPosition(box, pos);
        }
        if (barAstray || fabsf(static_cast<float>(px) - wantW) > 2 || fabsf(static_cast<float>(R32(box + 0x35C)) - wantH) > 2)
        {
            hk::Call<void(__thiscall*)(u32, float, float)>(exe::RichSetSize, box, wantW / k, wantH / k);
            if (exe::RichLayout.ok())
                hk::Call<u32(__thiscall*)(u32)>(exe::RichLayout, box);
        }
    }
    if (const u32 bar = boardscreen::TextBar(mode))
        PlaceAt(bar, -f.W, 0);
    static const char* const kJobs[] = {"MissionVendorIcon", "MissionVendorTitle", "MissionVendorMissionReward", "MissionVendorMissionDest", "MissionVendorMissionTitle", "MissionVendorMissionReject", "MissionVendorAcceptButton", "MissionVendorAcceptText", "MissionVendorCloseButton"};
    static const char* const kNews[] = {"NewsStoryIcon", "NewsTitle", "NewsStoryHeadline", "NewsTicker", "NewsCloseButton"};
    if (jobs)
        for (const char* n : kJobs)
            PlaceAt(uiclick::FindVisible(n), -f.W, 0);
    else
        for (const char* n : kNews)
            PlaceAt(uiclick::FindVisible(n), -f.W, 0);
}

// ---- the frame
static Kind KindNow()
{
    if (uiclick::FindVisible("NN_CommodityByBase"))
        return Kind::Prices;
    if (uiclick::FindVisible("NN_Repair"))
        return Kind::Repair;
    if (uiclick::FindVisible("NN_ShipTrader"))
        return Kind::Ships;
    if (uiclick::FindVisible(boardscreen::kJobsName))
        return Kind::Jobs;
    if (uiclick::FindVisible(boardscreen::kNewsName))
        return Kind::News;
    const u32 d = uiclick::FindVisible("NN_Dealer");
    if (!d || !uiclick::FindVisible("NN_Inventory"))
        return Kind::None;
    const u32 k = R32(d + kDealerKind);
    return k == 1 ? Kind::Commodity : k == 3 ? Kind::Equipment : k == 2 ? Kind::Final : Kind::None;
}

void Update(float dt, bool pad)
{
    g_pad = pad;
    // The switches take effect at the next opening: a page standing keeps its windows off the screen and its controls in its places until it closes.
    // Leaving HUD 2 (hud_style 0, the classic HUD) is not such a switch: the page's draw stops at once, and what it moved (the game's window off the screen, the board's
    // controls, the icons in its rows) stood so on the classic look until the window closed (audit 65e F3). The game's window closes through its own close button,
    // as the page's CLOSE does; it opens again at its own places. The close waits one frame: windowtext stands down after this Update in the same skin frame
    // (hud2_entry Skin) and gives the window's rows their names and places back, which a close before it had already destroyed (audit 95_: it wrote into the
    // freed controls, a /GS fail-fast 0xC0000409 at the ship dealer's final stage).
    static int s_leaving = 0; // 0 nothing, 1 HUD 2 left (the close next frame), 2 the close pressed
    const bool leaving = hudrows::g.hudStyle == 0;
    if (g_kind == Kind::None)
        g_pagesOn = !leaving, s_leaving = 0;
    else if (!leaving)
        s_leaving = s_leaving == 1 ? 0 : s_leaving; // (back on HUD 2 before the close: windowtext takes the window up again)
    else if (s_leaving < 2 && !room::NNWindowBusy() && s_leaving++ == 1)
        PressClose(), events::Push("basepage: HUD 2 left over a page: its window's close pressed");
    const Kind kind = g_pagesOn ? KindNow() : Kind::None;
    const u32 d = kind == Kind::Ships ? uiclick::FindVisible("NN_ShipTrader") : kind == Kind::Jobs ? uiclick::FindVisible(boardscreen::kJobsName) : kind == Kind::News ? uiclick::FindVisible(boardscreen::kNewsName) : kind == Kind::Repair ? uiclick::FindVisible("NN_Repair") : kind == Kind::Prices ? uiclick::FindVisible("NN_CommodityByBase") : kind != Kind::None ? uiclick::FindVisible("NN_Dealer") : 0;
    const bool opened = kind != g_kind || d != g_dealer; // (each page's focus opens in its model: the runtime's Init on an opening)
    g_kind = kind, g_dealer = d;
    pagerun::Keep(RuntimeId(kind), d, pagerun::kShellBase); // (a page of the runtime that went: its window is closing, never given back)
    if (kind != Kind::Ships)
        shiptradescreen::HoldWheel(false);
    if (kind == Kind::None)
        return;
    if (const char* id = RuntimeId(kind))
    {
        // the job board's and the news's text in the panel's column before the page reads it (out of the window's slide); the
        // news's ticker runs on whichever of the two stands
        if (Board(kind))
        {
            boardscreen::Advance(dt * 0.06f * winplates::g_H);
            if (!room::NNWindowBusy())
                BoardPlace(Bounds());
        }
        // the page reads, parks and scrolls itself (pagerun Frame); the window's own close button stays off the screen too
        // (placed after the park, which moves it with its window)
        // (the dealer works its partner too: NN_Inventory, parked beside it)
        const int wheelNow = hostcalls::g.RealMouseWheel ? hostcalls::g.RealMouseWheel() : 0;
        pagerun::Frame(id, d, opened, wheelNow, dt, g_pad, Market(kind) ? uiclick::FindVisible(marketscreen::kPartnerName) : 0);
        if (kind == Kind::Prices && !room::NNWindowBusy())
            PlaceAt(uiclick::FindVisible(pricesscreen::kCloseButton), -winplates::g_W, 0);
        if (kind == Kind::Ships && !room::NNWindowBusy())
            shiptradescreen::ViewBack(); // (the game's 3D ship back in the screen's middle: it went with its window)
    }
}

void IconsUnder()
{
    if (g_kind != Kind::None && KindNow() == g_kind)
        pagerun::IconsUnder(); // (the dealer's two lists, as its view last drew them)
}

void Draw()
{
    pagerun::DrawStart(g_holes); // (a list this Draw does not draw sends nothing; the docked pages share one ground's holes)
    if (g_kind == Kind::None)
    {
        g_holes = Holes();
        return;
    }
    baseinfo::Refresh(); // (the pad's close runs between the page's Update and this: the list must be this moment's)
    const Frame f = Bounds();
    PageLane(f);
    if (KindNow() != g_kind)
        return;
    if (RuntimeId(g_kind))
        DrawRuntime(f);
}
void DrawAfter() {}
// (under the game's UI: the icons' cells stand on the page's ground, not on the room)
void DrawUnder()
{
    if (RuntimeId(g_kind))
    {
        pagerun::GroundUnder();
        return;
    }
    for (int i = 0; i < g_holes.n; ++i)
        GroundBut(g_holes.r[i][0], g_holes.r[i][1], g_holes.r[i][2], g_holes.r[i][3], Holes());
}
bool On() { return g_kind != Kind::None; }
bool Hides() { return g_kind != Kind::None; }

// What the rest of HUD 2 asks of these pages while they stand (pages\pagelist.h LegacyPage), until they move onto the
// runtime (audit 70b F3). nnpage's shell draws them (its Draw and DrawAfter dispatch here); the ground under the game's
// icons is theirs.
static bool IsBaseWindow(int fam)
{
    using namespace winplates;
    static const int kFamNews = FamilyIndex("news");
    return fam == kFamInventory || fam == kFamDealer || fam == kFamShipDealer || fam == kFamJobBoard || fam == kFamNews;
}
static const pagelist::LegacyPage kBaseEntry = {"base", Hides, IsBaseWindow, Hides, nullptr, nullptr, DrawUnder};
LEGACY_PAGE_REGISTER(kBaseEntry);

int Prompts(views::PagePrompt* out, int max)
{
    if (RuntimeId(g_kind))
        return pagerun::Prompts(out, max); // (the page's own: repair_page.cpp, prices_page.cpp, market_page.cpp, shiptrade_page.cpp, board_page.cpp)
    return 0;
}

unsigned Pad(unsigned pressed, unsigned held, float dt)
{
    if (g_kind == Kind::None)
        return 0;
    static float repeat = 0;
    const unsigned walk = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN;
    if (held & walk)
    {
        repeat += dt;
        if (repeat > 0.3f) // (0.3 s, then 20 rows a second, as the NeuroNet pages: issue 68)
            pressed |= held & walk, repeat -= 0.05f;
    }
    else
        repeat = 0;
    if (RuntimeId(g_kind))
        return pagerun::Pad(pressed, held, dt); // (the page's own, with the repeat above: repair_page.cpp, prices_page.cpp, market_page.cpp, board_page.cpp)
    return 0;
}

void Shutdown()
{
    shiptradescreen::HoldWheel(false);
    g_kind = Kind::None;
    g_dealer = 0;
}

// HUD 2 left (hud2_entry.cpp Leave): a page standing closes its window through the window's own close button, as Update does when the skin goes off
// under HUD 2: the game's window stood off the screen and its rows cut until it closed (audit 65e F3). The close comes one call after the first, which
// is the frame windowtext gave the window's rows back: a close before it destroyed the controls it then wrote into (audit 95_, 0xC0000409).
bool Leave(bool first)
{
    shiptradescreen::HoldWheel(false);
    if (g_kind == Kind::None)
        return true;
    if (first || room::NNWindowBusy())
        return false;
    PressClose(), events::Push("basepage: HUD 2 left over a page: its window's close pressed");
    Shutdown();
    return true;
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "press")
    {
        std::string b;
        in >> b;
        const struct
        {
            const char* n;
            unsigned m;
        } kMap[] = {{"x", XINPUT_GAMEPAD_X}, {"y", XINPUT_GAMEPAD_Y}, {"up", XINPUT_GAMEPAD_DPAD_UP}, {"down", XINPUT_GAMEPAD_DPAD_DOWN}, {"left", XINPUT_GAMEPAD_DPAD_LEFT}, {"right", XINPUT_GAMEPAD_DPAD_RIGHT}, {"ls", XINPUT_GAMEPAD_LEFT_THUMB}};
        for (const auto& k : kMap)
            if (b == k.n)
                return "basepage: took 0x" + std::to_string(Pad(k.m, 0, 0));
        if (b == "trade")
        {
            const auto* model = Market(g_kind) ? static_cast<const marketscreen::Model*>(pagerun::Model(marketpage::kId)) : nullptr;
            if (model)
                pagerun::Act(marketscreen::kActTrade, model->FocusArg()); // (the trade's button at once, no hold: the walks' own action)
            return "basepage: traded";
        }
        if (b == "a")
        {
            Pad(XINPUT_GAMEPAD_A, 0, 0);
            return "basepage: a";
        }
        return "basepage press <a|x|y|up|down|left|right|ls|trade>";
    }
    if (what == "card") // the dealer's item card: its first line shown, and the lines and room the view last laid out (tests\state_fixes.ps1 rscard)
    {
        const auto* model = Market(g_kind) ? static_cast<const marketscreen::Model*>(pagerun::Model(marketpage::kId)) : nullptr;
        if (!model)
            return "basepage card: n/a: no dealer page";
        return "basepage card first=" + std::to_string(model->cardFirst) + " lines=" + std::to_string(model->cardLines) + " room=" + std::to_string(model->cardRoom);
    }
    std::ostringstream o;
    // the pages on the runtime: their lines from their models, word for word as before (tests\base_pages.ps1 reads them)
    if (g_kind == Kind::Repair)
    {
        static const repairscreen::Model none;
        const auto* model = static_cast<const repairscreen::Model*>(pagerun::Model(repairpage::kId));
        const repairscreen::Model& rm = model ? *model : none;
        o << "basepage repair sel=" << rm.sel << " can=" << repairscreen::CanMend() << " total=" << text::Utf8(repairscreen::TotalText());
        for (const repairscreen::Row& m : rm.rows)
            o << " [" << m.slot << " " << text::Utf8(m.name) << " $" << m.price << " tick=" << m.ticked << " health=" << m.health << " arch=" << m.arch << "]";
        return o.str();
    }
    if (g_kind == Kind::Prices)
    {
        static const pricesscreen::Model none;
        const auto* model = static_cast<const pricesscreen::Model*>(pagerun::Model(pricespage::kId));
        const pricesscreen::Model& pm = model ? *model : none;
        o << "basepage prices n=" << pm.quotes.size() << " sel=" << pm.list.sel << " first=" << pm.list.first << " rows=" << pm.list.rows;
        for (const screens::Quote& q : pm.quotes)
            o << " [" << text::Utf8(q.base) << " $" << q.price << " " << text::Utf8(q.sells) << "]";
        return o.str();
    }
    // the dealer's model (market_page.cpp); none yet, or no dealer standing: its fields as an opening left them (every kind's
    // opening, None's too, reset them: the focus on the dealer's side, no rows)
    static const marketscreen::Model kNoMarket;
    const auto* market = Market(g_kind) ? static_cast<const marketscreen::Model*>(pagerun::Model(marketpage::kId)) : nullptr;
    const marketscreen::Model& mm = market ? *market : kNoMarket;
    const int side = market ? mm.side : g_kind == Kind::Final ? marketscreen::kYours : marketscreen::kTheirs;
    if (g_kind == Kind::Final)
        o << "basepage final stop=" << (market ? mm.buyStop : true) << " can=" << marketscreen::CanBuyFinal() << " back=" << marketscreen::BackStands() << " why=" << text::Utf8(screens::TextIn("ShipBuyProblem")) << " tradein=" << static_cast<int>(R32(g_dealer + 0x9FC)) << " ";
    if (g_kind == Kind::Ships)
    {
        // the ship dealer's model (shiptrade_page.cpp); none yet: its fields as an opening left them
        static const shiptradescreen::Model kNoShips;
        const auto* ships = static_cast<const shiptradescreen::Model*>(pagerun::Model(shiptradepage::kId));
        const shiptradescreen::Model& sm = ships ? *ships : kNoShips;
        o << "basepage ships sel=" << sm.sel << " can=" << shiptradescreen::CanBuy(sm) << " side=" << sm.focus << " zoomable=" << shiptradescreen::Zoomable();
        if (const u32 v = shiptradescreen::ViewCtl())
        {
            float f[3] = {};
            safemem::Read(v + 0x3EC, &f[0], 4), safemem::Read(v + 0x520, &f[1], 8);
            o << " zoom=" << f[0] << " yaw=" << f[1] << " pitch=" << f[2];
        }
        for (const shiptradescreen::Offer& s : sm.offers)
            o << " [" << text::Utf8(s.name) << " $" << s.price << " ref=" << s.refusal << " lvl=" << s.level << " facts=" << gamedata::ShipNamed(s.name).ok << "]";
        return o.str();
    }
    if (Board(g_kind))
    {
        // the job board's or the news's model (board_page.cpp); none yet: the side an opening leaves (the list)
        const int mode = g_kind == Kind::Jobs ? boardscreen::kJobs : boardscreen::kNews;
        const auto* board = static_cast<const boardscreen::Model*>(pagerun::Model(RuntimeId(g_kind)));
        const u32 bar = boardscreen::TextBar(mode);
        o << "basepage board sel=" << boardscreen::Sel(mode) << " bar=" << bar << " side=" << (board ? board->side : 0);
        if (bar)
            o << " textAt=" << uiclick::Scroll(bar, 0, false, nullptr, nullptr);
        return o.str();
    }
    o << "basepage kind=" << static_cast<int>(g_kind) << " side=" << side << " yours=" << mm.rows[marketscreen::kYours].size() << " theirs=" << mm.rows[marketscreen::kTheirs].size() << " sel=" << mm.list[side].sel;
    if (g_kind != Kind::None)
    {
        constexpr u32 kRow = 0x3A4; // (a list window's selected row)
        const u32 inv = market ? mm.inv : uiclick::FindVisible(marketscreen::kPartnerName);
        o << " filter=" << static_cast<int>(R32(inv + 0x32C)) << " rowY=" << static_cast<int>(R32(inv + kRow)) << " rowT=" << static_cast<int>(R32(g_dealer + kRow)) << " canTrade=" << marketscreen::CanTrade(mm) << " qty=" << marketscreen::Qty() << "/" << marketscreen::QtyMost() << " credits=" << baseinfo::Credits();
        if (const marketscreen::Row* r = mm.Focus())
        {
            o << " item=" << text::Utf8(r->name) << " card=" << mm.cardText.size() << " mounted=" << r->mounted << " tagged=" << gamedata::TaggedCompare();
            if (!r->mounted)
            {
                const gamedata::Versus v = gamedata::Against(r->arch);
                o << " vs=" << text::Utf8(v.arch ? hudinfo::EquipName(v.arch) : std::wstring(L"-")) << " vsTagged=" << v.tagged << " free=" << v.freeHp;
            }
        }
    }
    return o.str();
}
} // namespace basepage
