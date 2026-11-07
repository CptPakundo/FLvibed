// What the game's data files say: gamedata.h.
#include "game/gamedata.h"

#include "game/baseinfo.h"
#include "game/ini.h"
#include "game/universe_data.h"
#include "log.h"
#include "str.h"
#include "text.h"

#include <unordered_map>

namespace gamedata
{
using namespace equiprules;

static u32 Id(const char* nick)
{
    static auto createId = [] { HMODULE c = GetModuleHandleA("common.dll"); return c ? reinterpret_cast<u32(__cdecl*)(const char*)>(GetProcAddress(c, "?CreateID@@YAIPBD@Z")) : nullptr; }();
    return createId ? createId(nick) : 0;
}
static u32 Id(const std::string& nick) { return Id(nick.c_str()); }
static float Num(const ini::Section& s, const char* key, float none = 0)
{
    const std::string* v = s.Get(key);
    return v ? static_cast<float>(atof(v->c_str())) : none;
}
static std::string Str(const ini::Section& s, const char* key)
{
    const std::string* v = s.Get(key);
    return v ? *v : std::string();
}

// ---- equipment
struct Blast
{
    float hull = 0, energy = 0;
};
static std::unordered_map<u32, Spec> g_equip;
static std::unordered_map<u32, u32> g_info;
static bool g_equipRead = false;

static void ReadEquip()
{
    if (g_equipRead)
        return;
    g_equipRead = true;
    std::unordered_map<std::string, Blast> blasts;
    std::unordered_map<std::string, ini::Section> ammo; // [Mine], [CounterMeasure] by nickname
    std::vector<ini::Section> all;
    for (const std::string& f : ini::DataFiles("equipment"))
    {
        std::vector<ini::Section> part = ini::Read(f);
        all.insert(all.end(), part.begin(), part.end());
    }
    for (const ini::Section& s : all)
    {
        const std::string nick = Lower(Str(s, "nickname"));
        if (s.type == "explosion")
            blasts[nick] = {Num(s, "hull_damage"), Num(s, "energy_damage")};
        else if (s.type == "mine" || s.type == "countermeasure")
            ammo[nick] = s;
    }
    for (const ini::Section& s : all)
    {
        const std::string nick = Str(s, "nickname");
        if (nick.empty())
            continue;
        if (const u32 info = static_cast<u32>(Num(s, "ids_info")))
            g_info[Id(nick)] = info;
        Spec spec;
        if (s.type == "gun")
            spec.type = Lower(Str(s, "hp_gun_type"));
        else if (s.type == "shieldgenerator")
        {
            spec.type = Lower(Str(s, "hp_type"));
            spec.stats = {{L"CAPACITY", Num(s, "max_capacity"), 1, L""}, {L"REGENERATION", Num(s, "regeneration_rate"), 1, L"/S"}, {L"ENERGY DRAW", Num(s, "constant_power_draw"), -1, L"/S"}, {L"REBUILD", Num(s, "offline_rebuild_time"), -1, L"S"}};
        }
        else if (s.type == "thruster")
        {
            spec.type = "hp_thruster";
            spec.stats = {{L"FORCE", Num(s, "max_force"), 1, L""}, {L"ENERGY DRAW", Num(s, "power_usage"), -1, L"/S"}};
        }
        else if (s.type == "countermeasuredropper")
        {
            spec.type = "hp_countermeasure_dropper";
            const auto it = ammo.find(Lower(Str(s, "projectile_archetype")));
            if (it != ammo.end())
                spec.stats = {{L"RANGE", Num(it->second, "range"), 1, L"M"}, {L"DIVERTS", Num(it->second, "diversion_pctg"), 1, L"%"}};
            spec.stats.push_back({L"REFIRE", Num(s, "refire_delay"), -1, L"S"});
        }
        else if (s.type == "minedropper")
        {
            spec.type = "hp_mine_dropper";
            const auto it = ammo.find(Lower(Str(s, "projectile_archetype")));
            if (it != ammo.end())
            {
                const auto b = blasts.find(Lower(Str(it->second, "explosion_arch")));
                if (b != blasts.end())
                    spec.stats = {{L"HULL DAMAGE", b->second.hull, 1, L""}, {L"SHIELD DAMAGE", b->second.energy, 1, L""}};
                spec.stats.push_back({L"SEEKS", Num(it->second, "seek_dist"), 1, L"M"});
                spec.stats.push_back({L"SPEED", Num(it->second, "top_speed"), 1, L"M/S"});
            }
            spec.stats.push_back({L"REFIRE", Num(s, "refire_delay"), -1, L"S"});
        }
        else if (s.type == "scanner")
            spec.stats = {{L"RANGE", Num(s, "range"), 1, L"M"}, {L"CARGO SCAN", Num(s, "cargo_scan_range"), 1, L"M"}};
        else if (s.type == "power")
            spec.stats = {{L"CAPACITY", Num(s, "capacity"), 1, L""}, {L"CHARGE", Num(s, "charge_rate"), 1, L"/S"}};
        else if (s.type == "armor")
            spec.stats = {{L"HULL FACTOR", Num(s, "hit_pts_scale"), 1, L"X"}};
        else
            continue;
        g_equip[Id(nick)] = std::move(spec);
    }
    Log("gamedata: %d items", static_cast<int>(g_equip.size()));
}

const Spec* Equip(u32 arch)
{
    ReadEquip();
    const auto it = g_equip.find(arch);
    if (it == g_equip.end())
        return nullptr;
    if (!it->second.type.empty() && it->second.stats.empty() && SlotOf(it->second.type).filter == 0)
    {
        baseinfo::Gun g;
        if (baseinfo::GunOf(arch, g))
        {
            if (g.hullDps >= 0)
                it->second.stats.push_back({L"HULL DAMAGE", g.hullDps, 1, L"/S"});
            if (g.shieldDps >= 0)
                it->second.stats.push_back({L"SHIELD DAMAGE", g.shieldDps, 1, L"/S"});
            if (g.energyPs >= 0)
                it->second.stats.push_back({L"ENERGY DRAW", g.energyPs, -1, L"/S"});
            if (g.range >= 0)
                it->second.stats.push_back({L"RANGE", g.range, 1, L"M"});
            if (g.speed >= 0)
                it->second.stats.push_back({L"SHOT SPEED", g.speed, 1, L"M/S"});
            if (g.rate >= 0)
                it->second.stats.push_back({L"SHOTS", g.rate, 0, L"/S"});
        }
    }
    return &it->second;
}

u32 EquipInfo(u32 arch)
{
    ReadEquip();
    const auto it = g_info.find(arch);
    return it != g_info.end() ? it->second : 0;
}

// ---- ships
struct ShipData
{
    std::vector<Hp> hps;
    ShipFacts facts;
};
static std::unordered_map<u32, ShipData> g_ships;
static bool g_shipsRead = false;
static void ReadShips()
{
    if (g_shipsRead)
        return;
    g_shipsRead = true;
    for (const std::string& f : ini::DataFiles("ships"))
        for (const ini::Section& s : ini::Read(f))
        {
            if (s.type != "ship")
                continue;
            const std::string nick = Str(s, "nickname");
            if (nick.empty())
                continue;
            ShipData d;
            for (const std::string& line : s.All("hp_type"))
                AddLine(d.hps, ini::Fields(line));
            ShipFacts& k = d.facts;
            k.arch = Id(nick), k.nameIds = static_cast<u32>(Num(s, "ids_name"));
            k.infoIds[0] = static_cast<u32>(Num(s, "ids_info")), k.infoIds[1] = static_cast<u32>(Num(s, "ids_info1")), k.infoIds[2] = static_cast<u32>(Num(s, "ids_info2"));
            k.hull = Num(s, "hit_pts"), k.hold = Num(s, "hold_size"), k.nanobots = Num(s, "nanobot_limit"), k.batteries = Num(s, "shield_battery_limit");
            for (const Hp& h : d.hps)
            {
                const Slot b = BestOf(h);
                if (!b.word)
                    continue;
                if (b.rank == 0)
                    ++k.guns, k.gunClass = std::max(k.gunClass, b.cls);
                else if (b.rank == 1)
                    ++k.turrets;
                else if (b.rank == 2)
                    ++k.torpedoes;
                else if (b.rank == 3)
                    k.shieldClass = std::max(k.shieldClass, b.cls);
            }
            k.ok = true;
            g_ships[k.arch] = std::move(d);
        }
}
const std::vector<Hp>& ShipHardpoints(u32 shipArch)
{
    static const std::vector<Hp> none;
    ReadShips();
    const auto it = g_ships.find(shipArch);
    return it != g_ships.end() ? it->second.hps : none;
}
ShipFacts Ship(u32 shipArch)
{
    ReadShips();
    const auto it = g_ships.find(shipArch);
    return it != g_ships.end() ? it->second.facts : ShipFacts();
}

ShipFacts ShipNamed(const std::wstring& name)
{
    ReadShips();
    static std::unordered_map<std::wstring, u32> byName;
    if (byName.empty())
        for (const auto& s : g_ships)
            if (s.second.facts.hull > 0 && s.second.facts.nameIds) // (the first of a name with a hull: the story's copies share it)
                byName.emplace(text::Ids(s.second.facts.nameIds), s.first);
    const auto it = byName.find(name);
    return it != byName.end() ? Ship(it->second) : ShipFacts();
}

static float Strength(const Spec* s) { return s && !s->stats.empty() ? s->stats[0].value : 0; }

Versus Against(u32 arch)
{
    Versus v;
    const Spec* me = Equip(arch);
    if (!me || me->type.empty())
        return v;
    v.hasSlot = true;
    const std::vector<baseinfo::Item> items = baseinfo::Equipment();
    float weakest = 0;
    for (const Hp& h : ShipHardpoints(baseinfo::ShipArch()))
    {
        if (!Accepts(h, me->type))
            continue;
        v.fits = true;
        u32 on = 0;
        for (const baseinfo::Item& it : items)
            if (it.mounted && Lower(it.hardpoint) == h.name)
                on = it.arch;
        if (!on)
        {
            v.freeHp = true;
            continue;
        }
        const float s = Strength(Equip(on));
        if (!v.arch || s < weakest)
            v.arch = on, weakest = s;
    }
    return v;
}

std::vector<Hp> FreeHardpoints(int filter)
{
    std::vector<std::string> used;
    for (const baseinfo::Item& it : baseinfo::Equipment())
        if (it.mounted && !it.hardpoint.empty())
            used.push_back(it.hardpoint);
    return Free(ShipHardpoints(baseinfo::ShipArch()), used, filter);
}

std::vector<u32> HoldItemsFor(const Hp& hp)
{
    std::vector<u32> out;
    for (const baseinfo::Item& it : baseinfo::Equipment())
    {
        if (it.mounted)
            continue;
        const Spec* s = Equip(it.arch);
        if (s && !s->type.empty() && Accepts(hp, s->type))
            out.push_back(it.arch);
    }
    return out;
}

// ---- factions
static std::unordered_map<std::string, u32> g_groups;
u32 FactionIds(const std::string& nick)
{
    static bool read = false;
    if (!read)
    {
        read = true;
        for (const ini::Section& s : ini::Read(ini::DataPath("initialworld.ini")))
            if (s.type == "group")
                g_groups[Lower(Str(s, "nickname"))] = static_cast<u32>(Num(s, "ids_name"));
    }
    const auto it = g_groups.find(Lower(nick));
    return it != g_groups.end() ? it->second : 0;
}
bool Standing(const std::string& nick, float& out) { return baseinfo::Standing(nick.c_str(), out); }

static void SortFactions(std::vector<Faction>& list)
{
    std::stable_sort(list.begin(), list.end(), [](const Faction& a, const Faction& b) { return a.weight > b.weight; });
    for (Faction& f : list)
    {
        f.nameIds = FactionIds(f.nick);
        Standing(f.nick, f.standing);
    }
}
static void AddFaction(std::vector<Faction>& list, const std::string& nick, float weight)
{
    const std::string n = Lower(nick);
    for (Faction& f : list)
        if (f.nick == n)
        {
            f.weight += weight;
            return;
        }
    Faction f;
    f.nick = n, f.weight = weight;
    list.push_back(f);
}

// ---- the universe: read once; a base's rooms and a system's file when first asked
struct GoodData
{
    u32 arch = 0;
    float price = 0;
};
static std::vector<ini::Section> g_universe;
static std::unordered_map<u32, BaseFacts> g_bases;
static std::unordered_map<u32, std::string> g_baseFile;
static std::unordered_map<std::string, std::wstring> g_systemName;
static bool g_worldRead = false;

static void ReadWorld()
{
    if (g_worldRead)
        return;
    g_worldRead = true;
    g_universe = ini::Read(ini::DataPath("UNIVERSE\\universe.ini"));
    for (const ini::Section& s : g_universe)
        if (s.type == "base")
        {
            const u32 id = Id(Str(s, "nickname"));
            g_bases[id].id = id, g_bases[id].nameIds = static_cast<u32>(Num(s, "strid_name"));
            g_baseFile[id] = Str(s, "file");
        }
        else if (s.type == "system")
            g_systemName[Lower(Str(s, "nickname"))] = text::Ids(static_cast<u32>(Num(s, "strid_name")));
    std::unordered_map<u32, GoodData> goods;
    for (const std::string& f : ini::DataFiles("goods"))
        for (const ini::Section& s : ini::Read(f))
            if (s.type == "good" && Str(s, "category") == "commodity")
                goods[Id(Str(s, "nickname"))] = {Id(Str(s, "equipment")), Num(s, "price")};
    for (const ini::Section& s : ini::Read(ini::DataPath("EQUIPMENT\\market_commodities.ini")))
    {
        if (s.type != "basegood")
            continue;
        const auto b = g_bases.find(Id(Str(s, "base")));
        if (b == g_bases.end())
            continue;
        for (const std::string& line : s.All("marketgood"))
        {
            const std::vector<std::string> f = ini::Fields(line);
            if (f.size() < 7)
                continue;
            const auto g = goods.find(Id(f[0]));
            if (g == goods.end())
                continue;
            Good good;
            good.arch = g->second.arch, good.factor = static_cast<float>(atof(f[6].c_str())), good.price = g->second.price * good.factor;
            good.sold = atoi(f[5].c_str()) == 0;
            b->second.goods.push_back(good);
        }
    }
    BaseFacts* at = nullptr;
    for (const ini::Section& s : ini::Read(ini::DataPath("MISSIONS\\mbases.ini")))
    {
        if (s.type == "mbase")
        {
            const auto b = g_bases.find(Id(Str(s, "nickname")));
            at = b != g_bases.end() ? &b->second : nullptr;
        }
        else if (at && s.type == "mvendor")
        {
            const std::vector<std::string> f = ini::Fields(Str(s, "num_offers"));
            if (f.size() >= 2)
                at->offersMin = atoi(f[0].c_str()), at->offersMax = atoi(f[1].c_str());
        }
        else if (at && s.type == "basefaction")
            AddFaction(at->factions, Str(s, "faction"), Num(s, "weight"));
    }
}

const BaseFacts& Base(u32 baseId)
{
    static const BaseFacts none;
    ReadWorld();
    const auto it = g_bases.find(baseId);
    if (it == g_bases.end())
        return none;
    BaseFacts& b = it->second;
    if (!b.ok)
    {
        b.ok = true;
        for (const ini::Section& s : ini::Read(ini::DataPath(g_baseFile[baseId])))
            if (s.type == "room")
            {
                const std::string n = Lower(Str(s, "nickname"));
                b.bar = b.bar || n == "bar";
                b.trader = b.trader || n == "trader";
                b.equipment = b.equipment || n == "equipment";
                b.ships = b.ships || n == "shipdealer";
            }
        std::stable_sort(b.goods.begin(), b.goods.end(), [](const Good& x, const Good& y) { return x.factor > y.factor; });
    }
    SortFactions(b.factions); // (the standing moves)
    return b;
}

static std::unordered_map<u32, SystemFacts> g_systems;
const SystemFacts& System(u32 systemId)
{
    ReadWorld();
    SystemFacts& y = g_systems[systemId];
    if (!y.ok)
    {
        y.ok = true, y.id = systemId;
        const std::string file = unidata::SystemFile(g_universe, systemId, Id);
        for (const ini::Section& s : file.empty() ? std::vector<ini::Section>() : ini::Read(ini::DataPath("UNIVERSE\\" + file)))
            if (s.type == "object")
            {
                SolarFact o;
                o.id = Id(Str(s, "nickname"));
                const std::string base = Str(s, "base");
                o.base = base.empty() ? 0 : Id(base);
                o.owner = FactionIds(Str(s, "reputation"));
                const std::vector<std::string> go = ini::Fields(Str(s, "goto"));
                if (!go.empty())
                {
                    o.goesTo = Lower(go[0]);
                    const auto n = g_systemName.find(o.goesTo);
                    if (n != g_systemName.end())
                        o.goesToName = n->second;
                }
                if (o.base || !o.goesTo.empty())
                    y.solars.push_back(o);
            }
            else if (s.type == "zone")
            {
                for (const std::string& line : s.All("faction"))
                {
                    const std::vector<std::string> f = ini::Fields(line);
                    if (!f.empty())
                        AddFaction(y.factions, f[0], f.size() > 1 ? static_cast<float>(atof(f[1].c_str())) : 1);
                }
            }
            else if (s.type == "asteroids")
                ++y.fields;
            else if (s.type == "nebula")
                ++y.nebulae;
    }
    SortFactions(y.factions);
    return y;
}
} // namespace gamedata
