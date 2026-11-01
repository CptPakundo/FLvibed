// The jobs' objective texts (jobs.h).
#include "jobs.h"
#include "str.h"

#include <algorithm>

namespace jobs
{
Table FromSections(const std::vector<ini::Section>& sections)
{
    Table t;
    for (const ini::Section& sec : sections)
    {
        int node = -1;
        if (const std::string* n = sec.Get("node_id"))
            node = atoi(n->c_str());
        for (const auto& kv : sec.kv)
        {
            if (kv.first != "objective_text")
                continue;
            const std::vector<std::string> f = ini::Fields(kv.second);
            if (f.size() < 2)
                continue;
            ObjectiveText o;
            o.kind = str::Lower(f[0]);
            o.textId = static_cast<u32>(strtoul(f[1].c_str(), nullptr, 10));
            if (o.kind.empty() || !o.textId)
                continue;
            o.args.assign(f.begin() + 2, f.end());
            o.node = node;
            t.rows.push_back(o);
        }
    }
    t.ok = !t.rows.empty();
    return t;
}

const Table& Install()
{
    static Table t;
    static bool read = false;
    if (!read)
    {
        read = true;
        t = FromSections(ini::Read(ini::DataPath("RANDOMMISSIONS\\vignetteparams.ini")));
    }
    return t;
}

std::string KindOf(const Table& t, u32 textId, int* rows, std::vector<std::string>* kinds)
{
    std::vector<std::string> k;
    int n = 0;
    for (const ObjectiveText& o : t.rows)
        if (o.textId == textId && textId)
        {
            ++n;
            if (std::find(k.begin(), k.end(), o.kind) == k.end())
                k.push_back(o.kind);
        }
    if (rows)
        *rows = n;
    if (kinds)
        *kinds = k;
    return k.empty() ? std::string() : k.size() > 1 ? std::string("ambiguous") : k[0];
}

std::vector<std::string> Kinds(const Table& t)
{
    std::vector<std::string> k;
    for (const ObjectiveText& o : t.rows)
        if (std::find(k.begin(), k.end(), o.kind) == k.end())
            k.push_back(o.kind);
    return k;
}
} // namespace jobs
