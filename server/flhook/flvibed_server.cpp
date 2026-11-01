// flvibed_server.dll - an FLHook 4.0 plugin (HD Edition's FLHook, FLHDE/FLHook): the server tells FLvibed's clients what
// it allows FLvibed's Advanced Autopilot (docs\MULTIPLAYER.md "Server permission"; the owner, 28 Oct 2026: the server
// decides, through an FLHook plugin among others). Server-side; players do not need it.
//
// What it does: when a player logs in (IServerImpl::CharacterSelect, after), and to every player online when FLHook
// reloads its settings, it sends one server line holding only the tag "[flvibed autopilot=on|travel|off]" (sender
// 0x10000 = the server, as every FLHook system line). FLvibed reads the tag and hides such a line from the chat; a
// player without FLvibed sees the one line. A live change: edit the config, then FLHook's admin command
// `reloadplugin flvibed` resends the rule to everyone online.
// Config: EXE\config\flvibed.json  { "autopilot": "travel" }  (on | travel | off; missing or unreadable = travel).
//
// Built against HDE's FLHook.dll exports only (build.cmd flhook: an import library from flvibed_server.def), not
// FLHook's headers: the few types it needs are declared below, matching FLHook's include\plugin.h and
// include\Tools\Enums.hpp at FLHDE/FLHook 8829534 (Jun 2024 - HDE 0.7.1 ships that state: plugin API 4.0). Another
// FLHook line (Discovery's fork, TheStarport's 3.x) needs its own build.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

// ---- FLHook 4.0's plugin ABI (names and layouts as FLHook exports them)
enum class PluginMajorVersion
{
    UNDEFINED = -1,
    VERSION_04 = 4,
};
enum class PluginMinorVersion
{
    UNDEFINED = -1,
    VERSION_00 = 0,
};
enum class ReturnCode
{
    Default = 0,
};
enum class HookStep
{
    Before,
    After
};
// HookedCall's values at 8829534 (include\Tools\Enums.hpp: one entry per line, none assigned)
constexpr int kHookLoadSettings = 18;    // FLHook__LoadSettings - void()
constexpr int kHookCharacterSelect = 78; // IServerImpl__CharacterSelect - void(std::string& charFile, const uint& client)

class PluginHook
{
  public:
    int target;
    void* hook;
    HookStep step;
    int priority;
};

struct PluginInfo
{
    __declspec(dllimport) void versionMajor(PluginMajorVersion version);
    __declspec(dllimport) void versionMinor(PluginMinorVersion version);
    __declspec(dllimport) void name(const char* name);
    __declspec(dllimport) void shortName(const char* shortName);
    __declspec(dllimport) void mayUnload(bool unload);
    __declspec(dllimport) void returnCode(ReturnCode* returnCode);
    __declspec(dllimport) void addHook(const PluginHook& hook);
};

namespace Hk
{
namespace Client
{
__declspec(dllimport) bool IsValidClientID(unsigned client);
}
namespace Message
{
// the chat packet to one client, sender 0x10000 (the server), payload = Freelancer rich text (RDL)
__declspec(dllimport) void FMsgSendChat(unsigned client, char* buffer, unsigned size);
}
} // namespace Hk

namespace
{
ReturnCode g_returnCode = ReturnCode::Default;
std::wstring g_rule = L"travel";

std::string ConfigPath()
{
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string p = exe;
    return p.substr(0, p.find_last_of('\\')) + "\\config\\flvibed.json";
}

// { "autopilot": "on|travel|off" } - the one key; anything else keeps travel (and the file is written when missing)
void LoadConfig()
{
    std::wstring rule = L"travel";
    const std::string path = ConfigPath();
    if (FILE* f = fopen(path.c_str(), "rb"))
    {
        char buf[4096] = {};
        fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        if (const char* k = strstr(buf, "\"autopilot\""))
            for (const char* v : {"on", "travel", "off"})
            {
                const std::string want = std::string("\"") + v + "\"";
                const char* colon = strchr(k + 11, ':');
                if (colon && strncmp(colon + 1 + strspn(colon + 1, " \t\r\n"), want.c_str(), want.size()) == 0)
                    rule = std::wstring(v, v + strlen(v));
            }
    }
    else if (FILE* w = fopen(path.c_str(), "wb"))
    {
        fputs("{\n    \"autopilot\": \"travel\"\n}\n", w);
        fclose(w);
    }
    g_rule = rule;
}

// One RDL line: a TRA node (white, plain) and a TEXT node (UTF-16, NUL-terminated) - nodes {u32 type, u32 length,
// payload}, packed (as the game's own chat lines).
void SendTag(unsigned client)
{
    const std::wstring text = L"[flvibed autopilot=" + g_rule + L"]";
    char buf[512];
    unsigned at = 0;
    auto node = [&](unsigned type, const void* data, unsigned len) {
        memcpy(buf + at, &type, 4);
        memcpy(buf + at + 4, &len, 4);
        memcpy(buf + at + 8, data, len);
        at += 8 + len;
    };
    const unsigned tra[2] = {0xFFFFFF00u, 0xFFFFFFFFu}; // colour white (0xBBGGRR00), mask all
    node(1, tra, sizeof tra);
    node(2, text.c_str(), static_cast<unsigned>((text.size() + 1) * sizeof(wchar_t)));
    Hk::Message::FMsgSendChat(client, buf, at);
}

void __cdecl OnLoadSettings()
{
    LoadConfig();
    for (unsigned client = 1; client <= 255; ++client)
        if (Hk::Client::IsValidClientID(client))
            SendTag(client);
}

void __cdecl OnCharacterSelect(const void* /*charFile*/, const unsigned& client) { SendTag(client); }
} // namespace

extern "C" __declspec(dllexport) void ExportPluginInfo(PluginInfo* pi)
{
    pi->name("FLvibed server rules");
    pi->shortName("flvibed");
    pi->mayUnload(true);
    pi->returnCode(&g_returnCode);
    pi->versionMajor(PluginMajorVersion::VERSION_04);
    pi->versionMinor(PluginMinorVersion::VERSION_00);
    pi->addHook(PluginHook{kHookLoadSettings, reinterpret_cast<void*>(&OnLoadSettings), HookStep::After, 0});
    pi->addHook(PluginHook{kHookCharacterSelect, reinterpret_cast<void*>(&OnCharacterSelect), HookStep::After, 0});
    // FLHook calls LoadSettings at its start only: a load later (admin `reloadplugin flvibed` after editing the config)
    // reads the config here and tells everyone online at once (at the server's start nobody is: nothing is sent)
    OnLoadSettings();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(module);
    return TRUE;
}
