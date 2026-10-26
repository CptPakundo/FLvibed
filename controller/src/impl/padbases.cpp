#include "padbases.h"

#include "../dimouse.h"
#include "../events.h"
#include "../fl.h"
#include "device.h"
#include "game/baseinfo.h"
#include "game/gamefont.h"
#include "hud2_kit.h"
#include "uistrip.h"
#include "game/render.h"
#include "game/room.h"
#include "game/uidraw.h"
#include "gamepad.h"
#include "glyphs.h"
#include "host.h" // HostSetCursorHidden (v20)
#include "log.h"
#include "options_page.h"
#include "osk.h"
#include "padmap.h"
#include "promptbar.h"
#include "game/menu.h"
#include "text.h"
#include "ui.h"
#include "uikit.h"
#include "uiopt.h"

#include <algorithm>
#include <sstream>

// (ASCII only: the build reads the sources as cp1252, so non-ASCII text goes in as \u escapes)
namespace padbases
{
using glyphs::Id;

// ---------------------------------------------------------------- the screens (by the game's visible windows)
// Control names verified in game 25 Oct 2026 (HDE 0.7.1, Manhattan; docs\FREELANCER_RE.md "Base windows").
enum class Screen
{
    None,
    Room,       // a base room: the NavBar's buttons (and the bar's characters)
    Offer,      // ScriptSequence_OfferDialog: OfferDialogAcceptButton / DeclineButton
    Jobs,       // NN_MissionVendor: MissionVendorSummaryBox0..4 (+ IconFrame), ...SummaryScroll, ...AcceptButton, ...CloseButton
    News,       // NN_News: NewsHeadlineBox0..3 (+ IconFrame), NewsCloseButton
    Commodity,  // NN_Dealer without filters: ItemButton1..5 / ItemView01..05 (#2 = the cargo list NN_Inventory),
                // Inventory_Scroll, QuantitySlider, BuySellTradeButton, Inventory_Close
    Equipment,  // NN_Dealer with Filter_Weapons / _Ammo / _Equipment / _CargoBay; the player's rows carry EquipToggleN
    ShipDealer, // NN_ShipTrader: ShipTraderShipNameFrame0..2 (+ IconFrame), ShipTraderBuyButton, ShipTraderCancelButton
    NnLog,      // NeuroNetStoryStar: MissionButton / LogButton, ScrollV_1, StoryStarCloseButton
    NnPlayer,   // NN_PlayerInfo: PlayerInfoFactionFrame0..5, PlayerInfoScroll, PlayerInfoCloseButton
    NnHelp,     // NN_Help: ScrollV_2, PlayerInfoCloseButton (the same name as the status window's)
    NnInventory, // NN_Inventory alone (in space): Inventory_Close
    Repair,      // NN_Repair (IDS_NN_REPAIR_YOUR_SHIP): RepairCheckBox0..8, RepairToggleChecks, RepairOkButton (only
                 // with something damaged), RepairCancelButton (research\evidence\controller_ui\hud2x_inventory.md 1.13)
    NavMap       // NeuroNetNavMap while docked: padmap owns it as in space (the room's NavBar stays visible under it)
};
static const char* ScreenName(Screen s)
{
    switch (s)
    {
    case Screen::Room: return "room";
    case Screen::Offer: return "offer";
    case Screen::Jobs: return "jobboard";
    case Screen::News: return "news";
    case Screen::Commodity: return "commodity";
    case Screen::Equipment: return "equipment";
    case Screen::ShipDealer: return "shipdealer";
    case Screen::NnLog: return "missionlog";
    case Screen::NnPlayer: return "playerinfo";
    case Screen::NnHelp: return "help";
    case Screen::NnInventory: return "inventory";
    case Screen::Repair: return "repair";
    case Screen::NavMap: return "navmap";
    default: return "none";
    }
}

struct Ring
{
    std::string label, control; // "Bar", "ButtonControl2"
    u32 ctl = 0;
    int number = 0;
};

static Where g_where = Where::Menu;
static Screen g_screen = Screen::None;
static float g_screenRefresh = 0;
static u32 g_owned = 0;          // XINPUT masks consumed this frame
static bool g_cursorShown = false; // the stick showed the cursor last (a D-pad / bumper press hides it)
static bool g_cursorBefore = false; // ... as it was before this frame's buttons
static float g_dt = 0;
static float g_release = 0; // a cursor snap in flight: release the drive after this
static u32 g_lastHovered = 0;
static float g_lastScrollPos = -1; // the dealers' list bar: a scroll under the focus re-selects the row
static bool g_padCursorNoted = false;
static bool g_arrowHidden = false; // the game's cursor arrow hidden through the host (HostApi v20 SetCursorHidden)
static bool g_modal = false;       // a modal window is over the base: this code stands down (ModalOpen)
// The frame's arrow decision (NoteFrame / Update) and Overlay's stand-down: overlay passes since the last pad frame.
static bool g_wantHidden = false;
static bool g_menuScreen = false; // padmenus has one of its screens up this frame
static int g_overlaysIdle = 99;

static void ApplyCursorHidden(bool hidden)
{
    if (hidden == g_arrowHidden)
        return;
    g_arrowHidden = hidden;
    HostSetCursorHidden(hidden);
}
// The pad cursor rule (UI_PLAN 6.2): the arrow hides while the pad cursor is hidden - a direct button hid it, or
// pad_cursor 0 allows no free cursor at all; the stick shows it again.
static bool ArrowRule();

// rooms
static std::vector<Ring> g_ring;
static int g_current = -1; // index into g_ring
static std::string g_lastGoto;
static room::Hotspot g_launch, g_news, g_jobs; // ctl 0 = absent
static std::string g_busy;
static float g_roomRefresh = 0;
static std::string g_pendingRoom; // a ring step while the NavBar is busy
static float g_pendingFor = 0;

// the room's characters, in screen order (left to right by the game's own pick box, baseinfo::CharacterScreenRect)
struct Char
{
    void* obj;
    std::string offer; // talk_story ...
    int rank;          // the offer's rank (0 = the story star)
    float sx;          // the box's centre x on screen (pixels), -1 unknown
    bool player;       // the player's own room character (Trent): talk_blowoff, a one-line name card (no faction)
};
static std::vector<Char> g_chars;
static std::vector<void*> g_charObjs; // the host's live list as last read (a change = another room / cast: re-read)
static int g_charFocus = -1;
static void* g_hovered = nullptr; // the character room::Hover is on
static float g_charRefresh = 0;
static float g_rehover = 0;
static bool g_conversation = false; // a character talks to the player / the room scene runs (InConversation)

// dealers / jobs / ships
static float g_aHeld = 0; // seconds A has been down (0 = up)
static bool g_aFired = false;
static float g_trigRepeat[2] = {}; // LT, RT repeat timers
static float g_trigHeld[2] = {};
static int g_filter = -1;
static int g_nnTab = -1;
static int g_qtyPos = -1, g_qtyMax = -1; // the quantity slider as last read (the quantity line)
static int g_side = 1;                   // 1 dealer, 2 the player's side (as last seen under the focus)
static float g_enterSnap = 0;            // a dealer just opened: put the focus on its first row (retried this long)
static float g_closeHeld = 0;            // B came during the window's open / close slide: clicked when it ends (s left)
constexpr float kCloseHoldSec = 2.0f;    // the slides take ~0.5 s each (measured at 190 fps and in vanilla 1.1)

static const char* kFilters[4] = {"Filter_Weapons", "Filter_Ammo", "Filter_Equipment", "Filter_CargoBay"};

// ---------------------------------------------------------------- helpers over the game's controls
static u32 Vis(const char* name) { return uiclick::FindVisible(name); }
static bool ClickName(const char* name, const char* why)
{
    const u32 ctl = Vis(name);
    const bool ok = ctl && uiclick::Click(ctl);
    events::Push("pad %s: %s %s", why, ok ? "clicked" : "no click on", name);
    return ok;
}
static bool Starts(const char* s, const char* prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }
static int TrailingNumber(const std::string& s)
{
    size_t i = s.size();
    while (i > 0 && isdigit(static_cast<u8>(s[i - 1])))
        --i;
    return i < s.size() ? atoi(s.c_str() + i) : 0;
}
// The window a dealer row belongs to, by the names up its parent chain: 1 the dealer's list (under the control named
// NN_Dealer), 2 the player's (the other list: its window is named NN_Inventory in the commodity trader and carries no
// visible name in the equipment dealer, 25 Oct 2026). 0 when `ctl` is no row control (`isRow` false) and not under the
// dealer's window.
static int SideOf(u32 ctl, bool isRow)
{
    __try
    {
        u32 c = ctl;
        for (int guard = 0; c && guard < 12; ++guard)
        {
            const char* n = ui::ControlName(c);
            if (strcmp(n, "NN_Dealer") == 0)
                return 1;
            if (strcmp(n, "NN_Inventory") == 0)
                return 2;
            c = Mem<u32>(c + 4);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return ctl && isRow ? 2 : 0;
}
// The control under the game's cursor by the game's own hit test (ui::HitTest), not the game's hovered pointer: out of
// focus (background tests) the game does not recompute its hover for the virtual cursor's moves (25 Oct 2026), the
// hit test answers for any position. This is "the focus": the generic D-pad navigation parks the cursor on a control.
static u32 Focused()
{
    return static_cast<u32>(ui::HitTest(static_cast<float>(fl::MouseX()), static_cast<float>(fl::MouseY())));
}
// A room character's hovered flag (+0x2F8: set by its hover-enter, FREELANCER_RE "Base rooms / characters").
static int HoverFlag(void* obj)
{
    __try
    {
        return Mem<u8>(reinterpret_cast<uintptr_t>(obj) + 0x2F8);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}
// Puts the focus (the game's cursor) on a control, as UpdateUiNav's SnapTo does.
static bool Snap(u32 ctl)
{
    if (!ctl)
        return false;
    // the control tree's rectangle, when the game's hit test agrees (the dealer window's player-side rows project
    // elsewhere in the tree walk: ItemView01#2 read (2476, 1541) while drawn at ~(1060, 650) at 4K, 25 Oct 2026);
    // else the hit-test probe's rectangle (the game's own answer for every screen cell)
    for (const ui::Item& it : ui::Collect())
        if (it.ctl == ctl && ui::HitTest(it.x, it.y) == ctl)
        {
            dimouse::MoveTo(it.x, it.y);
            g_release = 0.12f;
            return true;
        }
    // (PERF 25 Oct 2026: the shared probe result, ui::CollectAllCached - a fresh ui::Probe here cost up to 5.6 ms a call
    // and the dealer's entry snap retried it every frame for a second)
    for (const ui::Item& it : ui::CollectAllCached())
        if (it.ctl == ctl)
        {
            dimouse::MoveTo(it.x, it.y);
            g_release = 0.12f;
            return true;
        }
    return false;
}
// A row of a list window (walking rows selects them: the game's transfer panel / description follows the focus).
static bool IsRow(const char* n)
{
    return Starts(n, "ItemButton") || Starts(n, "ItemView") || Starts(n, "MissionVendorSummaryBox") ||
           Starts(n, "MissionVendorIconFrame") || Starts(n, "NewsHeadlineBox") || Starts(n, "NewsHeadlineIconFrame") ||
           Starts(n, "ShipTraderShipNameFrame") || Starts(n, "ShipTraderShipNameIconFrame") ||
           Starts(n, "PlayerInfoFactionFrame");
}

// A modal window over the base or the NeuroNet window, the game's or FLvibed's: the pause menu (NN_Pause), its
// confirm (NN_Confirm), the in-game load / save screens, the death dialog, the story popup (MMOkButton), the options
// page, the OSK and the New Game prompt. While one is up this code stands down: it owns no button, draws nothing and
// hides no cursor (release acceptance 25 Oct 2026: with the pause menu open in a bar the character strip drew under
// the menu and A talked to the focused character while the game was paused). The offer dialog is this code's own
// screen (Screen::Offer), not a modal. The game's windows by their verified control names (FREELANCER_RE "Main menu,
// load screens, pause menu"; padmenus.cpp Detect uses the same), looked up with the screen refresh (10 Hz).
static bool ModalOpen()
{
    if (menu::PauseMenu())
        return true;
    // (the game's Options screen from the pause menu: the pause menu is hidden behind it - fix track 25 Oct 2026, the
    // character strip drew over the Options page in both ui_style values; its page buttons and close by name)
    for (const char* n : {"Confirm_delBtnYes", "LoadSave_LoadGame", "LoadSave_SaveGame", "LoadAutosaveButton", "MMOkButton", "MMYesButton", "MMNoButton", "MMLaterButton",
                          "PreferencesCloseButton", "VideoButton", "keymapButton"})
        if (Vis(n))
            return true;
    return false;
}
// FLvibed's own modals, per frame (cheap flags).
static bool OwnModalOpen() { return options_page::WantsInput() || osk::IsOpen(); }

static Screen Detect(Where where)
{
    // (the rumour / info window is the same dialog with OfferDialogClose2Button alone - CLOSE; verified 25 Oct 2026, a
    // Manhattan bar rumour: A on the generic focus did not close it out of focus, the game's hover is not recomputed)
    if (Vis("OfferDialogAcceptButton") || Vis("OfferDialogClose2Button"))
        return Screen::Offer;
    // (the repair window and the docked nav map lie over the room with its NavBar still visible: before the room)
    if (Vis("NN_Repair"))
        return Screen::Repair;
    if (Vis("NN_MissionVendor"))
        return Screen::Jobs;
    if (Vis("NN_News"))
        return Screen::News;
    if (Vis("NN_ShipTrader"))
        return Screen::ShipDealer;
    if (Vis("NN_Dealer"))
        return Vis("Filter_Weapons") ? Screen::Equipment : Screen::Commodity;
    if (Vis("NeuroNetStoryStar"))
        return Screen::NnLog;
    if (Vis("NN_PlayerInfo"))
        return Screen::NnPlayer;
    if (Vis("NN_Help"))
        return Screen::NnHelp;
    if (Vis("NN_Inventory"))
        return Screen::NnInventory;
    if (where == Where::Docked && Vis("NeuroNetNavMap"))
        return Screen::NavMap;
    if (where == Where::Docked && Vis("NavBar"))
        return Screen::Room;
    return Screen::None;
}

// ---------------------------------------------------------------- rooms
static int RankOf(const std::string& offer)
{
    static const char* order[] = {"talk_story", "talk_mission", "talk_rumor", "talk_bribe", "talk_info", "talk_blowoff"};
    for (int i = 0; i < 6; ++i)
        if (offer == order[i])
            return i;
    return offer.find("dealer") != std::string::npos ? 6 : 7;
}
// The offer word the name card carries: only what the character offers (the story character is the yellow star, a
// blowoff and the dealers offer nothing to name).
static const wchar_t* OfferWord(int rank)
{
    switch (rank)
    {
    case 1: return L"JOB";
    case 2: return L"RUMOUR";
    case 3: return L"BRIBE";
    case 4: return L"INFO";
    default: return nullptr;
    }
}

static int RingIndex(const std::string& label);

static void RefreshRoom()
{
    const std::vector<room::Hotspot> hs = room::Hotspots();
    g_busy = room::NavBusy();
    g_ring.clear();
    g_launch = g_news = g_jobs = room::Hotspot();
    std::string derived;
    for (const room::Hotspot& h : hs)
    {
        if (h.behaviour == 1 && h.ctl)
            g_ring.push_back({h.label, h.control, h.ctl, TrailingNumber(h.control)});
        else if (h.label == "IDS_HOTSPOT_LAUNCH" && h.ctl)
            g_launch = h;
        else if (h.behaviour == 2 && h.ctl)
            g_news = h, derived = "Bar";
        else if (h.behaviour == 3 && h.ctl)
            g_jobs = h, derived = "Bar";
        else if (h.behaviour == 5)
            derived = "Trader";
        else if (h.behaviour == 8)
            derived = "Equipment";
        else if (h.behaviour == 7)
            derived = "ShipDealer";
    }
    std::sort(g_ring.begin(), g_ring.end(), [](const Ring& a, const Ring& b) { return a.number < b.number; });
    // the current room: the one the room's own action hotspots name; else the launch area (Cityscape / Planetscape /
    // Deck: the room with the ship) when a launch button or no dealer is here; the last room we went to breaks a tie
    auto find = [&](const std::string& label) { return RingIndex(label); };
    int cur = derived.empty() ? -1 : find(derived);
    if (cur < 0 && !g_lastGoto.empty())
        cur = find(g_lastGoto);
    if (cur < 0)
        for (const char* area : {"Cityscape", "Planetscape", "Deck"})
            if (cur < 0)
                cur = find(area);
    if (cur < 0 && !g_ring.empty())
        cur = 0;
    g_current = cur;
}

static int RingIndex(const std::string& label)
{
    for (size_t i = 0; i < g_ring.size(); ++i)
        if (_stricmp(g_ring[i].label.c_str(), label.c_str()) == 0)
            return static_cast<int>(i);
    return -1;
}

static void StepRoom(int dir)
{
    if (g_ring.size() < 2)
        return;
    // from the room we are going to when a step is queued or a transition runs (a second press during the entry
    // transition stepped from the room left behind, 25 Oct 2026), else from the room we are in
    int from = -1;
    if (!g_pendingRoom.empty())
        from = RingIndex(g_pendingRoom);
    if (from < 0 && !g_busy.empty() && !g_lastGoto.empty())
        from = RingIndex(g_lastGoto);
    if (from < 0)
        from = g_current;
    if (from < 0)
        return;
    const Ring& to = g_ring[(from + dir + static_cast<int>(g_ring.size())) % g_ring.size()];
    if (!g_busy.empty())
    {
        g_pendingRoom = to.label; // the NavBar swallows clicks while busy: step when it is ready
        g_pendingFor = 0;
        events::Push("pad room: %s queued (busy: %s)", to.label.c_str(), g_busy.c_str());
        return;
    }
    std::string what;
    if (room::Goto(to.label, &what))
        g_lastGoto = to.label, g_current = -1;
    events::Push("pad room: %s -> %s", to.label.c_str(), what.c_str());
}

// The host's live room characters (the ctor / dtor detours; HostApi v5) - the cheap read, no hover.
static std::vector<void*> HostChars()
{
    std::vector<void*> out;
    if (!HostAtLeast(5) || !g_host->GetRoomCharacters)
        return out;
    void* objs[128];
    const int n = g_host->GetRoomCharacters(objs, 128);
    out.assign(objs, objs + std::max(0, std::min(n, 128)));
    return out;
}

// A character's place on screen: its pick box's centre x (pixels); -1 unknown; -2 the whole box off the screen.
static float ScreenX(void* obj)
{
    float x, y, w, h;
    if (!baseinfo::CharacterScreenRect(obj, x, y, w, h))
        return -1;
    const float W = static_cast<float>(fl::ScreenWidth());
    if (W > 0 && (x + w < 0 || x > W))
        return -2;
    return x + w * 0.5f;
}

static void* FocusObj()
{
    return g_charFocus >= 0 && g_charFocus < static_cast<int>(g_chars.size()) ? g_chars[g_charFocus].obj : nullptr;
}

// Screen order, left to right (unknown places last, by offer); the focus stays on its character.
static void SortChars()
{
    void* focusObj = FocusObj();
    std::stable_sort(g_chars.begin(), g_chars.end(),
                     [](const Char& a, const Char& b)
                     {
                         const bool ka = a.sx >= 0, kb = b.sx >= 0;
                         if (ka != kb)
                             return ka;
                         return ka ? a.sx < b.sx : a.rank < b.rank;
                     });
    g_charFocus = -1;
    for (size_t i = 0; i < g_chars.size(); ++i)
        if (g_chars[i].obj == focusObj)
            g_charFocus = static_cast<int>(i);
}

// Reads the cast with its offers (room::List hover-enters and leaves each: our hover is redone by HoverFocus) - on a new
// room / cast and after a conversation (an offer can change: a job taken). The focus stays on its character; on a new
// cast it starts on the player's own character (Trent: the owner, 25 Oct 2026 - he stays in the walk and is the default
// focus), else on the story character (the star) if there is one, else on the leftmost.
// The player's own character: the game's name card holds his name alone (no faction / role line, FREELANCER_RE "Room
// characters' name card") and he offers only the blowoff - no name compare (every language).
static bool IsPlayerChar(void* obj, const std::string& offer)
{
    baseinfo::Card card;
    return offer == "talk_blowoff" && baseinfo::CharacterCard(obj, card) && card.faction.empty() && !card.name.empty();
}
static void RefreshChars()
{
    const std::vector<room::Character> list = room::List();
    void* focusObj = FocusObj();
    g_chars.clear();
    for (const room::Character& c : list)
    {
        const float sx = ScreenX(c.obj);
        if (sx == -2)
            continue; // off the screen: the player cannot see (or point at) it either
        g_chars.push_back({c.obj, c.offer, RankOf(c.offer), sx, IsPlayerChar(c.obj, c.offer)});
    }
    g_charFocus = -1; // (SortChars keeps a focus only when it is set)
    SortChars();
    for (size_t i = 0; i < g_chars.size(); ++i)
        if (g_chars[i].obj == focusObj)
            g_charFocus = static_cast<int>(i);
    for (size_t i = 0; i < g_chars.size() && g_charFocus < 0; ++i)
        if (g_chars[i].player)
            g_charFocus = static_cast<int>(i);
    if (g_charFocus < 0)
        for (size_t i = 0; i < g_chars.size(); ++i)
            if (g_chars[i].rank == 0)
            {
                g_charFocus = static_cast<int>(i);
                break;
            }
    if (g_charFocus < 0 && !g_chars.empty())
        g_charFocus = 0;
    g_hovered = nullptr; // List() left every character unhovered
    std::string order;
    for (const Char& c : g_chars)
        order += (order.empty() ? "" : ",") + std::to_string(static_cast<int>(c.sx)) + ":" +
                 c.offer.substr(c.offer.find('_') + 1) + (c.player ? "(player)" : "");
    Log("pad room: %d characters in screen order [%s], focus %d", static_cast<int>(g_chars.size()), order.c_str(), g_charFocus);
}

// The camera moved (a room's camera track, another view): re-sort when two neighbours have clearly swapped places
// (more than 3 % of the screen height: idle animations never flip the order back and forth).
static void ResortIfMoved()
{
    const float tol = static_cast<float>(fl::ScreenHeight()) * 0.03f;
    bool swapped = false;
    for (Char& c : g_chars)
        c.sx = ScreenX(c.obj);
    for (size_t i = 0; i + 1 < g_chars.size(); ++i)
        if (g_chars[i].sx >= 0 && g_chars[i + 1].sx >= 0 && g_chars[i].sx > g_chars[i + 1].sx + tol)
            swapped = true;
    if (swapped)
    {
        SortChars();
        Log("pad room: characters re-sorted (camera), focus %d", g_charFocus);
    }
}

static bool Talking()
{
    if (g_busy == "scene")
        return true;
    for (void* o : g_charObjs) // (the whole cast: the partner may stand off the screen)
        if (baseinfo::CharacterTalking(o))
            return true;
    return false;
}

static void HoverFocus()
{
    void* want = g_charFocus >= 0 && g_charFocus < static_cast<int>(g_chars.size()) && !g_cursorShown
                     ? g_chars[g_charFocus].obj
                     : nullptr;
    if (want == g_hovered && g_rehover > 0)
        return;
    if (g_hovered && g_hovered != want)
        room::Unhover(g_hovered);
    g_hovered = want;
    if (want)
        room::Hover(want);
    g_rehover = 0.5f; // the game's own picking may clear it: re-assert twice a second
}

static void LeaveChars()
{
    if (g_hovered)
        room::Unhover(g_hovered);
    g_hovered = nullptr;
}

// ---------------------------------------------------------------- the frame
// The screen's prompts (UI_SPEC.json `prompts`, confirm first, at most five) into the shared prompt bar (promptbar.*,
// D-MENUS's component: a decision pending keeps it up under CONTEXTUAL).
static void Prompts(const Gamepad& pad)
{
    using promptbar::Prompt;
    Prompt p[6];
    int n = 0;
    auto add = [&](Id b, const wchar_t* label, bool hold = false, Id b2 = Id::COUNT, float progress = 0)
    {
        if (n < 5)
            p[n++] = {b, label, hold, b2, progress};
    };
    bool decision = false;
    const float aProgress = g_aHeld > 0 ? std::min(1.0f, g_aHeld / 0.6f) : 0.0f;
    switch (g_screen)
    {
    case Screen::Room:
        if (g_conversation)
            break; // a conversation: no room prompts (they come back when it ends)
        if (!g_chars.empty())
        {
            add(Id::DL, L"CHARACTER", false, Id::DR);
            // (the player's own character, Trent: the game's click on him starts nothing - no conversation, no line;
            // verified 25 Oct 2026, critique 4 - so no A pill while he holds the focus)
            const bool onPlayer = !g_cursorShown && g_charFocus >= 0 && g_charFocus < static_cast<int>(g_chars.size()) &&
                                  g_chars[g_charFocus].player;
            if (!onPlayer)
                add(Id::A, g_cursorShown ? L"SELECT" : L"TALK");
            // Y as the press below acts: the room's launch hotspot first (Trent on the launch pad is a character, so
            // the launch area has a strip too - Y LAUNCH was missing there, 25 Oct 2026), else the job board
            if (g_launch.ctl)
                add(Id::Y, L"LAUNCH");
            else if (g_jobs.ctl)
                add(Id::Y, L"JOB BOARD");
            if (g_news.ctl)
                add(Id::X, L"NEWS");
            add(Id::LB, L"ROOM", false, Id::RB);
        }
        else
        {
            add(Id::LB, L"ROOM", false, Id::RB);
            if (g_launch.ctl)
                add(Id::Y, L"LAUNCH");
            else if (g_jobs.ctl)
                add(Id::Y, L"JOB BOARD");
            if (g_news.ctl)
                add(Id::X, L"NEWS");
            add(Id::A, L"SELECT");
            if (CursorAllowed())
                add(Id::LS, L"CURSOR");
        }
        break;
    case Screen::Offer:
        if (!Vis("OfferDialogAcceptButton"))
        {
            // a rumour / info: nothing to decide - A and B both close it (every menu shows A and B: docs\UI_HUD2_EXTENDED.md 13)
            add(Id::A, L"OK");
            add(Id::B, L"CLOSE");
            break;
        }
        add(Id::A, L"ACCEPT");
        add(Id::B, L"DECLINE");
        decision = true;
        break;
    case Screen::Jobs:
        add(Id::DU, L"JOB", false, Id::DD);
        add(Id::A, L"ACCEPT", true, Id::COUNT, aProgress);
        add(Id::RS, L"SCROLL");
        add(Id::B, L"CLOSE");
        break;
    case Screen::News:
        add(Id::DU, L"STORY", false, Id::DD);
        add(Id::A, L"SELECT"); // (the generic A: the game's click on the focused control)
        add(Id::RS, L"SCROLL");
        add(Id::B, L"CLOSE");
        break;
    case Screen::Commodity:
    case Screen::Equipment:
    {
        const bool eq = g_screen == Screen::Equipment;
        const bool buy = Vis("BuySellTradeButton") != 0;
        // (the spec lists six prompts here; the bar takes five: the D-pad's ITEM, the obvious one, is left out)
        // (the list side: short, so HUD 2's pills fit the column right of the description at 16:9 - promptbar rule 4)
        add(Id::LB, L"SIDE", false, Id::RB);
        if (eq)
            add(Id::X, L"FILTER");
        else
            add(Id::LT, L"QUANTITY", false, Id::RT);
        add(Id::A, eq ? (g_side == 2 ? L"MOUNT" : L"BUY") : (g_side == 2 ? L"SELL" : L"BUY"));
        add(Id::Y, L"MAX");
        add(Id::B, L"CLOSE");
        decision = buy;
        break;
    }
    case Screen::ShipDealer:
        add(Id::DU, L"SHIP", false, Id::DD);
        add(Id::RS, L"STATS");
        add(Id::A, L"BUY", true, Id::COUNT, aProgress);
        add(Id::B, L"CLOSE");
        decision = Vis("ShipTraderBuyButton") != 0;
        break;
    case Screen::NnLog:
        add(Id::LB, L"MISSION / LOG", false, Id::RB);
        add(Id::A, L"SELECT");
        add(Id::RS, L"SCROLL");
        add(Id::B, L"CLOSE");
        break;
    case Screen::NnPlayer:
        add(Id::DU, L"FACTION", false, Id::DD);
        add(Id::A, L"SELECT");
        add(Id::RS, L"SCROLL");
        add(Id::B, L"CLOSE");
        break;
    case Screen::NnHelp:
        add(Id::A, L"SELECT");
        add(Id::RS, L"SCROLL");
        add(Id::B, L"CLOSE");
        break;
    case Screen::NnInventory:
        add(Id::DU, L"ITEM", false, Id::DD);
        add(Id::A, L"USE / MOUNT");
        add(Id::RS, L"INFO");
        add(Id::B, L"CLOSE");
        break;
    case Screen::Repair:
    {
        // A = the game's click on the focused control (the generic focus): its label follows the focus
        const char* f = ui::ControlName(Focused());
        const bool ok = Vis("RepairOkButton") != 0;
        add(Id::A, strcmp(f, "RepairOkButton") == 0       ? L"REPAIR"
                   : strcmp(f, "RepairCancelButton") == 0 ? L"CANCEL"
                   : Starts(f, "RepairCheckBox") || strcmp(f, "RepairToggleChecks") == 0 ? L"TICK"
                                                                                         : L"SELECT");
        if (ok)
            add(Id::Y, L"REPAIR");
        add(Id::X, L"TICK ALL");
        add(Id::DU, L"ITEM", false, Id::DD);
        add(Id::B, L"CANCEL");
        decision = ok;
        break;
    }
    default: break;
    }
    (void)pad;
    if (n)
        promptbar::Set(ScreenName(g_screen), p, n, decision);
}

// The close button of the open window (B): verified names; empty = none (B falls through to the game's Escape).
static const char* CloseName()
{
    switch (g_screen)
    {
    case Screen::Jobs: return "MissionVendorCloseButton";
    case Screen::News: return "NewsCloseButton";
    case Screen::Commodity:
    case Screen::Equipment:
    case Screen::NnInventory: return "Inventory_Close";
    case Screen::ShipDealer: return "ShipTraderCancelButton";
    case Screen::NnLog: return "StoryStarCloseButton";
    case Screen::NnPlayer:
    case Screen::NnHelp: return "PlayerInfoCloseButton";
    case Screen::Repair: return "RepairCancelButton";
    default: return "";
    }
}

static void ReadQuantity()
{
    g_qtyPos = g_qtyMax = -1;
    const u32 slider = Vis("QuantitySlider");
    if (!slider)
        return;
    float lo = 0, hi = 0;
    const float pos = uiclick::Scroll(slider, 0, false, &lo, &hi);
    if (pos < 0)
        return;
    g_qtyPos = static_cast<int>(pos + 0.5f);
    g_qtyMax = static_cast<int>(hi - 1 + 0.5f); // the range's top = n + 1 page (1); its top position = what fits
}

static void Quantity(int delta, const char* why)
{
    const u32 slider = Vis("QuantitySlider");
    if (!slider)
        return;
    const float pos = uiclick::Scroll(slider, static_cast<float>(delta), false, nullptr, nullptr);
    if (why)
        events::Push("pad dealer: %s quantity -> %.0f", why, pos);
}

// The first row of a side's list (ItemView01 of the dealer's window / of the player's), 0 when that list has no rows.
static u32 FirstRow(int side)
{
    for (int n = 1; n <= 2; ++n)
    {
        char name[16];
        sprintf_s(name, "ItemView01#%d", n);
        const u32 ctl = Vis(name);
        if (ctl && SideOf(ctl, true) == side)
            return ctl;
    }
    return 0;
}

static void SwitchSide()
{
    const int to = g_side == 1 ? 2 : 1;
    const u32 ctl = FirstRow(to);
    if (ctl && Snap(ctl))
    {
        g_side = to;
        events::Push("pad dealer: side -> %s", to == 1 ? "dealer" : "yours");
        return;
    }
    events::Push("pad dealer: no rows on the %s side", to == 1 ? "dealer" : "your");
}

// A control of the open window the pad may click by A (never a room button behind it: the focus can rest on the NavBar
// when a window opens without a root of its own - the dealers' lists hang under the NeuroNet's root).
static bool IsWindowControl(const char* n)
{
    return IsRow(n) || Starts(n, "Filter_") || Starts(n, "Inventory_") || Starts(n, "BuySell") || Starts(n, "EquipToggle") ||
           Starts(n, "ShipTrader") || Starts(n, "MissionVendor") || Starts(n, "News") || Starts(n, "StoryStar") ||
           Starts(n, "PlayerInfo") || Starts(n, "MissionButton") || Starts(n, "LogButton") || Starts(n, "ShipView_");
}

// The repair window's focus ring, top to bottom: the damaged items' check boxes, the tick-all box, OK (only with
// something damaged), Cancel - the clickable ones as the control tree has them now (the generic D-pad navigation
// left the window for the room's NavBar: Up from the tick-all box went to ButtonControl1, 25 Oct 2026).
static std::vector<u32> RepairRing()
{
    std::vector<std::pair<int, u32>> order;
    for (const ui::Item& it : ui::Collect())
    {
        const char* n = it.name;
        int rank = -1;
        if (Starts(n, "RepairCheckBox") && !Starts(n, "RepairCheckBoxFrame"))
            rank = TrailingNumber(n);
        else if (strcmp(n, "RepairToggleChecks") == 0)
            rank = 100;
        else if (strcmp(n, "RepairOkButton") == 0)
            rank = 101;
        else if (strcmp(n, "RepairCancelButton") == 0)
            rank = 102;
        if (rank >= 0)
            order.push_back({rank, static_cast<u32>(it.ctl)});
    }
    std::sort(order.begin(), order.end());
    std::vector<u32> ring;
    for (const auto& o : order)
        ring.push_back(o.second);
    return ring;
}
static void StepRepair(int dir)
{
    const std::vector<u32> ring = RepairRing();
    if (ring.empty())
        return;
    const u32 cur = Focused();
    int i = -1;
    for (size_t k = 0; k < ring.size(); ++k)
        if (ring[k] == cur)
            i = static_cast<int>(k);
    const int n = static_cast<int>(ring.size());
    const int to = i < 0 ? (dir > 0 ? 0 : n - 1) : (i + dir + n) % n;
    if (Snap(ring[to]))
        events::Push("pad repair: focus %s", ui::ControlName(ring[to]));
}

static void CycleFilter()
{
    for (int i = 1; i <= 4; ++i)
    {
        const int f = (g_filter + i) % 4;
        if (Vis(kFilters[f]))
        {
            if (ClickName(kFilters[f], "filter"))
                g_filter = f;
            return;
        }
    }
}

static void UpdateRoom(const Gamepad& pad)
{
    g_roomRefresh -= g_dt;
    if (g_roomRefresh <= 0)
    {
        RefreshRoom();
        g_roomRefresh = 0.25f;
    }
    g_charRefresh -= g_dt;
    g_rehover -= g_dt;
    if (g_charRefresh <= 0)
    {
        // the cast by the host's list (cheap); a new cast -> its offers and screen order, a camera move -> re-sorted,
        // a conversation's end -> the offers again (one may have changed)
        g_charRefresh = 0.25f;
        std::vector<void*> objs = HostChars();
        std::sort(objs.begin(), objs.end());
        const bool newCast = objs != g_charObjs;
        g_charObjs = objs;
        const bool was = g_conversation;
        g_conversation = Talking();
        if (was != g_conversation)
            Log("pad room: conversation %s", g_conversation ? "begins: the room layer stands down" : "ends");
        static bool stale = false; // a new cast seen during a conversation: read once it ends
        stale |= newCast;
        if (!g_conversation && (stale || was))
            RefreshChars(), stale = false;
        else if (!g_conversation)
            ResortIfMoved();
    }
    // a queued ring step once the NavBar is ready
    if (!g_pendingRoom.empty())
    {
        g_pendingFor += g_dt;
        if (g_busy.empty())
        {
            std::string what;
            if (room::Goto(g_pendingRoom, &what))
                g_lastGoto = g_pendingRoom, g_current = -1;
            events::Push("pad room: queued %s -> %s", g_pendingRoom.c_str(), what.c_str());
            g_pendingRoom.clear();
        }
        else if (g_pendingFor > 15)
            g_pendingRoom.clear();
    }
    const bool strip = !g_chars.empty();
    g_owned |= XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER;
    if (g_launch.ctl || g_jobs.ctl)
        g_owned |= XINPUT_GAMEPAD_Y;
    if (g_news.ctl)
        g_owned |= XINPUT_GAMEPAD_X;
    if (strip && !g_conversation) // (in a conversation the game's own windows - the rumour's CLOSE - take the pad's focus)
    {
        g_owned |= XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
        if (!g_cursorShown)
            g_owned |= XINPUT_GAMEPAD_A;
    }
    if (pad.Pressed(XINPUT_GAMEPAD_LEFT_SHOULDER))
        StepRoom(-1);
    if (pad.Pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER))
        StepRoom(+1);
    if (pad.Pressed(XINPUT_GAMEPAD_Y))
    {
        const room::Hotspot& h = g_launch.ctl ? g_launch : g_jobs;
        if (h.ctl)
        {
            if (!g_busy.empty())
                events::Push("pad room: %s refused (busy: %s)", g_launch.ctl ? "launch" : "job board", g_busy.c_str());
            else
            {
                const bool ok = uiclick::Click(h.ctl);
                events::Push("pad room: %s %s (%s)", g_launch.ctl ? "launch" : "job board", ok ? "clicked" : "no click",
                             h.control.c_str());
            }
        }
    }
    if (pad.Pressed(XINPUT_GAMEPAD_X) && g_news.ctl)
    {
        if (!g_busy.empty())
            events::Push("pad room: news refused (busy: %s)", g_busy.c_str());
        else
            events::Push("pad room: news %s (%s)", uiclick::Click(g_news.ctl) ? "clicked" : "no click", g_news.control.c_str());
    }
    if (strip && g_conversation)
        LeaveChars(); // (no talk, no focus walk mid-conversation: the buttons are left to the game's windows)
    else if (strip)
    {
        const int n = static_cast<int>(g_chars.size());
        const bool left = pad.Pressed(XINPUT_GAMEPAD_DPAD_LEFT), right = pad.Pressed(XINPUT_GAMEPAD_DPAD_RIGHT);
        if (left || right)
        {
            // from the character the pad cursor (or the game's own picking) hovers, when it is not the focus
            for (int i = 0; i < n; ++i)
                if (g_cursorBefore && g_chars[i].obj != g_hovered && HoverFlag(g_chars[i].obj) == 1)
                {
                    g_charFocus = i;
                    break;
                }
            if (g_charFocus < 0)
                g_charFocus = 0;
            else
                g_charFocus = (g_charFocus + (right ? 1 : -1) + n) % n;
            events::Push("pad room: character %d/%d %s", g_charFocus + 1, n, g_chars[g_charFocus].offer.c_str());
        }
        HoverFocus();
        if (pad.Pressed(XINPUT_GAMEPAD_A) && !g_cursorShown && g_charFocus >= 0 && !g_chars[g_charFocus].player)
        {
            const Char& c = g_chars[g_charFocus];
            const bool ok = room::Talk(c.obj);
            events::Push("pad bar: talk %d %s -> %s", g_charFocus, c.offer.c_str(), ok ? "ok" : "failed");
        }
    }
    else
        LeaveChars();
}

static void UpdateWindow(const Gamepad& pad)
{
    LeaveChars();
    const u32 hovered = Focused();
    char hname[33];
    strcpy_s(hname, ui::ControlName(hovered));
    const bool dealer = g_screen == Screen::Commodity || g_screen == Screen::Equipment;
    // walking rows selects them (the game's own click on the row): the description / transfer panel follows the focus
    bool reselect = false;
    if (hovered != g_lastHovered)
    {
        g_lastHovered = hovered;
        reselect = !g_cursorShown && IsRow(hname);
    }
    // The list windows whose scroll bar the generic navigation does not see (MissionVendorSummaryScroll is not in its
    // item set; the dealers' Inventory_Scroll is, and UpdateUiNav scrolls it itself): up / down on the list's edge row
    // scrolls the bar one row natively and the row is re-selected (it shows the next item now); when the bar does not
    // move, the press is left to the generic navigation.
    {
        const char* bar = g_screen == Screen::Jobs ? "MissionVendorSummaryScroll" : g_screen == Screen::NnPlayer ? "PlayerInfoScroll" : "";
        const int rows = g_screen == Screen::Jobs ? 5 : 6; // the visible rows (0-based names)
        if (*bar && IsRow(hname) && !g_cursorShown)
        {
            const int row = TrailingNumber(hname);
            const bool down = pad.Pressed(XINPUT_GAMEPAD_DPAD_DOWN), up = pad.Pressed(XINPUT_GAMEPAD_DPAD_UP);
            if ((down && row == rows - 1) || (up && row == 0))
            {
                if (const u32 b = Vis(bar))
                {
                    float lo, hi;
                    const float before = uiclick::Scroll(b, 0, false, &lo, &hi);
                    if (uiclick::Scroll(b, down ? 1.0f : -1.0f, false, nullptr, nullptr) != before)
                    {
                        g_owned |= down ? XINPUT_GAMEPAD_DPAD_DOWN : XINPUT_GAMEPAD_DPAD_UP;
                        reselect = true;
                        events::Push("pad %s: list scrolled %s", ScreenName(g_screen), down ? "down" : "up");
                    }
                }
            }
        }
    }
    if (dealer)
    {
        // the side = the list whose row the focus is on (the transfer panel and the slider belong to the dealer's
        // window: a focus there must not flip the side, 25 Oct 2026)
        const int side = IsRow(hname) ? SideOf(hovered, true) : 0;
        if (side)
            g_side = side;
        if (const u32 bar = Vis("Inventory_Scroll"))
        {
            float lo, hi;
            const float pos = uiclick::Scroll(bar, 0, false, &lo, &hi);
            if (pos != g_lastScrollPos && g_lastScrollPos >= 0 && !g_cursorShown && IsRow(hname))
                reselect = true; // the list scrolled under the focused row: the row shows another item now
            g_lastScrollPos = pos;
        }
        ReadQuantity();
    }
    if (reselect && g_closeHeld <= 0 && uiclick::Click(hovered))
        events::Push("pad %s: selected %s", ScreenName(g_screen), hname);

    // B = the window's close button, natively
    const char* close = CloseName();
    const bool haveClose = *close && Vis(close);
    if (haveClose)
        g_owned |= XINPUT_GAMEPAD_B;
    if (g_screen == Screen::Offer)
        g_owned |= XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B;
    if (pad.Pressed(XINPUT_GAMEPAD_B))
    {
        if (g_screen == Screen::Offer)
            ClickName(Vis("OfferDialogAcceptButton") ? "OfferDialogDeclineButton" : "OfferDialogClose2Button", "offer");
        else if (haveClose && room::NNWindowBusy())
        {
            // the window is still sliding in (or out): the game's close would do nothing now - B is held and clicked the
            // first frame the slide is over (dealer close bug, 26 Oct 2026: a B in the first ~0.5 s was lost)
            if (g_closeHeld <= 0)
                events::Push("pad close: held (the window slides)");
            g_closeHeld = kCloseHoldSec;
        }
        else if (haveClose)
            ClickName(close, "close");
    }
    else if (g_closeHeld > 0)
    {
        g_closeHeld -= g_dt;
        if (!haveClose)
            g_closeHeld = 0; // closed or gone meanwhile
        else if (!room::NNWindowBusy())
            g_closeHeld = 0, ClickName(close, "close (held)");
        else if (g_closeHeld <= 0)
            events::Push("pad close: dropped (the slide outlasted %.1f s)", kCloseHoldSec); // fails closed: never late
    }

    // A: accept the offer; a job / ship by a short hold; buy / sell / mount in the dealers
    const bool aDown = pad.Down(XINPUT_GAMEPAD_A);
    const float heldBefore = g_aHeld;
    g_aHeld = aDown ? g_aHeld + g_dt : 0;
    if (!aDown)
        g_aFired = false;
    switch (g_screen)
    {
    case Screen::Offer:
        if (pad.Pressed(XINPUT_GAMEPAD_A))
            ClickName(Vis("OfferDialogAcceptButton") ? "OfferDialogAcceptButton" : "OfferDialogClose2Button", "offer");
        break;
    case Screen::Jobs:
    case Screen::ShipDealer:
    {
        g_owned |= XINPUT_GAMEPAD_A;
        const char* accept = g_screen == Screen::Jobs ? "MissionVendorAcceptButton" : "ShipTraderBuyButton";
        if (aDown && !g_aFired && g_aHeld >= 0.6f)
        {
            g_aFired = true;
            ClickName(accept, g_screen == Screen::Jobs ? "job accept" : "ship buy");
        }
        else if (!aDown && heldBefore > 0 && heldBefore < 0.6f && !g_aFired && IsRow(hname))
            events::Push("pad %s: row %s %s", ScreenName(g_screen), hname, uiclick::Click(hovered) ? "clicked" : "no click");
        break;
    }
    case Screen::Commodity:
    case Screen::Equipment:
    {
        g_owned |= XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_Y | XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER |
                   XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
        if (g_screen == Screen::Equipment)
            g_owned |= XINPUT_GAMEPAD_X;
        if (pad.Pressed(XINPUT_GAMEPAD_A))
        {
            // the equipment dealer, one of the player's own rows: its mount toggle (EquipToggleN) first - the toggle
            // class's own action through uiclick::Click (exe::ToggleButtonActivate: the dealer mounts / unmounts the
            // item, the box flips); selling one's own equipment is never done by A here
            bool done = false;
            if (g_screen == Screen::Equipment && g_side == 2 && IsRow(hname))
            {
                char tname[24];
                sprintf_s(tname, "EquipToggle%d", TrailingNumber(hname));
                const u32 t = Vis(tname);
                events::Push("pad equipment: %s %s", tname,
                             t ? (uiclick::Click(t) ? "toggled" : "no click (class not signed)") : "absent");
                done = true;
            }
            if (!done && Vis("BuySellTradeButton"))
                ClickName("BuySellTradeButton", g_side == 2 ? "sell" : "buy");
            else if (!done && hovered && IsWindowControl(hname) && uiclick::Click(hovered))
                events::Push("pad dealer: %s clicked", hname);
        }
        if (pad.Pressed(XINPUT_GAMEPAD_Y))
            Quantity(1000, "max");
        if (pad.Pressed(XINPUT_GAMEPAD_LEFT_SHOULDER) || pad.Pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER) ||
            pad.Pressed(XINPUT_GAMEPAD_DPAD_LEFT) || pad.Pressed(XINPUT_GAMEPAD_DPAD_RIGHT))
            SwitchSide();
        if (g_screen == Screen::Equipment && pad.Pressed(XINPUT_GAMEPAD_X))
            CycleFilter();
        // the triggers: one step per press, repeating while held (faster after 1.5 s)
        const u32 trig[2] = {PAD_LT, PAD_RT};
        for (int i = 0; i < 2; ++i)
        {
            if (!pad.Down(trig[i]))
            {
                g_trigHeld[i] = 0;
                continue;
            }
            const bool first = g_trigHeld[i] == 0;
            g_trigHeld[i] += g_dt;
            g_trigRepeat[i] -= g_dt;
            if (first || g_trigRepeat[i] <= 0)
            {
                Quantity(i == 0 ? -1 : 1, first ? (i == 0 ? "LT" : "RT") : nullptr);
                g_trigRepeat[i] = first ? 0.4f : (g_trigHeld[i] > 1.5f ? 0.04f : 0.1f);
            }
        }
        break;
    }
    case Screen::Repair:
        // A = the game's own click on the focused control of the window (a check box toggles, OK repairs, Cancel
        // closes): natively, as the dealers' rows (the free cursor's button press needs the game's hover, which out of
        // focus is not recomputed - 25 Oct 2026: A on the focused Cancel did nothing in a background test)
        g_owned |= XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN |
                   XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
        if (pad.Pressed(XINPUT_GAMEPAD_A))
        {
            if (Starts(hname, "Repair") && strcmp(hname, "RepairList") != 0)
                events::Push("pad repair: %s %s", hname, uiclick::Click(hovered) ? "clicked" : "no click");
            else
                events::Push("pad repair: no focused control (%s)", hname);
        }
        if (pad.Pressed(XINPUT_GAMEPAD_DPAD_UP) || pad.Pressed(XINPUT_GAMEPAD_DPAD_LEFT))
            StepRepair(-1);
        if (pad.Pressed(XINPUT_GAMEPAD_DPAD_DOWN) || pad.Pressed(XINPUT_GAMEPAD_DPAD_RIGHT))
            StepRepair(+1);
        if (pad.Pressed(XINPUT_GAMEPAD_X))
            ClickName("RepairToggleChecks", "repair");
        if (Vis("RepairOkButton"))
        {
            g_owned |= XINPUT_GAMEPAD_Y;
            if (pad.Pressed(XINPUT_GAMEPAD_Y))
                ClickName("RepairOkButton", "repair");
        }
        break;
    case Screen::NnLog:
        g_owned |= XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER;
        if (pad.Pressed(XINPUT_GAMEPAD_LEFT_SHOULDER))
            ClickName("MissionButton", "log tab"), g_nnTab = 0;
        if (pad.Pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER))
            ClickName("LogButton", "log tab"), g_nnTab = 1;
        break;
    default: break;
    }
}

void Update(float dt, const Gamepad& pad, Where where)
{
    g_owned = 0;
    g_dt = dt;
    const bool wasActive = g_where != Where::Menu;
    g_where = where;
    if (g_release > 0)
    {
        g_release -= dt;
        if (g_release <= 0)
            dimouse::ReleaseTarget();
    }
    g_overlaysIdle = 0; // a pad frame ran (Overlay applies the decision below)
    // the cursor: the stick shows it, a direct button hides it (the menus' screens too)
    const PadState& s = pad.Cur();
    g_cursorBefore = g_cursorShown;
    if ((s.lx != 0 || s.ly != 0) && CursorAllowed())
        g_cursorShown = true;
    if (pad.Pressed(XINPUT_GAMEPAD_DPAD_UP) || pad.Pressed(XINPUT_GAMEPAD_DPAD_DOWN) ||
        pad.Pressed(XINPUT_GAMEPAD_DPAD_LEFT) || pad.Pressed(XINPUT_GAMEPAD_DPAD_RIGHT) ||
        pad.Pressed(XINPUT_GAMEPAD_LEFT_SHOULDER) || pad.Pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER))
        g_cursorShown = false;
    if (where == Where::Menu || !device::Pad())
    {
        if (wasActive)
            LeaveChars();
        g_screen = Screen::None;
        // the main menu / load screens: the arrow follows the pad cursor rule while padmenus has its screen up
        g_wantHidden = g_menuScreen && device::Pad() && ArrowRule();
        padmap::Update(dt, pad, false);
        return;
    }
    // the arrow itself (UI_PLAN 6.2: hidden in pad mode while the pad cursor is hidden - a direct button hid it, or
    // pad_cursor 0): the host skips the game's cursor draw (HostApi v20, applied by Overlay); the hit test and the
    // focus work on
    g_wantHidden = ArrowRule();

    g_screenRefresh -= dt;
    const bool refresh = g_screenRefresh <= 0; // (one timer for the modal check and the screen detection, 10 Hz)
    if (refresh)
        g_modal = ModalOpen();
    if (g_modal || OwnModalOpen())
    {
        // a modal window is up: nothing here acts, draws or hides the arrow until it is gone (the screen is detected
        // afresh then: a dealer re-enters with its first-row snap as on its opening)
        if (g_screen != Screen::None)
        {
            Log("pad screen %s -> none (modal window)", ScreenName(g_screen));
            LeaveChars();
            g_screen = Screen::None;
            g_aHeld = 0, g_aFired = false, g_lastScrollPos = -1, g_lastHovered = 0, g_closeHeld = 0;
        }
        // the pause menu, its confirm, the save / load screens, the death dialog: padmenus' screens follow the pad
        // cursor rule; any other modal (a story popup) keeps the arrow
        g_wantHidden = g_menuScreen && ArrowRule();
        if (refresh)
            g_screenRefresh = 0.1f; // (re-armed only when it fired: an every-frame re-arm never let it fire again)
        padmap::Update(dt, pad, false);
        return;
    }
    if (refresh)
    {
        const Screen s2 = Detect(where);
        if (s2 != g_screen)
        {
            Log("pad screen %s -> %s", ScreenName(g_screen), ScreenName(s2));
            g_aHeld = 0, g_aFired = false, g_lastScrollPos = -1, g_lastHovered = 0, g_closeHeld = 0;
            g_qtyPos = g_qtyMax = -1;
            if (g_screen == Screen::Room)
                LeaveChars();
            if (s2 == Screen::Room)
                g_charObjs.clear(), g_charRefresh = g_roomRefresh = 0; // (the cast, its order and the conversation state read at once)
            g_screen = s2;
            // a dealer window hangs under the NeuroNet's root (no root of its own): the generic auto-focus does not
            // enter it - put the focus on the dealer's first row (the equipment dealer opens with no list: its first
            // filter button), as a window with its own root gets it
            if (s2 == Screen::Commodity || s2 == Screen::Equipment)
                g_side = 1, g_enterSnap = 1.0f;
            if (s2 == Screen::Repair) // (the first damaged item's box, else the tick-all box)
                g_enterSnap = 1.0f;
        }
        g_screenRefresh = 0.1f;
    }
    // (no entry focus while a held B waits to close the window: the snap's row select would open the transfer panel's
    // BUY under a close the player already asked for)
    if (g_enterSnap > 0 && g_closeHeld <= 0 &&
        (g_screen == Screen::Commodity || g_screen == Screen::Equipment || g_screen == Screen::Repair))
    {
        // (retried for a second: right as the window appears its controls have no hit-testable place yet)
        // (tried at most every 0.1 s: the cached probe refreshes at its own pace)
        static float tryIn = 0;
        g_enterSnap -= dt;
        tryIn -= dt;
        if (tryIn <= 0)
        {
            tryIn = 0.1f;
            u32 first = 0;
            if (g_screen == Screen::Repair)
            {
                const std::vector<u32> ring = RepairRing();
                first = ring.empty() ? 0 : ring.front();
            }
            else if (!(first = FirstRow(1)))
                first = Vis("Filter_Weapons");
            if (first && Snap(first))
                g_enterSnap = 0, g_lastHovered = 0, tryIn = 0;
        }
    }
    else
        g_enterSnap = 0;
    // the nav map (padmap) owns the frame while it is open, in space and docked
    const bool mapOpen =
        padmap::Update(dt, pad, (where == Where::SpaceUi && g_screen == Screen::None) || g_screen == Screen::NavMap);
    if (mapOpen)
        return;
    if (g_screen == Screen::Room)
        UpdateRoom(pad);
    else if (g_screen != Screen::None && g_screen != Screen::NavMap) // (the map's first 100 ms: padmap opens it)
        UpdateWindow(pad);
    if (g_screen != Screen::None)
        Prompts(pad);
}

bool Owns(u32 mask) { return ((g_owned | padmap::Owned()) & mask) != 0; }

bool CursorAllowed() { return uiopt::Get().padCursor != 0; }

static bool ArrowRule() { return !g_cursorShown || !CursorAllowed(); }

void NoteFrame(bool menuScreen)
{
    g_overlaysIdle = 0;
    g_menuScreen = menuScreen;
    // (Update, when it runs this frame, decides again for the base screens; in flight nothing hides the arrow)
    g_wantHidden = menuScreen && ArrowRule();
}

void Overlay()
{
    // two overlay passes without a pad frame (frames and overlay passes need not alternate strictly): the pad layer
    // is not running - keyboard and mouse, the controller off, the autopilot flying, the options page - so nothing of
    // it may stay: the arrow back, the character the pad hovered released (inventory 3.5, 25 Oct 2026: the name card
    // stayed after the switch to the mouse)
    if (g_overlaysIdle < 99)
        ++g_overlaysIdle;
    const bool running = g_overlaysIdle <= 2 && device::Pad();
    if (!running)
    {
        if (g_hovered)
        {
            Log("pad stands down (%s): room hover released", device::Pad() ? "no pad frame" : "keyboard and mouse");
            LeaveChars();
        }
        ApplyCursorHidden(false);
        return;
    }
    ApplyCursorHidden(g_wantHidden);
}

bool SpaceWindowOpen()
{
    static ULONGLONG at = 0;
    static bool open = false;
    const ULONGLONG now = GetTickCount64();
    if (now - at >= 100)
    {
        at = now;
        open = false;
        for (const char* n : {"NeuroNetNavMap", "NN_Inventory", "NeuroNetStoryStar", "NN_PlayerInfo", "NN_Help"})
            if (Vis(n))
            {
                open = true;
                break;
            }
    }
    return open;
}

// ---------------------------------------------------------------- the pad cursor's magnetism
static std::vector<ui::Item> g_magnetItems;
static float g_magnetRefresh = 0;

void ShapeCursor(float& dx, float& dy)
{
    const float m = uiopt::Get().cursorMagnet / 100.0f;
    if (m <= 0 || g_where == Where::Menu || (dx == 0 && dy == 0))
        return;
    g_magnetRefresh -= g_dt;
    if (g_magnetRefresh <= 0)
    {
        g_magnetItems = ui::CollectAllCached(); // (the shared probe result: a fresh CollectAll cost ~7 ms on a dealer)
        g_magnetRefresh = 0.1f;
    }
    const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
    const float H = static_cast<float>(fl::ScreenHeight());
    const float reach = H * 0.08f;
    int inside = -1, nearest = -1;
    float best = 1e30f;
    for (size_t i = 0; i < g_magnetItems.size(); ++i)
    {
        const ui::Item& it = g_magnetItems[i];
        if (it.hw * 2 > H * 0.5f || it.hh * 2 > H * 0.5f)
            continue; // panels and lists: no pull toward a whole window
        const float ox = fabsf(mx - it.x) - it.hw, oy = fabsf(my - it.y) - it.hh;
        if (ox <= 0 && oy <= 0)
        {
            inside = static_cast<int>(i);
            break;
        }
        const float d = sqrtf(std::max(ox, 0.0f) * std::max(ox, 0.0f) + std::max(oy, 0.0f) * std::max(oy, 0.0f));
        if (d < best)
            best = d, nearest = static_cast<int>(i);
    }
    if (inside >= 0)
    {
        // slows over the element (never below a third of the stick's speed: it can always leave)
        const float k = 1.0f - 0.65f * m;
        dx *= k, dy *= k;
        return;
    }
    if (nearest < 0 || best > reach)
        return;
    // pulls toward the nearest element's centre, strongest close by, never faster than the stick's own motion
    const ui::Item& it = g_magnetItems[nearest];
    const float vx = it.x - mx, vy = it.y - my, len = sqrtf(vx * vx + vy * vy);
    if (len < 1)
        return;
    const float stick = sqrtf(dx * dx + dy * dy);
    const float pull = std::min(stick, len) * m * (1.0f - best / reach) * 0.6f;
    dx += vx / len * pull;
    dy += vy / len * pull;
}

// ---------------------------------------------------------------- drawing
constexpr u32 kHud = 0x82C9F3;
constexpr u32 kBack = 0x0D0B1A;
static u32 Argb(u32 rgb, float a) { return (static_cast<u32>(Clampf(a, 0, 1) * 255 + 0.5f) << 24) | (rgb & 0xFFFFFF); }
static struct
{
    bool label = false;
    bool qty = false;
} g_drawn;
static float g_qtyRect[4] = {}; // the QUANTITY line as drawn (pixels): the prompt lane's keep-out (QuantityRect)

// The room layer shows: pad mode with a pad frame this pass, a room, no conversation, no modal.
static bool RoomLayer()
{
    return g_overlaysIdle <= 2 && device::Pad() && g_where == Where::Docked && g_screen == Screen::Room && !g_conversation &&
           !g_modal && !OwnModalOpen();
}
static const Char* HoveredChar()
{
    if (!RoomLayer())
        return nullptr;
    for (const Char& c : g_chars)
        if (HoverFlag(c.obj) == 1)
            return &c;
    return nullptr;
}
const wchar_t* HoveredOffer()
{
    const Char* c = HoveredChar();
    return c ? OfferWord(c->rank) : nullptr;
}
bool HoveredIsStory()
{
    const Char* c = HoveredChar();
    return c && c->rank == 0;
}
bool InConversation() { return g_screen == Screen::Room && g_conversation; }

// The story star (five points, filled) centred on cx, cy with outer radius r.
static void Star(float cx, float cy, float r, u32 argb)
{
    float px[10], py[10];
    for (int i = 0; i < 10; ++i)
    {
        const float a = -1.5707963f + i * 0.62831853f, rr = (i & 1) ? r * 0.42f : r;
        px[i] = cx + cosf(a) * rr, py[i] = cy + sinf(a) * rr;
    }
    for (int i = 0; i < 10; i += 2) // the points
        hud2kit::Tri(px[(i + 9) % 10], py[(i + 9) % 10], px[i], py[i], px[i + 1], py[i + 1], argb);
    float inner[10];
    for (int i = 0; i < 5; ++i)
        inner[2 * i] = px[2 * i + 1], inner[2 * i + 1] = py[2 * i + 1];
    hud2kit::Poly(inner, 5, argb);
}

// ui_style 0: the game's own grey name card stays; a small HUD 2 label with the offer word sits on its top edge,
// left-aligned with it (nothing when the character offers nothing). Under ui_style 1 uistrip's card carries the word.
static void DrawOfferLabel(void* device, int sw, int sh)
{
    if (uistrip::Active())
        return;
    const wchar_t* word = HoveredOffer();
    const bool story = HoveredIsStory();
    float cx = 0, cy = 0;
    if ((!word && !story) || !uistrip::GameCardCorner(cx, cy))
        return;
    const float H = static_cast<float>(sh);
    const float k = std::min(1.3f, std::max(0.8f, uiopt::Get().textSize / 100.0f));
    const float size = H * 0.020f * k, padX = H * 0.008f, h = H * 0.030f * k;
    const float starW = story ? h * 0.8f : 0;
    const float tw = word ? hud2kit::TextWidth(word, size, 0.08f) : 0;
    const float w = padX * 2 + starW + (word && story ? padX * 0.6f : 0) + tw;
    const float x0 = std::max(H * 0.01f, cx), y1 = cy - H * 0.004f, y0 = y1 - h;
    if (y0 < 0 || !hud2kit::Begin(device, sw, sh, std::max(8, static_cast<int>(size + 0.5f))))
        return;
    hud2kit::SetAlpha(1), hud2kit::SetOffset(0, 0);
    hud2kit::Plate(x0, y0, x0 + w, y1, H * 0.006f, hud2kit::kTL | hud2kit::kBR, 0.85f);
    float x = x0 + padX;
    if (story)
        Star(x + starW * 0.5f, (y0 + y1) * 0.5f, starW * 0.5f, hud2kit::Argb(hud2kit::kActive, 1)), x += starW + padX * 0.6f;
    if (word)
        hud2kit::Text(x, (y0 + y1) * 0.5f, word, size, hud2kit::Argb(hud2kit::kInk, 1), hud2kit::Left, 0.08f);
    hud2kit::End();
    g_drawn.label = true;
}

bool QuantityRect(float& x0, float& y0, float& x1, float& y1)
{
    if (!g_drawn.qty)
        return false;
    x0 = g_qtyRect[0], y0 = g_qtyRect[1], x1 = g_qtyRect[2], y1 = g_qtyRect[3];
    return true;
}

void Draw(void* device)
{
    g_drawn = decltype(g_drawn)();
    if (!device || g_where == Where::Menu || !device::Pad() || g_modal || OwnModalOpen())
        return; // (the options page owns the frame before Update runs: its flag is read here as well)
    int sw0 = 0, sh0 = 0;
    render::SetDevice(device);
    if (g_screen == Screen::Room && render::ScreenSize(sw0, sh0) && sh0 >= 100)
        DrawOfferLabel(device, sw0, sh0);
    const bool qty = (g_screen == Screen::Commodity || g_screen == Screen::Equipment) && g_qtyPos >= 0;
    if (!qty)
        return;
    render::SetDevice(device);
    glyphs::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    const float H = static_cast<float>(sh), W = static_cast<float>(sw);
    const uikit::Fonts& f = uikit::FontsFor(sh, uiopt::Get().textSize);
    if (!f.ok)
        return;
    const render::Font& font = f.label;
    if (!render::Begin(font))
        return;
    {
        // the running quantity in the middle panel, from the game's own slider (its top = what fits / what you have);
        // the price and total are the game's own lines above it (not duplicated: their text is not readable)
        wchar_t line[64];
        if (g_qtyMax >= 0)
            swprintf_s(line, L"QUANTITY %d / %d", g_qtyPos, g_qtyMax);
        else
            swprintf_s(line, L"QUANTITY %d", g_qtyPos);
        const float tw = static_cast<float>(render::TextWidth(font, line));
        const float x = W * 0.5f - tw * 0.5f, y = H * 0.655f;
        render::Gradient(x - H * 0.01f, y - H * 0.003f, tw + H * 0.02f, font.lineHeight + H * 0.006f, Argb(kBack, 0.0f),
                         Argb(kBack, 0.5f), Argb(kBack, 0.5f), Argb(kBack, 0.0f));
        render::Text(x, y, line, Argb(kHud, 0.95f));
        g_drawn.qty = true;
        g_qtyRect[0] = x - H * 0.01f, g_qtyRect[1] = y - H * 0.003f, g_qtyRect[2] = x + tw + H * 0.01f,
        g_qtyRect[3] = y + font.lineHeight + H * 0.003f;
    }
    render::End(false);
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    std::ostringstream o;
    if (what == "rooms")
    {
        for (size_t i = 0; i < g_ring.size(); ++i)
            o << (static_cast<int>(i) == g_current ? "* " : "  ") << g_ring[i].label << " " << g_ring[i].control << "\n";
        o << "launch=" << (g_launch.ctl ? g_launch.control : "-") << " news=" << (g_news.ctl ? g_news.control : "-")
          << " jobs=" << (g_jobs.ctl ? g_jobs.control : "-") << " busy='" << g_busy << "' pending='" << g_pendingRoom
          << "' lastGoto='" << g_lastGoto << "'";
        return o.str();
    }
    if (what == "chars")
    {
        for (size_t i = 0; i < g_chars.size(); ++i)
            o << (static_cast<int>(i) == g_charFocus ? "* " : "  ") << i << " " << g_chars[i].obj << " "
              << g_chars[i].offer << " x=" << static_cast<int>(g_chars[i].sx) << " hovered=" << HoverFlag(g_chars[i].obj)
              << " talking=" << baseinfo::CharacterTalking(g_chars[i].obj) << (g_chars[i].player ? " player" : "") << "\n";
        o << "focus=" << g_charFocus << " hoveredObj=" << g_hovered << " cursorShown=" << g_cursorShown
          << " gameCursor=" << fl::CursorName();
        return o.str();
    }
    o << "bases where=" << (g_where == Where::Menu ? "menu" : g_where == Where::Docked ? "docked" : "space-ui")
      << " screen=" << ScreenName(g_screen) << " modal=" << (g_modal || OwnModalOpen()) << " pad=" << device::Pad()
      << " owned=0x" << std::hex << g_owned << std::dec
      << " cursorShown=" << g_cursorShown << " arrowHidden=" << g_arrowHidden << " menuScreen=" << g_menuScreen
      << " idle=" << g_overlaysIdle << " hoveredObj=" << g_hovered << " side=" << g_side << " qty=" << g_qtyPos << "/" << g_qtyMax
      << " filter=" << g_filter << " aHeld=" << g_aHeld << " closeHeld=" << g_closeHeld << " focused=" << ui::ControlName(Focused())
      << " gameHover=" << ui::ControlName(ui::Hovered()) << "\n";
    o << "drawn label=" << g_drawn.label << " conversation=" << g_conversation << " offer="
      << (HoveredOffer() ? text::Utf8(HoveredOffer()) : std::string("-")) << " story=" << HoveredIsStory() << " qty=" << g_drawn.qty << " | " << promptbar::Status() << "\n";
    o << padmap::Status();
    return o.str();
}

void Shutdown()
{
    LeaveChars();
    ApplyCursorHidden(false);
    padmap::Shutdown();
    promptbar::Shutdown();
}
} // namespace padbases
