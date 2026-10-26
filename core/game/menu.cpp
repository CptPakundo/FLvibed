#include "game/menu.h"

#include "flexe.h"
#include "game/room.h"
#include "str.h"

#include <cctype>
#include <sstream>

namespace menu
{
// Exact signatures (`ret N` of the targets, checked 18 Oct 2026): screen vt[0x5C] message (msg, id, param) ret 0Ch
// (main menu 0x566BD0, load screen 0x56C360); list row vt[0x10] item id, no args; vt[8] owner window, no args.
typedef u32(__thiscall* GetFn)(u32 self);
typedef u32(__thiscall* ScreenMsgFn)(u32 self, u32 msg, u32 id, u32 param);
typedef void(__thiscall* LoadIndexFn)(u32 self, int index);

// Every screen whose fields are read by offset is checked to be that class first (signed vtables, core\flexe.h):
// another build or a UI mod's screen fails closed instead of reading garbage.
static bool IsClass(u32 obj, hk::Sym& vt)
{
    __try
    {
        return obj && vt.is(Mem<u32>(obj));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

using str::Lower;

static bool CallMain(u32 ctl, u32 id)
{
    __try
    {
        const u32 parent = Mem<u32>(ctl + 4);
        if (!IsClass(parent, exe::VtMainMenu) || !Mem<u8>(ctl + 0xE6)) // +0xE6: enabled (the handler checks it too)
            return false;
        reinterpret_cast<ScreenMsgFn>((*reinterpret_cast<void***>(parent))[0x5C / 4])(parent, 0x3004, id, 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static u32 FindMainButton(u32 id)
{
    __try
    {
        const u32 head = Mem<u32>(exe::UiControls);
        u32 node = head ? Mem<u32>(head + 4) : 0;
        for (int guard = 0; node && node != head && guard < 5000; ++guard, node = Mem<u32>(node + 4))
        {
            const u32 ctl = Mem<u32>(node + 8);
            if (ctl && (Mem<u8>(ctl + 0x6C) & 1) &&
                exe::MainMenuButtonClick.is((*reinterpret_cast<u32**>(ctl))[0x4C / 4]) && Mem<u32>(ctl + 0xEC) == id)
                return ctl;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return 0;
}

bool ClickMainButton(u32 id, std::string* what)
{
    const u32 ctl = FindMainButton(id);
    char buf[64];
    if (!ctl)
    {
        sprintf_s(buf, "no visible main menu button %X", id);
        *what = buf;
        return false;
    }
    const bool ok = CallMain(ctl, id);
    sprintf_s(buf, ok ? "clicked main menu button %X" : "main menu button %X disabled", id);
    *what = buf;
    return ok;
}

static u32 OwnerWindow(u32 ctl)
{
    __try
    {
        const u32 parent = Mem<u32>(ctl + 4);
        return parent ? reinterpret_cast<GetFn>((*reinterpret_cast<void***>(parent))[8 / 4])(parent) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

u32 LoadScreen()
{
    const u32 button = uiclick::FindVisible("LoadSave_LoadGame");
    const u32 screen = button ? OwnerWindow(button) : 0;
    return IsClass(screen, exe::VtGameLoadScreen) ? screen : 0;
}

static bool ReadPendingId(char* id, size_t n)
{
    __try
    {
        memcpy(id, reinterpret_cast<const char*>(exe::PendingLoadId.get()), n);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

std::string PendingLoad()
{
    // CHARACTER_ID = a char buffer (FLSDK: char[64]); 0x5B0810 clears its first byte = none pending.
    char id[65] = {};
    if (!exe::PendingLoadId.ok() || !ReadPendingId(id, 64))
        return "";
    for (char c : id)
        if (c && !isprint(static_cast<u8>(c)))
            return ""; // not a file name: refuse to guess
    return id;
}

// The main menu's load screen (character select, vtable 0x5E2A84): the parent of its LoadCharacter button.
// Its save list is the list control at +0xD8: rows +0x8A4 count, +0x8AC selected, +0x8C row data (CHARACTER_ID),
// +0x48C row objects (vt[0x10] = item id), +0x8A2 enabled. The list's own click (mouse handler 0x5A2710) sets
// the selection and notifies the screen with vt[0x5C](0xF001, item id, row); LoadCharacter (id 0x3003) loads.
static u32 CharSelect()
{
    const u32 button = uiclick::FindVisible("LoadCharacter");
    __try
    {
        const u32 screen = button ? Mem<u32>(button + 4) : 0;
        return screen && exe::VtLoadScreen.is(Mem<u32>(screen)) ? screen : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

static int ReadCharSelect(u32 screen, char (*out)[260], int max)
{
    int n = 0;
    __try
    {
        const u32 list = Mem<u32>(screen + 0xD8);
        const int count = list && exe::VtFrontList.is(Mem<u32>(list)) ? static_cast<int>(Mem<u32>(list + 0x8A4)) : 0;
        for (int i = 0; i < count && i < 256 && n < max; ++i, ++n)
        {
            const char* id = Mem<const char*>(list + 0x8C + 4 * i);
            out[n][0] = 0;
            if (id)
                strncpy_s(out[n], 260, id, _TRUNCATE);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return n;
}

// Selects row `index` the way the list's click does, then clicks LoadCharacter (enabled by the selection).
static bool CharSelectLoad(u32 screen, int index)
{
    __try
    {
        const u32 list = Mem<u32>(screen + 0xD8);
        if (!IsClass(list, exe::VtFrontList) || index < 0 || index >= static_cast<int>(Mem<u32>(list + 0x8A4)))
            return false;
        const u32 row = Mem<u32>(list + 0x48C + 4 * index);
        if (!IsClass(row, exe::VtListRow) || !Mem<u8>(list + 0x8A2))
            return false;
        Mem<u32>(list + 0x8AC) = static_cast<u32>(index);
        const u32 item = reinterpret_cast<GetFn>((*reinterpret_cast<void***>(row))[0x10 / 4])(row);
        const auto notify = reinterpret_cast<ScreenMsgFn>((*reinterpret_cast<void***>(screen))[0x5C / 4]);
        notify(screen, 0xF001, item, static_cast<u32>(index));
        const u32 load = Mem<u32>(screen + 0xC8); // LoadCharacter
        if (!load || !Mem<u8>(load + 0x498))
            return false;
        notify(screen, 0x3004, Mem<u32>(load + 0x4A4), 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// NN_LoadSave +0x344 / +0x348: vector of 36-byte entries; entry +0 points at the save's CHARACTER_ID (its file
// name as a string; 0x5B2C70 copies it to 0x67EA78).
struct RawSave
{
    char id[260];
};

static int ReadSaves(u32 screen, RawSave* out, int max)
{
    int n = 0;
    __try
    {
        const u32 first = Mem<u32>(screen + 0x344), last = Mem<u32>(screen + 0x348);
        if (!first || last < first)
            return 0;
        const int count = static_cast<int>((last - first) / 36);
        for (int i = 0; i < count && n < max; ++i, ++n)
        {
            const char* id = Mem<const char*>(first + i * 36);
            out[n].id[0] = 0;
            if (id)
                strncpy_s(out[n].id, id, _TRUNCATE);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return n;
}

std::vector<SaveEntry> Saves()
{
    std::vector<SaveEntry> out;
    static RawSave raw[256];
    static char rows[256][260];
    if (const u32 cs = CharSelect())
    {
        const int n = ReadCharSelect(cs, rows, 256);
        for (int i = 0; i < n; ++i)
            out.push_back({i, rows[i]});
        return out;
    }
    const u32 screen = LoadScreen();
    const int n = screen ? ReadSaves(screen, raw, 256) : 0;
    for (int i = 0; i < n; ++i)
        out.push_back({i, raw[i].id});
    return out;
}

static bool CallLoadIndex(u32 screen, int index)
{
    __try
    {
        if (!exe::LoadSaveIndex.ok())
            return false;
        hk::Call<LoadIndexFn>(exe::LoadSaveIndex, screen, index);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool Load(const std::string& file, std::string* what)
{
    const u32 cs = CharSelect();
    const u32 screen = cs ? 0 : LoadScreen();
    if (!cs && !screen)
    {
        *what = "no load screen open (main menu: menu load; in game: the pause menu's Load)";
        return false;
    }
    std::string want = Lower(file);
    if (want.size() < 3 || want.compare(want.size() - 3, 3, ".fl") != 0)
        want += ".fl";
    for (const SaveEntry& e : Saves())
    {
        std::string id = Lower(e.id);
        const size_t slash = id.find_last_of("\\/");
        if (slash != std::string::npos)
            id = id.substr(slash + 1);
        if (id == want)
        {
            const bool ok = cs ? CharSelectLoad(cs, e.index) : CallLoadIndex(screen, e.index);
            *what = (ok ? "loading " : "load faulted: ") + e.id + " (row " + std::to_string(e.index) + ")";
            return ok;
        }
    }
    *what = "no save " + file + " in the load screen (saves are read at game start)";
    return false;
}

static bool OpenPauseRaw()
{
    __try
    {
        if (!exe::OpenPauseMenu.ok())
            return false;
        hk::Call<void(__cdecl*)()>(exe::OpenPauseMenu);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// NN_Pause: the owner of its PauseButtonBox0. Buttons at +0x338 + 4*i, their action codes at +0x360 + 4*i (5 slots,
// set per context when the menu is built, 0x4A6CA0..); action 0x4A7210 thiscall(code).
u32 PauseMenu()
{
    const u32 box = uiclick::FindVisible("PauseButtonBox0");
    const u32 menu = box ? OwnerWindow(box) : 0;
    return IsClass(menu, exe::VtPauseMenu) ? menu : 0;
}

// Test aid: destroys the player's ship through the game server's own API (pub::SpaceObj::Destroy, server.dll,
// DestroyType Fuse = explodes), so tests reach the death dialog without crashing into something. Player ship id at
// 0x673378 (0 when docked or in menus).
std::string DestroyObject(u32 id)
{
    HMODULE server = GetModuleHandleA("server.dll");
    auto destroy = server ? reinterpret_cast<int(__cdecl*)(u32, int)>(
                                GetProcAddress(server, "?Destroy@SpaceObj@pub@@YAHIW4DestroyType@@@Z"))
                          : nullptr;
    if (!destroy)
        return "server.dll pub::SpaceObj::Destroy not found";
    return "destroy " + std::to_string(id) + " -> " + std::to_string(destroy(id, 1));
}

static std::string SelfDestruct()
{
    const u32 ship = Mem<u32>(exe::PlayerShipId);
    return ship ? DestroyObject(ship) : "not in space";
}

bool Command(const std::string& cmd, const std::string& rest, std::string* reply)
{
    if (cmd == "selfdestruct") // test aid: the player's ship explodes (death dialog follows)
    {
        *reply = SelfDestruct();
        return true;
    }
    if (cmd == "pause") // open the pause menu as Escape does (0x4A75C0: refuses when the game does)
    {
        *reply = PauseMenu() ? "pause menu already open" : OpenPauseRaw() ? "pause requested" : "pause faulted";
        return true;
    }
    if (cmd == "pauseload") // click the open pause menu's Load button (action code 3) natively
    {
        const u32 p = PauseMenu();
        *reply = "pause menu not open";
        if (p)
            for (int i = 0; i < 5; ++i)
                if (Mem<u32>(p + 0x360 + 4 * i) == 3)
                {
                    const u32 box = Mem<u32>(p + 0x338 + 4 * i);
                    *reply = box && uiclick::Click(box) ? "clicked Load (PauseButtonBox" + std::to_string(i) + ")"
                                                        : "pause Load button not clickable";
                    break;
                }
        return true;
    }
    if (cmd == "pausecodes")
    {
        const u32 p = PauseMenu();
        if (!p)
        {
            *reply = "pause menu not open";
            return true;
        }
        std::ostringstream out;
        for (int i = 0; i < 5; ++i)
        {
            const u32 box = Mem<u32>(p + 0x338 + 4 * i);
            out << "PauseButtonBox" << i << " ctl " << std::hex << box << " code " << std::dec
                << Mem<u32>(p + 0x360 + 4 * i) << "\n";
        }
        *reply = out.str();
        return true;
    }
    if (cmd == "menu") // menu <new|load|multiplayer|options|exit>: main menu button, native
    {
        static const struct
        {
            const char* name;
            u32 id;
        } kButtons[] = {{"new", kNewGame},
                        {"load", kLoadGame},
                        {"multiplayer", kMultiplayer},
                        {"options", kOptions},
                        {"exit", kExit}};
        for (const auto& b : kButtons)
            if (Lower(rest) == b.name)
            {
                ClickMainButton(b.id, reply);
                return true;
            }
        *reply = "usage: menu new|load|multiplayer|options|exit";
        return true;
    }
    if (cmd == "pendingload") // the game's pending-load CHARACTER_ID (menu::PendingLoad)
    {
        const std::string id = PendingLoad();
        *reply = id.empty() ? (exe::PendingLoadId.ok() ? "pending load: none" : "pending load: symbol unresolved")
                            : "pending load: " + id;
        return true;
    }
    if (cmd == "saves") // the open load screen's save list
    {
        std::ostringstream out;
        const std::vector<SaveEntry> s = Saves();
        for (const SaveEntry& e : s)
            out << e.index << " " << e.id << "\n";
        out << "saves " << s.size()
            << (CharSelect()   ? " (main menu load screen)"
                : LoadScreen() ? " (in-game load screen)"
                               : " (no load screen open)");
        *reply = out.str();
        return true;
    }
    if (cmd == "loadsave") // loadsave <file>: load that save through the open load screen
    {
        Load(rest, reply);
        return true;
    }
    return false;
}
} // namespace menu
