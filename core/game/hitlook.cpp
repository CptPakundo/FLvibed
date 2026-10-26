#include "hitlook.h"

#include "flhash.h"
#include "ini.h"
#include "log.h"
#include "utf.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace hitlook
{
static std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return s;
}
static std::wstring Wide(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// (in-tree, bit-identical to Common.dll's export: core\game\flhash.h)
static u32 CreateId(const std::string& nickname) { return flhash::CreateId(nickname); }

// ---------------------------------------------------------------- FLCRC32 (Alchemy name hashes)
// CRC-32 (init / final xor 0xFFFFFFFF) over alchemy.dll's OWN table (t[1] = 0x09073096; docs\FREELANCER_RE.md
// "Hit effects"), found in the loaded module by the CRC-table property t[i^j] = t[i] ^ t[j] and t[128] = 0xEDB88320.
static const u32* AlchemyTable()
{
    static const u32* table = nullptr;
    static bool searched = false;
    if (searched)
        return table;
    searched = true;
    const auto base = reinterpret_cast<const u8*>(GetModuleHandleA("alchemy.dll"));
    if (!base)
        return nullptr;
    __try
    {
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
        const u32 size = nt->OptionalHeader.SizeOfImage;
        for (u32 o = 0; o + 1024 <= size; o += 4)
        {
            const u32* t = reinterpret_cast<const u32*>(base + o);
            if (t[0] != 0 || t[128] != 0xEDB88320u || t[1] == 0)
                continue;
            bool ok = true;
            for (int a = 1; a < 16 && ok; ++a)
                for (int c = 1; c < 16; ++c)
                    if (t[a ^ c] != (t[a] ^ t[c]))
                    {
                        ok = false;
                        break;
                    }
            if (ok)
            {
                table = t;
                break;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        table = nullptr;
    }
    if (!table)
        Log("hitlook: alchemy.dll CRC table not found");
    return table;
}

static u32 FlCrc(const std::string& s)
{
    const u32* t = AlchemyTable();
    if (!t)
        return 0;
    u32 c = 0xFFFFFFFFu;
    for (unsigned char ch : s)
        c = t[(c ^ ch) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

// ---------------------------------------------------------------- .ale (Alchemy) parsing
// Format: research\repos\KnowledgeBase\docs\file-structures\utf\ale.md; tools\aledump.js is the reference reader.
struct Reader
{
    const std::vector<u8>& b;
    size_t p = 0;
    bool bad = false;
    bool Need(size_t n)
    {
        if (p + n > b.size())
            bad = true;
        return !bad;
    }
    u8 U8() { return Need(1) ? b[p++] : 0; }
    u16 U16()
    {
        if (!Need(2))
            return 0;
        u16 v;
        memcpy(&v, &b[p], 2);
        p += 2;
        return v;
    }
    u32 U32()
    {
        if (!Need(4))
            return 0;
        u32 v;
        memcpy(&v, &b[p], 4);
        p += 4;
        return v;
    }
    float F32()
    {
        if (!Need(4))
            return 0;
        float v;
        memcpy(&v, &b[p], 4);
        p += 4;
        return v;
    }
    std::string Str() // count includes the NUL; padded to an even byte count
    {
        const u16 n = U16();
        if (!Need(n))
            return {};
        std::string s(reinterpret_cast<const char*>(&b[p]), n);
        p += n + (n & 1);
        s.erase(std::find(s.begin(), s.end(), '\0'), s.end());
        return s;
    }
};

struct Prop
{
    u16 base = 0;
    std::string str;
    float f = 0;
    int i = 0;
    u32 blend[2] = {};
    std::vector<Key> keys;       // animated float (first set) / curve (value keys, or the default as one key)
    std::vector<ColorKey> color; // animated colour (first set)
};
struct Node
{
    std::string type;
    std::unordered_map<u32, Prop> props; // by FLCRC32 of the property name
};

static void ReadAnim(Reader& r, Prop& p, bool color)
{
    r.U8();
    const u8 n = r.U8();
    for (u8 i = 0; i < n && !r.bad; ++i)
    {
        r.F32(); // the set's key (first set used)
        r.U8();
        const u8 m = r.U8();
        for (u8 j = 0; j < m && !r.bad; ++j)
        {
            const float t = r.F32();
            if (color)
            {
                const float cr = r.F32(), cg = r.F32(), cb = r.F32();
                if (i == 0)
                    p.color.push_back({t, cr, cg, cb});
            }
            else
            {
                const float v = r.F32();
                if (i == 0)
                    p.keys.push_back({t, v});
            }
        }
    }
}
static void ReadCurve(Reader& r, Prop* p)
{
    r.U8();
    const u8 n = r.U8();
    for (u8 i = 0; i < n && !r.bad; ++i)
    {
        r.F32();
        const float def = r.F32();
        r.U16();
        const u16 m = r.U16();
        for (u16 j = 0; j < m && !r.bad; ++j)
        {
            const float t = r.F32(), v = r.F32();
            r.F32(), r.F32();
            if (p && i == 0)
                p->keys.push_back({t, v});
        }
        if (p && i == 0 && m == 0)
            p->keys.push_back({0, def});
    }
}

struct Ale
{
    std::unordered_map<u32, Node> nodes; // by FLCRC32 of Node_Name
    struct Instance
    {
        u32 flags, name;
        int parent, id;
    };
    struct Effect
    {
        std::string name;
        std::vector<Instance> inst;
        std::vector<std::pair<int, int>> pairs;
    };
    std::vector<Effect> effects;
};

static bool ParseAle(const std::string& file, Ale& out)
{
    const auto leaves = utf::Leaves(Wide(file));
    const std::vector<u8>* nl = nullptr;
    const std::vector<u8>* el = nullptr;
    for (const auto& l : leaves)
    {
        const std::string p = Lower(l.path);
        if (p.size() >= 18 && p.compare(p.size() - 18, 18, "alchemynodelibrary") == 0)
            nl = &l.data;
        else if (p.size() >= 11 && p.compare(p.size() - 11, 11, "aleffectlib") == 0)
            el = &l.data;
    }
    if (!nl || !el || !AlchemyTable())
        return false;
    const u32 kNodeName = FlCrc("Node_Name");
    Reader r{*nl};
    r.F32();
    const u32 count = r.U32();
    for (u32 i = 0; i < count && !r.bad; ++i)
    {
        Node n;
        n.type = r.Str();
        for (;;)
        {
            const u16 tp = r.U16();
            if (tp == 0 || r.bad)
                break;
            const u32 id = r.U32();
            Prop p;
            p.base = tp & 0x7FFF;
            switch (p.base)
            {
            case 0x001: p.i = (tp & 0x8000) ? 1 : 0; break;
            case 0x002: p.i = static_cast<int>(r.U32()); break;
            case 0x003: p.f = r.F32(); break;
            case 0x103: p.str = r.Str(); break;
            case 0x104: p.blend[0] = r.U32(), p.blend[1] = r.U32(); break;
            case 0x105:
                if (r.U32() >> 24)
                    for (int k = 0; k < 9; ++k)
                        ReadCurve(r, nullptr);
                break;
            case 0x200: ReadAnim(r, p, false); break;
            case 0x201: ReadAnim(r, p, true); break;
            case 0x202: ReadCurve(r, &p); break;
            default: r.bad = true; break;
            }
            if (!r.bad)
                n.props[id] = std::move(p);
        }
        const auto it = n.props.find(kNodeName);
        if (it != n.props.end())
            out.nodes[FlCrc(it->second.str)] = std::move(n);
    }
    if (r.bad)
        Log("hitlook: %s: node library unreadable", file.c_str());
    Reader e{*el};
    const float ver = e.F32();
    const u32 ecount = e.U32();
    for (u32 i = 0; i < ecount && !e.bad; ++i)
    {
        Ale::Effect fx;
        fx.name = e.Str();
        if (ver > 1.05f)
            for (int k = 0; k < 4; ++k)
                e.F32();
        const u32 ni = e.U32();
        for (u32 k = 0; k < ni && !e.bad; ++k)
        {
            Ale::Instance in;
            in.flags = e.U32(), in.name = e.U32();
            in.parent = static_cast<int>(e.U32()), in.id = static_cast<int>(e.U32());
            fx.inst.push_back(in);
        }
        const u32 np = e.U32();
        for (u32 k = 0; k < np && !e.bad; ++k)
        {
            const int s = static_cast<int>(e.U32()), t = static_cast<int>(e.U32());
            fx.pairs.emplace_back(s, t);
        }
        out.effects.push_back(std::move(fx));
    }
    return !r.bad && !e.bad;
}

// ---------------------------------------------------------------- game data
struct VisEffect
{
    std::string alchemy;
    u32 crc = 0;
    std::vector<std::string> textures;
};
struct Data
{
    bool loaded = false;
    std::unordered_map<std::string, std::string> effectVis; // [Effect] nickname -> vis_effect (lower case)
    std::unordered_map<std::string, VisEffect> vis;         // [VisEffect] nickname -> ...
    struct Tier
    {
        float damage;
        std::string effect;
    };
    std::unordered_map<u32, std::vector<Tier>> shieldTiers; // shield arch -> tiers (ascending damage)
    std::unordered_map<u32, std::string> munitionHit;       // munition arch -> munition_hit_effect
    std::unordered_map<u32, std::vector<Tier>> hullTiers;   // ship arch -> surface_hit_effects tiers (ascending damage)
    struct Fuse
    {
        float hitPts;
        std::string name;
    };
    std::unordered_map<u32, std::vector<Fuse>> shipFuses; // ship arch -> its damage fuses (lit below hitPts)
    std::unordered_map<std::string, std::vector<std::string>> fuseEffects; // non-death fuse -> start_effect effects
    std::unordered_map<u32, std::vector<DamageStage>> stages;              // ship arch -> built damage stages (cache)
    std::unordered_map<std::string, Look> looks;                           // by effect nickname (lower case)
    std::unordered_map<std::string, Ale> ales;                             // by DATA-relative path (lower case)
};
static Data g;

static void ListInis(const std::string& dir, std::vector<std::string>& out, int depth = 0)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do
    {
        const std::string n = fd.cFileName;
        if (n == "." || n == "..")
            continue;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && depth < 4)
            ListInis(dir + "\\" + n, out, depth + 1);
        else if (n.size() > 4 && _stricmp(n.c_str() + n.size() - 4, ".ini") == 0)
            out.push_back(dir + "\\" + n);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static void Load()
{
    if (g.loaded)
        return;
    g.loaded = true;
    for (const auto& s : ini::Read(ini::DataPath("FX\\effects.ini")))
        if (s.type == "effect")
            if (const auto* n = s.Get("nickname"))
                if (const auto* v = s.Get("vis_effect"))
                    g.effectVis[Lower(*n)] = Lower(*v);
    std::vector<std::string> files;
    ListInis(ini::DataPath("FX"), files);
    for (const auto& f : files)
        for (const auto& s : ini::Read(f))
            if (s.type == "viseffect")
                if (const auto* n = s.Get("nickname"))
                {
                    VisEffect v;
                    if (const auto* a = s.Get("alchemy"))
                        v.alchemy = *a;
                    if (const auto* c = s.Get("effect_crc"))
                        v.crc = static_cast<u32>(strtoll(c->c_str(), nullptr, 10));
                    v.textures = s.All("textures");
                    g.vis[Lower(*n)] = std::move(v);
                }
    for (const auto& s : ini::Read(ini::DataPath("EQUIPMENT\\st_equip.ini")))
        if (s.type == "shieldgenerator")
            if (const auto* n = s.Get("nickname"))
            {
                std::vector<Data::Tier> tiers;
                for (const auto& v : s.All("shield_hit_effects"))
                {
                    const auto f = ini::Fields(v);
                    if (f.size() >= 2)
                        tiers.push_back({static_cast<float>(atof(f[0].c_str())), Lower(f[1])});
                }
                std::sort(tiers.begin(), tiers.end(),
                          [](const Data::Tier& a, const Data::Tier& b) { return a.damage < b.damage; });
                if (!tiers.empty())
                    g.shieldTiers[CreateId(*n)] = std::move(tiers);
            }
    for (const auto& s : ini::Read(ini::DataPath("EQUIPMENT\\weapon_equip.ini")))
        if (s.type == "munition")
            if (const auto* n = s.Get("nickname"))
                if (const auto* h = s.Get("munition_hit_effect"))
                    g.munitionHit[CreateId(*n)] = Lower(*h);
    // ships (freelancer.ini [Data] ships = ...): surface_hit_effects = <damage>, <effect>... (the game picks one of the
    // tier's effects; all tiers of the stock ships share one visual and differ only in sound) and fuse = <name>, <t>,
    // <hit points> (the fuse lights when the hull drops below the hit points)
    for (const auto& f : ini::DataFiles("ships"))
        for (const auto& s : ini::Read(f))
            if (s.type == "ship")
                if (const auto* n = s.Get("nickname"))
                {
                    const u32 id = CreateId(*n);
                    std::vector<Data::Tier> tiers;
                    for (const auto& v : s.All("surface_hit_effects"))
                    {
                        const auto fl = ini::Fields(v);
                        if (fl.size() >= 2)
                            tiers.push_back({static_cast<float>(atof(fl[0].c_str())), Lower(fl[1])});
                    }
                    std::sort(tiers.begin(), tiers.end(),
                              [](const Data::Tier& a, const Data::Tier& b) { return a.damage < b.damage; });
                    if (!tiers.empty())
                        g.hullTiers[id] = std::move(tiers);
                    for (const auto& v : s.All("fuse"))
                    {
                        const auto fl = ini::Fields(v);
                        if (fl.size() >= 3 && atof(fl[2].c_str()) > 0)
                            g.shipFuses[id].push_back({static_cast<float>(atof(fl[2].c_str())), Lower(fl[0])});
                    }
                }
    // fuses (freelancer.ini [Data] fuses = ...): [fuse] name / death_fuse, then its [start_effect] effect = ... sections
    for (const auto& f : ini::DataFiles("fuses"))
    {
        std::string cur;
        for (const auto& s : ini::Read(f))
        {
            if (s.type == "fuse")
            {
                const auto* n = s.Get("name");
                const auto* d = s.Get("death_fuse");
                cur = n && !(d && Lower(*d) == "true") ? Lower(*n) : std::string();
                if (!cur.empty())
                    g.fuseEffects[cur];
            }
            else if (s.type == "start_effect" && !cur.empty())
                if (const auto* e = s.Get("effect"))
                    g.fuseEffects[cur].push_back(Lower(*e));
        }
    }
    Log("hitlook: %zu effects, %zu visual effects, %zu shields, %zu munitions with hit effects, %zu ships with hull "
        "hit "
        "effects, %zu with damage fuses, %zu non-death fuses",
        g.effectVis.size(), g.vis.size(), g.shieldTiers.size(), g.munitionHit.size(), g.hullTiers.size(),
        g.shipFuses.size(), g.fuseEffects.size());
}

static float FirstMax(const Prop* p)
{
    if (!p || p->keys.empty())
        return 0;
    float m = p->keys[0].v;
    for (const auto& k : p->keys)
        m = std::max(m, k.v);
    return m;
}

static const Look* Build(const std::string& effect)
{
    if (effect.empty())
        return nullptr;
    if (const auto it = g.looks.find(effect); it != g.looks.end())
        return it->second.ok() ? &it->second : nullptr;
    Look& look = g.looks[effect];
    look.effect = effect;
    const auto ev = g.effectVis.find(effect);
    if (ev == g.effectVis.end())
        return nullptr;
    look.vis = ev->second;
    const auto vi = g.vis.find(ev->second);
    if (vi == g.vis.end() || vi->second.alchemy.empty())
        return nullptr;
    const VisEffect& v = vi->second;
    const std::string key = Lower(v.alchemy);
    auto ai = g.ales.find(key);
    if (ai == g.ales.end())
    {
        ai = g.ales.emplace(key, Ale{}).first;
        if (!ParseAle(ini::DataPath(v.alchemy), ai->second))
            Log("hitlook: %s not readable", v.alchemy.c_str());
    }
    const Ale& ale = ai->second;
    const Ale::Effect* fx = nullptr;
    for (const auto& e : ale.effects)
        if (FlCrc(e.name) == v.crc || _stricmp(e.name.c_str(), ev->second.c_str()) == 0)
        {
            fx = &e;
            break;
        }
    if (!fx)
        return nullptr;
    const u32 kTex = FlCrc("BasicApp_TexName"), kColor = FlCrc("BasicApp_Color"), kAlpha = FlCrc("BasicApp_Alpha"),
              kSize = FlCrc("BasicApp_Size"), kBlend = FlCrc("BasicApp_BlendInfo"), kName = FlCrc("Node_Name"),
              kLife = FlCrc("Emitter_InitLifeSpan"), kInit = FlCrc("Emitter_InitialParticles"),
              kFreq = FlCrc("Emitter_Frequency"), kNodeLife = FlCrc("Node_LifeSpan");
    auto prop = [](const Node& n, u32 id) -> const Prop*
    {
        const auto it = n.props.find(id);
        return it == n.props.end() ? nullptr : &it->second;
    };
    for (const auto& in : fx->inst)
    {
        const auto ni = ale.nodes.find(in.name);
        if (in.flags || ni == ale.nodes.end())
            continue;
        const Node& n = ni->second;
        const Prop* tex = prop(n, kTex);
        if (!tex || tex->str.empty())
            continue;
        Layer l;
        if (const Prop* p = prop(n, kName))
            l.name = p->str;
        l.tex = tex->str;
        l.libs = v.textures;
        if (const Prop* p = prop(n, kBlend))
            l.additive = p->blend[1] == 2; // dest ONE
        if (const Prop* p = prop(n, kColor))
            l.color = p->color;
        if (const Prop* p = prop(n, kAlpha))
            l.alpha = p->keys;
        if (const Prop* p = prop(n, kSize))
            l.size = p->keys;
        int count = 0;
        for (const auto& pr : fx->pairs)
        {
            if (pr.second != in.id)
                continue;
            for (const auto& src : fx->inst)
            {
                if (src.id != pr.first)
                    continue;
                const auto si = ale.nodes.find(src.name);
                if (si == ale.nodes.end())
                    continue;
                l.life = std::max(l.life, FirstMax(prop(si->second, kLife)));
                l.rate = std::max(l.rate, FirstMax(prop(si->second, kFreq)));
                if (const Prop* p = prop(si->second, kInit))
                    count += p->i;
            }
        }
        if (l.life <= 0)
            if (const Prop* p = prop(n, kNodeLife))
                l.life = std::min(p->f, 3.0f);
        l.count = std::max(1, count);
        look.layers.push_back(std::move(l));
    }
    return look.ok() ? &look : nullptr;
}

const Look* Shield(u32 shieldArch, float damage)
{
    Load();
    const auto it = g.shieldTiers.find(shieldArch);
    if (it == g.shieldTiers.end())
        return nullptr;
    std::string effect = it->second.front().effect;
    for (const auto& t : it->second)
        if (damage >= t.damage)
            effect = t.effect;
    return Build(effect);
}

const Look* Impact(u32 munitionArch)
{
    Load();
    const auto it = g.munitionHit.find(munitionArch);
    return it == g.munitionHit.end() ? nullptr : Build(it->second);
}

const Look* Hull(u32 shipArch, float damage)
{
    Load();
    const auto it = g.hullTiers.find(shipArch);
    if (it == g.hullTiers.end())
        return nullptr;
    std::string effect = it->second.front().effect;
    for (const auto& t : it->second)
        if (damage >= t.damage)
            effect = t.effect;
    return Build(effect);
}

const std::vector<DamageStage>& DamageStages(u32 shipArch)
{
    Load();
    if (const auto it = g.stages.find(shipArch); it != g.stages.end())
        return it->second;
    std::vector<DamageStage>& out = g.stages[shipArch];
    const auto sf = g.shipFuses.find(shipArch);
    if (sf == g.shipFuses.end())
        return out;
    for (const auto& fuse : sf->second)
    {
        const auto fe = g.fuseEffects.find(fuse.name);
        if (fe == g.fuseEffects.end())
            continue; // a death fuse or unknown
        DamageStage st;
        st.hitPts = fuse.hitPts;
        st.fuse = fuse.name;
        for (const auto& e : fe->second)
            if (const Look* l = Build(e))
                if (std::find(st.looks.begin(), st.looks.end(), l) == st.looks.end())
                    st.looks.push_back(l);
        if (!st.looks.empty())
            out.push_back(std::move(st));
    }
    std::sort(out.begin(), out.end(), [](const DamageStage& a, const DamageStage& b) { return a.hitPts > b.hitPts; });
    return out;
}

float Eval(const std::vector<Key>& k, float t, float def)
{
    if (k.empty())
        return def;
    if (t <= k.front().t)
        return k.front().v;
    for (size_t i = 1; i < k.size(); ++i)
        if (t <= k[i].t)
        {
            const float span = k[i].t - k[i - 1].t;
            const float w = span > 1e-6f ? (t - k[i - 1].t) / span : 1.0f;
            return k[i - 1].v + (k[i].v - k[i - 1].v) * w;
        }
    return k.back().v;
}

void EvalColor(const std::vector<ColorKey>& k, float t, float& r, float& g2, float& b)
{
    r = g2 = b = 1;
    if (k.empty())
        return;
    const ColorKey* a = &k.front();
    const ColorKey* c = &k.front();
    for (size_t i = 0; i < k.size(); ++i)
    {
        c = &k[i];
        if (k[i].t >= t)
            break;
        a = &k[i];
    }
    const float span = c->t - a->t;
    const float w = span > 1e-6f ? std::clamp((t - a->t) / span, 0.0f, 1.0f) : 1.0f;
    r = a->r + (c->r - a->r) * w, g2 = a->g + (c->g - a->g) * w, b = a->b + (c->b - a->b) * w;
}

bool Texture(const Layer& l, int& w, int& h, std::vector<u32>& argb)
{
    // leaf paths start with the root's own "\" ("\\Texture library\<name>\MIP0"): match by suffix
    const std::string want = "\\texture library\\" + Lower(l.tex) + "\\mip0";
    const std::string rectsPath = "\\texture library\\" + Lower(l.tex) + "\\frame rects";
    const std::string frame0 = "\\texture library\\" + Lower(l.tex) + "_0\\mip0"; // animated: <name>_0 holds the frames
    auto ends = [](const std::string& p, const std::string& s)
    { return p.size() >= s.size() && p.compare(p.size() - s.size(), s.size(), s) == 0; };
    for (const auto& lib : l.libs)
    {
        const auto leaves = utf::Leaves(Wide(ini::DataPath(lib)));
        const utf::Leaf* image = nullptr;
        const utf::Leaf* rects = nullptr;
        bool exact = false;
        for (const auto& lf : leaves)
        {
            const std::string p = Lower(lf.path);
            if (ends(p, want))
                image = &lf, exact = true;
            else if (!exact && ends(p, frame0))
                image = &lf;
            else if (ends(p, rectsPath))
                rects = &lf;
        }
        if (!image || !utf::DecodeTga(image->data, w, h, argb))
            continue;
        // animated: crop to the first frame rect {texture index, u0, v0, u1, v1}; V counts from the bottom (lightningaxm
        // frame 0 = v 1.0 -> 0.75: the top row of a 4 x 4 atlas, verified 19 Oct 2026)
        if (rects && rects->data.size() >= 20)
        {
            float r[4];
            memcpy(r, rects->data.data() + 4, 16);
            const float u0 = std::min(r[0], r[2]), u1 = std::max(r[0], r[2]);
            const float v0 = std::min(r[1], r[3]), v1 = std::max(r[1], r[3]);
            if (u0 >= 0 && v0 >= 0 && u1 <= 1.001f && v1 <= 1.001f && u1 > u0 && v1 > v0)
            {
                const int x0 = static_cast<int>(u0 * w), y0 = static_cast<int>((1.0f - v1) * h);
                const int cw = std::max(1, static_cast<int>((u1 - u0) * w)),
                          ch = std::max(1, static_cast<int>((v1 - v0) * h));
                std::vector<u32> crop(static_cast<size_t>(cw) * ch);
                for (int y = 0; y < ch; ++y)
                    for (int x = 0; x < cw; ++x)
                        crop[static_cast<size_t>(y) * cw + x] =
                            argb[static_cast<size_t>(std::min(h - 1, y0 + y)) * w + std::min(w - 1, x0 + x)];
                argb = std::move(crop);
                w = cw, h = ch;
            }
        }
        return true;
    }
    return false;
}

std::string Describe(const Look* l)
{
    if (!l)
        return "no look";
    std::ostringstream o;
    o << l->effect << " (vis " << l->vis << ")";
    for (const auto& y : l->layers)
    {
        float r0, g0, b0, r1, g1, b1;
        EvalColor(y.color, 0, r0, g0, b0);
        EvalColor(y.color, 1, r1, g1, b1);
        char c[96];
        sprintf_s(c, "rgb(%.2f,%.2f,%.2f)->(%.2f,%.2f,%.2f)", r0, g0, b0, r1, g1, b1);
        o << "\n  " << y.name << " tex=" << y.tex << (y.additive ? " additive " : " alpha ") << c << " size "
          << Eval(y.size, 0, 1) << "->" << Eval(y.size, 1, 1) << " life=" << y.life << "s x" << y.count;
        int w = 0, h = 0;
        std::vector<u32> px;
        o << (Texture(y, w, h, px) ? " texture " + std::to_string(w) + "x" + std::to_string(h)
                                   : std::string(" texture missing"));
    }
    return o.str();
}
} // namespace hitlook
