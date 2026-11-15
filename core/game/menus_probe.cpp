// FLvibed - the menus' proving experiment (audit 70b work order W6, results audit_2026-11-12\82_menus_probe.md): the debug
// verb `menuprobe`, test tooling only, reached through the debug channel (which exists only with the test switch,
// core\common.h TestSwitch) and refusing anyway without it.
//
// It answers, with NO load / save window open, what the game's load / save window (NN_LoadSave) would list and refuse,
// and it saves, overwrites, deletes and loads by the same calls with the same arguments as the window's handlers (read in
// Ghidra: audit 82a the list and the gates, 82b the load, 82c the options, the pause count and the confirm box). With
// the window open it reads the window's own rows beside them, so the two can be compared line by line.
//
//   menuprobe state                       the gates' inputs: single player, the docked base, scenes idle, the pause
//                                         count, GamePaused, UiBusy, world loading, the cursor, the requested game
//                                         state, the pending load, which windows are open; then "may" for each action
//   menuprobe list                        the save list from the client's own store (CharSession +0x1B8) as the window
//                                         shows it: file, description, time, system, rank, credits, ship, autosave
//   menuprobe window                      the open NN_LoadSave's own rows and fields (DELETE's enabled byte, the confirm)
//   menuprobe rows                        the open save list's two columns (front or in game): rows held / drawn, box and
//                                         line height in pixels, the rows that fit; the shared scroll bar
//   menuprobe compare                     list vs window: EQUAL or the lines that differ
//   menuprobe select <file>               the open window's own row click (vt[0x5C](0xF001, 0, row)), for comparing
//                                         the window's SAVE over a row and DELETE with the probe's
//   menuprobe refetch                     CharListFetch, as the window does after its save and delete
//   menuprobe may save|overwrite|delete|load [file]   the refusal the game's route would give (0 = allowed)
//   menuprobe save [description]          SAVE with a typed description (empty = the window's "save<N>" template)
//   menuprobe overwrite <file>            SAVE over a listed save (the window: its row selected, its description kept)
//   menuprobe delete <file>               DELETE (the window's YES on its confirm box)
//   menuprobe load <file> [keep]          the load experiment: LoadGame(record, record +0x201), then, when the probe
//                                         holds a modal count (`modal on`), its release (LeaveModal) as the window's
//                                         load-by-index does - unless `keep` (the 17 Oct 2026 failure's suspected state)
//   menuprobe modal on|off                take / release one pause count as a window does (EnterModal / LeaveModal)
//   menuprobe opt list | get <name> | set <name> <value> | write    the game's options, as the Options screen sets them
// Single player only: the window offers no load or save in multiplayer.
#include "common.h"
#include "flexe.h"
#include "hook.h"
#include "log.h"
#include "safemem.h"
#include "game/coreframe.h"
#include "game/fl.h"
#include "game/iserver.h"
#include "game/menu.h"
#include "game/room.h"
#include "game/shipname.h"
#include "game/space.h"
#include "text.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace debug
{
namespace
{
bool g_modal = false, g_modalCursor = false; // the probe holds one pause count (`modal on`) and the cursor it found

// ---- guarded reads
u32 R32(uintptr_t a)
{
    u32 v = 0;
    return a && safemem::Read(a, &v, 4) ? v : 0;
}
u8 R8(uintptr_t a)
{
    u8 v = 0;
    return a && safemem::Read(a, &v, 1) ? v : 0;
}
float RF(uintptr_t a)
{
    float v = 0;
    return a && safemem::Read(a, &v, 4) ? v : 0.0f;
}
std::string RStr(uintptr_t a, size_t max)
{
    std::string s;
    for (size_t i = 0; a && i < max; ++i)
    {
        const char c = static_cast<char>(R8(a + i));
        if (!c)
            break;
        s += c;
    }
    return s;
}
std::wstring RWStr(uintptr_t a, size_t max)
{
    std::wstring s;
    for (size_t i = 0; a && i < max; ++i)
    {
        wchar_t c = 0;
        if (!safemem::Read(a + 2 * i, &c, 2) || !c)
            break;
        s += c;
    }
    return s;
}
std::string N(const std::wstring& w) { return text::Utf8(w); }
std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

// ---- the save list (CharSession +0x1B8: an std::list, node {next, prev, record}; FREELANCER_RE "The save list")
// A record (0x2DC bytes, filled by the list's callback 0x5AA9C0 from the server's answer):
constexpr u32 kRecHidden = 0x200;   // Restart.fl: never listed
constexpr u32 kRecAuto = 0x201;     // AutoSave.fl: listed as "AUTO SAVE", no save over it, no delete
constexpr u32 kRecDesc = 0x218;     // wchar_t* description (typed)
constexpr u32 kRecDescIds = 0x224;  // the description's IDS (the autosave 1628, a mission's Act_Save), 0 = typed
constexpr u32 kRecTime = 0x228;     // FILETIME, already local time (the server writes GetLocalTime)
constexpr u32 kRecShip = 0x230;     // ship archetype
constexpr u32 kRecCredits = 0x234;  // seen in game 12 Nov 2026: SaveC701's money (500000) here, the rank at +0x270
constexpr u32 kRecSystem = 0x260;
constexpr u32 kRecRank = 0x270;
constexpr u32 kListRows = 0x100;    // the window shows at most 256 rows (0x4877F0)

struct Rec
{
    uintptr_t at = 0;
    std::string file;
    bool hidden = false, autosave = false;
    std::wstring desc, time;
    u32 system = 0, rank = 0, credits = 0, ship = 0, descIds = 0;
};

// The description as the window shows it (0x488B70): the IDS's text when +0x224 is set, else the typed text.
std::wstring DescOf(uintptr_t r)
{
    const u32 ids = R32(r + kRecDescIds);
    if (ids)
        return text::Ids(ids);
    return RWStr(R32(r + kRecDesc), 64);
}
// The time as the window shows it: Common.dll filetime_to_wstring (0x6303360): "" for 0, else the user's short date,
// a space and the 24-hour time without seconds, no UTC conversion (audit 82a section 1).
std::wstring TimeOf(uintptr_t r)
{
    FILETIME ft{};
    if (!safemem::Read(r + kRecTime, &ft, sizeof ft) || (!ft.dwLowDateTime && !ft.dwHighDateTime))
        return L"";
    SYSTEMTIME st{};
    if (!FileTimeToSystemTime(&ft, &st))
        return L"";
    char d[64] = {}, t[64] = {};
    GetDateFormatA(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, d, sizeof d);
    GetTimeFormatA(LOCALE_USER_DEFAULT, TIME_NOSECONDS | TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT, &st, nullptr, t, sizeof t);
    const std::string s = std::string(d) + " " + t;
    wchar_t w[160] = {};
    mbstowcs(w, s.c_str(), 159);
    return w;
}
std::vector<Rec> Records()
{
    std::vector<Rec> out;
    const uintptr_t head = R32(exe::SaveList.get());
    if (!exe::SaveList.ok() || !head)
        return out;
    for (uintptr_t n = R32(head); n && n != head && out.size() < 4096; n = R32(n))
    {
        const uintptr_t r = R32(n + 8);
        if (!r)
            continue;
        Rec x;
        x.at = r;
        x.file = RStr(r, 0x200);
        x.hidden = R8(r + kRecHidden) != 0;
        x.autosave = R8(r + kRecAuto) != 0;
        x.desc = DescOf(r);
        x.descIds = R32(r + kRecDescIds);
        x.time = TimeOf(r);
        x.system = R32(r + kRecSystem);
        x.rank = R32(r + kRecRank);
        x.credits = R32(r + kRecCredits);
        x.ship = R32(r + kRecShip);
        out.push_back(x);
    }
    return out;
}
// The rows the window would show: hidden records skipped, the list's order, at most 256 (0x4877F0).
std::vector<Rec> Rows()
{
    std::vector<Rec> rows;
    for (const Rec& r : Records())
        if (!r.hidden && rows.size() < kListRows)
            rows.push_back(r);
    return rows;
}
bool SameFile(const std::string& a, std::string b)
{
    std::string x = Lower(a);
    b = Lower(b);
    if (b.size() < 3 || b.compare(b.size() - 3, 3, ".fl") != 0)
        b += ".fl";
    const size_t slash = x.find_last_of("\\/");
    if (slash != std::string::npos)
        x = x.substr(slash + 1);
    return x == b;
}
const Rec* Find(const std::vector<Rec>& rows, const std::string& file)
{
    for (const Rec& r : rows)
        if (SameFile(r.file, file))
            return &r;
    return nullptr;
}

// The system's name (Universe::get_system: ISystem +0x4C id, +0x68 strid_name, as core\game\baseinfo.cpp reads it).
uintptr_t SystemOf(u32 sys)
{
    static const u8*(__cdecl * getSystem)(u32) = reinterpret_cast<const u8*(__cdecl*)(u32)>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?get_system@Universe@@YAPBUISystem@1@I@Z"));
    if (!getSystem || !sys)
        return 0;
    __try
    {
        return reinterpret_cast<uintptr_t>(getSystem(sys));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}
std::wstring SystemName(u32 sys)
{
    const uintptr_t a = SystemOf(sys);
    return a && R32(a + 0x4C) == sys ? text::Ids(R32(a + 0x68)) : L"";
}

// The window's "save<N>" template for an empty description (0x4877F0 / 0x487550): IDS 1629 ("save") followed by one
// more than the highest N among the rows whose description is the template's text followed by a whole decimal number
// (N <= 1,000,000,000; case-sensitive; "save" alone counts nothing).
std::wstring TemplateText(const std::vector<Rec>& rows)
{
    const std::wstring base = text::Ids(1629);
    long max = 0;
    for (const Rec& r : rows)
    {
        if (base.empty() || r.desc.size() <= base.size() || r.desc.compare(0, base.size(), base) != 0)
            continue;
        wchar_t* end = nullptr;
        const long v = wcstol(r.desc.c_str() + base.size(), &end, 10);
        if (end && !*end && v <= 1000000000 && v > max)
            max = v;
    }
    return base + std::to_wstring(max + 1);
}

// ---- the gates (audit 82a section 4)
bool Docked() { return R32(exe::DockedBaseId.get()) != 0; }
bool ScenesIdle()
{
    if (!exe::ScriptsIdle.ok() || !exe::ThornScripts.ok())
        return false;
    __try
    {
        return hk::Call<bool(__thiscall*)(uintptr_t)>(exe::ScriptsIdle, exe::ThornScripts.get());
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
int PauseCount() { return static_cast<int>(R32(exe::PauseCount.get())); }
bool Paused() { return fl::GamePaused(); }
bool LoadPending() { return R32(exe::GameStateReq.get()) == 7; }
bool FrontEnd() { return coreframe::g_mode == gamemode::Mode::Menu; }
bool UiBusyOrLoading() { return R8(exe::UiBusy.get()) || R8(exe::WorldLoading.get()); }

// The open load / save window (NN_LoadSave), in every mode: the owner of its Load button (load modes) or of its Save
// button (save mode, where Load is hidden), checked by its vtable.
u32 OwnerOf(u32 ctl)
{
    __try
    {
        const u32 parent = ctl ? R32(ctl + 4) : 0;
        typedef u32(__thiscall * GetFn)(u32);
        return parent ? reinterpret_cast<GetFn>((*reinterpret_cast<void***>(parent))[8 / 4])(parent) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}
u32 LoadSaveWindow()
{
    u32 w = menu::LoadScreen();
    if (!w)
        w = OwnerOf(uiclick::FindVisible("LoadSave_SaveGame"));
    return w && exe::VtGameLoadScreen.is(R32(w)) ? w : 0;
}
// The pause counts other windows hold: the count less the probe's own (it stands for the page's own pause, as
// NN_Pause's own +1 is its own), less an open NN_Pause's (the menu the window is opened from) and an open
// NN_LoadSave's (the window itself, compared beside the probe).
int OtherCounts()
{
    return PauseCount() - (g_modal ? 1 : 0) - (menu::PauseMenu() ? 1 : 0) - (LoadSaveWindow() ? 1 : 0);
}
// A row click in the open window, exactly as its list sends it: vt[0x5C](0xF001, item, row); 0x486CA0 reads only the
// row (p2 unused), range-checks it, writes the selection +0xE88 / +0x1744 and fills the info panel and, in save mode,
// the edit field with the row's description (0x487CB0; audit 82_ "row click").
bool SelectRow(u32 w, int row)
{
    __try
    {
        typedef bool(__thiscall * Fn)(u32, u32, u32, int);
        return reinterpret_cast<Fn>((*reinterpret_cast<void***>(w))[0x5C / 4])(w, 0xF001, 0, row), true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The refusals, 0 = allowed. A page may only send what the game's own route would send (the lead's rule):
//   save      the pause menu's Save exists only in single player, docked, with no scene running (0x4A6BA0), and the
//             server refuses a save without a ship docked anyway (Server.dll 0x6D459D0)
//   overwrite as save, and the window disables SAVE on the autosave row (0x487CB0)
//   delete    the window disables DELETE on the autosave row; the window exists only through the pause menu or a
//             death dialog (mode 3 hides DELETE)
//   load      single player (no Load in multiplayer), a listed row, no load already requested; never from the front
//             end: there the game loads through its character select (0x56C360 -> 0x5743A0 -> CharacterSelect), and
//             LoadGame there brought every room up without characters until a restart (seen 12 Nov 2026, audit 82_)
//   every one: the window is reached only through the pause menu, which does not open while UiBusy or the world
//             loading flag is set or another window holds a pause count (OpenPauseMenu 0x4A75C0)
enum Refusal
{
    kOk = 0,
    kMultiplayer = 1,
    kNotDocked = 2,
    kScene = 3,
    kNoRow = 4,
    kAutosaveRow = 5,
    kLoadPending = 6,
    kSymbols = 7,
    kFrontEnd = 8,
    kOtherWindow = 9,
    kBusy = 10,
};
const char* RefusalName(int r)
{
    static const char* const k[] = {"ok", "multiplayer", "not docked", "a scene runs", "no such row", "autosave row",
                                    "a load is pending", "a symbol is unresolved", "front end", "another window holds a pause count",
                                    "UI busy or the world loading"};
    return r >= 0 && r <= 10 ? k[r] : "?";
}
int May(const std::string& what, const std::string& file)
{
    if (!exe::ServerIface.ok() || !exe::OwnClientId.ok() || !exe::CharSession.ok() || !exe::SaveList.ok() ||
        !exe::DockedBaseId.ok() || !exe::LoadGame.ok() || !exe::UiBusy.ok() || !exe::WorldLoading.ok())
        return kSymbols;
    if (space::Multiplayer())
        return kMultiplayer;
    if (FrontEnd())
        return kFrontEnd; // DockedBaseId keeps the last base there (seen 12 Nov 2026), so this test comes first
    if (UiBusyOrLoading())
        return kBusy;
    if (OtherCounts() > 0)
        return kOtherWindow;
    const std::vector<Rec> rows = Rows();
    const Rec* r = file.empty() ? nullptr : Find(rows, file);
    if (what == "save" || what == "overwrite")
    {
        if (!Docked())
            return kNotDocked;
        if (!ScenesIdle())
            return kScene;
        if (what == "overwrite")
        {
            if (!r)
                return kNoRow;
            if (r->autosave)
                return kAutosaveRow;
        }
        return kOk;
    }
    if (what == "delete")
    {
        if (!r)
            return kNoRow;
        return r->autosave ? kAutosaveRow : kOk;
    }
    if (what == "load")
    {
        if (!r)
            return kNoRow;
        return LoadPending() ? kLoadPending : kOk;
    }
    return kNoRow;
}

// ---- the calls, each exactly as the window's handler makes it
void* Server() { return reinterpret_cast<void*>(R32(exe::ServerIface.get())); }
u32 Client() { return R32(exe::OwnClientId.get()); }
bool Refetch()
{
    __try
    {
        return exe::CharListFetch.ok() &&
               hk::Call<int(__thiscall*)(uintptr_t)>(exe::CharListFetch, exe::CharSession.get()) == 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
// IServer::SaveGame (vt+0xE8): (const CHARACTER_ID*, const wchar_t* description, client), thiscall on the server; the
// window passes a pointer to its 0x200-byte id copy (empty = a new SaveXXXX file) and its typed text (0x48730B..).
bool SaveGameCall(const char (&id)[0x200], const wchar_t* desc)
{
    void* s = Server();
    if (!s)
        return false;
    __try
    {
        typedef void(__thiscall * Fn)(void*, const char*, const wchar_t*, u32);
        reinterpret_cast<Fn>((*reinterpret_cast<void***>(s))[iserver::kSaveGame / 4])(s, id, desc, Client());
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
bool DestroyCall(const char (&id)[0x200])
{
    __try
    {
        return exe::CharDestroy.ok() &&
               hk::Call<int(__thiscall*)(uintptr_t, const char*)>(exe::CharDestroy, exe::CharSession.get(), id) == 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
bool LoadCall(uintptr_t record, bool skipAutosave)
{
    __try
    {
        hk::Call<void(__cdecl*)(uintptr_t, bool)>(exe::LoadGame, record, skipAutosave);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// ---- the probe's own modal count (as a window holds one while open; g_modal is declared at the top)
bool ModalOn()
{
    if (g_modal || !exe::EnterModal.ok())
        return false;
    __try
    {
        g_modalCursor = hk::Call<bool(__cdecl*)()>(exe::EnterModal);
        g_modal = true;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
bool ModalOff()
{
    if (!g_modal || !exe::LeaveModal.ok())
        return false;
    __try
    {
        hk::Call<void(__cdecl*)(bool)>(exe::LeaveModal, g_modalCursor);
        g_modal = false;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void PutState(std::ostringstream& o)
{
    const u32 ls = LoadSaveWindow();
    o << "mp=" << space::Multiplayer() << " docked_base=0x" << std::hex << R32(exe::DockedBaseId.get()) << std::dec
      << " scenes_idle=" << ScenesIdle() << " pause_count=" << PauseCount() << " paused=" << Paused()
      << " ui_busy=" << static_cast<int>(R8(exe::UiBusy.get())) << " world_loading=" << static_cast<int>(R8(exe::WorldLoading.get()))
      << " cursor=" << static_cast<int>(R8(exe::CursorShown.get())) << " state_req=" << R32(exe::GameStateReq.get())
      << " pending=\"" << RStr(exe::PendingLoadId.get(), 0x200) << "\" probe_modal=" << g_modal
      << " loadsave=" << (ls ? 1 : 0) << " loadsave_mode=" << (ls ? R32(ls + 0x334) : 0)
      << " pause_menu=" << (menu::PauseMenu() ? 1 : 0) << " front_end=" << FrontEnd() << " other_counts=" << OtherCounts()
      << " rows=" << Rows().size() << "\n";
    o << "may save=" << May("save", "") << " (" << RefusalName(May("save", "")) << ")\n";
}

void PutRow(std::ostringstream& o, int i, const Rec& r)
{
    o << i << "\t" << r.file << "\t" << (r.autosave ? "auto" : "-") << "\t\"" << N(r.desc) << "\"\t\"" << N(r.time)
      << "\"\tsys=" << N(SystemName(r.system)) << "\trank=" << r.rank << "\tcredits=" << r.credits
      << "\tship=" << N(shipname::Name(r.ship)) << "\n";
}

// The open window's rows (NN_LoadSave +0x344..+0x348, 0x24-byte entries: +0 the record, +8 the time text, +0x18 the
// description text: the two columns it shows; audit 82a section 0).
struct WinRow
{
    uintptr_t record;
    std::wstring desc, time;
};
std::vector<WinRow> WindowRows(u32 w)
{
    std::vector<WinRow> v;
    const uintptr_t b = R32(w + 0x344), e = R32(w + 0x348);
    const u32 shown = R32(w + 0xE80);
    for (uintptr_t p = b; b && p + 0x24 <= e && v.size() < shown && v.size() < 4096; p += 0x24)
        v.push_back({R32(p), RWStr(R32(p + 0x18), 64), RWStr(R32(p + 8), 64)});
    return v;
}

// ---- the options (audit 82c): the list at OptionList +4, node {next, prev, option*}
struct Opt
{
    uintptr_t at;
    std::string name;
    u32 ids;
    float pending, value;
};
std::vector<Opt> Options()
{
    std::vector<Opt> v;
    const uintptr_t head = R32(exe::OptionList.get() + 4);
    if (!exe::OptionList.ok() || !head)
        return v;
    for (uintptr_t n = R32(head); n && n != head && v.size() < 256; n = R32(n))
    {
        const uintptr_t o = R32(n + 8);
        if (o)
            v.push_back({o, RStr(R32(o + 4), 64), R32(o + 8), RF(o + 0x10), RF(o + 0x14)});
    }
    return v;
}
// Setting one value as the Options screen does (0x4B1450 and its siblings, inlined there): pending := v; vt[0](v);
// if value != pending { value := pending; vt[1](v) }.
bool OptSet(uintptr_t o, float v)
{
    __try
    {
        typedef void(__thiscall * Fn)(uintptr_t, float);
        void** vt = *reinterpret_cast<void***>(o);
        *reinterpret_cast<float*>(o + 0x10) = v;
        reinterpret_cast<Fn>(vt[0])(o, v);
        if (*reinterpret_cast<float*>(o + 0x14) != *reinterpret_cast<float*>(o + 0x10))
        {
            *reinterpret_cast<float*>(o + 0x14) = *reinterpret_cast<float*>(o + 0x10);
            reinterpret_cast<Fn>(vt[1])(o, v);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
bool OptWrite()
{
    __try
    {
        hk::Call<void(__cdecl*)(const char*)>(exe::PerfOptionsWrite, "PerfOptions.ini");
        return exe::PerfOptionsWrite.ok();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

std::string RestOf(std::istream& in)
{
    std::string s;
    std::getline(in, s);
    const size_t a = s.find_first_not_of(' ');
    return a == std::string::npos ? "" : s.substr(a);
}
} // namespace
} // namespace debug

// The verb (debugverbs.cpp's dispatch declares it at its call, so it lives in the global namespace as DealProbe does).
// Test tooling: refuses without the test switch.
std::string MenuProbe(std::istream& in)
{
    using namespace debug;
    if (!TestSwitch())
        return "menuprobe: test games only";
    std::string what;
    in >> what;
    std::ostringstream o;
    if (what == "state" || what.empty())
    {
        PutState(o);
        return o.str();
    }
    if (what == "rows") // the open load / save list's columns: rows held / drawn / first, box and line in pixels, the bar
    {
        menu::SaveLists s;
        if (!menu::OpenSaveLists(s))
            return "menuprobe: no save list open";
        o << (s.front ? "front" : "game") << " gameRows " << s.gameRows;
        for (const menu::SaveColumn* c : {&s.names, &s.dates})
        {
            const float room = s.scrollPx > 0 && s.scrollPx < c->boxPx ? s.scrollPx : c->boxPx; // (the panel's room: the bar, or the box)
            o << "\n" << (c == &s.names ? "names" : "dates") << " count " << c->count << " rows " << c->rows << " first " << c->first
              << " box " << c->boxPx << " room " << room << " line " << c->linePx << " drawn " << c->rows * c->linePx << " fits "
              << static_cast<int>(room / c->linePx + 0.05f) << " height " << c->height;
        }
        if (s.scroll)
            o << "\nscroll px " << s.scrollPx << " shown " << ((R32(s.scroll + 0x6C) & 3) == 3) << " range " << RF(s.scroll + 0x3C0) << ".." << RF(s.scroll + 0x3C4)
              << " page " << RF(s.scroll + 0x3CC) << " value " << RF(s.scroll + 0x3C8);
        return o.str();
    }
    if (what == "list")
    {
        const std::vector<Rec> rows = Rows();
        int i = 0;
        for (const Rec& r : rows)
            PutRow(o, i++, r);
        o << "rows " << rows.size() << " (records " << Records().size() << ") template \"" << N(TemplateText(rows)) << "\"";
        return o.str();
    }
    if (what == "select")
    {
        std::string file;
        in >> file;
        const u32 w = LoadSaveWindow();
        if (!w)
            return "menuprobe: no load / save window open";
        const std::vector<WinRow> wr = WindowRows(w);
        int row = -1;
        for (size_t i = 0; i < wr.size(); ++i)
            if (SameFile(RStr(wr[i].record, 0x200), file))
                row = static_cast<int>(i);
        if (row < 0)
            return "menuprobe: no row " + file + " in the window";
        const bool ok = SelectRow(w, row);
        o << "select " << file << " row " << row << " " << (ok ? "sent" : "faulted") << " selected "
          << static_cast<int>(R32(w + 0xE88)) << " / " << static_cast<int>(R32(w + 0x1744));
        return o.str();
    }
    if (what == "window" || what == "compare")
    {
        const u32 w = LoadSaveWindow();
        if (!w)
            return "menuprobe: no load / save window open";
        const std::vector<WinRow> wr = WindowRows(w);
        if (what == "window")
        {
            // (+0x5D6: DELETE enabled, written by 0x487CB0 from a row select; +0x338: the confirm pending, 1 = delete)
            o << "mode " << R32(w + 0x334) << " selected " << static_cast<int>(R32(w + 0xE88)) << " delete_on "
              << static_cast<int>(R8(w + 0x5D6)) << " confirm " << R32(w + 0x338) << " template_next "
              << R32(w + 0x364) << " rows " << wr.size() << "\n";
            for (size_t i = 0; i < wr.size(); ++i)
                o << i << "\t" << RStr(wr[i].record, 0x200) << "\t\"" << N(wr[i].desc) << "\"\t\"" << N(wr[i].time) << "\"\n";
            return o.str();
        }
        const std::vector<Rec> rows = Rows();
        int diff = 0;
        for (size_t i = 0; i < wr.size() || i < rows.size(); ++i)
        {
            const bool hasW = i < wr.size(), hasL = i < rows.size();
            const std::string wf = hasW ? RStr(wr[i].record, 0x200) : "", lf = hasL ? rows[i].file : "";
            if (!hasW || !hasL || wf != lf || wr[i].desc != rows[i].desc || wr[i].time != rows[i].time)
            {
                ++diff;
                o << "DIFF " << i << " window[" << wf << " \"" << (hasW ? N(wr[i].desc) : "") << "\" \""
                  << (hasW ? N(wr[i].time) : "") << "\"] list[" << lf << " \"" << (hasL ? N(rows[i].desc) : "")
                  << "\" \"" << (hasL ? N(rows[i].time) : "") << "\"]\n";
            }
        }
        // the template's number: the window's +0x364 against the probe's
        const std::wstring t = TemplateText(rows), base = text::Ids(1629);
        const u32 wn = R32(w + 0x364);
        const bool tEq = t == base + std::to_wstring(wn);
        o << (diff || !tEq ? "DIFFER" : "EQUAL") << " rows window " << wr.size() << " list " << rows.size()
          << " differing " << diff << " template window " << wn << " probe \"" << N(t) << "\"";
        return o.str();
    }
    if (what == "refetch")
        return Refetch() ? "refetched rows " + std::to_string(Rows().size()) : "refetch failed";
    if (what == "may")
    {
        std::string act, file;
        in >> act >> file;
        const int r = May(act, file);
        o << "may " << act << " " << file << " = " << r << " (" << RefusalName(r) << ")";
        return o.str();
    }
    if (what == "save" || what == "overwrite")
    {
        std::string file;
        std::wstring desc;
        char id[0x200] = {};
        if (what == "overwrite")
        {
            in >> file;
            const int r = May("overwrite", file);
            if (r)
                return "menuprobe: refused " + std::string(RefusalName(r));
            const std::vector<Rec> rows = Rows();
            const Rec* rec = Find(rows, file);
            // the window's SAVE on a selected row: the id is that record's (its first 0x200 bytes), the text the
            // row's own description (a different text would make a new file instead)
            if (!safemem::Read(rec->at, id, sizeof id))
                return "menuprobe: unreadable record";
            desc = rec->desc;
        }
        else
        {
            const int r = May("save", "");
            if (r)
                return "menuprobe: refused " + std::string(RefusalName(r));
            const std::string typed = RestOf(in);
            wchar_t w[0x1F] = {};
            MultiByteToWideChar(CP_UTF8, 0, typed.c_str(), -1, w, 0x1E); // the edit field keeps 30 characters
            w[0x1E] = 0;
            desc = w[0] ? std::wstring(w) : TemplateText(Rows());
        }
        const size_t before = Records().size();
        if (!SaveGameCall(id, desc.c_str()))
            return "menuprobe: SaveGame faulted";
        const bool fetched = Refetch();
        o << what << " \"" << N(desc) << "\" id \"" << id << "\" refetch " << fetched << " records " << before << " -> "
          << Records().size();
        return o.str();
    }
    if (what == "delete")
    {
        std::string file;
        in >> file;
        const int r = May("delete", file);
        if (r)
            return "menuprobe: refused " + std::string(RefusalName(r));
        const std::vector<Rec> rows = Rows();
        char id[0x200] = {};
        if (!safemem::Read(Find(rows, file)->at, id, sizeof id))
            return "menuprobe: unreadable record";
        const size_t before = Records().size();
        const bool ok = DestroyCall(id); // CharDestroy waits for the server's answer itself; the window then refetches
        const bool fetched = Refetch();
        o << "delete " << id << " destroy " << ok << " refetch " << fetched << " records " << before << " -> "
          << Records().size();
        return o.str();
    }
    if (what == "load")
    {
        std::string file, opt;
        in >> file >> opt;
        const int r = May("load", file);
        if (r)
            return "menuprobe: refused " + std::string(RefusalName(r));
        const std::vector<Rec> rows = Rows();
        const Rec* rec = Find(rows, file);
        o << "before: ";
        PutState(o);
        const bool skipAuto = rec->autosave; // the window passes record +0x201
        if (!LoadCall(rec->at, skipAuto))
            return "menuprobe: LoadGame faulted";
        // what the window's load-by-index 0x4880F0 does next with its own count: LeaveModal (its window teardown
        // otherwise only frees its controls)
        const bool released = opt != "keep" && g_modal && ModalOff();
        o << "load " << rec->file << " skipAutosave " << skipAuto << " released " << released << " after: ";
        PutState(o);
        return o.str();
    }
    if (what == "modal")
    {
        std::string on;
        in >> on;
        const bool ok = on == "on" ? ModalOn() : on == "off" ? ModalOff() : false;
        o << "modal " << on << " " << (ok ? "ok" : "refused") << " pause_count=" << PauseCount() << " paused=" << Paused()
          << " cursor=" << static_cast<int>(R8(exe::CursorShown.get()));
        return o.str();
    }
    if (what == "opt")
    {
        std::string sub, name;
        in >> sub >> name;
        if (!exe::OptionList.ok())
            return "menuprobe: OptionList unresolved";
        const std::vector<Opt> opts = Options();
        if (sub == "list")
        {
            for (const Opt& x : opts)
                o << x.name << "\tids=" << x.ids << "\tpending=" << x.pending << "\tvalue=" << x.value << "\n";
            o << "options " << opts.size();
            return o.str();
        }
        if (sub == "write")
            return OptWrite() ? "written PerfOptions.ini" : "menuprobe: write failed";
        for (const Opt& x : opts)
            if (x.name == name)
            {
                if (sub == "get")
                    o << x.name << " pending=" << x.pending << " value=" << x.value;
                else if (sub == "set")
                {
                    float v = 0;
                    in >> v;
                    o << x.name << " set " << v << " " << (OptSet(x.at, v) ? "ok" : "faulted") << " value=" << RF(x.at + 0x14);
                }
                return o.str();
            }
        return "menuprobe: no option " + name;
    }
    return "menuprobe state | list | window | compare | select <file> | refetch | may <act> [file] | save [desc] | overwrite <file> | "
           "delete <file> | load <file> [keep] | modal on|off | opt list|get|set|write";
}
