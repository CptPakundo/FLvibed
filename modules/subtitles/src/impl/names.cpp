// What the game knows about a comm's sender: faction name and attitude to the player, and bar speakers by base and voice.
#include "names.h"

#include "data.h"
#include "flexe.h"
#include "game/ini.h"
#include "game/space.h"
#include "log.h"
#include "text.h"

#include <fstream>
#include <unordered_map>
#include <vector>

namespace names
{
std::wstring IdsText(u32 ids) { return text::Ids(ids); }

typedef int(__cdecl* GetRepFn)(u32, int&);
typedef int(__cdecl* GetAffiliationFn)(const int&, u32&);
typedef int(__cdecl* GetGroupNameFn)(const u32&, u32&);
typedef int(__cdecl* GetAttitudeFn)(const int&, const int&, float&);

Sender Lookup(u32 obj)
{
    static GetRepFn getRep;
    static GetAffiliationFn getAffiliation;
    static GetGroupNameFn getShortName;
    static GetAttitudeFn getAttitude;
    static bool init = false;
    if (!init)
    {
        init = true;
        HMODULE server = GetModuleHandleA("server.dll");
        if (server)
        {
            getRep = reinterpret_cast<GetRepFn>(GetProcAddress(server, "?GetRep@SpaceObj@pub@@YAHIAAH@Z"));
            getAffiliation = reinterpret_cast<GetAffiliationFn>(
                GetProcAddress(server, "?GetAffiliation@Reputation@pub@@YAHABHAAI@Z"));
            getShortName = reinterpret_cast<GetGroupNameFn>(
                GetProcAddress(server, "?GetShortGroupName@Reputation@pub@@YAHABIAAI@Z"));
            getAttitude =
                reinterpret_cast<GetAttitudeFn>(GetProcAddress(server, "?GetAttitude@Reputation@pub@@YAHABH0AAM@Z"));
        }
        Log("names: server.dll %p rep=%p aff=%p name=%p att=%p", server, getRep, getAffiliation, getShortName,
            getAttitude);
    }
    Sender s;
    if (!obj)
        return s;
    int rep = 0;
    if (getRep && getAffiliation && getShortName && getRep(obj, rep) == 0 && rep)
    {
        u32 group = 0, ids = 0;
        if (getAffiliation(rep, group) == 0 && group && getShortName(group, ids) == 0)
            s.faction = IdsText(ids);
        const u32 player = Mem<u32>(exe::PlayerShipId);
        int playerRep = 0;
        if (getAttitude && player && getRep(player, playerRep) == 0 && playerRep)
            getAttitude(rep, playerRep, s.attitude);
        s.known = true;
        return s;
    }
    // multiplayer: server.dll holds no world (RemoteClient loads it empty) - the client's reputation data, as the HUD
    // shows it (Common.dll Reputation::Vibe::GetAffiliation, Reputation::get_short_name; CEqObj::attitude_towards)
    rep = space::RepId(obj);
    if (!rep)
        return s;
    using VibeAffFn = int(__cdecl*)(const int&, u32&, bool);
    using ShortFn = u32(__cdecl*)(u32);
    static const HMODULE common = GetModuleHandleA("common.dll");
    static const auto vibeAff = reinterpret_cast<VibeAffFn>(
        common ? GetProcAddress(common, "?GetAffiliation@Vibe@Reputation@@YAHABHAAI_N@Z") : nullptr);
    static const auto shortName =
        reinterpret_cast<ShortFn>(common ? GetProcAddress(common, "?get_short_name@Reputation@@YAII@Z") : nullptr);
    u32 group = 0;
    if (vibeAff && shortName && vibeAff(rep, group, false) == 0 && group)
        s.faction = IdsText(shortName(group));
    float att = 0;
    if (space::AttitudeToPlayer(obj, att))
        s.attitude = att;
    s.known = true;
    return s;
}

// The bar characters of every base, read once: (base id, voice id) -> name ids + affiliation nickname. Ids are
// CreateID of the nicknames (case-insensitive), as the game keys bases and voices.
struct BarNpc
{
    u32 nameIds = 0;
    std::string affiliation;
    u32 voice = 0;
};
static std::unordered_map<unsigned long long, std::vector<BarNpc>> g_barNpcs;
static std::unordered_map<unsigned long long, BarNpc> g_barByName; // (base id, name ids) -> the character
static bool g_barLoaded = false; // mbases.ini is read at the first bar lookup; ResetBarNpcs (`reloaddata`) re-reads it
static std::wstring BarLabel(const BarNpc& n);

void ResetBarNpcs()
{
    g_barLoaded = false;
    g_barNpcs.clear();
    g_barByName.clear();
}

static void LoadBarNpcs(const std::string& dataDir)
{
    if (g_barLoaded)
        return;
    g_barLoaded = true;
    const std::vector<ini::Section> sections = ini::Read(dataDir + "MISSIONS\\mbases.ini"); // text or BINI (SWEEP #5)
    if (sections.empty())
    {
        Log("names: no mbases.ini under %s", dataDir.c_str());
        return;
    }
    u32 base = 0; // the [MBase] the following [GF_NPC] sections belong to
    size_t count = 0;
    for (const ini::Section& s : sections)
    {
        const bool inMBase = s.type == "mbase", npc = s.type == "gf_npc";
        BarNpc cur;
        u32 voice = 0;
        for (const auto& kv : s.kv)
        {
            const std::string& k = kv.first;
            const std::string& v = kv.second;
            if (inMBase && k == "nickname")
                base = data::CreateID(v.c_str());
            else if (npc && k == "individual_name")
                cur.nameIds = static_cast<u32>(strtoul(v.c_str(), nullptr, 10)), ++count;
            else if (npc && k == "affiliation")
                cur.affiliation = v;
            else if (npc && k == "voice")
                voice = data::CreateID(v.c_str());
        }
        if (npc && base && voice && cur.nameIds)
        {
            cur.voice = voice;
            g_barNpcs[(static_cast<unsigned long long>(base) << 32) | voice].push_back(cur);
            g_barByName[(static_cast<unsigned long long>(base) << 32) | cur.nameIds] = cur;
        }
    }
    Log("names: %zu bar characters from mbases.ini", count);
}

std::wstring BarSpeaker(const std::string& dataDir, u32 baseId, u32 voiceId)
{
    LoadBarNpcs(dataDir);
    if (!baseId || !voiceId)
        return {};
    const auto it = g_barNpcs.find((static_cast<unsigned long long>(baseId) << 32) | voiceId);
    if (it == g_barNpcs.end() || it->second.size() != 1)
        return {};
    return BarLabel(it->second[0]);
}

bool BarCharacter(const std::string& dataDir, u32 baseId, u32 nameIds, std::wstring& label, u32& voice)
{
    LoadBarNpcs(dataDir);
    const auto it = g_barByName.find((static_cast<unsigned long long>(baseId) << 32) | nameIds);
    if (!baseId || !nameIds || it == g_barByName.end())
        return false;
    label = BarLabel(it->second), voice = it->second.voice;
    return !label.empty();
}

// "Name · Faction" for a bar character (the faction's short group name; the name alone when it has none)
static std::wstring BarLabel(const BarNpc& n)
{
    std::wstring name = IdsText(n.nameIds);
    // some names are the character's card in lines - group, title, name ("Liberty Population / Manhattan Bartender /
    // Bob Swaffar", the fixers): the name with its title, "Bob Swaffar · Manhattan Bartender"
    std::vector<std::wstring> lines;
    for (size_t a = 0; a <= name.size();)
    {
        size_t b = name.find_first_of(L"\r\n", a);
        if (b == std::wstring::npos)
            b = name.size();
        std::wstring l = name.substr(a, b - a);
        while (!l.empty() && iswspace(l.back()))
            l.pop_back();
        while (!l.empty() && iswspace(l.front()))
            l.erase(0, 1);
        if (!l.empty())
            lines.push_back(l);
        a = b + 1;
    }
    if (lines.size() >= 2)
        return lines.back() + L" \x00B7 " + lines[lines.size() - 2];
    if (lines.size() == 1)
        name = lines[0];
    if (name.empty())
        return {};
    // the faction's short name, as the comm speakers' (server.dll: the group by nickname, its short name)
    typedef int(__cdecl * GetGroupFn)(u32&, const char*);
    typedef int(__cdecl * GetShortNameFn)(const u32&, u32&);
    static const HMODULE server = GetModuleHandleA("server.dll");
    static const auto getGroup = reinterpret_cast<GetGroupFn>(
        server ? GetProcAddress(server, "?GetReputationGroup@Reputation@pub@@YAHAAIPBD@Z") : nullptr);
    static const auto getShort = reinterpret_cast<GetShortNameFn>(
        server ? GetProcAddress(server, "?GetShortGroupName@Reputation@pub@@YAHABIAAI@Z") : nullptr);
    u32 group = 0, ids = 0;
    std::wstring faction;
    if (getGroup && getShort && !n.affiliation.empty() && getGroup(group, n.affiliation.c_str()) == 0 && group &&
        getShort(group, ids) == 0)
        faction = IdsText(ids);
    return faction.empty() || faction == name ? name : name + L" \x00B7 " + faction;
}
} // namespace names
