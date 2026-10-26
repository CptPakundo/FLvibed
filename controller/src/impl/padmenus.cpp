// The game's menu screens on the pad - see padmenus.h. Screen facts (verified in game 25 Oct 2026, instance 6, HDE
// 0.7.1; docs\FREELANCER_RE.md "Main menu, load screens, pause menu"): the main menu's five unnamed front buttons (ids
// 0x10 New Game ...); the front-end load screen `LoadCharacter` / `DeleteCharacter` / `MainMenu` with the list
// `CharSelDesc` and its bar `CharScroll`; the in-game NN_LoadSave with `LoadSave_LoadGame` or `LoadSave_SaveGame`,
// `LoadSave_Delete`, `LoadSave_MainMenu`, `LoadSave_ResumeGame`, `LoadSave_Close`, an unnamed game list (vt VtGameList)
// and `LoadSave_Scroll`; the save screen's description edit `LoadSave_Edit2` (vt 0x5E5BD4: entries of 12 bytes at
// +0x4C8 / +0x4CC, the caret +0x49C, the maximum length +0x510 (30 here), its key handler at +0x498 = the key focus; text
// arrives through the game's WM_CHAR path: fl::SendChar); the pause menu's boxes by their action code (1 Resume); the
// death dialog's `LoadAutosaveButton`; the chat's `CTID` (the edit itself, the mode stays `space`).
#include "padmenus.h"

#include "../dimouse.h"
#include "../events.h"
#include "../fl.h"
#include "device.h"
#include "host.h"
#include "log.h"
#include "newgame.h"
#include "osk.h"
#include "padbases.h" // NoteFrame / Overlay: the game's arrow on the menu screens
#include "promptbar.h"
#include "text.h"
#include "ui.h"
#include "uikit.h"
#include "uiopt.h"
#include "game/flhash.h"
#include "game/ini.h"
#include "game/menu.h"
#include "game/room.h"

#include <map>
#include <sstream>
#include <time.h>

namespace newgame
{
bool MainMenuVisible();
u32 MainMenuButton(u32 id);
bool SelectedSave(std::string& file);
} // namespace newgame

namespace padmenus
{
enum class Screen
{
    None,
    MainMenu,
    LoadFront,
    LoadGame,
    Save,
    Pause,
    Death,
    Chat,
    Confirm, // NN_Confirm (a child of NN_Pause: the quit question; the front-end delete uses it too)
    Options, // the game's Options screen (in game and front end; FLvibed's own page is options_page's)
    Popup    // the story / tutorial popup (NeuroNetMissionMessage: pub::Player::PopUpDialog's box, in space and docked)
};
static const char* kScreenName[] = {"none", "mainmenu", "load", "load", "save", "pause", "death", "chat", "confirm", "options", "popup"};

// The popup's buttons (Freelancer.exe's names; PopUpDialog's `buttons` bits 0x1 YES, 0x2 NO, 0x4 LATER, 0x8 OK - the
// story's tutorial boxes are 0x8, OK alone): A = the accepting one, B = the declining one, else the only one.
static const char* PopupAccept()
{
    for (const char* n : {"MMOkButton", "MMYesButton"})
        if (uiclick::FindVisible(n))
            return n;
    return nullptr;
}
static const char* PopupDecline()
{
    for (const char* n : {"MMNoButton", "MMLaterButton", "MMOkButton"})
        if (uiclick::FindVisible(n))
            return n;
    return nullptr;
}
static uikit::Input g_input;
static bool g_owns = false;
static bool g_latch = false; // a consumed press holds the frames until every button is up (no leak into the dispatch)
static Screen g_screen = Screen::None;
static ULONGLONG g_since = 0;
static bool g_focused = false, g_prefilled = false, g_deleteFired = false;
// a native click sequence on the focused list row: the virtual mouse button down, up, then the action
static std::string g_lastAction;
static std::map<u32, u32> g_baseNames; // base id -> name string id (universe.ini [Base] strid_name)
static bool g_baseNamesRead = false;

static bool Rd(u32 addr, void* out, size_t n)
{
    __try
    {
        memcpy(out, reinterpret_cast<const void*>(addr), n);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
template <typename T> static T RdOr(u32 addr, T def)
{
    T v;
    return Rd(addr, &v, sizeof(T)) ? v : def;
}

static const std::vector<ui::Item>& Items()
{
    // CollectAll: the front-end lists are found by the hit-test probe only (uiitems alike); the shared cache (the probe
    // again when the tree changed, else every 2 s; was: a fresh probe every 100 ms, ~7 ms each at 4K)
    return ui::CollectAllCached();
}
static const ui::Item* ItemOf(u32 ctl)
{
    for (const ui::Item& it : Items())
        if (it.ctl == ctl)
            return &it;
    return nullptr;
}
static void SnapTo(u32 ctl)
{
    const ui::Item* it = ItemOf(ctl);
    if (!it)
        return;
    float x = it->x, y = it->y, rowH, firstRow;
    if (ui::ListRows(it->ctl, rowH, firstRow))
        y = firstRow;
    dimouse::MoveTo(x, y);
    Log("menus: focus %s (%.0f,%.0f)", it->name[0] ? it->name : "(unnamed)", x, y);
}
// the list control under the cursor (0 none)
static u32 ListUnderCursor()
{
    const std::vector<ui::Item>& items = Items();
    const int i = ui::FindAt(items, static_cast<float>(fl::MouseX()), static_cast<float>(fl::MouseY()));
    return i >= 0 && ui::IsList(items[i].ctl) ? static_cast<u32>(items[i].ctl) : 0;
}
// The item under the game's cursor (0 none): the front buttons never report as hovered, so by rectangle.
static u32 CtlUnderCursor()
{
    const std::vector<ui::Item>& items = Items();
    const int i = ui::FindAt(items, static_cast<float>(fl::MouseX()), static_cast<float>(fl::MouseY()));
    return i >= 0 ? static_cast<u32>(items[i].ctl) : 0;
}
// The data row of a load / save list under the cursor: the list's own hovered row (+0x7C, what its click handler
// selects; verified 25 Oct 2026 on the front list: 16 with the cursor on row 16, 13 three rows up - the row pitch
// ui::ListRows assumes for the front list (0.0318 H) is 0.029 H in HDE 0.7.1 at 4K, so geometry drifts a row after a
// dozen steps). Falls back to the geometry (0.029 H rows from the list's top, plus the scroll bar's first row) when the
// field is out of range. -1 none.
static int RowUnderCursor(u32 list, bool front)
{
    if (!list)
        return -1;
    const int rows = static_cast<int>(RdOr<u32>(list + 0x8A4, 0u));
    const int hovered = static_cast<int>(RdOr<u32>(list + 0x7C, 0xFFFFFFFFu));
    if (hovered >= 0 && (rows <= 0 || hovered < rows))
        return hovered;
    const ui::Item* it = ItemOf(list);
    if (!it)
        return -1;
    const float H = static_cast<float>(fl::ScreenHeight()), pitch = H * 0.029f;
    const u32 bar = uiclick::FindVisible(front ? "CharScroll" : "LoadSave_Scroll");
    const int first = bar ? static_cast<int>(RdOr<float>(bar + 0x3C8, 0.0f) + 0.5f) : 0;
    const int r = static_cast<int>((static_cast<float>(fl::MouseY()) - (it->y - it->hh)) / pitch);
    return r < 0 ? -1 : first + r;
}
typedef u32(__thiscall* RowGetFn)(u32);
typedef u32(__thiscall* ScreenMsgFn)(u32, u32, u32, u32);
static bool NotifyFrontSelection(u32 screen, u32 list, u32 row, int index)
{
    __try
    {
        Mem<u32>(list + 0x8AC) = static_cast<u32>(index);
        const u32 item = reinterpret_cast<RowGetFn>((*reinterpret_cast<void***>(row))[0x10 / 4])(row);
        reinterpret_cast<ScreenMsgFn>((*reinterpret_cast<void***>(screen))[0x5C / 4])(screen, 0xF001, item,
                                                                                      static_cast<u32>(index));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
static bool WriteU32(u32 addr, u32 v)
{
    __try
    {
        Mem<u32>(addr) = v;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
// Selects a row the way the list's own click does (menu.cpp CharSelectLoad's first half): the front list's selected
// index +0x8AC and the screen's 0xF001 notification with the row object's item id; the in-game screen keeps its selected
// row at +0xE88 (what LoadSave_LoadGame / Delete act on). False when the row is out of range.
static bool SelectRow(bool front, int index)
{
    if (front)
    {
        const u32 button = uiclick::FindVisible("LoadCharacter");
        const u32 screen = button ? RdOr<u32>(button + 4, 0u) : 0;
        if (!screen || !exe::VtLoadScreen.is(RdOr<u32>(screen, 0u)))
            return false;
        const u32 list = RdOr<u32>(screen + 0xD8, 0u);
        if (!list || !exe::VtFrontList.is(RdOr<u32>(list, 0u)) || index < 0 ||
            index >= static_cast<int>(RdOr<u32>(list + 0x8A4, 0u)))
            return false;
        const u32 row = RdOr<u32>(list + 0x48C + 4 * index, 0u);
        if (!row || !exe::VtListRow.is(RdOr<u32>(row, 0u)))
            return false;
        return NotifyFrontSelection(screen, list, row, index);
    }
    const u32 screen = menu::LoadScreen();
    return screen && index >= 0 && WriteU32(screen + 0xE88, static_cast<u32>(index));
}

static bool ClickNamed(const char* name, const char* what)
{
    const u32 ctl = uiclick::FindVisible(name);
    const bool ok = ctl && uiclick::Click(ctl);
    g_lastAction = std::string(what) + (ok ? " ok" : " failed");
    events::Push("menus %s %s", what, ok ? "ok" : "failed");
    Log("menus: %s -> %s (%s)", what, name, ok ? "clicked" : "not clickable");
    return ok;
}
// The pause menu's box with the given action code (1 Resume, 2 Save, 3 Load, 4 Options, 5 Quit), 0 none.
static u32 PauseBox(u32 code)
{
    const u32 p = menu::PauseMenu();
    for (int i = 0; p && i < 6; ++i)
        if (RdOr<u32>(p + 0x360 + 4 * i, 0u) == code)
            return RdOr<u32>(p + 0x338 + 4 * i, 0u);
    return 0;
}

static Screen Detect()
{
    if (uiclick::FindVisible("CTID"))
        return Screen::Chat;
    // the confirmation lies over the pause menu (its parent) or the load screen whose delete asked: before them
    if (uiclick::FindVisible("Confirm_delBtnYes"))
        return Screen::Confirm;
    if (uiclick::FindVisible("LoadSave_SaveGame"))
        return Screen::Save;
    if (uiclick::FindVisible("LoadSave_LoadGame"))
        return Screen::LoadGame;
    if (uiclick::FindVisible("LoadCharacter"))
        return Screen::LoadFront;
    if (uiclick::FindVisible("LoadAutosaveButton"))
        return Screen::Death;
    // the story / tutorial popup (the owner's attended run 26 Oct 2026: "Basics of Combat" in M01 had no pad prompt and
    // no pad bind - with the pad in flight the cursor is the crosshair, so the mode stays `space` and the menu pad code
    // never saw the box)
    if (PopupAccept() || PopupDecline())
        return Screen::Popup;
    // the Options screen (FIX2: it was no screen here - no prompts, no focus, the arrow drawn, in game and in the front
    // end since D-MENUS; the in-game one hides the pause menu behind it): its page buttons by name, as padbases' modal
    // check (VideoButton / keymapButton) - before the pause menu
    if (uiclick::FindVisible("VideoButton") || uiclick::FindVisible("keymapButton"))
        return Screen::Options;
    if (menu::PauseMenu())
        return Screen::Pause;
    if (newgame::MainMenuVisible())
        return Screen::MainMenu;
    return Screen::None;
}

// ------------------------------------------------------------------ the save description's pre-fill
static void ReadBaseNames()
{
    g_baseNamesRead = true;
    for (const std::string& file : ini::DataFiles("universe"))
        for (const ini::Section& s : ini::Read(file))
        {
            if (s.type != "base")
                continue;
            const std::string* nick = s.Get("nickname");
            const std::string* ids = s.Get("strid_name");
            if (nick && ids)
                g_baseNames[flhash::CreateId(*nick)] = static_cast<u32>(strtoul(ids->c_str(), nullptr, 10));
        }
    Log("menus: %u base names from universe.ini", static_cast<unsigned>(g_baseNames.size()));
}
static std::wstring Location()
{
    const u32 base = fl::PlayerBase();
    if (base)
    {
        if (!g_baseNamesRead)
            ReadBaseNames();
        const auto it = g_baseNames.find(base);
        if (it != g_baseNames.end())
        {
            const std::wstring w = uikit::Widen(text::IdsUtf8(it->second).c_str());
            if (!w.empty())
                return w;
        }
    }
    return L"Space";
}
static std::wstring Description(size_t maxLen)
{
    wchar_t date[32] = {};
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_s(&tmv, &t);
    wcsftime(date, 32, L"%d %b %H:%M", &tmv);
    // (the spec writes "<location> <middle dot> <date>"; the source files are compiled as ANSI, so the dot
    // would reach the edit as two characters - a plain dash instead)
    std::wstring loc = Location();
    const std::wstring tail = std::wstring(L" - ") + date;
    if (maxLen && loc.size() + tail.size() > maxLen)
    {
        // keep the date: first the "Planet " / "Station " prefix goes, then the name is cut
        for (const wchar_t* prefix : {L"Planet ", L"Station ", L"Battleship "})
            if (loc.size() + tail.size() > maxLen && loc.rfind(prefix, 0) == 0)
                loc = loc.substr(wcslen(prefix));
        const size_t room = maxLen > tail.size() ? maxLen - tail.size() : 0;
        if (loc.size() > room)
            loc.resize(room);
    }
    std::wstring d = loc + tail;
    if (maxLen && d.size() > maxLen)
        d.resize(maxLen);
    return d;
}
static void Prefill()
{
    g_prefilled = true;
    const u32 edit = uiclick::FindVisible("LoadSave_Edit2");
    if (!edit)
    {
        Log("menus: save screen without LoadSave_Edit2: no pre-fill");
        return;
    }
    const u32 begin = RdOr<u32>(edit + 0x4C8, 0u), end = RdOr<u32>(edit + 0x4CC, 0u);
    if (begin != end)
    {
        Log("menus: the description already has %u characters: left alone", (end - begin) / 12);
        return;
    }
    if (!exe::KeyFocus.ok() || RdOr<u32>(static_cast<u32>(exe::KeyFocus.get()), 0u) != edit + 0x498)
    {
        Log("menus: the description edit has no key focus: no pre-fill");
        return;
    }
    const u32 maxLen = RdOr<u32>(edit + 0x510, 0u);
    const std::wstring desc = Description(maxLen ? maxLen : 30);
    const int typed = osk::Type(desc);
    const u32 after = (RdOr<u32>(edit + 0x4CC, 0u) - RdOr<u32>(edit + 0x4C8, 0u)) / 12;
    Log("menus: description pre-filled \"%s\" (%d sent, %u in the edit, max %u)", uikit::Narrow(desc).c_str(), typed,
        after, maxLen);
    events::Push("menus prefill chars=%u", after);
}

// ------------------------------------------------------------------ load / delete by the row under the cursor
static void LoadRow(bool front, u32 list)
{
    const int idx = RowUnderCursor(list, front);
    const std::vector<menu::SaveEntry> saves = menu::Saves();
    if (idx < 0 || idx >= static_cast<int>(saves.size()))
    {
        g_lastAction = "load: no row under the cursor";
        Log("menus: A on the list: row %d of %u", idx, static_cast<unsigned>(saves.size()));
        return;
    }
    std::string what;
    const std::string& file = saves[idx].id;
    newgame::NoteLoading(file);
    const bool ok = menu::Load(file, &what); // the game's own select + load (CharSelectLoad / LoadSaveIndex)
    g_lastAction = "load " + file + (ok ? " ok" : " failed");
    events::Push("menus load row=%d %s %s", idx, file.c_str(), ok ? "ok" : "failed");
    Log("menus: %s", what.c_str());
}
static void DeleteRow(bool front, u32 list)
{
    const int idx = RowUnderCursor(list, front);
    if (!SelectRow(front, idx))
    {
        g_lastAction = "delete: no row";
        Log("menus: delete: row %d not selectable", idx);
        return;
    }
    events::Push("menus delete row=%d", idx);
    ClickNamed(front ? "DeleteCharacter" : "LoadSave_Delete", "delete");
}

void Init() { g_input.Init(); }
void Shutdown() {}

static bool AnyButtonDown(const uikit::Input::PadExtra& x)
{
    return x.aDown || x.bDown || x.xDown || x.yDown || x.menuDown || x.viewDown || x.lbDown || x.rbDown;
}

void Update(float dt, const char* modeName)
{
    const bool owned = g_owns;
    g_owns = false;
    const uikit::UiInput in = g_input.Poll(dt, false);
    const uikit::Input::PadExtra x = g_input.Extra();
    if (owned && AnyButtonDown(x))
        g_latch = true; // the press that was consumed is still down
    if (g_latch)
    {
        if (!AnyButtonDown(x))
            g_latch = false;
        else
            g_owns = true;
    }
    const ULONGLONG now = GetTickCount64();
    // (this runs only while the pad is the device: a screen first seen after a pause - the box came up under the
    // keyboard and mouse, the pad's first press switched the device - has been up for a while: settled at once. The
    // 26 Oct 2026 test: B on a tutorial popup that came up under the mouse was held as the device switch's first press)
    static ULONGLONG lastRun = 0;
    const bool resumed = now - lastRun > 200;
    lastRun = now;
    const Screen s = Detect();
    if (s != g_screen)
    {
        Log("menus: screen %s -> %s (%s)", kScreenName[static_cast<int>(g_screen)], kScreenName[static_cast<int>(s)],
            modeName ? modeName : "");
        g_screen = s;
        g_since = resumed && s == Screen::Popup ? now - 1000 : now;
        g_focused = g_prefilled = g_deleteFired = false;
        if (osk::IsOpen() && !(s == Screen::Save && osk::Current() == osk::Purpose::Save))
            osk::Close();
        if (s == Screen::Chat)
            osk::Open(osk::Purpose::Chat);
    }
    // the game's arrow on the menu screens (padbases owns the decision; the chat in flight keeps the flight cursor)
    padbases::NoteFrame(s != Screen::None && s != Screen::Chat && s != Screen::Popup); // (the popup: natively, no arrow)
    if (osk::IsOpen())
    {
        osk::Input(in, x);
        g_owns = true;
        return;
    }
    if (newgame::PromptOpen())
    {
        if (s != Screen::MainMenu)
            newgame::ClosePrompt();
        else
        {
            newgame::PromptInput(in, x);
            g_owns = true;
            return;
        }
    }
    const bool settled = now - g_since > 250; // after UpdateUiNav's own auto-focus of the new window
    const float holdSec = 1.0f;
    switch (s)
    {
    case Screen::MainMenu:
    {
        const u32 newGame = newgame::MainMenuButton(menu::kNewGame);
        const ui::Item* ng = newGame ? ItemOf(newGame) : nullptr;
        if (!g_focused && settled && ng && ng->x > ng->hw) // once the game's menu animation has slid it on screen
            SnapTo(newGame), g_focused = true;
        static const promptbar::Prompt p[] = {{glyphs::A, L"SELECT"}};
        promptbar::Set("mainmenu", p, 1, false, 10);
        if (in.accept && newGame && CtlUnderCursor() == newGame)
        {
            newgame::OpenPrompt();
            g_owns = true;
        }
        if (in.back)
            g_owns = true; // B does nothing here (no Escape)
        break;
    }
    case Screen::LoadFront:
    case Screen::LoadGame:
    {
        const bool front = s == Screen::LoadFront;
        const u32 list = ListUnderCursor();
        promptbar::Prompt p[] = {{glyphs::A, L"LOAD"}, {glyphs::X, L"DELETE", true}, {glyphs::RS, L"SCROLL"},
                                 {glyphs::B, L"BACK"}};
        if (x.xDown && list)
            p[1].progress = x.xHeld / holdSec;
        promptbar::Set("load", p, 4, x.xDown && list, 10);
        if (in.accept && list)
        {
            LoadRow(front, list);
            g_owns = true;
        }
        else if (x.x && list)
        {
            g_deleteFired = false; // the hold starts (its sweep in the prompt bar)
            g_owns = true;
        }
        else if (x.xDown)
        {
            g_owns = true; // (no right click while X is held here)
            if (x.xHeld >= holdSec && !g_deleteFired && list)
            {
                g_deleteFired = true;
                DeleteRow(front, list);
            }
        }
        else if (in.back)
        {
            ClickNamed(front ? "MainMenu" : "LoadSave_Close", "back");
            g_owns = true;
        }
        break;
    }
    case Screen::Save:
    {
        if (!g_prefilled && now - g_since > 100)
            Prefill();
        const u32 list = ListUnderCursor();
        promptbar::Prompt p[] = {{glyphs::A, L"SAVE"}, {glyphs::Y, L"KEYBOARD"}, {glyphs::X, L"DELETE", true},
                                 {glyphs::RS, L"SCROLL"}, {glyphs::B, L"CLOSE"}};
        if (x.xDown && list)
            p[2].progress = x.xHeld / holdSec;
        promptbar::Set("save", p, 5, x.xDown && list, 10);
        if (in.accept && !list)
        {
            ClickNamed("LoadSave_SaveGame", "save");
            g_owns = true;
        }
        else if (x.y)
        {
            osk::Open(osk::Purpose::Save);
            g_owns = true;
        }
        else if (x.x && list)
        {
            g_deleteFired = false;
            g_owns = true;
        }
        else if (x.xDown)
        {
            g_owns = true;
            if (x.xHeld >= holdSec && !g_deleteFired && list)
            {
                g_deleteFired = true;
                DeleteRow(false, list);
            }
        }
        else if (in.back)
        {
            ClickNamed("LoadSave_Close", "close");
            g_owns = true;
        }
        break;
    }
    case Screen::Pause:
    {
        const u32 resume = PauseBox(1);
        if (!g_focused && settled && resume)
            SnapTo(resume), g_focused = true;
        static const promptbar::Prompt p[] = {{glyphs::A, L"SELECT"}, {glyphs::B, L"RESUME"}};
        promptbar::Set("pause", p, 2, false, 10);
        if (in.back)
        {
            const bool ok = resume && uiclick::Click(resume);
            g_lastAction = ok ? "resume ok" : "resume failed";
            events::Push("menus resume %s", ok ? "ok" : "failed");
            if (!ok)
                fl::TapKey(VK_ESCAPE); // the game's own close of the menu
            g_owns = true;
        }
        break;
    }
    case Screen::Confirm:
    {
        // the focus starts on NO (the safe answer; the generic D-pad focus moves it); A = the game's own click on the
        // focused answer, natively (the free cursor's button needs the game's hover, not recomputed out of focus); B =
        // NO by the game's own button
        const u32 yes = uiclick::FindVisible("Confirm_delBtnYes"), no = uiclick::FindVisible("Confirm_delBtnNo");
        if (!g_focused && settled && no && ItemOf(no))
            SnapTo(no), g_focused = true;
        // prompts (critique 4: "A NO · D-pad YES / NO · B NO" read as two NOs): A = the focused answer's verb, the D-pad
        // CHOOSEs, B = NO
        const u32 under = CtlUnderCursor();
        const promptbar::Prompt p[] = {{glyphs::A, under == yes ? L"YES" : under == no ? L"NO" : L"SELECT"},
                                       {glyphs::DL, L"CHOOSE", false, glyphs::DR},
                                       {glyphs::B, L"NO"}};
        promptbar::Set("confirm", p, 3, true, 10);
        if (in.accept && under && (under == yes || under == no))
        {
            ClickNamed(under == yes ? "Confirm_delBtnYes" : "Confirm_delBtnNo", under == yes ? "confirm yes" : "confirm no");
            g_owns = true;
        }
        else if (in.back)
        {
            if (!ClickNamed("Confirm_delBtnNo", "confirm no"))
                fl::TapKey(VK_ESCAPE); // the game's own cancel of the question
            g_owns = true;
        }
        break;
    }
    case Screen::Death:
    {
        // (FIX2: the focus on LOAD AUTOSAVE once the dialog is up, so A SELECT has a mark - the game's own rollover)
        const u32 autosave = uiclick::FindVisible("LoadAutosaveButton");
        if (!g_focused && settled && autosave && ItemOf(autosave))
            SnapTo(autosave), g_focused = true;
        static const promptbar::Prompt p[] = {{glyphs::A, L"SELECT"}};
        promptbar::Set("death", p, 1, true, 10);
        if (in.back)
            g_owns = true; // B does nothing here on purpose
        break;
    }
    case Screen::Options:
    {
        // the focus on entry: the first page button the player can click (the current page's button is disabled: not
        // clickable, so not among the items) - the pad then walks the page with the generic D-pad focus; A = the
        // game's click under the focus (the generic dispatch); B = the screen's own way back: the close X in game,
        // MAIN MENU in the front end (the game's buttons, natively)
        // (once the screen's slide has stopped: the button's rect the same as the frame before - the front end slides it in)
        // (not the current page's: in game it stays clickable, its caption in the game's selected yellow - text +0x348,
        // bytes R G B A, red well above blue; the normal caption is light blue 87 C3 E0)
        static float lastX = -1, lastY = -1;
        static const char* const tabs[][2] = {{"interfaceButton", "InterfaceText"}, {"keymapButton", "keymapText"},
                                              {"VideoButton", "VideoText"},         {"AudioButton", "AudioText"},
                                              {"CreditsButton", "CreditsText"}};
        // (the game lights the current page's caption ~1.5 s after the front end's screen opens: until one is lit - at
        // most 3 s - the current page is not known and no snap is made)
        bool anyLit = false;
        for (const auto& tb : tabs)
            if (const u32 cap = uiclick::FindVisible(tb[1]))
            {
                const u32 c = RdOr<u32>(cap + 0x348, 0u);
                anyLit |= (c & 0xFF) > ((c >> 16) & 0xFF) + 0x40;
            }
        if (!g_focused && settled && (anyLit || now - g_since > 3000))
            for (const auto& tb : tabs)
                if (const u32 b = uiclick::FindVisible(tb[0]))
                    if (const ui::Item* it = ItemOf(b))
                    {
                        const u32 cap = uiclick::FindVisible(tb[1]);
                        const u32 c = cap ? RdOr<u32>(cap + 0x348, 0u) : 0;
                        // (and no disabled button: state +0x6C bit 8 clear - not clickable)
                        if ((c & 0xFF) > ((c >> 16) & 0xFF) + 0x40 || !(RdOr<u8>(b + 0x6C, 0) & 8))
                            continue; // the current page, or not clickable

                        // (still for 200 ms and wholly on the screen: the probe's cache refreshes every 50 ms at
                        // most, so two frames alike say nothing - the front end's first snap landed at x 1, mid-slide)
                        static ULONGLONG stillSince = 0;
                        if (it->x != lastX || it->y != lastY)
                            stillSince = now;
                        else if (now - stillSince >= 200 && it->x - it->hw >= 0)
                            SnapTo(b), g_focused = true;
                        lastX = it->x, lastY = it->y;
                        break;
                    }
        static const promptbar::Prompt p[] = {{glyphs::A, L"SELECT"}, {glyphs::B, L"BACK"}};
        promptbar::Set("options", p, 2, false, 10);
        if (in.back)
        {
            if (!ClickNamed(uiclick::FindVisible("PreferencesCloseButton") ? "PreferencesCloseButton" : "MainMenuButton",
                            "options back"))
                fl::TapKey(VK_ESCAPE);
            g_owns = true;
        }
        break;
    }
    case Screen::Popup:
    {
        // A = OK / YES, B = NO / LATER, or OK when it is the box's only button: the game's own buttons, natively
        // (uiclick: no cursor - the box sits over the flight view where the pad's cursor is the crosshair)
        const char* acc = PopupAccept();
        const char* dec = PopupDecline();
        const bool single = acc && dec && strcmp(acc, dec) == 0;
        promptbar::Prompt p[2];
        int n = 0;
        if (acc)
            p[n++] = {glyphs::A, strcmp(acc, "MMYesButton") == 0 ? L"YES" : L"OK"};
        if (dec)
            p[n++] = {glyphs::B, single ? L"CLOSE" : strcmp(dec, "MMNoButton") == 0 ? L"NO" : L"LATER"};
        promptbar::Set("popup", p, n, true, 10);
        if (in.accept && acc && settled)
        {
            ClickNamed(acc, "popup accept");
            g_owns = true;
        }
        else if (in.back && dec && settled)
        {
            ClickNamed(dec, "popup close");
            g_owns = true;
        }
        else if (in.accept || in.back)
            g_owns = true; // (inside the box's first 250 ms: held, not a flight action)
        break;
    }
    case Screen::Chat: // the OSK handles it (opened at the screen change; closed when CTID goes)
    case Screen::None: break;
    }
}

bool OwnsInput() { return g_owns; }

void Idle(float dt)
{
    // the pad's edges stay current while another layer drives the pad (FLUF's windows): the press that closed FLUF's
    // window is not a new press here the next frame (26 Oct 2026: one B closed the palette AND the tutorial popup)
    (void)g_input.Poll(dt, false);
    g_owns = false;
}

bool OnRawKey(int vk, bool down, bool)
{
    // a real key: the player went to the keyboard - FLvibed's prompt and keyboard step aside (never swallowed, except
    // Escape while the New Game prompt is up, which only closes the prompt)
    if (!down)
        return false;
    if (newgame::PromptOpen() && vk == VK_ESCAPE)
    {
        newgame::ClosePrompt();
        return true;
    }
    if (osk::IsOpen() && vk != VK_SHIFT && vk != VK_CONTROL && vk != VK_MENU)
        osk::Close();
    return false;
}

void Draw()
{
    padbases::Overlay(); // the pad screens' arrow decision, or their stand-down when no pad frame ran (every pass)
    newgame::Draw(); // the prompt and the load screens' tags
    osk::Draw();     // (the prompt bar is drawn by OnRenderOverlay itself, after every pad screen)
}

std::string Status()
{
    std::ostringstream o;
    o << "menus screen=" << kScreenName[static_cast<int>(g_screen)] << " owns=" << g_owns << " focused=" << g_focused
      << " prefilled=" << g_prefilled << " device=" << (device::Pad() ? "pad" : "kbm")
      << " last=" << (g_lastAction.empty() ? "-" : g_lastAction) << "\n"
      << osk::Status() << "\n" << newgame::Status() << "\n" << promptbar::Status();
    return o.str();
}

std::string Command(std::istream& in)
{
    std::string sub;
    in >> sub;
    if (sub == "prefill")
    {
        g_prefilled = false;
        Prefill();
        return Status();
    }
    if (sub == "row") // menus row: the list under the cursor, its hovered-row field and the row this code would act on
    {
        const u32 list = ListUnderCursor();
        std::ostringstream o;
        o << "list=" << std::hex << list << std::dec << " hovered(+0x7C)=" << (list ? static_cast<int>(RdOr<u32>(list + 0x7C, 0xFFFFFFFFu)) : -1)
          << " rows(+0x8A4)=" << (list ? RdOr<u32>(list + 0x8A4, 0u) : 0) << " row=" << RowUnderCursor(list, uiclick::FindVisible("LoadCharacter") != 0)
          << " mouse=" << fl::MouseX() << "," << fl::MouseY();
        return o.str();
    }
    return Status();
}
} // namespace padmenus
