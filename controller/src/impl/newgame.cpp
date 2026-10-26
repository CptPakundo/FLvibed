// The New Game prompt and the mode of every save - see newgame.h.
#include "newgame.h"

#include "../events.h"
#include "../fl.h"
#include "controller.h"
#include "host.h"
#include "livesplit.h"
#include "log.h"
#include "promptbar.h"
#include "ui.h"
#include "uiopt.h"
#include "game/menu.h"
#include "game/render.h"
#include "game/room.h"
#include "game/uidraw.h"

#include <fstream>
#include <map>
#include <sstream>

namespace newgame
{
using uikit::Widen;

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
static bool g_prompt = false;
static int g_pick = 0;
static std::string g_lastWhat; // the last New Game click's reply
static float g_selPoll = 0, g_menuPoll = 0;
static bool g_mainMenu = false;
static int g_tagsDrawn = 0;
static std::vector<menu::SaveEntry> g_saves; // the open load screen's list, refreshed at 2 Hz (256 strings a copy)
static ULONGLONG g_savesTick = 0;
static const std::vector<menu::SaveEntry>& CachedSaves()
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
    std::ofstream out(g_sidecar, std::ios::trunc);
    out << "; FLvibed: the mode of every save file (written by FLvibed, never read by the game): <file> = <mode> "
           "<speedrun clock ms> <checkpoint index>\n[Saves]\n";
    for (const auto& kv : g_recs)
        out << kv.first << " = " << ModeName(kv.second.mode) << ' ' << kv.second.runMs << ' ' << kv.second.cp << '\n';
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

static void Rescan()
{
    if (g_saveDir.empty())
        return;
    std::map<std::string, ULONGLONG> now;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((g_saveDir + "*.fl").c_str(), &fd);
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
            Log("newgame: save %s -> %s (run %u ms, cp %d)", kv.first.c_str(), ModeName(r.mode), r.runMs, r.cp);
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

// The front-end load screen (character select): the parent of LoadCharacter when it is that class; its list +0xD8.
static u32 CharSelect()
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
static void ApplySession(Mode m)
{
    g_session = m;
    if (m == SPEEDRUN || m == ASSISTED)
    {
        std::istringstream cmd(m == SPEEDRUN ? "preset speedrun" : "preset competent");
        const std::string r = ctl::AutopilotCommand(cmd); // `ap preset <name>`: the autopilot's own bundle
        Log("newgame: session %s: %s", ModeName(m), r.substr(0, 60).c_str());
    }
    events::Push("session mode=%s run=%u cp=%d", ModeName(m), g_runMs, g_cp);
}

void Init()
{
    g_sidecar = std::string(g_host && g_host->dllDir ? g_host->dllDir : "") + "FLvibed\\saves.ini";
    g_pendingSeen = LowerName(menu::PendingLoad()); // a value from before this logic started is not a load
    LoadSidecar();
    g_saveDir = SaveFolder();
    if (!g_saveDir.empty())
    {
        g_watch = FindFirstChangeNotificationA(g_saveDir.c_str(), FALSE,
                                               FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
        Rescan();
    }
    Log("newgame: %u save records, folder %s (watch %s)", static_cast<unsigned>(g_recs.size()),
        g_saveDir.empty() ? "unknown" : g_saveDir.c_str(), g_watch == INVALID_HANDLE_VALUE ? "off" : "on");
}

void Shutdown()
{
    if (g_watch != INVALID_HANDLE_VALUE)
        FindCloseChangeNotification(g_watch);
    g_watch = INVALID_HANDLE_VALUE;
}

void Update(float dt, const char* modeName)
{
    const std::string mode = modeName ? modeName : "";
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
            // the save about to load: a NEW value of the game's own pending-load CHARACTER_ID names it (the in-game load
            // screen's load and the death dialog's LOAD AUTOSAVE write it; exe::PendingLoadId, HOST integration 25 Oct
            // 2026 - verified: "SaveC702.fl" the moment `loadsave` runs; the game never clears it, not even at the main
            // menu, and the main menu's character select does not write it, so only a change counts), else the load
            // screen's selected row
            std::string pending = LowerName(menu::PendingLoad());
            if (!pending.empty() && pending != g_pendingSeen)
            {
                g_pendingSeen = pending;
                if (pending.size() < 3 || pending.compare(pending.size() - 3, 3, ".fl") != 0)
                    pending += ".fl"; // the records are keyed by the file name
                g_loading = pending, g_loadingTick = GetTickCount64();
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
        Log("newgame: main menu: session %s ended", ModeName(g_session));
        g_session = UNKNOWN;
        g_runMs = 0, g_cp = -1;
        events::Push("session mode=none");
    }
    // the mode transitions
    if (mode != g_lastMode)
    {
        if (g_lastMode == "menu" && mode != "menu")
        {
            const ULONGLONG now = GetTickCount64();
            if (g_pendingNew != UNKNOWN)
            {
                g_runMs = 0, g_cp = -1;
                ApplySession(g_pendingNew);
                Log("newgame: new game started as %s", ModeName(g_pendingNew));
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
                    ApplySession(it->second.mode);
                    Log("newgame: loaded %s -> %s (run %u ms, cp %d)", file.c_str(), ModeName(it->second.mode),
                        g_runMs, g_cp);
                }
                else if (!file.empty() || g_session == UNKNOWN)
                {
                    g_runMs = 0, g_cp = -1;
                    ApplySession(NORMAL);
                    Log("newgame: %s loaded without a record -> normal", file.empty() ? "a game" : file.c_str());
                }
            }
            g_loading.clear();
            g_deathDialog = false;
        }
        g_lastMode = mode;
    }
}

Mode Session() { return g_session; }
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

// ------------------------------------------------------------------ the prompt
struct Option
{
    const wchar_t* name;
    const wchar_t* tag;
    const wchar_t* desc;
};
static const Option kOptions[3] = {
    {L"NORMAL GAME", L"", L"The game as it is. Nothing of the autopilot shows until you switch it on once."},
    {L"ASSISTED", L"AUTOPILOT READY",
     L"The Advanced Autopilot can fly, fight and dock for you from the first mission (competent preset: it knows only "
     L"what you have seen). Switch it on and off any time."},
    {L"SPEEDRUN MODE", L"TIMER / SPLITS / LIVESPLIT",
     L"In-game timer, a split per story checkpoint, LiveSplit fed over TCP. The autopilot knows the whole game. The "
     L"choice is written into the save and locked for it."},
};

void OpenPrompt()
{
    g_prompt = true;
    g_pick = 0;
    Log("newgame: prompt open");
}
void ClosePrompt() { g_prompt = false; }
bool PromptOpen() { return g_prompt; }

static void Start(int pick)
{
    g_pendingNew = static_cast<Mode>(pick);
    livesplit::OnNewGame(pick == SPEEDRUN);
    const bool ok = menu::ClickMainButton(menu::kNewGame, &g_lastWhat);
    events::Push("newgame pick=%s click=%d", ModeName(g_pendingNew), ok ? 1 : 0);
    Log("newgame: start %s: %s", ModeName(g_pendingNew), g_lastWhat.c_str());
    if (!ok)
        g_pendingNew = UNKNOWN;
    g_prompt = false;
}

void PromptInput(const uikit::UiInput& in, const uikit::Input::PadExtra&)
{
    if (!g_prompt)
        return;
    if (in.up)
        g_pick = (g_pick + 2) % 3;
    if (in.down)
        g_pick = (g_pick + 1) % 3;
    if (in.accept)
        Start(g_pick);
    else if (in.back)
        ClosePrompt();
    static const promptbar::Prompt prompts[] = {{glyphs::A, L"START"}, {glyphs::B, L"BACK"}};
    promptbar::Set("mainmenu.newgame", prompts, 2, true, 10);
}

static std::vector<std::wstring> Wrap(const render::Font& f, const std::wstring& s, float width)
{
    std::vector<std::wstring> lines;
    std::wstring line, word;
    std::wistringstream ws(s);
    while (ws >> word)
    {
        const std::wstring probe = line.empty() ? word : line + L" " + word;
        if (!line.empty() && render::TextWidth(f, probe) > width)
            lines.push_back(line), line = word;
        else
            line = probe;
    }
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

static void DrawPrompt(const uikit::Fonts& f, float W, float H)
{
    const float w = H * 0.62f, rowH = f.label.lineHeight * 1.7f;
    const float descW = w - H * 0.06f;
    render::SetFont(f.body);
    const std::vector<std::wstring> desc = Wrap(f.body, kOptions[g_pick].desc, descW);
    const float h = H * 0.03f + f.title.lineHeight * 1.6f + 3 * rowH + H * 0.015f + desc.size() * f.body.lineHeight * 1.15f +
                    H * 0.03f;
    const float x0 = (W - w) * 0.5f, y0 = H * 0.5f - h * 0.5f;
    const double now = GetTickCount64() / 1000.0;
    uidraw::HudPanel(x0, y0, w, h, 0.96f, H, now);
    float y = y0 + H * 0.03f;
    render::SetFont(f.title);
    uidraw::TextShadow(x0 + (w - render::TextWidth(f.title, L"NEW GAME")) * 0.5f, y, L"NEW GAME", uidraw::kText, 1);
    y += f.title.lineHeight * 1.6f;
    render::SetFont(f.label);
    for (int i = 0; i < 3; ++i)
    {
        const bool sel = i == g_pick;
        if (sel)
            uidraw::FocusFrame(x0 + H * 0.02f, y, w - H * 0.04f, rowH, H, uidraw::kFrame);
        const u32 col = sel ? uidraw::kSelected : uidraw::kText;
        render::Text(x0 + H * 0.035f, y + (rowH - f.label.lineHeight) * 0.5f, kOptions[i].name, col);
        if (kOptions[i].tag[0])
        {
            render::SetFont(f.body);
            render::Text(x0 + w - H * 0.035f - render::TextWidth(f.body, kOptions[i].tag),
                         y + (rowH - f.body.lineHeight) * 0.5f, kOptions[i].tag, 0xFFB9C7D2);
            render::SetFont(f.label);
        }
        y += rowH;
    }
    y += H * 0.015f;
    render::SetFont(f.body);
    for (const std::wstring& l : desc)
    {
        render::Text(x0 + H * 0.03f, y, l, uidraw::kText);
        y += f.body.lineHeight * 1.15f;
    }
}

// ------------------------------------------------------------------ the tags on the load screens' rows
static void DrawTags(const uikit::Fonts& f, float H)
{
    g_tagsDrawn = 0;
    u32 list = 0, bar = 0;
    bool front = false;
    // the lists come from the hit-test probe, not the tree; the shared cache (was: a fresh probe every drawn frame)
    const std::vector<ui::Item>& items = ui::CollectAllCached();
    if (const u32 cs = CharSelect())
    {
        list = RdOr<u32>(cs + 0xD8, 0u);
        bar = uiclick::FindVisible("CharScroll");
        front = true;
    }
    else if (const u32 screen = menu::LoadScreen())
    {
        // the widest game-list child of the screen (the description column)
        float best = 0;
        for (const ui::Item& it : items)
            if (it.root == screen && exe::VtGameList.is(RdOr<u32>(static_cast<u32>(it.ctl), 0u)) && it.hw > best)
                best = it.hw, list = static_cast<u32>(it.ctl);
        bar = uiclick::FindVisible("LoadSave_Scroll");
    }
    if (!list)
        return;
    // the drawn rows (measured 25 Oct 2026 at 4K, HDE 0.7.1): a pitch of 0.029 H on both lists; the first row's text
    // centre 0.2486 H above the screen's middle on the front list, 0.2246 H in game (ui::ListRows' 0.0318 H front pitch
    // is the hit cell's of another layout: it drifts a row after a dozen rows here)
    const float rowH = H * 0.029f;
    const float firstY = H * 0.5f - H * (front ? 0.2486f : 0.2246f);
    const int first = bar ? static_cast<int>(RdOr<float>(bar + 0x3C8, 0.0f) + 0.5f) : 0;
    const std::vector<menu::SaveEntry>& saves = CachedSaves();
    // the list's rectangle (its screen item) for the right edge and the bottom
    float right = 0, bottom = 0;
    for (const ui::Item& it : items)
        if (it.ctl == list)
            right = it.x + it.hw, bottom = it.y + it.hh;
    if (right <= 0)
        return;
    render::SetFont(f.body);
    const float textDy = 0;
    for (int r = 0; r < 64; ++r)
    {
        const float y = firstY + r * rowH;
        if (y + rowH * 0.5f > bottom)
            break;
        const int idx = first + r;
        if (idx < 0 || idx >= static_cast<int>(saves.size()))
            continue;
        const Mode m = OfSave(saves[idx].id);
        if (m != ASSISTED && m != SPEEDRUN)
            continue;
        const wchar_t* tag = m == SPEEDRUN ? L"SPEEDRUN" : L"ASSISTED";
        const float tw = static_cast<float>(render::TextWidth(f.body, tag));
        const float x = right - tw - H * 0.012f, ty = y + textDy - f.body.lineHeight * 0.5f;
        render::Box(x - H * 0.004f, ty, tw + H * 0.008f, static_cast<float>(f.body.lineHeight),
                    m == SPEEDRUN ? 0xC0603010 : 0xC0104060);
        render::Text(x, ty, tag, m == SPEEDRUN ? 0xFFFFC860 : 0xFF8ACEF1);
        ++g_tagsDrawn;
    }
}

void Draw()
{
    const bool loadScreen = CharSelect() || menu::LoadScreen(); // (the in-game one opens from the pause menu, docked or in space)
    if (!g_prompt && !loadScreen)
        return;
    void* device = g_host && g_host->GetD3DDevice ? g_host->GetD3DDevice() : nullptr;
    if (!device)
        return;
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    const uikit::Fonts& f = uikit::FontsFor(sh, uiopt::Get().textSize);
    if (!f.ok || !render::Begin(f.label))
        return;
    if (loadScreen)
        DrawTags(f, static_cast<float>(sh));
    if (g_prompt)
        DrawPrompt(f, static_cast<float>(sw), static_cast<float>(sh));
    render::End(false);
}

std::string Status()
{
    std::ostringstream o;
    o << "newgame session=" << ModeName(g_session) << " pending=" << ModeName(g_pendingNew) << " run=" << g_runMs
      << " cp=" << g_cp << " prompt=" << g_prompt << " pick=" << g_pick << " records=" << g_recs.size()
      << " files=" << g_files.size() << " loading=" << (g_loading.empty() ? "-" : g_loading)
      << " mainmenu=" << g_mainMenu << " tags=" << g_tagsDrawn << " dir=" << g_saveDir;
    return o.str();
}

std::string Command(std::istream& in)
{
    std::string sub;
    in >> sub;
    if (sub == "open")
    {
        OpenPrompt();
        return Status();
    }
    if (sub == "close")
    {
        ClosePrompt();
        return Status();
    }
    if (sub == "pick") // newgame pick <0..2>: as A on that entry (the game's New Game runs)
    {
        int n = 0;
        in >> n;
        if (n < 0 || n > 2)
            return "pick 0 normal | 1 assisted | 2 speedrun";
        g_prompt = true;
        Start(n);
        return g_lastWhat + " | " + Status();
    }
    if (sub == "mode") // newgame mode <0..2>: force the session's mode (tests)
    {
        int n = 0;
        in >> n;
        ApplySession(static_cast<Mode>(n < 0 ? -1 : n > 2 ? 2 : n));
        return Status();
    }
    if (sub == "records")
    {
        std::ostringstream o;
        for (const auto& kv : g_recs)
            o << kv.first << " " << ModeName(kv.second.mode) << " " << kv.second.runMs << " " << kv.second.cp << "\n";
        return o.str() + Status();
    }
    if (sub == "rescan")
    {
        Rescan();
        return Status();
    }
    return Status() + "\nusage: newgame open|close|pick <n>|mode <n>|records|rescan|state";
}
} // namespace newgame
