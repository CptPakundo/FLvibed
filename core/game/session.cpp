// The play session's mode and the mode of every save (session.h).
#include "game/session.h"

#include "game/events.h"
#include "game/hostcalls.h"
#include "game/menu.h"
#include "game/modslots.h"
#include "game/room.h"
#include "log.h"
#include "modules_rules.h"
#include "ui/views.h"

#include <fstream>
#include <map>
#include <sstream>

namespace session
{
struct Rec
{
    Mode mode = UNKNOWN;
    u32 runMs = 0;
    int cp = -1;
};
static std::map<std::string, Rec> g_recs; // the sidecar: lower-case file name -> record
static std::string g_sidecar;            // EXE\FLvibed\saves.ini
static std::string g_saveDir;            // the game's save folder (Common.dll GetUserDataPath + Accts\SinglePlayer\)
static HANDLE g_watch = INVALID_HANDLE_VALUE;
static std::map<std::string, ULONGLONG> g_files; // the folder's *.fl with their write times (the last scan)
static bool g_scanned = false;
static Mode g_session = UNKNOWN, g_pendingNew = UNKNOWN;
static std::string g_loading; // the save the load screen had selected / a caller named
static ULONGLONG g_loadingTick = 0;
static std::string g_pendingSeen; // the game's pending-load id as last seen (menu::PendingLoad; a change names a load)
static bool g_deathDialog = false; // the death dialog was up during this menu period (LOAD AUTOSAVE loads AutoSave.fl)
static std::string g_lastMode = "menu";
static u32 g_runMs = 0;
static int g_cp = -1;
static bool g_loadedFlag = false;
static Rec g_loadedRec;
static float g_menuPoll = 0;
static bool g_mainMenu = false;
static std::vector<menu::SaveEntry> g_saves; // the open load screen's list, refreshed at 2 Hz (256 strings a copy)
static ULONGLONG g_savesTick = 0;
const std::vector<menu::SaveEntry>& CachedSaves()
{
    const ULONGLONG now = GetTickCount64();
    if (now - g_savesTick > 500)
        g_saves = menu::Saves(), g_savesTick = now;
    return g_saves;
}

const char* ModeName(Mode m)
{
    switch (m)
    {
    case NORMAL: return "normal";
    case ASSISTED: return "assisted";
    case SPEEDRUN: return "speedrun";
    default: return "none";
    }
}
static Mode ModeFromName(const std::string& s)
{
    if (s == "normal" || s == "0")
        return NORMAL;
    if (s == "assisted" || s == "1")
        return ASSISTED;
    if (s == "speedrun" || s == "2")
        return SPEEDRUN;
    return UNKNOWN;
}
static std::string LowerName(std::string s)
{
    const size_t slash = s.find_last_of("\\/");
    if (slash != std::string::npos)
        s = s.substr(slash + 1);
    for (char& c : s)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

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

// ------------------------------------------------------------------ the sidecar
static void LoadSidecar()
{
    g_recs.clear();
    std::ifstream in(g_sidecar);
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == ';' || line[0] == '[')
            continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::istringstream v(line.substr(eq + 1));
        std::string mode;
        Rec r;
        v >> mode >> r.runMs >> r.cp;
        r.mode = ModeFromName(mode);
        std::string key = line.substr(0, eq);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
            key.pop_back(); // (the writer puts a space before '=': without the trim the next rescan pruned every record)
        if (r.mode != UNKNOWN)
            g_recs[LowerName(key)] = r;
    }
}
static void SaveSidecar()
{
    const size_t slash = g_sidecar.find_last_of('\\');
    if (slash != std::string::npos)
        CreateDirectoryA(g_sidecar.substr(0, slash).c_str(), nullptr);
    // (a new file, then over the old: a crash or a full disk must not cost every save's mode)
    const std::string tmp = g_sidecar + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        out << "; FLvibed: the mode of every save file (written by FLvibed, never read by the game): <file> = <mode> "
               "<speedrun clock ms> <checkpoint index>\n[Saves]\n";
        for (const auto& kv : g_recs)
            out << kv.first << " = " << ModeName(kv.second.mode) << ' ' << kv.second.runMs << ' ' << kv.second.cp << '\n';
        if (!out.flush())
        {
            out.close();
            DeleteFileA(tmp.c_str());
            Log("session: %s not written", tmp.c_str());
            return;
        }
    }
    if (!MoveFileExA(tmp.c_str(), g_sidecar.c_str(), MOVEFILE_REPLACE_EXISTING))
        Log("session: %s not replaced (%lu)", g_sidecar.c_str(), GetLastError());
}

// ------------------------------------------------------------------ the save folder
static std::string SaveFolder()
{
    HMODULE common = GetModuleHandleA("common.dll");
    auto getPath = common ? reinterpret_cast<bool(__cdecl*)(char*)>(GetProcAddress(common, "?GetUserDataPath@@YA_NQAD@Z"))
                          : nullptr;
    char buf[MAX_PATH * 2] = {};
    if (!getPath || !getPath(buf) || !buf[0])
        return "";
    std::string p = buf;
    if (p.back() != '\\' && p.back() != '/')
        p += '\\';
    return p + "Accts\\SinglePlayer\\";
}

void Rescan()
{
    if (g_saveDir.empty())
        return;
    std::map<std::string, ULONGLONG> now;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((g_saveDir + "*.fl").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
        return; // the folder is out of reach for now, which says nothing about the saves: every record stays
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            ULARGE_INTEGER t;
            t.LowPart = fd.ftLastWriteTime.dwLowDateTime, t.HighPart = fd.ftLastWriteTime.dwHighDateTime;
            now[LowerName(fd.cFileName)] = t.QuadPart;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    bool dirty = false;
    if (g_scanned)
    {
        for (const auto& kv : now)
        {
            const auto old = g_files.find(kv.first);
            if (old != g_files.end() && old->second == kv.second)
                continue;
            if (g_session == UNKNOWN)
                continue; // not ours to tag (the menu writes nothing of a session)
            Rec r;
            r.mode = g_session, r.runMs = g_runMs, r.cp = g_cp;
            g_recs[kv.first] = r;
            dirty = true;
            Log("session: save %s -> %s (run %u ms, cp %d)", kv.first.c_str(), ModeName(r.mode), r.runMs, r.cp);
        }
        for (auto it = g_recs.begin(); it != g_recs.end();)
        {
            if (now.count(it->first))
                ++it;
            else
                it = g_recs.erase(it), dirty = true; // the file is gone (deleted in the load screen)
        }
    }
    g_files.swap(now);
    g_scanned = true;
    if (dirty)
        SaveSidecar();
}

// ------------------------------------------------------------------ the screens
// The main menu's buttons (unnamed front buttons with the main menu's handler): visible = the main menu is up.
static u32 MainButton(u32 id)
{
    if (!exe::UiControls.ok() || !exe::MainMenuButtonClick.ok())
        return 0;
    const u32 head = RdOr<u32>(static_cast<u32>(exe::UiControls.get()), 0u);
    u32 node = head ? RdOr<u32>(head + 4, 0u) : 0;
    for (int guard = 0; node && node != head && guard < 5000; ++guard, node = RdOr<u32>(node + 4, 0u))
    {
        const u32 ctl = RdOr<u32>(node + 8, 0u);
        if (!ctl || !(RdOr<u8>(ctl + 0x6C, 0) & 1))
            continue;
        const u32 vt = RdOr<u32>(ctl, 0u);
        const u32 handler = vt ? RdOr<u32>(vt + 0x4C, 0u) : 0;
        if (!exe::MainMenuButtonClick.is(handler))
            continue;
        if (id == 0 || RdOr<u32>(ctl + 0xEC, 0u) == id)
            return ctl;
    }
    return 0;
}
bool MainMenuVisible() { return g_mainMenu; }
u32 MainMenuButton(u32 id) { return MainButton(id); }
u32 MainMenuRoot()
{
    const u32 button = MainButton(0);
    return button ? RdOr<u32>(button + 4, 0u) : 0; // (a control's parent, as CharSelect reads it)
}

// The front-end load screen (character select): the parent of LoadCharacter when it is that class; its list +0xD8.
u32 CharSelect()
{
    const u32 button = uiclick::FindVisible("LoadCharacter");
    const u32 screen = button ? RdOr<u32>(button + 4, 0u) : 0;
    return screen && exe::VtLoadScreen.is(RdOr<u32>(screen, 0u)) ? screen : 0;
}
// The selected row of the open load screen (-1 none): front list +0x8AC, in-game screen +0xE88 (FREELANCER_RE).
static int SelectedRow()
{
    if (const u32 cs = CharSelect())
    {
        const u32 list = RdOr<u32>(cs + 0xD8, 0u);
        return list && exe::VtFrontList.is(RdOr<u32>(list, 0u)) ? static_cast<int>(RdOr<u32>(list + 0x8AC, 0xFFFFFFFFu)) : -1;
    }
    if (const u32 screen = menu::LoadScreen())
        return static_cast<int>(RdOr<u32>(screen + 0xE88, 0xFFFFFFFFu));
    return -1;
}

// The save the open load screen has selected (its file name), false when none.
bool SelectedSave(std::string& file)
{
    const int sel = SelectedRow();
    if (sel < 0)
        return false;
    const std::vector<menu::SaveEntry> saves = menu::Saves(); // fresh: the selection just changed
    if (sel >= static_cast<int>(saves.size()))
        return false;
    file = LowerName(saves[sel].id);
    return true;
}

// ------------------------------------------------------------------ session
static void Apply(Mode m)
{
    g_session = m;
    if (m == SPEEDRUN || m == ASSISTED)
    {
        std::istringstream cmd(m == SPEEDRUN ? "preset speedrun" : "preset competent");
        const std::string r = views::ApCommand(cmd, DllDir() + "FLController.ini"); // `ap preset <name>`: the autopilot's own bundle
        Log("session: session %s: %s", ModeName(m), r.substr(0, 60).c_str());
    }
    events::Push("session mode=%s run=%u cp=%d", ModeName(m), g_runMs, g_cp);
}

void Init()
{
    g_sidecar = DllDir() + "FLvibed\\saves.ini";
    g_pendingSeen = LowerName(menu::PendingLoad()); // a value from before this logic started is not a load
    LoadSidecar();
    g_saveDir = SaveFolder();
    if (!g_saveDir.empty())
    {
        g_watch = FindFirstChangeNotificationA(g_saveDir.c_str(), FALSE,
                                               FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
        Rescan();
    }
    Log("session: %u save records, folder %s (watch %s)", static_cast<unsigned>(g_recs.size()),
        g_saveDir.empty() ? "unknown" : g_saveDir.c_str(), g_watch == INVALID_HANDLE_VALUE ? "off" : "on");
}

void Shutdown()
{
    if (g_watch != INVALID_HANDLE_VALUE)
        FindCloseChangeNotification(g_watch);
    g_watch = INVALID_HANDLE_VALUE;
}

// The save about to load: a NEW value of the game's own pending-load CHARACTER_ID names it (the in-game load screen's load and the death dialog's
// LOAD AUTOSAVE write it; exe::PendingLoadId, verified: "SaveC702.fl" the moment `loadsave` runs; the game never clears it, not even at the main menu,
// and the main menu's character select does not write it, so only a change counts)
static bool NotePendingLoad()
{
    std::string pending = LowerName(menu::PendingLoad());
    if (pending.empty() || pending == g_pendingSeen)
        return false;
    g_pendingSeen = pending;
    if (pending.size() < 3 || pending.compare(pending.size() - 3, 3, ".fl") != 0)
        pending += ".fl"; // the records are keyed by the file name
    g_loading = pending, g_loadingTick = GetTickCount64();
    return true;
}

void Update(float dt, const char* modeName)
{
    const std::string mode = modeName ? modeName : "";
    // A load from the pause menu goes space, space-ui, space and never meets "menu": the game's load count says one passed
    static bool loadsRead = false;
    static u32 loadsSeen = 0;
    static bool loadPassed = false;
    const u32 loads = hostcalls::g.GameLoads ? hostcalls::g.GameLoads() : 0;
    if (loads != loadsSeen)
        loadPassed = loadsRead, loadsSeen = loads;
    loadsRead = true;
    if (mode == "space-ui")
        NotePendingLoad(); // (each frame: the id is there for only a few of them on this way)
    // the folder: a change notification, then one scan
    if (g_watch != INVALID_HANDLE_VALUE && WaitForSingleObject(g_watch, 0) == WAIT_OBJECT_0)
    {
        FindNextChangeNotification(g_watch);
        Rescan();
    }
    // the main menu, the load screens' selection, the death dialog: at 5 Hz
    g_menuPoll -= dt;
    if (g_menuPoll <= 0)
    {
        g_menuPoll = 0.2f;
        g_mainMenu = mode == "menu" && MainButton(0) != 0;
        if (mode == "menu")
        {
            // the save about to load, else the load screen's selected row
            if (NotePendingLoad())
            {
            }
            else if (const int sel = SelectedRow(); sel >= 0)
            {
                const std::vector<menu::SaveEntry>& saves = CachedSaves();
                if (sel < static_cast<int>(saves.size()))
                    g_loading = LowerName(saves[sel].id), g_loadingTick = GetTickCount64();
            }
            if (uiclick::FindVisible("LoadAutosaveButton"))
                g_deathDialog = true;
        }
    }
    if (g_mainMenu && g_session != UNKNOWN)
    {
        Log("session: main menu: session %s ended", ModeName(g_session));
        g_session = UNKNOWN;
        g_runMs = 0, g_cp = -1;
        events::Push("session mode=none");
    }
    // the mode transitions
    if (mode != g_lastMode)
    {
        if ((g_lastMode == "menu" || (loadPassed && mode != "space-ui")) && mode != "menu")
        {
            loadPassed = false;
            const ULONGLONG now = GetTickCount64();
            if (g_pendingNew != UNKNOWN)
            {
                g_runMs = 0, g_cp = -1;
                Apply(g_pendingNew);
                Log("session: new game started as %s", ModeName(g_pendingNew));
                g_pendingNew = UNKNOWN;
            }
            else
            {
                std::string file;
                if (!g_loading.empty() && now - g_loadingTick < 120000)
                    file = g_loading;
                else if (g_deathDialog)
                    file = "autosave.fl";
                const auto it = file.empty() ? g_recs.end() : g_recs.find(file);
                if (it != g_recs.end())
                {
                    g_runMs = it->second.runMs, g_cp = it->second.cp;
                    g_loadedRec = it->second, g_loadedFlag = true;
                    Apply(it->second.mode);
                    Log("session: loaded %s -> %s (run %u ms, cp %d)", file.c_str(), ModeName(it->second.mode),
                        g_runMs, g_cp);
                }
                else if (!file.empty() || g_session == UNKNOWN)
                {
                    g_runMs = 0, g_cp = -1;
                    Apply(NORMAL);
                    Log("session: %s loaded without a record -> normal", file.empty() ? "a game" : file.c_str());
                }
            }
            g_loading.clear();
            g_deathDialog = false;
        }
        g_lastMode = mode;
    }
}

Mode Current() { return g_session; }
Mode OfSave(const std::string& file)
{
    const auto it = g_recs.find(LowerName(file));
    return it == g_recs.end() ? UNKNOWN : it->second.mode;
}
void NoteLoading(const std::string& file) { g_loading = LowerName(file), g_loadingTick = GetTickCount64(); }
void SetRunState(u32 runMs, int cp) { g_runMs = runMs, g_cp = cp; }
bool TakeLoadedRunState(u32& runMs, int& cp, Mode& mode)
{
    if (!g_loadedFlag)
        return false;
    g_loadedFlag = false;
    runMs = g_loadedRec.runMs, cp = g_loadedRec.cp, mode = g_loadedRec.mode;
    return true;
}

bool Begin(Mode pick, std::string* what)
{
    g_pendingNew = pick;
    modslots::NewGame(pick == SPEEDRUN);
    const bool ok = menu::ClickMainButton(menu::kNewGame, what);
    events::Push("newgame pick=%s click=%d", ModeName(g_pendingNew), ok ? 1 : 0);
    Log("session: start %s: %s", ModeName(g_pendingNew), what->c_str());
    if (!ok)
        g_pendingNew = UNKNOWN;
    return ok;
}

void Force(Mode m) { Apply(m); }

static const Choice kChoices[3] = {
    {L"NORMAL GAME", L"", L"The game as it is. Nothing of the autopilot shows until you switch it on once."},
    {L"ASSISTED", L"AUTOPILOT READY",
     L"The Advanced Autopilot can fly, fight and dock for you from the first mission (competent preset: it knows only "
     L"what you have seen). Switch it on and off any time."},
    {L"SPEEDRUN MODE", L"TIMER / SPLITS / LIVESPLIT",
     L"In-game timer, a split per story checkpoint, LiveSplit fed over TCP. The autopilot knows the whole game. The "
     L"choice is written into the save and locked for it."},
};

const Choice& ChoiceOf(int mode) { return kChoices[mode]; }

// ASSISTED is the autopilot's, SPEEDRUN the speedrun module's: without its module a row is not offered (a host with no answer means on).
bool Offered(int mode)
{
    if (mode == ASSISTED)
        return views::ApPresent();
    if (mode == SPEEDRUN)
        return !hostcalls::g.ModuleOn || hostcalls::g.ModuleOn(modules::kSpeedrun);
    return true;
}

const wchar_t* DescOf(int mode)
{
    if (mode == SPEEDRUN && !views::ApPresent())
        return L"In-game timer, a split per story checkpoint, LiveSplit fed over TCP. The choice is written into the save and locked for it.";
    return kChoices[mode].desc;
}

std::string Status()
{
    std::ostringstream o;
    o << "session=" << ModeName(g_session) << " pending=" << ModeName(g_pendingNew) << " run=" << g_runMs << " cp=" << g_cp << " records=" << g_recs.size()
      << " files=" << g_files.size() << " loading=" << (g_loading.empty() ? "-" : g_loading) << " mainmenu=" << g_mainMenu << " dir=" << g_saveDir;
    return o.str();
}

std::string Records()
{
    std::ostringstream o;
    for (const auto& kv : g_recs)
        o << kv.first << " " << ModeName(kv.second.mode) << " " << kv.second.runMs << " " << kv.second.cp << "\n";
    return o.str();
}
} // namespace session
