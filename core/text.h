// Freelancer's string resources (one instance for the logic, one for the host).
#pragma once
#include "common.h"

#include <string>

namespace text
{
// The text of a string id: dll index = ids >> 16 (0 = resources.dll, then the DLLs of freelancer.ini [Resources] in
// order), LoadStringW(ids & 0xFFFF). Empty for 0 or an unknown id.
std::wstring Ids(u32 ids);
// Ids() as UTF-8.
std::string IdsUtf8(u32 ids);
// An infocard (an HTML resource of the same numbering, the game's RDL markup) as plain text: paragraphs on newlines, "" unknown. Plain strings answer as themselves.
std::wstring InfoCard(u32 ids);
// The RDL markup's text: what <TEXT> holds, <PARA/> a newline, the entities decoded. Pure.
inline std::wstring InfoCardText(const std::wstring& xml)
{
    std::wstring out;
    size_t i = 0;
    while (i < xml.size())
    {
        if (xml[i] == L'<')
        {
            const size_t e = xml.find(L'>', i);
            if (e == std::wstring::npos)
                break;
            const std::wstring tag = xml.substr(i + 1, e - i - 1);
            if (tag.compare(0, 4, L"PARA") == 0 && !out.empty())
                out += L'\n';
            i = e + 1;
            continue;
        }
        if (xml[i] == L'&')
        {
            static const struct
            {
                const wchar_t* name;
                wchar_t c;
            } kEntities[] = {{L"&amp;", L'&'}, {L"&lt;", L'<'}, {L"&gt;", L'>'}, {L"&quot;", L'"'}, {L"&apos;", L'\''}};
            bool done = false;
            for (const auto& k : kEntities)
            {
                const size_t len = wcslen(k.name);
                if (xml.compare(i, len, k.name) == 0)
                {
                    out += k.c, i += len, done = true;
                    break;
                }
            }
            if (done)
                continue;
        }
        if (xml[i] != L'\r' && xml[i] != L'\n')
            out += xml[i];
        ++i;
    }
    while (!out.empty() && (out.back() == L'\n' || out.back() == L' '))
        out.pop_back();
    return out;
}
// UTF-16 -> UTF-8.
std::string Utf8(const std::wstring& w);
} // namespace text
