#include "speakers.h"

#include "data.h"
#include "display.h"
#include "log.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace speakers
{
// The rules (the same as tools\speaker_colors.py, which fits the colours): speakers within kWindow turns of each other
// "appear together" and need 0.10 in OKLab (two characters), 0.08 (a character and a station / ship / wing) or 0.06
// (two of those); main characters 0.08 apart from each other and 0.10 from the reserved colours; side characters 0.06
// from the rest of their family, 0.04 from other characters.
static const int kWindow = 3;
static const double kReserved = 0.10;

enum class Tier
{
    Main,
    Side,
    Story
};

struct Entry
{
    u32 rgb = 0;
    Tier tier = Tier::Story;
    std::string family;
    std::string stance = "neutral";               // the usual stance
    std::map<std::string, std::string> byMission; // "m04" -> "hostile"
};
static std::map<std::wstring, Entry> g_table;
// who speaks near whom: speaker -> neighbour -> weight (turns apart: 1, 1/2, 1/3)
static std::map<std::wstring, std::map<std::wstring, double>> g_nb;
static std::vector<std::string> g_findings;

struct Lab
{
    double L, a, b;
};

static double Linear(u32 c8)
{
    const double c = c8 / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

static Lab OkLab(u32 rgb)
{
    const double r = Linear(rgb >> 16 & 255), g = Linear(rgb >> 8 & 255), b = Linear(rgb & 255);
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

static double Dist(u32 x, u32 y)
{
    const Lab p = OkLab(x), q = OkLab(y);
    return std::sqrt((p.L - q.L) * (p.L - q.L) + (p.a - q.a) * (p.a - q.a) + (p.b - q.b) * (p.b - q.b));
}

static std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return s;
}

// "dx_m01_0130_donau" -> "m01", "dx_s064x_0101_juni" -> "s064"; "" for other ids.
static std::string Group(const std::string& id)
{
    if (id.size() < 5 || _strnicmp(id.c_str(), "dx_", 3) != 0)
        return "";
    const char k = static_cast<char>(tolower(static_cast<unsigned char>(id[3])));
    if (k != 'm' && k != 's')
        return "";
    std::string g(1, k);
    for (size_t i = 4; i < id.size() && isdigit(static_cast<unsigned char>(id[i])); ++i)
        g += id[i];
    return g.size() > 1 ? g : "";
}

// Story lines of missions (dx_mNN) and scenes (dx_sNNN) in script order (the id's number) -> speaker turns per group.
static void BuildGraph()
{
    g_nb.clear();
    struct Turn
    {
        std::string group, id;
        const std::wstring* who;
    };
    std::vector<Turn> turns;
    for (const data::Line& ln : data::Lines())
        if (ln.kind == data::Kind::Story && !ln.speaker.empty())
        {
            const std::string g = Group(ln.id);
            if (!g.empty())
                turns.push_back({g, Lower(ln.id), &ln.speaker});
        }
    std::sort(turns.begin(), turns.end(),
              [](const Turn& x, const Turn& y) { return x.group != y.group ? x.group < y.group : x.id < y.id; });
    for (size_t i = 0; i < turns.size();)
    {
        std::vector<const std::wstring*> seq;
        size_t j = i;
        for (; j < turns.size() && turns[j].group == turns[i].group; ++j)
            if (seq.empty() || *seq.back() != *turns[j].who)
                seq.push_back(turns[j].who);
        for (size_t a = 0; a < seq.size(); ++a)
            for (size_t b = a + 1; b < seq.size() && b <= a + kWindow; ++b)
                if (*seq[a] != *seq[b])
                {
                    const double w = 1.0 / static_cast<double>(b - a);
                    g_nb[*seq[a]][*seq[b]] += w;
                    g_nb[*seq[b]][*seq[a]] += w;
                }
        i = j;
    }
}

static u32 ColourOf(const std::wstring& s)
{
    if (s == L"Trent")
        return display::NameRgb(display::Relation::Player);
    auto it = g_table.find(s);
    return it != g_table.end() ? it->second.rgb : 0;
}

static Tier TierOf(const std::wstring& s)
{
    auto it = g_table.find(s);
    return s == L"Trent" || it == g_table.end() ? Tier::Main : it->second.tier;
}

// The distance a and b need (0: none) - tools\speaker_colors.py rules().
static double Needed(const std::wstring& a, const std::wstring& b)
{
    const Tier ta = TierOf(a), tb = TierOf(b);
    auto nb = g_nb.find(a);
    if (nb != g_nb.end() && nb->second.count(b))
        return 0.06 + 0.02 * ((ta != Tier::Story) + (tb != Tier::Story));
    if (ta == Tier::Story || tb == Tier::Story)
        return 0;
    if (ta == Tier::Main && tb == Tier::Main)
        return 0.08;
    auto fa = g_table.find(a), fb = g_table.find(b);
    const bool same = fa != g_table.end() && fb != g_table.end() && fa->second.family == fb->second.family;
    return same ? 0.06 : 0.04;
}

static void Check()
{
    g_findings.clear();
    char buf[256];
    std::map<std::wstring, bool> spoken;
    for (const data::Line& ln : data::Lines())
        if (!ln.speaker.empty())
            spoken[ln.speaker] = true;
    const std::pair<const char*, display::Relation> reserved[] = {{"Trent", display::Relation::Player},
                                                                  {"friendly", display::Relation::Ally},
                                                                  {"neutral", display::Relation::Neutral},
                                                                  {"hostile", display::Relation::Hostile},
                                                                  {"Neural Net", display::Relation::Computer}};
    std::vector<std::wstring> all{L"Trent"};
    for (const auto& e : g_table)
        all.push_back(e.first);
    for (size_t i = 0; i < all.size(); ++i)
        for (size_t j = i + 1; j < all.size(); ++j)
        {
            const double need = Needed(all[i], all[j]), d = Dist(ColourOf(all[i]), ColourOf(all[j]));
            if (need > 0 && d < need)
            {
                sprintf_s(buf, "%.3f apart, %.2f needed: %s / %s", d, need, data::Narrow(all[i]).c_str(),
                          data::Narrow(all[j]).c_str());
                g_findings.push_back(buf);
            }
        }
    for (const auto& e : g_table)
    {
        const std::string name = data::Narrow(e.first);
        if (!spoken.count(e.first))
            g_findings.push_back("not in the subtitle data: " + name);
        if (e.second.tier != Tier::Main)
            continue;
        for (const auto& r : reserved)
            if (Dist(e.second.rgb, display::NameRgb(r.second)) < kReserved)
            {
                sprintf_s(buf, "%s is %.3f from the %s colour", name.c_str(),
                          Dist(e.second.rgb, display::NameRgb(r.second)), r.first);
                g_findings.push_back(buf);
            }
    }
    for (const auto& s : spoken)
        if (s.first != L"Trent" && !g_table.count(s.first))
            for (const data::Line& ln : data::Lines())
                if (ln.speaker == s.first && ln.kind == data::Kind::Story && !Group(ln.id).empty())
                {
                    g_findings.push_back("story speaker not in the table (relation colours): " + data::Narrow(s.first));
                    break;
                }
}

bool Load(const std::string& path)
{
    g_table.clear();
    std::ifstream in(path, std::ios::binary);
    std::string l;
    std::map<std::string, size_t> col; // column name -> index (speaker, tier, family, stance, colour, why)
    while (in && std::getline(in, l))
    {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (l.empty() || l[0] == '#')
            continue;
        const std::vector<std::string> f =
            data::SplitTsv(l); // (a trailing tab adds an empty field: get() reads it as absent)
        if (col.empty())
        {
            for (size_t i = 0; i < f.size(); ++i)
                col[f[i]] = i;
            if (!col.count("speaker") || !col.count("colour"))
            {
                Log("speakers: unexpected header in %s", path.c_str());
                return false;
            }
            continue;
        }
        auto get = [&](const char* name)
        {
            auto c = col.find(name);
            return c != col.end() && c->second < f.size() ? f[c->second] : std::string();
        };
        Entry e;
        const std::string tier = get("tier");
        e.tier = tier == "main" ? Tier::Main : tier == "side" ? Tier::Side : Tier::Story;
        e.family = get("family");
        std::istringstream stance(get("stance"));
        for (std::string w; stance >> w;)
        {
            const size_t colon = w.find(':');
            if (colon == std::string::npos)
                e.stance = w;
            else
                e.byMission[Lower(w.substr(0, colon))] = w.substr(colon + 1);
        }
        e.rgb = static_cast<u32>(strtoul(get("colour").c_str(), nullptr, 16)) & 0xFFFFFF;
        if (e.rgb && !get("speaker").empty())
            g_table[data::Widen(get("speaker"))] = e;
    }
    BuildGraph();
    Check();
    Log("speakers: %zu speakers coloured from %s, %zu check findings%s", g_table.size(), path.c_str(),
        g_findings.size(), g_findings.empty() ? "" : " (debug `colors`)");
    for (const std::string& s : g_findings)
        Log("speakers: %s", s.c_str());
    return !g_table.empty();
}

u32 Rgb(const std::wstring& speaker, bool hostileNow)
{
    auto it = g_table.find(speaker);
    if (it == g_table.end())
        return 0;
    // a station / ship / wing turned hostile against its usual stance (the Battleship Unity in M04): the hostile red;
    // characters keep their colour (the marker says it)
    if (hostileNow && it->second.tier == Tier::Story && it->second.stance != "hostile")
        return 0;
    return it->second.rgb;
}

bool Hostile(const std::wstring& speaker, const std::string& lineId)
{
    auto it = g_table.find(speaker);
    if (it == g_table.end())
        return false;
    auto m = it->second.byMission.find(Group(lineId));
    return (m != it->second.byMission.end() ? m->second : it->second.stance) == "hostile";
}

std::wstring Names()
{
    std::wstring all;
    for (const auto& e : g_table)
        all += e.first;
    return all;
}

std::string Report(const std::string& filter)
{
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(3);
    const std::string f = Lower(filter);
    for (const auto& e : g_table)
    {
        const std::string name = data::Narrow(e.first);
        if (!f.empty() && Lower(name).find(f) == std::string::npos)
            continue;
        char hex[8];
        sprintf_s(hex, "%06X", e.second.rgb);
        static const char* kTier[] = {"main", "side", "story"};
        o << name << "\t" << hex << "\t" << kTier[static_cast<int>(e.second.tier)] << "\t" << e.second.family << "\t"
          << e.second.stance;
        for (const auto& m : e.second.byMission)
            o << " " << m.first << ":" << m.second;
        // the closest coloured neighbours (characters who appear together)
        std::vector<std::pair<double, std::wstring>> near;
        for (const auto& n : g_nb[e.first])
            if (const u32 c = ColourOf(n.first))
                near.push_back({Dist(e.second.rgb, c), n.first});
        std::sort(near.begin(), near.end());
        for (size_t i = 0; i < near.size() && i < 3; ++i)
            o << "\t" << data::Narrow(near[i].second) << " " << near[i].first;
        o << "\n";
    }
    if (f.empty())
    {
        o << "check: " << (g_findings.empty() ? "clean" : std::to_string(g_findings.size()) + " findings") << "\n";
        for (const std::string& s : g_findings)
            o << "  " << s << "\n";
    }
    return o.str();
}
} // namespace speakers
