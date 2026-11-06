// FLvibed - the settings bus (core\settings_bus.h): the one instance in FLvibed.dll, compiled plain beside
// core\flvibed.cpp (build.cmd :link). The module hosts hand it to their logic through `GetSettingsBus` (controller
// HostApi v18, subtitles HostApi v6). Everything runs on the game's thread (Register at the logic's Init, Set from the
// options page inside the frame), so there is no locking; every entry is defensive instead: null checks, bounds, a
// fixed table of 16 sections, string buffers bounded. A section's defaults are snapshotted at Register when the owner
// passes none (the registry's values at that moment: the struct initialisers, since the logics register before their
// ini loads), so Default / Reset need no second copy of any value.
#include "pre.h"

#include "settings_bus.h"

namespace
{
constexpr int kMaxSections = 16;
constexpr int kDefaultLen = 96;  // one default as text (Vec3 "x, y, z", a key name, a pilot nickname)

struct Slot
{
    int id = 0;                 // 0 = free
    SettingsBusSection section; // the owner's record, copied (its pointers stay the owner's)
    std::vector<std::string> defaults; // snapshotted when the owner passed none
};

Slot g_slots[kMaxSections];
int g_nextId = 1;
u32 g_generation = 0;

bool InSection(const Slot& s, const char* section)
{
    return section && s.id && s.section.section && _stricmp(s.section.section, section) == 0;
}

const settings::Def* FindDef(const Slot& s, const char* key, size_t* index)
{
    if (!key || !s.section.defs)
        return nullptr;
    for (size_t i = 0; i < s.section.count; ++i)
        if (s.section.defs[i].name && _stricmp(s.section.defs[i].name, key) == 0)
        {
            if (index)
                *index = i;
            return &s.section.defs[i];
        }
    return nullptr;
}

// The row and its owner: a section may have several owners (`[Controller]` holds HUD, speedrun and window rows), a key has one.
Slot* FindOwner(const char* section, const char* key, const settings::Def** def, size_t* index = nullptr)
{
    for (Slot& s : g_slots)
        if (InSection(s, section))
            if (const settings::Def* d = FindDef(s, key, index))
            {
                if (def)
                    *def = d;
                return &s;
            }
    return nullptr;
}

// An owner's second registration (a logic that did not unregister) or a key another owner has: fail closed, the old one stays.
bool Clashes(const SettingsBusSection& section)
{
    for (const Slot& s : g_slots)
        if (InSection(s, section.section))
        {
            if (section.module && s.section.module && _stricmp(section.module, s.section.module) == 0)
                return true;
            for (size_t i = 0; i < section.count; ++i)
                if (FindDef(s, section.defs[i].name, nullptr))
                    return true;
        }
    return false;
}

int BusRegister(const SettingsBusSection* section)
{
    if (!section || !section->section || !*section->section || !section->defs || section->count == 0 ||
        section->count > kSettingsBusMaxRows)
        return 0;
    if (Clashes(*section))
        return 0;
    for (Slot& s : g_slots)
    {
        if (s.id)
            continue;
        s.section = *section;
        s.defaults.clear();
        if (!section->defaults)
        {
            s.defaults.reserve(section->count);
            for (size_t i = 0; i < section->count; ++i)
                s.defaults.push_back(settings::Value(section->defs[i]));
        }
        s.id = g_nextId++;
        ++g_generation;
        return s.id;
    }
    return 0; // full
}

void BusUnregister(int id)
{
    if (id <= 0)
        return;
    for (Slot& s : g_slots)
        if (s.id == id)
        {
            s.id = 0;
            s.section = SettingsBusSection();
            s.defaults.clear();
            ++g_generation;
        }
}

int BusCount()
{
    int n = 0;
    for (const Slot& s : g_slots)
        n += s.id ? 1 : 0;
    return n;
}

const SettingsBusSection* BusSection(int i)
{
    if (i < 0)
        return nullptr;
    int n = 0;
    for (const Slot& s : g_slots)
        if (s.id && n++ == i)
            return &s.section;
    return nullptr;
}

const settings::Def* BusFind(const char* section, const char* key)
{
    const settings::Def* d = nullptr;
    return FindOwner(section, key, &d) ? d : nullptr;
}

bool CopyOut(const std::string& v, char* out, int outLen)
{
    if (!out || outLen <= 0)
        return false;
    strncpy_s(out, static_cast<size_t>(outLen), v.c_str(), _TRUNCATE);
    return true;
}

bool DefaultOf(const Slot& s, size_t index, std::string& out)
{
    if (s.section.defaults)
    {
        const char* d = index < s.section.count ? s.section.defaults[index] : nullptr;
        out = d ? d : "";
        return d != nullptr;
    }
    if (index >= s.defaults.size())
        return false;
    out = s.defaults[index];
    return true;
}

bool BusDefault(const char* section, const char* key, char* out, int outLen)
{
    if (out && outLen > 0)
        out[0] = 0;
    size_t index = 0;
    const Slot* s = FindOwner(section, key, nullptr, &index);
    if (!s)
        return false;
    std::string d;
    if (!DefaultOf(*s, index, d))
        return false;
    return CopyOut(d, out, outLen);
}

bool BusSet(const char* section, const char* key, const char* value)
{
    const settings::Def* d = nullptr;
    Slot* s = FindOwner(section, key, &d);
    if (!s || !value || !d->ptr)
        return false;
    if (!settings::Apply(*d, value))
        return false;
    ++g_generation;
    if (s->section.applied)
        s->section.applied(s->section.ctx, d);
    return true;
}

bool BusGet(const char* section, const char* key, char* out, int outLen)
{
    if (out && outLen > 0)
        out[0] = 0;
    const settings::Def* d = BusFind(section, key);
    if (!d || !d->ptr)
        return false;
    return CopyOut(settings::Value(*d), out, outLen);
}

bool BusReset(const char* section, const char* key)
{
    char d[kDefaultLen];
    if (!BusDefault(section, key, d, sizeof(d)))
        return false;
    return BusSet(section, key, d);
}

bool BusSave(const char* section)
{
    bool saved = false; // each owner writes its own rows
    for (const Slot& s : g_slots)
        if (InSection(s, section) && s.section.save)
        {
            s.section.save(s.section.ctx);
            saved = true;
        }
    return saved;
}

u32 BusGeneration() { return g_generation; }

const SettingsBus g_bus = {1,        BusRegister, BusUnregister, BusCount, BusSection, BusFind,
                           BusDefault, BusSet,    BusGet,        BusReset, BusSave,    BusGeneration};
} // namespace

// The hosts (in their module namespaces) reach the bus through this plain C symbol.
extern "C" const SettingsBus* flv_settings_bus() { return &g_bus; }
