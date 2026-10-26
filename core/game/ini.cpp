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

std::vector<Section> Read(const std::string& fullPath)
{
    std::vector<Section> out;
    std::ifstream in(fullPath);
    std::string line;
    while (std::getline(in, line))
    {
        if (line.compare(0, 4, "BINI") == 0) // a binary ini (the game's compiled form): nothing to read as text
            return {};
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
