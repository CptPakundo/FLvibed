// FLvibed's one-click installer and uninstaller for players: one self-contained exe (FLvibed-<version>-Setup.exe) that
// carries the release FLvibed.dll and the shipped data as resources (setup.rc), finds the game, and installs, updates or
// uninstalls. Built by `build.cmd setup` after `build.cmd release` (tools\package.ps1 does both).
//
// Install = the steps of the README / tools\deploy.ps1: the files into EXE\ (FLvibed.dll, FLSubtitles\*.tsv,
// FLvibed\autopilot.3db), the line FLvibed.dll in EXE\dacom.ini [Libraries] after the plugins already listed, a default
// FLvibed.ini only when there is none (the player's settings stay), a copy of this exe as the uninstaller
// (EXE\FLvibed\Uninstall FLvibed.exe) and an entry in Windows' installed apps (per user, HKCU). No game file other than
// dacom.ini is touched; dacom.ini is backed up once to EXE\FLvibed\dacom.ini.before-flvibed.
// Uninstall = the dacom.ini line out, FLvibed's files out, the settings files out unless the player keeps them, the entry
// gone. Windows-only, no runtime (static CRT), no administrator rights unless the game folder needs them (then it asks).
//
// Command line (tests, scripts): /install <game folder> | /uninstall <game folder> [/keepsettings] [/quiet] [/log <file>]
// /quiet shows no window; the exit code is 0 on success. Without arguments: the dialog.
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cwchar>
#include <iterator>
#include <string>
#include <vector>

#include "version.h"

#define W2(x) L##x
#define WIDEN(x) W2(x)
static const wchar_t* kVersion = WIDEN(FLV_VERSION);
static const wchar_t* kTitle = L"FLvibed " WIDEN(FLV_VERSION) L" Setup";
static const wchar_t* kHdeUninstallKey = L"{F40FDCDA-3A45-4CC3-9FDA-167EE480A1E0}_is1"; // HD Edition's Inno Setup AppId
static const wchar_t* kUninstallerName = L"Uninstall FLvibed.exe";

// The payload: resource id -> path under EXE\ (setup.rc embeds the same files under these ids).
struct Payload { int id; const wchar_t* path; };
static const Payload kPayload[] = {
    {101, L"FLvibed.dll"},
    {102, L"FLSubtitles\\subtitles.tsv"},
    {103, L"FLSubtitles\\movies.tsv"},
    {104, L"FLSubtitles\\movie_frames.tsv"},
    {105, L"FLSubtitles\\speaker_colors.tsv"},
    {106, L"FLSubtitles\\word_times.tsv"},
    {107, L"FLvibed\\autopilot.3db"},
};
// Files FLvibed writes in EXE\ while it runs: its settings (kept on request) and its logs (always removed).
static const wchar_t* kSettings[] = {L"FLvibed.ini", L"FLController.ini", L"FLSubtitles.ini", L"FLvibed\\saves.ini"};
static const wchar_t* kLogs[] = {L"FLController.log", L"FLSubtitles.log"};

static bool g_quiet = false;
static FILE* g_log = nullptr;

static void Log(const std::wstring& s)
{
    if (g_log)
    {
        fwprintf(g_log, L"%ls\n", s.c_str());
        fflush(g_log);
    }
}

static std::wstring Lower(std::wstring s)
{
    for (auto& c : s)
        c = static_cast<wchar_t>(towlower(c));
    return s;
}

static std::wstring Trim(const std::wstring& s)
{
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos)
        return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::wstring StripSlash(std::wstring s)
{
    while (s.size() > 3 && (s.back() == L'\\' || s.back() == L'/'))
        s.pop_back();
    return s;
}

static bool FileExists(const std::wstring& p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool DirExists(const std::wstring& p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring SelfPath()
{
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    return std::wstring(buf, n);
}

// A Freelancer folder: the one holding EXE\Freelancer.exe and EXE\dacom.ini. Accepts the EXE folder itself too.
static std::wstring NormalizeGameDir(std::wstring dir)
{
    dir = StripSlash(Trim(dir));
    if (dir.empty())
        return L"";
    if (FileExists(dir + L"\\EXE\\Freelancer.exe") && FileExists(dir + L"\\EXE\\dacom.ini"))
        return dir;
    if (FileExists(dir + L"\\Freelancer.exe") && FileExists(dir + L"\\dacom.ini"))
    {
        size_t cut = dir.find_last_of(L"\\/");
        if (cut != std::wstring::npos && Lower(dir.substr(cut + 1)) == L"exe")
            return dir.substr(0, cut);
    }
    return L"";
}

static std::wstring RegString(HKEY root, const wchar_t* key, const wchar_t* name, REGSAM view)
{
    HKEY h;
    if (RegOpenKeyExW(root, key, 0, KEY_READ | view, &h) != ERROR_SUCCESS)
        return L"";
    wchar_t buf[2048];
    DWORD type = 0, size = sizeof(buf);
    std::wstring out;
    if (RegQueryValueExW(h, name, nullptr, &type, reinterpret_cast<BYTE*>(buf), &size) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ))
        out.assign(buf, wcsnlen(buf, size / sizeof(wchar_t)));
    RegCloseKey(h);
    return out;
}

static std::wstring EnvPath(const wchar_t* var, const wchar_t* tail)
{
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetEnvironmentVariableW(var, buf, static_cast<DWORD>(std::size(buf)));
    if (n == 0 || n >= std::size(buf))
        return L"";
    return std::wstring(buf, n) + tail;
}

struct Game
{
    std::wstring dir;
    std::wstring label; // what the dialog shows
};

static void AddGame(std::vector<Game>& out, const std::wstring& raw, const std::wstring& what)
{
    std::wstring dir = NormalizeGameDir(raw);
    if (dir.empty())
        return;
    for (auto& g : out)
        if (Lower(g.dir) == Lower(dir))
            return;
    out.push_back({dir, what + L"\n" + dir});
}

// Every Freelancer install this PC knows: HD Edition's installer entry, the original game's registry key, the default
// folders, and the folder this setup was started from.
static std::vector<Game> FindGames()
{
    std::vector<Game> out;
    const std::wstring hde = std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") + kHdeUninstallKey;
    for (REGSAM view : {KEY_WOW64_32KEY, KEY_WOW64_64KEY})
        for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER})
            AddGame(out, RegString(root, hde.c_str(), L"InstallLocation", view), L"Freelancer: HD Edition");
    for (REGSAM view : {KEY_WOW64_32KEY, KEY_WOW64_64KEY})
        AddGame(out, RegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Microsoft Games\\Freelancer\\1.0", L"AppPath", view),
                L"Freelancer");
    AddGame(out, EnvPath(L"SystemDrive", L"\\Games\\Freelancer HD Edition"), L"Freelancer: HD Edition");
    AddGame(out, EnvPath(L"LOCALAPPDATA", L"\\Freelancer HD Edition"), L"Freelancer: HD Edition");
    AddGame(out, EnvPath(L"ProgramFiles(x86)", L"\\Microsoft Games\\Freelancer"), L"Freelancer");
    AddGame(out, EnvPath(L"ProgramFiles", L"\\Microsoft Games\\Freelancer"), L"Freelancer");
    std::wstring self = SelfPath();
    for (int up = 0; up < 3; ++up)
    {
        size_t cut = self.find_last_of(L'\\');
        if (cut == std::wstring::npos)
            break;
        self = self.substr(0, cut);
        AddGame(out, self, L"Freelancer (this folder)");
    }
    return out;
}

static bool IsInstalled(const std::wstring& game)
{
    return FileExists(game + L"\\EXE\\FLvibed.dll");
}

static std::wstring InstalledVersion(const std::wstring& game)
{
    std::wstring dll = game + L"\\EXE\\FLvibed.dll";
    DWORD handle = 0, size = GetFileVersionInfoSizeW(dll.c_str(), &handle);
    if (!size)
        return L"";
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(dll.c_str(), 0, size, data.data()))
        return L"";
    wchar_t* v = nullptr;
    UINT len = 0;
    if (VerQueryValueW(data.data(), L"\\StringFileInfo\\040904b0\\ProductVersion", reinterpret_cast<void**>(&v), &len) && v)
        return v;
    return L"";
}

static std::wstring FinalDir(const std::wstring& dir);

// The game running from this folder (by any path to it) holds FLvibed.dll open: it must be closed first.
static bool GameRunning(const std::wstring& game)
{
    const std::wstring want = Lower(FinalDir(game + L"\\EXE"));
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    PROCESSENTRY32W pe{sizeof(pe)};
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe))
    {
        if (_wcsicmp(pe.szExeFile, L"Freelancer.exe") != 0)
            continue;
        HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!p)
            continue;
        wchar_t path[MAX_PATH * 2];
        DWORD n = static_cast<DWORD>(std::size(path));
        if (QueryFullProcessImageNameW(p, 0, path, &n))
        {
            std::wstring img(path, n);
            size_t cut = img.find_last_of(L'\\');
            if (cut != std::wstring::npos && Lower(FinalDir(img.substr(0, cut))) == want)
                found = true;
        }
        CloseHandle(p);
    }
    CloseHandle(snap);
    return found;
}

// ---------------------------------------------------------------- dacom.ini (kept byte for byte apart from our line)
static bool ReadBytes(const std::wstring& p, std::string& out)
{
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(h, &sz);
    out.resize(static_cast<size_t>(sz.QuadPart));
    DWORD got = 0;
    BOOL ok = out.empty() || ReadFile(h, &out[0], static_cast<DWORD>(out.size()), &got, nullptr);
    CloseHandle(h);
    return ok && got == out.size();
}

static DWORD WriteBytes(const std::wstring& p, const void* data, size_t size)
{
    HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return GetLastError();
    DWORD put = 0;
    BOOL ok = WriteFile(h, data, static_cast<DWORD>(size), &put, nullptr);
    DWORD err = ok && put == size ? 0 : GetLastError();
    CloseHandle(h);
    return err ? err : 0;
}

static std::vector<std::string> SplitLines(const std::string& s, std::string& eol)
{
    eol = s.find("\r\n") != std::string::npos || s.find('\n') == std::string::npos ? "\r\n" : "\n";
    std::vector<std::string> lines;
    size_t i = 0;
    while (i <= s.size())
    {
        size_t j = s.find('\n', i);
        if (j == std::string::npos)
        {
            if (i < s.size())
                lines.push_back(s.substr(i));
            break;
        }
        std::string line = s.substr(i, j - i);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
        i = j + 1;
    }
    return lines;
}

static std::string TrimA(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos)
        return "";
    return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}

static bool IsOurLine(const std::string& line)
{
    std::string t = TrimA(line);
    return _stricmp(t.c_str(), "FLvibed.dll") == 0;
}

static std::string JoinLines(const std::vector<std::string>& lines, const std::string& eol, bool finalEol)
{
    std::string out;
    for (size_t k = 0; k < lines.size(); ++k)
        out += lines[k] + (k + 1 < lines.size() || finalEol ? eol : "");
    return out;
}

// 0 = done (or already there), else a Win32 error; ERROR_NOT_FOUND = no [Libraries] section.
static DWORD DacomAdd(const std::wstring& game)
{
    const std::wstring path = game + L"\\EXE\\dacom.ini";
    std::string text;
    if (!ReadBytes(path, text))
        return GetLastError() ? GetLastError() : ERROR_READ_FAULT;
    std::string eol;
    auto lines = SplitLines(text, eol);
    for (auto& l : lines)
        if (IsOurLine(l))
            return 0;
    size_t sec = lines.size();
    for (size_t k = 0; k < lines.size(); ++k)
        if (_stricmp(TrimA(lines[k]).c_str(), "[Libraries]") == 0)
        {
            sec = k;
            break;
        }
    if (sec == lines.size())
        return ERROR_NOT_FOUND;
    size_t at = sec + 1;
    while (at < lines.size() && TrimA(lines[at]).rfind('[', 0) != 0)
        ++at;
    while (at > sec + 1 && TrimA(lines[at - 1]).empty())
        --at;
    lines.insert(lines.begin() + static_cast<long>(at), "FLvibed.dll");
    std::string out = JoinLines(lines, eol, !text.empty() && text.back() == '\n');
    return WriteBytes(path, out.data(), out.size());
}

static DWORD DacomRemove(const std::wstring& game)
{
    const std::wstring path = game + L"\\EXE\\dacom.ini";
    std::string text;
    if (!ReadBytes(path, text))
        return 0; // no dacom.ini: nothing loads FLvibed
    std::string eol;
    auto lines = SplitLines(text, eol);
    size_t before = lines.size();
    std::vector<std::string> kept;
    for (auto& l : lines)
        if (!IsOurLine(l))
            kept.push_back(l);
    if (kept.size() == before)
        return 0;
    std::string out = JoinLines(kept, eol, !text.empty() && text.back() == '\n');
    return WriteBytes(path, out.data(), out.size());
}

// ---------------------------------------------------------------- the installed-apps entry (per user)
// The folder's real location: junctions, symbolic links, short (8.3) names and mapped drives resolved, so one game
// folder reached by two paths is one folder (player install test, 26 Oct 2026: installed by the real path, uninstalled
// through a junction, the entry stayed behind). Falls back to the path as given.
static std::wstring FinalDir(const std::wstring& dir)
{
    HANDLE h = CreateFileW(dir.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return StripSlash(dir);
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetFinalPathNameByHandleW(h, buf, static_cast<DWORD>(std::size(buf)), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(h);
    if (n == 0 || n >= std::size(buf))
        return StripSlash(dir);
    std::wstring s(buf, n);
    if (s.rfind(L"\\\\?\\UNC\\", 0) == 0)
        s = L"\\\\" + s.substr(8);
    else if (s.rfind(L"\\\\?\\", 0) == 0)
        s = s.substr(4);
    return StripSlash(s);
}

static std::wstring EntryKey(const std::wstring& game)
{
    unsigned h = 2166136261u; // FNV-1a over the lower-case real folder: one entry per game folder
    for (wchar_t c : Lower(FinalDir(game)))
    {
        h ^= static_cast<unsigned>(c);
        h *= 16777619u;
    }
    wchar_t buf[96];
    swprintf_s(buf, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FLvibed-%08X", h);
    return buf;
}

static void SetStr(HKEY k, const wchar_t* name, const std::wstring& v)
{
    RegSetValueExW(k, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(v.c_str()), static_cast<DWORD>((v.size() + 1) * sizeof(wchar_t)));
}

static void RegisterEntry(const std::wstring& game)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, EntryKey(game).c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS)
        return;
    std::wstring leaf = game.substr(game.find_last_of(L'\\') + 1);
    std::wstring uninst = game + L"\\EXE\\FLvibed\\" + kUninstallerName;
    SetStr(k, L"DisplayName", std::wstring(L"FLvibed (") + leaf + L")");
    SetStr(k, L"DisplayVersion", kVersion);
    SetStr(k, L"Publisher", L"FLvibed");
    SetStr(k, L"InstallLocation", game + L"\\EXE");
    SetStr(k, L"DisplayIcon", uninst + L",0"); // FLvibed's logo, carried by the uninstaller
    SetStr(k, L"UninstallString", L"\"" + uninst + L"\" /uninstall \"" + game + L"\"");
    SetStr(k, L"QuietUninstallString", L"\"" + uninst + L"\" /uninstall \"" + game + L"\" /quiet");
    DWORD one = 1;
    RegSetValueExW(k, L"NoModify", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&one), sizeof(one));
    RegSetValueExW(k, L"NoRepair", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&one), sizeof(one));
    RegCloseKey(k);
}

static void UnregisterEntry(const std::wstring& game)
{
    RegDeleteTreeW(HKEY_CURRENT_USER, EntryKey(game).c_str());
    // and any FLvibed entry whose folder is this one by another path (one written before FinalDir, or by another path)
    const std::wstring base = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    const std::wstring me = Lower(FinalDir(game + L"\\EXE"));
    HKEY root;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, base.c_str(), 0, KEY_READ, &root) != ERROR_SUCCESS)
        return;
    std::vector<std::wstring> drop;
    wchar_t name[256];
    for (DWORD i = 0;; ++i)
    {
        DWORD len = static_cast<DWORD>(std::size(name));
        if (RegEnumKeyExW(root, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;
        if (wcsncmp(name, L"FLvibed-", 8) != 0)
            continue;
        std::wstring loc = RegString(HKEY_CURRENT_USER, (base + L"\\" + name).c_str(), L"InstallLocation", 0);
        if (!loc.empty() && Lower(FinalDir(loc)) == me)
            drop.push_back(name);
    }
    RegCloseKey(root);
    for (auto& k : drop)
        RegDeleteTreeW(HKEY_CURRENT_USER, (base + L"\\" + k).c_str());
}

// ---------------------------------------------------------------- install / uninstall
static std::wstring ErrText(DWORD err)
{
    wchar_t* msg = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0,
                   reinterpret_cast<wchar_t*>(&msg), 0, nullptr);
    std::wstring s = msg ? Trim(msg) : L"error " + std::to_wstring(err);
    if (msg)
        LocalFree(msg);
    return s;
}

struct Result
{
    DWORD err = 0;
    std::wstring what; // the step that failed
};

// A folder this process can write to (a protected folder needs administrator rights instead of a retry).
static bool FolderWritable(const std::wstring& dir)
{
    const std::wstring probe = dir + L"\\flvibed-setup.probe";
    HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    CloseHandle(h);
    return true;
}

// Windows keeps a just-closed game's DLLs locked for a moment (player install test, 26 Oct 2026: an uninstall right
// after the game closed could not delete FLvibed.dll). A step that fails on a lock in a writable folder is retried for
// up to 10 s; anything else returns at once.
template <class F>
static DWORD Retry(const std::wstring& dir, F step)
{
    DWORD e = step();
    for (int k = 0; k < 40 && (e == ERROR_SHARING_VIOLATION || e == ERROR_LOCK_VIOLATION || e == ERROR_ACCESS_DENIED) &&
                    FolderWritable(dir); ++k)
    {
        Sleep(250);
        e = step();
    }
    return e;
}

static DWORD EnsureDir(const std::wstring& d)
{
    if (DirExists(d) || CreateDirectoryW(d.c_str(), nullptr))
        return 0;
    return GetLastError() == ERROR_ALREADY_EXISTS ? 0 : GetLastError();
}

static Result Install(const std::wstring& game)
{
    const std::wstring exe = game + L"\\EXE\\";
    Result r;
    auto fail = [&](DWORD e, const std::wstring& step) {
        r.err = e;
        r.what = step;
        Log(L"FAILED " + step + L": " + ErrText(e));
        return r;
    };
    for (const wchar_t* d : {L"FLSubtitles", L"FLvibed"})
        if (DWORD e = EnsureDir(exe + d))
            return fail(e, std::wstring(L"create ") + exe + d);
    // dacom.ini backed up once, before FLvibed ever touched it
    const std::wstring backup = exe + L"FLvibed\\dacom.ini.before-flvibed";
    if (!FileExists(backup))
    {
        std::string text;
        if (ReadBytes(exe + L"dacom.ini", text))
        {
            std::string e;
            bool ours = false;
            for (auto& l : SplitLines(text, e))
                ours = ours || IsOurLine(l);
            if (!ours)
                WriteBytes(backup, text.data(), text.size());
        }
    }
    HMODULE self = GetModuleHandleW(nullptr);
    for (const Payload& p : kPayload)
    {
        HRSRC res = FindResourceW(self, MAKEINTRESOURCEW(p.id), RT_RCDATA);
        HGLOBAL mem = res ? LoadResource(self, res) : nullptr;
        const void* data = mem ? LockResource(mem) : nullptr;
        if (!data)
            return fail(ERROR_RESOURCE_DATA_NOT_FOUND, std::wstring(L"read the setup's ") + p.path);
        const DWORD size = SizeofResource(self, res);
        if (DWORD e = Retry(exe.substr(0, exe.size() - 1), [&] { return WriteBytes(exe + p.path, data, size); }))
            return fail(e, std::wstring(L"write ") + exe + p.path);
        Log(std::wstring(L"wrote ") + exe + p.path);
    }
    if (!FileExists(exe + L"FLvibed.ini"))
    {
        const char ini[] = "[Modules]\r\nsubtitles = 1\r\ncontroller = 1\r\n";
        if (DWORD e = WriteBytes(exe + L"FLvibed.ini", ini, sizeof(ini) - 1))
            return fail(e, L"write " + exe + L"FLvibed.ini");
        Log(L"wrote " + exe + L"FLvibed.ini");
    }
    if (DWORD e = DacomAdd(game))
        return fail(e, e == ERROR_NOT_FOUND ? L"find [Libraries] in " + exe + L"dacom.ini" : L"update " + exe + L"dacom.ini");
    Log(L"dacom.ini: FLvibed.dll listed");
    // the uninstaller: this exe, copied unless it already runs from there
    const std::wstring uninst = exe + L"FLvibed\\" + kUninstallerName;
    if (Lower(SelfPath()) != Lower(uninst) && !CopyFileW(SelfPath().c_str(), uninst.c_str(), FALSE))
        return fail(GetLastError(), L"write " + uninst);
    RegisterEntry(game);
    Log(L"installed FLvibed " + std::wstring(kVersion) + L" into " + game);
    return r;
}

// The running uninstaller cannot delete itself: a hidden cmd removes it (and the then empty folder) once every copy of
// it has exited - retried each second for up to 2 minutes (disc install test, 26 Oct 2026: from Program Files the
// uninstaller runs twice, the player's and the elevated one, and its last window can stay open for a while).
static void DeleteSelfLater(const std::wstring& exe, const std::wstring& folder)
{
    wchar_t comspec[MAX_PATH];
    if (!GetEnvironmentVariableW(L"ComSpec", comspec, MAX_PATH))
        wcscpy_s(comspec, L"cmd.exe");
    std::wstring cmd = L"\"" + std::wstring(comspec) + L"\" /c for /l %i in (1,1,120) do @(ping -n 2 127.0.0.1 >nul & "
                       L"del /f /q \"" + exe + L"\" 2>nul & if not exist \"" + exe + L"\" (rmdir \"" + folder +
                       L"\" 2>nul & exit))";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

// A game in a protected folder (the disc install's Program Files) runs without administrator rights, so Windows
// redirects what FLvibed writes into EXE\ - its settings and logs - to the player's VirtualStore copy of that folder
// (disc install test, 26 Oct 2026). The game reads them back from there transparently; the uninstall cleans them out
// there too (in the player's own process as well: the elevated one may belong to another account).
static void CleanVirtualStore(const std::wstring& game, bool keepSettings)
{
    std::wstring exe = FinalDir(game + L"\\EXE");
    wchar_t local[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    if (n == 0 || n >= MAX_PATH || exe.size() < 3 || exe[1] != L':')
        return;
    const std::wstring vs = std::wstring(local) + L"\\VirtualStore\\" + exe.substr(3) + L"\\";
    if (!DirExists(vs))
        return;
    for (const wchar_t* f : kLogs)
        DeleteFileW((vs + f).c_str());
    if (!keepSettings)
        for (const wchar_t* f : kSettings)
            DeleteFileW((vs + f).c_str());
    RemoveDirectoryW((vs + L"FLvibed").c_str());
    Log(L"VirtualStore cleaned: " + vs);
}

static Result Uninstall(const std::wstring& game, bool keepSettings)
{
    const std::wstring exe = game + L"\\EXE\\";
    Result r;
    if (DWORD e = DacomRemove(game))
    {
        r.err = e;
        r.what = L"update " + exe + L"dacom.ini";
        Log(L"FAILED " + r.what + L": " + ErrText(e));
        return r;
    }
    Log(L"dacom.ini: FLvibed.dll removed");
    for (const Payload& p : kPayload)
        if (FileExists(exe + p.path) &&
            (r.err = Retry(exe.substr(0, exe.size() - 1), [&] { return DeleteFileW((exe + p.path).c_str()) ? 0ul : GetLastError(); })))
        {
            r.what = std::wstring(L"delete ") + exe + p.path;
            Log(L"FAILED " + r.what + L": " + ErrText(r.err));
            return r;
        }
    for (const wchar_t* f : kLogs)
        DeleteFileW((exe + f).c_str());
    if (!keepSettings)
        for (const wchar_t* f : kSettings)
            DeleteFileW((exe + f).c_str());
    DeleteFileW((exe + L"FLvibed\\dacom.ini.before-flvibed").c_str());
    RemoveDirectoryW((exe + L"FLSubtitles").c_str());
    const std::wstring uninst = exe + L"FLvibed\\" + kUninstallerName;
    if (Lower(SelfPath()) == Lower(uninst))
        DeleteSelfLater(uninst, exe + L"FLvibed");
    else
    {
        DeleteFileW(uninst.c_str());
        RemoveDirectoryW((exe + L"FLvibed").c_str());
    }
    UnregisterEntry(game);
    CleanVirtualStore(game, keepSettings);
    Log(L"uninstalled FLvibed from " + game + (keepSettings ? L" (settings kept)" : L""));
    return r;
}

// ---------------------------------------------------------------- the dialogs
static int Ask(const std::wstring& main, const std::wstring& content, const std::vector<std::pair<int, std::wstring>>& buttons,
               TASKDIALOG_COMMON_BUTTON_FLAGS common, PCWSTR icon, const std::vector<std::wstring>* radios = nullptr,
               int* radioPicked = nullptr)
{
    std::vector<TASKDIALOG_BUTTON> b;
    for (auto& x : buttons)
        b.push_back({x.first, x.second.c_str()});
    std::vector<TASKDIALOG_BUTTON> rb;
    if (radios)
        for (size_t k = 0; k < radios->size(); ++k)
            rb.push_back({static_cast<int>(1000 + k), (*radios)[k].c_str()});
    TASKDIALOGCONFIG c{sizeof(c)};
    c.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | (b.empty() ? 0 : TDF_USE_COMMAND_LINKS);
    c.pszWindowTitle = kTitle;
    if (!icon)
    {
        c.hInstance = GetModuleHandleW(nullptr); // no system icon asked for: FLvibed's logo (setup.rc icon 1)
        icon = MAKEINTRESOURCEW(1);
    }
    c.pszMainIcon = icon;
    c.pszMainInstruction = main.c_str();
    c.pszContent = content.c_str();
    c.cButtons = static_cast<UINT>(b.size());
    c.pButtons = b.empty() ? nullptr : b.data();
    c.dwCommonButtons = common;
    c.cRadioButtons = static_cast<UINT>(rb.size());
    c.pRadioButtons = rb.empty() ? nullptr : rb.data();
    c.nDefaultRadioButton = rb.empty() ? 0 : 1000;
    int pressed = IDCANCEL, radio = 0;
    if (FAILED(TaskDialogIndirect(&c, &pressed, &radio, nullptr)))
        return IDCANCEL;
    if (radioPicked)
        *radioPicked = radio - 1000;
    return pressed;
}

static std::wstring PickFolder()
{
    std::wstring out;
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d))))
        return out;
    DWORD opt = 0;
    d->GetOptions(&opt);
    d->SetOptions(opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    d->SetTitle(L"Choose your Freelancer folder (the one that holds EXE\\Freelancer.exe)");
    if (SUCCEEDED(d->Show(nullptr)))
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(d->GetResult(&item)))
        {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)))
            {
                out = p;
                CoTaskMemFree(p);
            }
            item->Release();
        }
    }
    d->Release();
    return out;
}

static bool Elevated()
{
    HANDLE t;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t))
        return false;
    TOKEN_ELEVATION e{};
    DWORD n = 0;
    bool yes = GetTokenInformation(t, TokenElevation, &e, sizeof(e), &n) && e.TokenIsElevated;
    CloseHandle(t);
    return yes;
}

// Runs this exe again as administrator with the same action; returns its exit code (or -1 when declined / failed).
static int RunElevated(const std::wstring& args)
{
    std::wstring self = SelfPath();
    SHELLEXECUTEINFOW sei{sizeof(sei)};
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = self.c_str();
    sei.lpParameters = args.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei) || !sei.hProcess)
        return -1;
    WaitForSingleObject(sei.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return static_cast<int>(code);
}

static bool WaitForGameClosed(const std::wstring& game)
{
    while (GameRunning(game))
    {
        if (g_quiet)
            return false;
        if (Ask(L"Please close Freelancer first", L"Freelancer is running from\n" + game +
                L"\nand holds FLvibed's files open. Close the game, then choose Retry.",
                {}, TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON, TD_WARNING_ICON) != IDRETRY)
            return false;
    }
    return true;
}

static int DoInstall(const std::wstring& game)
{
    if (!WaitForGameClosed(game))
        return 3;
    std::wstring before = InstalledVersion(game);
    Result r = Install(game);
    if (r.err == ERROR_ACCESS_DENIED && !g_quiet && !Elevated())
    {
        if (Ask(L"This folder needs administrator rights", L"Windows protects\n" + game +
                L"\nFLvibed Setup can try again with administrator rights.",
                {{100, L"Try again as administrator"}}, TDCBF_CANCEL_BUTTON, TD_SHIELD_ICON) != 100)
            return 2;
        int code = RunElevated(L"/install \"" + game + L"\"");
        return code < 0 ? 2 : code;
    }
    if (r.err)
    {
        if (!g_quiet)
            Ask(L"FLvibed could not be installed", L"Could not " + r.what + L":\n" + ErrText(r.err) +
                L"\n\nNothing else was changed after this step; run Setup again, or uninstall from it.",
                {}, TDCBF_CLOSE_BUTTON, TD_ERROR_ICON);
        return 1;
    }
    if (!g_quiet)
        Ask(before.empty() ? L"FLvibed " + std::wstring(kVersion) + L" is installed" : L"FLvibed is updated to " + std::wstring(kVersion),
            L"Into: " + game + L"\n\nStart Freelancer as usual and pick up your controller. Settings: Options > FLVIBED "
            L"(also the pause menu's sixth button).\n\nTo remove FLvibed: Windows Settings > Apps > Installed apps > "
            L"FLvibed, or run this setup again.",
            {}, TDCBF_OK_BUTTON, TD_INFORMATION_ICON);
    return 0;
}

static int DoUninstall(const std::wstring& game, int keep)
{
    if (!WaitForGameClosed(game))
        return 3;
    if (keep < 0)
    {
        int b = Ask(L"Uninstall FLvibed?", L"From: " + game + L"\n\nFLvibed's line comes out of dacom.ini and its files "
                    L"are deleted. Your saves are not touched.",
                    {{200, L"Uninstall and remove my FLvibed settings\nFLvibed.ini, FLController.ini, FLSubtitles.ini and "
                           L"the per-save modes go too"},
                     {201, L"Uninstall, keep my settings\nThey are used again if you install FLvibed later"}},
                    TDCBF_CANCEL_BUTTON, TD_WARNING_ICON);
        if (b != 200 && b != 201)
            return 2;
        keep = b == 201;
    }
    Result r = Uninstall(game, keep != 0);
    if (r.err == ERROR_ACCESS_DENIED && !g_quiet && !Elevated())
    {
        int code = RunElevated(L"/uninstall \"" + game + L"\"" + (keep ? L" /keepsettings" : L" /removesettings"));
        if (code == 0)
        {
            CleanVirtualStore(game, keep != 0); // this player's copies (the elevated run may be another account)
            UnregisterEntry(game);              // and this player's installed-apps entry
        }
        return code < 0 ? 2 : code;
    }
    if (r.err)
    {
        if (!g_quiet)
            Ask(L"FLvibed could not be fully removed", L"Could not " + r.what + L":\n" + ErrText(r.err),
                {}, TDCBF_CLOSE_BUTTON, TD_ERROR_ICON);
        return 1;
    }
    if (!g_quiet)
        Ask(L"FLvibed is uninstalled", L"From: " + game + (keep ? L"\n\nYour FLvibed settings were kept." : L""), {},
            TDCBF_OK_BUTTON, TD_INFORMATION_ICON);
    return 0;
}

static int Interactive()
{
    for (;;)
    {
        std::vector<Game> games = FindGames();
        std::vector<std::wstring> radios;
        for (auto& g : games)
            radios.push_back(g.label + (IsInstalled(g.dir) ? L"   (FLvibed " + (InstalledVersion(g.dir).empty() ? std::wstring(L"installed") : InstalledVersion(g.dir)) + L")" : L""));
        std::wstring content = games.empty()
            ? L"No Freelancer installation was found. Choose your game folder: the one that holds EXE\\Freelancer.exe "
              L"(Freelancer: HD Edition 0.7.1, or the original Freelancer 1.1 with a decrypted Freelancer.exe)."
            : L"Native controller play, dialogue subtitles, better movies and the Advanced Autopilot for Freelancer. "
              L"FLvibed adds one line to EXE\\dacom.ini and its own files; no game file is changed.\n\nInstall into:";
        std::vector<std::pair<int, std::wstring>> buttons;
        if (!games.empty())
        {
            buttons.push_back({300, L"Install or update FLvibed " + std::wstring(kVersion) + L"\nInto the game selected above"});
            buttons.push_back({301, L"Uninstall FLvibed\nFrom the game selected above"});
        }
        buttons.push_back({302, games.empty() ? L"Choose your game folder..." : L"Choose another game folder..."});
        int picked = 0;
        int b = Ask(L"Install FLvibed " + std::wstring(kVersion), content, buttons, TDCBF_CANCEL_BUTTON, nullptr,
                    games.empty() ? nullptr : &radios, &picked);
        if (b == 302)
        {
            std::wstring raw = PickFolder();
            if (raw.empty())
                continue;
            std::wstring dir = NormalizeGameDir(raw);
            if (dir.empty())
            {
                Ask(L"That is not a Freelancer folder", raw + L"\n\nChoose the folder that holds EXE\\Freelancer.exe "
                    L"and EXE\\dacom.ini (for HD Edition usually C:\\Games\\Freelancer HD Edition).",
                    {}, TDCBF_OK_BUTTON, TD_WARNING_ICON);
                continue;
            }
            int c = Ask(L"Freelancer found", dir, {{300, L"Install or update FLvibed " + std::wstring(kVersion)},
                        {301, L"Uninstall FLvibed"}}, TDCBF_CANCEL_BUTTON, nullptr);
            if (c == 300)
                return DoInstall(dir);
            if (c == 301)
                return DoUninstall(dir, -1);
            continue;
        }
        if (picked < 0 || picked >= static_cast<int>(games.size()))
            return 2;
        if (b == 300)
            return DoInstall(games[picked].dir);
        if (b == 301)
        {
            if (!IsInstalled(games[picked].dir))
            {
                Ask(L"FLvibed is not installed there", games[picked].dir, {}, TDCBF_OK_BUTTON, TD_INFORMATION_ICON);
                continue;
            }
            return DoUninstall(games[picked].dir, -1);
        }
        return 2;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring install, uninstall;
    int keep = -1;
    for (int k = 1; k < argc; ++k)
    {
        std::wstring a = Lower(argv[k]);
        if (a == L"/install" && k + 1 < argc)
            install = argv[++k];
        else if (a == L"/uninstall" && k + 1 < argc)
            uninstall = argv[++k];
        else if (a == L"/keepsettings")
            keep = 1;
        else if (a == L"/removesettings")
            keep = 0;
        else if (a == L"/quiet")
            g_quiet = true;
        else if (a == L"/log" && k + 1 < argc)
            _wfopen_s(&g_log, argv[++k], L"w, ccs=UTF-8");
    }
    int code;
    if (!install.empty() || !uninstall.empty())
    {
        std::wstring raw = install.empty() ? uninstall : install;
        std::wstring dir = NormalizeGameDir(raw);
        if (dir.empty())
        {
            Log(L"FAILED not a Freelancer folder: " + raw);
            if (!g_quiet)
                Ask(L"That is not a Freelancer folder", raw, {}, TDCBF_OK_BUTTON, TD_ERROR_ICON);
            code = 4;
        }
        else if (!install.empty())
            code = DoInstall(dir);
        else
            code = DoUninstall(dir, g_quiet && keep < 0 ? 0 : keep);
    }
    else
        code = Interactive();
    if (g_log)
        fclose(g_log);
    LocalFree(argv);
    CoUninitialize();
    return code;
}
