// The NeuroNet's pages: nnpage.h. The plan and the game's facts: docs\NEURONET_PLAN.md, docs\research\neuronet-windows-facts.md; the rules: nnpage_rules.h.
#include "nnpage.h"

#include "game/baseinfo.h"
#include "game/deckmode.h"
#include "game/hostcalls.h"
#include "game/hudinfo.h"
#include "game/nntabs.h"
#include "game/nnwin.h"
#include "game/room.h"
#include "game/space.h"
#include "flexe.h"
#include "hudhost.h"
#include "hudvis.h"
#include "game/screens/inventory_screen.h"
#include "game/screens/mlog_screen.h"
#include "game/screens/nnhelp_screen.h"
#include "game/screens/places_screen.h"
#include "game/screens/status_screen.h"
#include "nnpage_kit.h"
#include "pages/dealers/inventory_page.h"
#include "pages/dealers/status_page.h"
#include "pages/neuronet/death_page.h"
#include "pages/neuronet/info_page.h"
#include "pages/neuronet/mlog_page.h"
#include "pages/pagelist.h"
#include "pages/pagerun.h"
#include "text.h"
#include "nnpage_rules.h"
#include "basepage.h"
#include "nnstrip.h"
#include "safemem.h"
#include "skinchip.h"
#include "skinkit.h"
#include "ui/hud2_kit.h"
#include "ui/hud_rows.h"
#include "ui/ui.h"
#include "ui/winplates.h"

#include <sstream>
#include <vector>

namespace nnpage
{
using namespace hud2kit;
using nnrules::List;
using namespace nnkit;
using pagekit::BandRgb;
using statusrules::BandWord;

enum class Page
{
    None,
    Log,
    Inventory,
    Status,
    Info,
    Death // the log after a death or a failed mission, single player: its three load buttons (issue 50)
};
static Page g_page = Page::None;
static u32 g_root = 0;
static bool g_pad = false, g_paused = false;
static float g_dt = 0;

// The windows' own fields (docs\research\neuronet-windows-facts.md; LOG's: core\game\screens\mlog_screen.cpp, STATUS's: status_screen.cpp,
// INVENTORY's: inventory_screen.cpp); the verb's two of NN_Inventory's
constexpr u32 kInvRow = 0x3A4, kInvFirst = 0x3C4; // the selected row, the list's first row

static const int kFamStatus = winplates::FamilyIndex("playerinfo");

static u32 RootOf(int fam)
{
    for (const winplates::Win& w : winplates::g_wins)
        if (w.fam == fam && winplates::Shown(w.root))
            return w.root;
    return 0;
}

// The shell's one chip: INFO's SHOW ON MAP.
static void ChipClick(int chip);
enum Chip
{
    kChipMap = 3,
};

static void Mouse(const Frame& f, const wchar_t* line)
{
    if (!g_pad)
        Text(f.left, (deckmode::LaneTopH() + deckmode::LaneBottomH()) / 2 * f.H, line, 0.026f * f.H, Argb(kChrome, 1));
}
static float ChipAt(float x, float y, float h, const std::wstring& word, int chip) { return nnkit::ChipAt(x, y, h, word, ChipClick, chip); }

// ---- STATUS (pages\dealers\status_*, its reads core\game\screens\status_*: on the runtime since audit 70b F4b); the shell
// cuts the card's hole (DrawStatus, below) and keeps the window's X and title off the page (Under)
static float g_placesRead = 0;

// ---- INVENTORY (pages\dealers\inventory_*, its reads and actions core\game\screens\inventory_*: on the runtime since audit 70b
// F4 unit b2); the shell places the game's controls the page draws round (DrawInventory, below), cuts the card's hole and keeps
// BEST PRICES' label blank.
// The game's BEST PRICES button stays hidden (the page offers the list itself: inventory_screen.cpp TradeOffered).
// The label is not the button's own (a text button, vt 0x5E614C, which PlaceAt moves off the page: its TRADE ROUTES stayed, 7 Nov 2026) but a title text of its own:
// its text goes blank through the text control's own setter (message 0x1D, as skinload blanks the loading word), right before the UI pass (Under).
static bool BlankText(u32 ctl)
{
    __try
    {
        const u32 fn = Mem<u32>(Mem<u32>(ctl) + 0xA8);
        if (!exe::TextCtlSetProperty.is(fn))
            return false;
        reinterpret_cast<u32(__thiscall*)(u32, u32, const void*, u32)>(fn)(ctl, 0x1D, L" ", 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
static int g_tradeBlanked = 0; // (debug: times the label was blanked, `nnpage`)
static void HideTrade()
{
    // the label is its own control, TradeRouteText (a title text, vt 0x5E4F24; `uinames Trade`, 7 Nov 2026), not the button: blank, so it never stands over the card
    const u32 t = uiclick::Find("TradeRouteText");
    std::wstring now;
    if (t && winplates::TextOf(t, now) && now != L" " && BlankText(t))
        ++g_tradeBlanked;
}
// The jettison panel is the page's (inventory_view.cpp DrawJettison): the game's slider and buttons stand off the screen for as long as it stands and go on answering
// by name; its number field comes into the page, it can still be typed in.
static const char* const kJetOff[] = {"Inv_Jettison_Title", "JettisonQuantitySlider", "JettisonQuantityText", "JettisonCancelButton", "JettisonAcceptButton"};
static float g_jetHome[5][3];
static u32 g_jetMoved[5] = {};
static void JettisonControlsBack()
{
    for (int i = 0; i < 5; ++i)
        if (g_jetMoved[i])
        {
            if (uiclick::FindVisible(kJetOff[i]) == g_jetMoved[i])
                SetPosition(g_jetMoved[i], g_jetHome[i]);
            g_jetMoved[i] = 0;
        }
}

// ---- LOG and INFO: the game's text in a column, HUD 2 round it
// The hole for the game's text starts a little below the text's top: the window's art (a tab's foot, a line) stands on the text's top left, and a hole grown up by
// 0.012 H like its other sides showed it above the first line (issue 63, Shockwave's Deck). At 4K the art's foot stood 8 px under the text's top, the first line's
// capitals 19 px: the hole starts between them, 13 px (0.006 H; the game's UI scales with the height).
constexpr float kTextTop = -0.006f;

// INFO's left column (its shell's, below): what the player has found here and the ships near (core\game\screens\places_*), read
// every 2 s; `places=` of the verb is the last read's count, INFO's or LOG's (LOG's page reads its own: mlog_screen.cpp).
static std::vector<screens::Place> g_places, g_ships;
static size_t g_placesShown = 0;
using pagekit::Fact;
static u32 Ink(const screens::Place&) { return kInk; }
static u32 ByStanding(const screens::Place& p) { return BandRgb(nnrules::BandOf(p.attitude)); }

// What the shell's pages on the runtime are given of the screen: the frame, the pad's form, the prompts' lane.
static page::ViewIn PageIn(const Frame& f)
{
    page::ViewIn in{f.W, f.H, f.left, f.right, f.top, f.bottom};
    in.titleY = f.titleY, in.pad = g_pad;
    in.laneY = LaneY(), in.laneH = LaneChipH(), in.laneMid = (deckmode::LaneTopH() + deckmode::LaneBottomH()) / 2 * f.H;
    return in;
}
// The hole round the game's text `ctl` as the shell cut it, handed to the view: the text shows through it once it stands laid
// out where it stays (OpenHole).
static void TextHole(page::ViewIn& in, const Box& hole, u32 ctl)
{
    if (hole.ok)
        in.text[0] = hole.x0, in.text[1] = hole.y0, in.text[2] = hole.x1, in.text[3] = hole.y1, in.textOk = true;
    in.textReady = windowtext::TextReady(ctl);
}
// LOG and the death page (pages\neuronet\mlog_*, death_*: on the runtime since audit 70b F4a) are given the journal's hole.
static void DrawJournal(const Frame& f)
{
    page::ViewIn in = PageIn(f);
    Box hole;
    const u32 text = uiclick::FindVisible(mlogscreen::kTextName);
    hole.Add(text, 0.012f * f.H, kTextTop * f.H);
    TextHole(in, hole, text);
    pagerun::Draw(in);
}
// STATUS (pages\dealers\status_*: on the runtime since audit 70b F4b) is given the faction card's hole once its page showed a
// card: the card's box, grown, and the heading the card lays out above its box's top.
static void DrawStatus(const Frame& f)
{
    page::ViewIn in = PageIn(f);
    const auto* m = static_cast<const statusscreen::Model*>(pagerun::Model(statuspage::kId));
    if (m && m->card)
    {
        Box hole;
        const u32 card = uiclick::FindVisible(statusscreen::kCardName);
        hole.Add(card, 0.012f * f.H);
        hole.y0 -= 0.05f * f.H;
        TextHole(in, hole, card);
    }
    pagerun::Draw(in);
}
// INVENTORY (pages\dealers\inventory_*): the game's controls the page draws round placed first, as the page placed them while it
// drew - the jettison panel's slider, buttons and title off the screen (back home after it) and its number field into the
// page's panel (its box handed in as the hole's), or BEST PRICES' button off the card and its label blank - then the card's
// hole cut. Nothing while the window does not stand (the page draws nothing then).
static void DrawInventory(const Frame& f)
{
    const float H = f.H, W = f.W;
    baseinfo::Refresh(); // (the pad's close runs between the page's Update and this: the list must be this moment's)
    page::ViewIn in = PageIn(f);
    if (uiclick::FindVisible(inventoryscreen::kWindowName))
    {
        if (inventoryscreen::JettisonOpen())
        {
            for (int i = 0; i < 5; ++i)
            {
                const u32 c = uiclick::FindVisible(kJetOff[i]);
                if (c && !g_jetMoved[i] && safemem::Read(c + 0xA8, g_jetHome[i], sizeof(g_jetHome[i])))
                    g_jetMoved[i] = c;
                PlaceAt(c, -W, 0);
            }
            const u32 edit = uiclick::FindVisible("JettisonQuantityEdit");
            if (!g_pad)
            {
                PlaceAt(edit, W / 2 + 0.16f * H, 0.715f * H + 0.092f * H / 2); // (the panel's count row: inventory_view.cpp DrawJettison)
                Box field;
                field.Add(edit, 0.004f * H);
                if (field.ok)
                    in.text[0] = field.x0, in.text[1] = field.y0, in.text[2] = field.x1, in.text[3] = field.y1, in.textOk = true;
            }
            else if (edit)
                PlaceAt(edit, -W, 0);
        }
        else
        {
            JettisonControlsBack();
            PlaceAt(uiclick::FindVisible("TradeRouteButton"), -W, 0); // (it stood in the card's hole: the page has its own word for it; its label is HideTrade's)
            HideTrade();
            Box hole;
            const u32 card = uiclick::FindVisible(inventoryscreen::kCardName);
            hole.Add(card, 0.012f * H);
            TextHole(in, hole, card);
        }
    }
    pagerun::Draw(in);
}
// The page of the runtime each of the shell's pages is (null: one still drawn here).
static const char* RuntimeId(Page p)
{
    return p == Page::Info        ? infopage::kId
           : p == Page::Log       ? mlogpage::kId
           : p == Page::Death     ? deathpage::kId
           : p == Page::Status    ? statuspage::kId
           : p == Page::Inventory ? inventorypage::kId
                                  : nullptr;
}


// A list of factions in the colour of the player's standing, by weight: who flies here, who keeps an office.
static float DrawFactions(float x0, float x1, float y, float foot, float H, const wchar_t* title, const std::vector<gamedata::Faction>& list)
{
    if (list.empty())
        return y;
    Text(x0, y, title, 0.024f * H, Argb(kChrome, 1));
    {
        // cut at its foot: "7 OF 11" beside the title, the weightiest first (as page_kit.h PlaceList)
        size_t named = 0;
        for (const gamedata::Faction& g : list)
            named += !text::Ids(g.nameIds).empty();
        const size_t fit = static_cast<size_t>(std::max(0.0f, (foot - y - 0.05f * H - 0.03f * H) / (0.034f * H) + 1));
        if (fit < named)
        {
            wchar_t of[32];
            swprintf_s(of, L"%u OF %u", static_cast<unsigned>(fit), static_cast<unsigned>(named));
            Text(x1, y, of, 0.024f * H, Argb(kInk, 1), Right);
        }
    }
    y += 0.026f * H;
    skinkit::Rule(x0, x1, y);
    y += 0.024f * H;
    float sum = 0;
    for (const gamedata::Faction& g : list)
        sum += g.weight;
    for (size_t i = 0; i < list.size() && y + 0.03f * H <= foot; ++i, y += 0.034f * H)
    {
        const gamedata::Faction& g = list[i];
        const std::wstring name = text::Ids(g.nameIds);
        if (name.empty())
        {
            y -= 0.034f * H;
            continue;
        }
        const nnrules::Band b = nnrules::BandOf(g.standing);
        wchar_t share[16];
        swprintf_s(share, L"%.0f %%", sum > 0 ? 100 * g.weight / sum : 0);
        TextMarquee(x0, y, name, 0.027f * H, x1 - x0 - 0.20f * H, Argb(BandRgb(b), 1));
        Text(x1 - 0.075f * H, y, BandWord(b), 0.022f * H, Argb(BandRgb(b), 0.9f), Right);
        Text(x1, y, share, 0.024f * H, Argb(kChrome, 1), Right);
    }
    return y;
}
// ---- INFO (audit R3; a page of the framework since audit 70b F2a): the page itself, its model, its park, its text and its
// input are pages\neuronet\info_page.* on the runtime (pages\pagerun.*); the NeuroNet's shell round it stays here (its
// left column and ships are drawn with LOG's helpers, pages\page_kit.h Fact / HereFacts / PlaceList, from core's reads,
// core\game\screens\places_screen.*).
// INFO's two columns: the left one from the game's data, the text at the right (its height `textUsed`), the ships under it
static void InfoColumns(const Frame& f, float textX0, float textTop, float textUsed)
{
    const float H = f.H;
    const float x1 = textX0 - 0.03f * H;
    float y = pagekit::HereFacts(f, f.top + 0.03f * H, x1, screens::ReadHere());
    const u32 base = fl::PlayerBase();
    if (base)
    {
        // the left column, from the game's data: the card at the right lists what is sold
        const gamedata::BaseFacts& b = gamedata::Base(base);
        const std::wstring offers = screens::Offers(b);
        if (!offers.empty())
            y = Fact(f, y, x1, L"HERE YOU FIND", offers);
        y = DrawFactions(f.left, x1, y + 0.01f * H, f.bottom - 0.03f * H, H, L"WHO KEEPS AN OFFICE HERE", b.factions);
        if (b.offersMax > 0)
        {
            wchar_t line[48];
            swprintf_s(line, L"THE JOB BOARD HOLDS %d TO %d OFFERS", b.offersMin, b.offersMax);
            Text(f.left, y + 0.012f * H, line, 0.024f * H, Argb(kChrome, 1));
        }
    }
    else
    {
        const float half = (y + f.bottom) / 2 + 0.03f * H;
        DrawFactions(f.left, x1, y + 0.01f * H, half - 0.03f * H, H, L"WHO FLIES IN THIS SYSTEM", gamedata::System(fl::PlayerSystem()).factions);
        pagekit::PlaceList(f, half, f.left, x1, L"FOUND IN THIS SYSTEM", g_places, Ink);
        // the game's text may be one line: the room under it says who is around
        if (textTop + textUsed + 0.08f * H < f.bottom)
            pagekit::PlaceList(f, textTop + textUsed + 0.05f * H, textX0, f.right, L"SHIPS NEAR YOU", g_ships, ByStanding);
    }
}
static void DrawInfo(const Frame& f)
{
    const float H = f.H;
    Cover(f.W, H, Box()); // (no hole: the whole page is HUD 2's)
    Title(f, L"CURRENT INFORMATION");
    const float x0 = 0.34f * f.W + 0.03f * H, top = f.top + 0.03f * H;
    // the ships stand under a short text in space; the text takes the column's height when it needs it
    const page::ViewOut text = pagerun::Draw({f.W, H, x0, f.right, top, f.bottom - 0.03f * H});
    InfoColumns(f, x0, top, text.used);
    if (!g_pad)
    {
        ChipAt(0.425f * f.W, LaneY(), LaneChipH(), L"SHOW ON MAP", kChipMap);
        Mouse(f, L"WHEEL SCROLLS THE TEXT");
    }
}

// ---- the clicks (INVENTORY's are its page's: inventory_page.cpp Hit, its actions' table)
static void ChipClick(int chip)
{
    if (chip == kChipMap)
        nnwin::Go(nntabs::kCmdMap);
}

// ---- the frame
void Update(float dt, bool pad)
{
    g_pad = pad;
    g_dt = dt;
    Page page = Page::None;
    u32 root = 0;
    if (winplates::g_on && !winplates::g_scene && nnwin::OpenTab() >= 0 && mlogscreen::DeathShown()) // (the lookups only with a NeuroNet window open: issue 68) // (the log after a death or a failed mission: its own page, no tabs - the strip stands down)
    {
        root = RootOf(winplates::kFamLog);
        page = root ? Page::Death : Page::None;
    }
    else if (nnstrip::Wanted() && !uiclick::FindVisible("LoadAutosaveButton"))
    {
        const int cmd = nnwin::OpenCommand();
        root = cmd == nntabs::kCmdLog         ? RootOf(winplates::kFamLog)
               : cmd == nntabs::kCmdInventory ? RootOf(winplates::kFamInventory)
               : cmd == nntabs::kCmdStatus    ? RootOf(kFamStatus)
               : cmd == nntabs::kCmdInfo      ? pagerun::Claim(infopage::kId)
                                              : 0;
        if (root)
            page = cmd == nntabs::kCmdLog ? Page::Log : cmd == nntabs::kCmdInventory ? Page::Inventory : cmd == nntabs::kCmdStatus ? Page::Status : Page::Info;
    }
    if (page != g_page || root != g_root)
    {
        inventoryscreen::Reset(); // (INVENTORY's read at once, its list and selection anew, no deferred step: on every change of page)
        g_placesRead = 0;
    }
    const bool opened = page != g_page || root != g_root;
    pagerun::Keep(RuntimeId(page), root); // (INFO's window back when INFO went; a closed one is not found)
    g_page = page, g_root = root;
    inventoryscreen::Clock(dt);
    // the world stops in single player, in space, the map too; an autopilot under the window goes on flying (the owner, 5 Nov, issue 25)
    // (the status window's numbers and factions come from the in-process server, which the pause stops: the world goes on the moment until they are in;
    // on the open frame its page's model is the last open's until the runtime reads it, below: not in yet)
    const auto* status = page == Page::Status ? static_cast<const statusscreen::Model*>(pagerun::Model(statuspage::kId)) : nullptr;
    const bool waiting = page == Page::Status && (opened || !status || status->reps.empty() || status->stat[0].empty());
    // A press or click on a page may ask the game something (a jettison, a mount, a route) that the in-process server answers, and the pause stopped the server: the
    // request was lost (the map's route, Shockwave, 7 Nov 2026). The world stays paused; the host lets the server take the request on a frame with no time in it (input.cpp).
    {
        static bool s_down = false;
        const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && GetForegroundWindow() == fl::MainWindow();
        if (down && !s_down && nnstrip::Wanted())
            hostcalls::WorldCommand();
        s_down = down;
    }
    g_paused = nnstrip::Wanted() && hudrows::g.nnPause && !nnwin::Multiplayer() && !fl::PlayerBase() && !views::ApActive() && hostcalls::g.SetWorldPause && !waiting;
    if (g_paused)
        hostcalls::g.SetWorldPause();
    if (page == Page::None)
        return;
    if (page == Page::Inventory)
        HideTrade(); // (before the game's UI pass too: its label never shows a frame)
    // (the ship view pulses its selected part by the UI's blink clock, which the pause stops: the page runs it)
    if (g_paused && page == Page::Inventory && exe::UiBlinkPhase.ok())
    {
        const u32 fn = static_cast<u32>(exe::UiBlinkPhase.get()), clock = R32(fn + 2);
        const float period = RF(R32(fn + 8));
        if (period > 0)
        {
            float c = fmodf(RF(clock) + dt, period);
            u32 bits;
            memcpy(&bits, &c, 4);
            W32(clock, bits);
        }
    }
    if (hostcalls::g.SetFlightLock)
        hostcalls::g.SetFlightLock(); // (a full-screen window: the keys and the mouse fly nothing)
    int wheel = hostcalls::g.RealMouseWheel ? hostcalls::g.RealMouseWheel() : 0;
    if (page == Page::Status)
        pagerun::Frame(statuspage::kId, root, opened, wheel, dt, g_pad); // (its numbers every 0.5 s, at once on the open; the wheel; the card follows the focus)
    else if (page == Page::Inventory)
        pagerun::Frame(inventorypage::kId, root, opened, wheel, dt, g_pad); // (the game's selection follows a walk, its list the page's rows, its rows read every 0.5 s; the wheel)
    else if (page == Page::Info)
        pagerun::Frame(infopage::kId, root, opened, wheel, dt); // (its text read at the open and a new target, its window parked)
    else if (page == Page::Log || page == Page::Death)
    {
        pagerun::Frame(RuntimeId(page), root, opened, wheel, dt, g_pad); // (LOG: its objective each frame, its places every 2 s)
        if (const auto* log = page == Page::Log ? static_cast<const mlogscreen::Model*>(pagerun::Model(mlogpage::kId)) : nullptr)
            g_placesShown = log->places.size();
    }
    if (page == Page::Info && (g_placesRead -= dt) <= 0)
        screens::ReadPlaces(g_places, g_ships), g_placesRead = 2.0f, g_placesShown = g_places.size();
}

void Draw()
{
    if (basepage::On())
    {
        basepage::Draw();
        return;
    }
    if (g_paused) // (never a hidden state: the word stands where the eye looks for the time)
    {
        const Frame f = Bounds();
        Text(f.right, f.titleY + (g_page == Page::None && !g_pad ? 0.06f * f.H : 0), L"PAUSED", 0.030f * f.H, Argb(kDim, 1), Right); // (under the map's LAYERS chip)
    }
    if (g_page == Page::None)
    {
        // CHAT and GROUP (multiplayer): no family knows them, the game's window stands on the same ground under the same title
        const int cmd = nnstrip::Wanted() ? nnwin::OpenCommand() : 0;
        if (cmd == nntabs::kCmdChat || cmd == nntabs::kCmdGroup)
            Title(Bounds(), cmd == nntabs::kCmdChat ? L"CHAT" : L"GROUP");
        // Between two windows (the old one's close, the new one's open: about a second) no root of a page is shown yet, and the room, the NeuroNet box and the
        // windows' slides stood under the scrim (issue 62): the page's ground and title stand for the tab already chosen. Not on the death screen (its own page).
        // The map has no page: its gap is the windows' slides (the map grows to its own ground the frame they end, navnew Grow), and its title waits for the map,
        // which may open on another view than the player's system.
        else if ((cmd == nntabs::kCmdLog || cmd == nntabs::kCmdInventory || cmd == nntabs::kCmdStatus || cmd == nntabs::kCmdInfo ||
                  (cmd == nntabs::kCmdMap && room::NNWindowBusy())) &&
                 !uiclick::FindVisible("LoadAutosaveButton"))
        {
            const Frame f = Bounds();
            Ground(0, 0, f.W, f.H);
            PageLane(f, cmd != nntabs::kCmdMap); // (the map has no page: its ground only)
            const std::wstring ship = cmd == nntabs::kCmdInventory ? Upper(baseinfo::ShipName()) : std::wstring();
            if (cmd != nntabs::kCmdMap)
                Title(f, cmd == nntabs::kCmdLog ? L"NEURAL NET LOG" : cmd == nntabs::kCmdStatus ? L"PLAYER STATUS" : cmd == nntabs::kCmdInfo ? L"CURRENT INFORMATION" : ship.empty() ? L"INVENTORY" : ship);
        }
        return;
    }
    const Frame f = Bounds();
    PageLane(f);
    switch (g_page)
    {
    case Page::Status: DrawStatus(f); break; // (pages\dealers\status_view.cpp)
    case Page::Inventory: DrawInventory(f); break; // (pages\dealers\inventory_view.cpp)
    case Page::Log:
    case Page::Death: DrawJournal(f); break; // (pages\neuronet\mlog_view.cpp, death_view.cpp)
    case Page::Info: DrawInfo(f); break;
    default: break;
    }
}

void Under()
{
    if (g_page == Page::Inventory)
        HideTrade();
    // INVENTORY's rows' item models, between the game's row fill and the UI pass that draws them (nnpage_kit.h RowIcons; its view's game list, pagerun's); a
    // window gone since the page drew sends nothing and is forgotten (the docked pages' lists are their shell's: basepage::IconsUnder)
    if (g_page == Page::Inventory && g_root && uiclick::FindVisible(inventoryscreen::kWindowName) == g_root && !basepage::On())
        pagerun::IconsUnder();
    else if (!basepage::On())
        pagerun::DropLists();
    basepage::IconsUnder();
    LentSweep(); // (a lent model no page sent this frame: its mesh's true name back)
    // STATUS: the card's window brings its X along (PlayerInfoCloseButton), cut at the page's top right (Shockwave's shots, the owner, 7 Nov 2026): off the page,
    // which closes by its own CLOSE. So does the window's own title, PlayerInfoMainTitle (IDS 931 PLAYER STATUS: its STATUS stood cut by the top edge of the
    // card's hole); the page's title says it. The card's heading, the faction's name, is the card's own and stays (docs\FREELANCER_RE.md "Base windows").
    if (g_page == Page::Status)
        for (const char* n : {"PlayerInfoCloseButton", "PlayerInfoMainTitle"})
            PlaceAt(uiclick::FindVisible(n), -static_cast<float>(fl::ScreenWidth()), 0);
}

void DrawAfter()
{
    basepage::DrawAfter();
    const inventoryscreen::Kept& inv = inventoryscreen::State(); // (INVENTORY's view placed the ship's view; not while its jettison panel stands)
    if (g_page == Page::Inventory && inv.shipView && !inventoryscreen::JettisonOpen())
        hudvis::DrawShipAt(inv.shipRect[0], inv.shipRect[1], inv.shipRect[2], inv.shipRect[3], inventoryscreen::ShipViewPart());
}

bool On() { return g_page != Page::None || basepage::On(); }
bool InfoOwns() { return g_page == Page::Info; }

// What the rest of HUD 2 asks of these pages while they stand (pages\pagelist.h LegacyPage), until they move onto the
// runtime (audit 70b F4). The shell stands with a base page too (On), and draws it (Draw dispatches to basepage's). INFO
// is its own entry: its window is parked by the runtime, its controls off the screen.
static bool IsNeuroNetWindow(int fam)
{
    using namespace winplates;
    static const int kFamPlayerInfo = FamilyIndex("playerinfo");
    return fam == kFamInventory || fam == kFamLog || fam == kFamPlayerInfo || fam == kFamHelp;
}
static bool Always() { return true; }
static const pagelist::LegacyPage kShellEntry = {"neuronet", On, IsNeuroNetWindow, nullptr, Draw, DrawAfter, nullptr};
static const pagelist::LegacyPage kInfoEntry = {"neuronet.info", InfoOwns, nullptr, Always, nullptr, nullptr, nullptr};
LEGACY_PAGE_REGISTER(kShellEntry);
LEGACY_PAGE_REGISTER(kInfoEntry);

int Prompts(views::PagePrompt* out, int max)
{
    using namespace glyphs;
    if (basepage::On())
        return basepage::Prompts(out, max);
    switch (g_page)
    {
    case Page::Inventory: // (inventory_page.cpp: FILTER, ITEM, MOUNT, JETTISON, BEST PRICES, SCROLL CARD, the tag)
    case Page::Status:    // (status_page.cpp: FACTION, SHOW <the next filter>, the sort)
    case Page::Log:    // (mlog_page.cpp: its tabs, SCROLL, SHOW ON MAP)
    case Page::Death:  // (none: the pad's prompts there are padmenus')
    case Page::Info: return pagerun::Prompts(out, max); // (its SCROLL while the text is longer than the page, SHOW ON MAP)
    default: break;
    }
    return 0;
}

unsigned Pad(unsigned pressed, unsigned held, float dt)
{
    if (basepage::On())
        return basepage::Pad(pressed, held, dt);
    if (pressed & (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y))
        hostcalls::WorldCommand(); // (what the press asks of the game gets its frame: Update's note on the pause)
    // STATUS: the right stick walks the factions as the D-pad does - its card has no scroll of its own, the stick moved the game's list under the page (Shockwave, issue 68)
    if (g_page == Page::Status)
    {
        static unsigned rsWas = 0;
        const unsigned rs = (held & views::kPagePadRsUp) ? XINPUT_GAMEPAD_DPAD_UP : (held & views::kPagePadRsDown) ? XINPUT_GAMEPAD_DPAD_DOWN : 0;
        pressed |= rs & ~rsWas;
        held |= rs;
        rsWas = rs;
    }
    // a held D-pad walks a list: 0.3 s, then 20 rows a second (0.4 s / 12.5 a second: "could be a little faster", Shockwave, issue 68)
    static float repeat = 0;
    const unsigned walk = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN;
    if ((held & walk) && (g_page == Page::Status || g_page == Page::Inventory)) // (STATUS's card stands as long as the page: it blocked the repeat there, issue 68)
    {
        repeat += dt;
        if (repeat > 0.3f)
            pressed |= held & walk, repeat -= 0.05f;
    }
    else
        repeat = 0;
    switch (g_page)
    {
    case Page::Inventory: return inventoryscreen::JettisonOpen() ? 0 : pagerun::Pad(pressed, held, dt); // (inventory_page.cpp; the game's panel takes the pad)
    case Page::Status: // (status_page.cpp: the D-pad walks the factions, X the filter, Y the sort)
    case Page::Log:    // (mlog_page.cpp: the D-pad's sides the tabs, Y the map)
    case Page::Death:  // (nothing taken: the pad's binds there are padmenus')
    case Page::Info: return pagerun::Pad(pressed, held, dt); // (Y the map; its text a line a press, 15 lines a second held)
    default: return 0;
    }
}

void Shutdown()
{
    LentSweep(true); // (the game's own window must not keep a lent model under another name)
    pagerun::Shutdown(); // (the module going with INFO open: the game's window back on the screen)
    g_page = Page::None;
    g_root = 0;
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
        } kMap[] = {{"a", XINPUT_GAMEPAD_A}, {"b", XINPUT_GAMEPAD_B}, {"x", XINPUT_GAMEPAD_X}, {"y", XINPUT_GAMEPAD_Y}, {"up", XINPUT_GAMEPAD_DPAD_UP}, {"down", XINPUT_GAMEPAD_DPAD_DOWN}, {"left", XINPUT_GAMEPAD_DPAD_LEFT}, {"right", XINPUT_GAMEPAD_DPAD_RIGHT}};
        for (const auto& k : kMap)
            if (b == k.n)
                return "nnpage: took 0x" + std::to_string(Pad(k.m, 0, 0));
        return "nnpage press <a|b|x|y|up|down|left|right>";
    }
    if (what == "info")
    {
        // `nnpage info card <ids>`: the page's next open shows that infocard alone (test switch only: nnhelp_screen.h TestCard)
        std::string sub;
        if (in >> sub)
        {
            unsigned long ids = 0;
            if (sub == "card" && (in >> ids) && infobody::TestCard(static_cast<u32>(ids)))
                return "nnpage info card " + std::to_string(ids) + ": the next INFO open shows it, once";
            return "nnpage info card <ids> (a number; FLVIBED_TEST=1 only)";
        }
        // the INFO page's text as HUD 2 composes it now, and the game's own window's text when it is open (the comparison test)
        const u32 target = fl::PlayerBase() ? 0 : fl::TargetId();
        const infobody::Body mine = infobody::Compose(target);
        infobody::Body game;
        const bool open = infobody::ReadWindow(uiclick::FindVisible("NN_Help"), game); // (by name: the classic HUD's window too)
        std::ostringstream o;
        o << "nnpage info case=" << static_cast<int>(mine.kase) << " target=0x" << std::hex << target << std::dec << " visit=" << infobody::VisitByte(target ? target : fl::PlayerBase())
          << " window=" << open << " parked=" << pagerun::Parked() << " " << text::Utf8(infobody::Probe(target)) << "\n[HUD2]\n" << text::Utf8(infobody::Dump(mine, true));
        if (open)
            o << "\n[GAME]\n" << text::Utf8(infobody::Dump(game, true));
        const page::PageState* st = pagerun::State(infopage::kId);
        const auto* model = static_cast<const infobody::Model*>(pagerun::Model(infopage::kId));
        if (g_page == Page::Info && st && model) // (what the page shows now: composed at its open and at a new target)
        {
            const infobody::Body& shown = model->body;
            const std::vector<inforules::Line>& lines = infopage::Lines();
            o << "\n[PAGE] case=" << static_cast<int>(shown.kase) << " lines=" << lines.size() << " first=" << st->first << " room=" << st->room << "\n"
              << text::Utf8(infobody::Dump(shown, true));
            // the lines as laid out (the comparison test: each paragraph's words drawn in order with their styles, the market's grid):
            // per line its paragraph, 1 for a market item, then each piece's left and right edge (pixels in the column), its
            // attributes (8 hex digits, "h" for the heading style: what the view draws it with) and its text, tab-separated
            o << "\n[LINES] wrap=" << static_cast<int>(st->wrapW);
            for (const inforules::Line& l : lines)
            {
                o << "\n" << l.para << "\t" << (l.item ? 1 : 0);
                for (const inforules::Piece& p : l.pieces)
                {
                    char e[64];
                    sprintf_s(e, "\t%.1f\t%.1f\t%08X%s\t", p.x, p.x + p.w, p.attrs, p.heading ? "h" : "");
                    o << e << text::Utf8(p.text);
                }
            }
        }
        return o.str();
    }
    // STATUS's fields from its page's model while it stands (none of it elsewhere: its focus only, which outlives an open)
    const auto* st = g_page == Page::Status ? static_cast<const statusscreen::Model*>(pagerun::Model(statuspage::kId)) : nullptr;
    const List& reps = statusscreen::Kept().list;
    const size_t nReps = st ? st->reps.size() : 0;
    const int shown = st ? reps.count : 0;
    // INVENTORY's fields from its kept state on every page (its focus, items and loadout outlive an open, as they always did)
    const inventoryscreen::Kept& inv = inventoryscreen::State();
    std::ostringstream o;
    o << "nnpage page=" << static_cast<int>(g_page) << " root=0x" << std::hex << g_root << std::dec << " pad=" << g_pad << " reps=" << nReps << " shown=" << shown << " sel=" << reps.sel
      << " paused=" << g_paused << " card=" << (st && st->card) << " cardFor=" << (st && shown > 0 && reps.sel < static_cast<int>(st->shown.size()) ? static_cast<int>(st->shown[reps.sel].index == st->cardShown) : -1) << " syncPending=" << inv.syncPending << " places=" << g_placesShown << " guns=" << inv.load.guns << " hullps=" << inv.load.hull << " items=" << inv.items.size() << " itemSel=" << inv.list.sel;
    if (g_page == Page::Inventory && g_root)
        o << " filter=" << inventoryscreen::FilterOf(g_root) << " gameRow=" << static_cast<int>(R32(g_root + kInvRow)) << " tradeBlanked=" << g_tradeBlanked << " gameFirst=" << static_cast<int>(R32(g_root + kInvFirst));
    o << " shownRows=";
    int sentRows[kListMeshCount];
    const int nSent = g_page == Page::Inventory ? pagerun::ListShown(0, sentRows, kListMeshCount) : 0; // (the rows the last under-pass gave a mesh)
    for (int i = 0; i < nSent; ++i)
        o << (i ? "," : "") << sentRows[i];
    o << " iconSends=" << g_iconSends << " iconLoads=" << g_iconLoads << " iconCatchUps=" << g_iconCatchUps << " iconHidden=" << g_iconHidden; // (both pages: nnpage_kit.h RowIcons)
    int lent = 0;
    for (const LentModel& l : g_lent)
        lent += l.file != nullptr;
    o << " lent=" << lent;
    for (int i = 0; st && i < statusscreen::kStats; ++i)
        if (!st->stat[i].empty())
            o << " s" << i << "=" << st->stat[i].size();
    return o.str();
}
} // namespace nnpage
