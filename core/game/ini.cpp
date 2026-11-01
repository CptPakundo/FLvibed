#include "ini.h"

#include "str.h"

#include <fstream>

namespace ini
{
using str::Lower;
using str::StripComment;
using str::Trim;

const std::string* Section::Get(const char* key) const
{
    for (const auto& p : kv)
        if (p.first == key)
            return &p.second;
    return nullptr;
}

std::vector<std::string> Section::All(const char* key) const
{
    std::vector<std::string> out;
    for (const auto& p : kv)
        if (p.first == key)
            out.push_back(p.second);
    return out;
}

std::string DataPath(const std::string& relative)
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH); // EXE\Freelancer.exe -> DATA\<relative>
    std::string p = path;
    const size_t slash = p.find_last_of('\\');
    p = (slash == std::string::npos ? std::string() : p.substr(0, slash + 1)) + "..\\DATA\\" + relative;
    return p;
}

std::vector<std::string> DataFiles(const char* key)
{
    std::vector<std::string> out;
    for (const auto& s : Read(DataPath("..\\EXE\\freelancer.ini")))
        if (s.type == "data")
            for (const auto& v : s.All(key))
                out.push_back(DataPath(v));
    return out;
}

// A binary ini (BINI: the vanilla 1.1 game's DATA files; HD Edition ships them as text): "BINI", u32 version, u32 the
// string table's offset; then sections {u16 name, u16 entries}, entries {u16 name, u8 values}, values {u8 type (1
// int, 2 float, 3 string), 4 bytes}; names and string values are offsets into the NUL-terminated string table. Decoded
// into the same sections a text file gives (values joined by ", ", floats in their shortest exact form), so every
// reader works on vanilla data too (29 Oct 2026; until then a BINI file read as empty - code audit; the decoder
// generalised from core\game\gamefont.cpp's).
static std::vector<Section> ReadBini(const std::vector<char>& b)
{
    std::vector<Section> out;
    auto u8at = [&](size_t at) { return static_cast<unsigned>(static_cast<unsigned char>(b[at])); };
    auto u16at = [&](size_t at) { return u8at(at) | (u8at(at + 1) << 8); };
    auto u32at = [&](size_t at) { return static_cast<u32>(u16at(at) | (u16at(at + 2) << 16)); };
    if (b.size() < 12)
        return out;
    const size_t strOff = u32at(8);
    if (strOff > b.size())
        return out;
    auto str = [&](size_t off) {
        std::string s;
        for (size_t at = strOff + off; at < b.size() && b[at]; ++at)
            s += b[at];
        return s;
    };
    size_t p = 12;
    while (p + 4 <= strOff)
    {
        out.push_back({Lower(str(u16at(p))), {}});
        const unsigned entries = u16at(p + 2);
        p += 4;
        for (unsigned e = 0; e < entries && p + 3 <= strOff; ++e)
        {
            const std::string key = Lower(str(u16at(p)));
            const unsigned vals = u8at(p + 2);
            p += 3;
            std::string value;
            for (unsigned v = 0; v < vals && p + 5 <= strOff; ++v, p += 5)
            {
                char buf[32] = {};
                const u32 raw = u32at(p + 1);
                if (u8at(p) == 1)
                    sprintf_s(buf, "%d", static_cast<int>(raw));
                else if (u8at(p) == 2)
                {
                    float f;
                    memcpy(&f, &raw, 4);
                    sprintf_s(buf, "%.9g", f);
                }
                value += (v ? ", " : "") + (u8at(p) == 3 ? Trim(str(raw)) : std::string(buf));
            }
            out.back().kv.emplace_back(key, value);
        }
    }
    return out;
}

std::vector<Section> Read(const std::string& fullPath)
{
    std::vector<Section> out;
    std::ifstream in(fullPath, std::ios::binary);
    char magic[4] = {};
    if (in.read(magic, 4) && memcmp(magic, "BINI", 4) == 0)
    {
        in.seekg(0, std::ios::end);
        std::vector<char> b(static_cast<size_t>(in.tellg()));
        in.seekg(0);
        in.read(b.data(), static_cast<std::streamsize>(b.size()));
        return ReadBini(b);
    }
    in.clear();
    in.seekg(0);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r') // (binary mode: the line end's CR stays; Trim drops it too)
            line.pop_back();
        line = Trim(StripComment(line));
        if (line.empty())
            continue;
        if (line[0] == '[')
        {
            const size_t close = line.find(']');
            out.push_back({Lower(line.substr(1, close == std::string::npos ? std::string::npos : close - 1)), {}});
            continue;
        }
        const size_t eq = line.find('=');
        if (out.empty() || eq == std::string::npos)
            continue;
        out.back().kv.emplace_back(Lower(Trim(line.substr(0, eq))), Trim(line.substr(eq + 1)));
    }
    return out;
}

std::vector<std::string> Fields(const std::string& value)
{
    std::vector<std::string> out;
    size_t s = 0;
    for (;;)
    {
        const size_t c = value.find(',', s);
        out.push_back(Trim(value.substr(s, c == std::string::npos ? std::string::npos : c - s)));
        if (c == std::string::npos)
            break;
        s = c + 1;
    }
    return out;
}
} // namespace ini
