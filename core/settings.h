// FLvibed - one registry type for every module's tunables. A setting is declared ONCE (name, type, where it lives,
// its range, one line of help) and everything else comes from here: parsing with clamping (`set` / `ap set`), printing,
// the ini load / save, the listing for `config` / `ap settings`, and later the settings UI and the generated docs.
// Before 24 Oct 2026 the controller had this table (kOptions) while the autopilot and the subtitles each spelt their key
// list out three to four times (apply, save, describe, docs) - and the autopilot's save had already lost three keys.
// Header-only: compiled into each module's namespace like the rest of core (build.cmd wraps every source).
#pragma once
#include "common.h"

namespace settings
{
enum Type : char
{
    Bool = 'b',   // "0" / "1" (any non-zero number = 1)
    Int = 'i',    // integer (truncated from the number given; clamped to [min, max] unless both are 0)
    Float = 'f',  // float (clamped likewise)
    Hex = 'u',    // u32: decimal or 0x hex (object ids)
    String = 's', // std::string (no clamping)
    Vec3 = '3',   // float[3] as "x, y, z"
};

constexpr float kNoMax = 1e30f; // max for a setting with only a lower bound (min alone would read as "unclamped")

struct Def
{
    const char* name; // the ini key and the command name (snake_case)
    Type type;
    void* ptr;        // the live value (a field of the module's settings struct)
    float min, max;   // Int / Float: the range; 0, 0 = unclamped
    const char* help; // one line: what it does, its values, the default's reason (the ini file, the docs, the UI)
};

inline const Def* Find(const Def* defs, size_t n, const std::string& name)
{
    for (size_t i = 0; i < n; ++i)
        if (name == defs[i].name)
            return &defs[i];
    return nullptr;
}

inline float ClampDef(const Def& d, float v) { return (d.min == 0 && d.max == 0) ? v : Clampf(v, d.min, d.max); }

// Applies one value; false when it does not parse (a Vec3 with fewer than three numbers). Numbers parse as atof does
// ("1", "0.5", "-1"); an Int truncates ("1.9" -> 1) as the hand-written parsers did (atoi).
inline bool Apply(const Def& d, const std::string& v)
{
    switch (d.type)
    {
    case Bool: *static_cast<bool*>(d.ptr) = atof(v.c_str()) != 0; return true;
    case Int:
        *static_cast<int*>(d.ptr) = static_cast<int>(ClampDef(d, static_cast<float>(atof(v.c_str()))));
        return true;
    case Float: *static_cast<float*>(d.ptr) = ClampDef(d, static_cast<float>(atof(v.c_str()))); return true;
    case Hex: *static_cast<u32*>(d.ptr) = static_cast<u32>(strtoul(v.c_str(), nullptr, 0)); return true;
    case String: *static_cast<std::string*>(d.ptr) = v; return true;
    case Vec3:
    {
        float* f = static_cast<float*>(d.ptr);
        float t[3];
        if (sscanf_s(v.c_str(), "%f , %f , %f", &t[0], &t[1], &t[2]) != 3)
            return false;
        f[0] = t[0], f[1] = t[1], f[2] = t[2];
        return true;
    }
    }
    return false;
}

inline std::string Value(const Def& d)
{
    char buf[96];
    switch (d.type)
    {
    case Bool: return *static_cast<bool*>(d.ptr) ? "1" : "0";
    case Int: return std::to_string(*static_cast<int*>(d.ptr));
    case Float: sprintf_s(buf, "%g", *static_cast<float*>(d.ptr)); return buf;
    case Hex: sprintf_s(buf, "%u", *static_cast<u32*>(d.ptr)); return buf;
    case String: return *static_cast<std::string*>(d.ptr);
    case Vec3:
    {
        const float* f = static_cast<const float*>(d.ptr);
        sprintf_s(buf, "%g, %g, %g", f[0], f[1], f[2]);
        return buf;
    }
    }
    return "";
}

// "name<eq>value" per setting, `sep` between them (the `config` listings: "name value" lines by default).
inline std::string Describe(const Def* defs, size_t n, const char* eq = " ", const char* sep = "\n")
{
    std::string s;
    for (size_t i = 0; i < n; ++i)
    {
        if (i)
            s += sep;
        s += defs[i].name;
        s += eq;
        s += Value(defs[i]);
    }
    return s;
}

// The reference listing: "name = value  ; help" per setting (the docs' settings section is generated from it).
inline std::string Reference(const Def* defs, size_t n)
{
    std::string s;
    for (size_t i = 0; i < n; ++i)
    {
        s += defs[i].name;
        s += " = ";
        s += Value(defs[i]);
        if (defs[i].help && *defs[i].help)
            s += std::string("  ; ") + defs[i].help;
        s += '\n';
    }
    return s;
}

// Reads the keys present in [section] of the ini (absent keys keep their value).
inline void Load(const char* section, const std::string& ini, const Def* defs, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        char buf[256] = {};
        if (GetPrivateProfileStringA(section, defs[i].name, "", buf, sizeof(buf), ini.c_str()) > 0)
            Apply(defs[i], buf);
    }
}

// Writes every setting into [section] of the ini (values only; other keys of the section stay).
inline void Save(const char* section, const std::string& ini, const Def* defs, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        WritePrivateProfileStringA(section, defs[i].name, Value(defs[i]).c_str(), ini.c_str());
}

// Writes [section] with a comment line per setting into an open file (the self-documenting ini written on first run
// and by `config save`).
inline void WriteCommented(FILE* f, const char* section, const Def* defs, size_t n)
{
    fprintf(f, "[%s]\n", section);
    for (size_t i = 0; i < n; ++i)
        fprintf(f, "; %s\n%s = %s\n\n", defs[i].help ? defs[i].help : "", defs[i].name, Value(defs[i]).c_str());
}
} // namespace settings
