#include "text.h"

#include <vector>

namespace text
{
std::wstring Ids(u32 ids)
{
    // Resource DLL index = ids >> 16: 0 = resources.dll, then freelancer.ini [Resources] order.
    static std::vector<HMODULE> dlls;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        char exe[MAX_PATH];
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        std::string dir = exe;
        dir = dir.substr(0, dir.find_last_of("\\/") + 1);
        std::vector<std::string> files = {"resources.dll"};
        char ini[4096];
        GetPrivateProfileSectionA("Resources", ini, sizeof(ini), (dir + "freelancer.ini").c_str());
        for (const char* p = ini; *p; p += strlen(p) + 1)
        {
            std::string kv = p;
            const size_t eq = kv.find('=');
            if (eq == std::string::npos || _stricmp(kv.substr(0, kv.find_first_of(" \t=")).c_str(), "DLL") != 0)
                continue;
            std::string v = kv.substr(eq + 1);
            v = v.substr(0, v.find(';'));
            v.erase(0, v.find_first_not_of(" \t"));
            v.erase(v.find_last_not_of(" \t") + 1);
            files.push_back(v);
        }
        for (const std::string& n : files)
            dlls.push_back(LoadLibraryExA((dir + n).c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE));
    }
    const u32 index = ids >> 16;
    if (!ids || index >= dlls.size() || !dlls[index])
        return L"";
    wchar_t w[1024];
    const int n = LoadStringW(dlls[index], ids & 0xFFFF, w, 1024);
    return n > 0 ? std::wstring(w, n) : L"";
}

std::string Utf8(const std::wstring& w)
{
    if (w.empty())
        return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

std::string IdsUtf8(u32 ids) { return Utf8(Ids(ids)); }
} // namespace text
