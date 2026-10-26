#include "gamefont.h"

#include "log.h"

#include <string>
#include <vector>

namespace gamefont
{
namespace
{
int g_state = -1; // -1 not tried, 0 label face fell back, 1 resolved
std::wstring g_label = L"Segoe UI", g_text = L"Segoe UI";
int g_labelSrc = 0, g_textSrc = 0;

struct Entry
{
    std::wstring section, key, value; // section and key lower case, value trimmed
};

std::wstring Lower(std::wstring s)
{
    for (wchar_t& c : s)
        if (c >= L'A' && c <= L'Z')
            c = static_cast<wchar_t>(c - L'A' + L'a');
    return s;
}

std::wstring Trim(const std::wstring& s)
{
    const size_t a = s.find_first_not_of(L" \t\r\n"), b = s.find_last_not_of(L" \t\r\n");
    return a == std::wstring::npos ? std::wstring() : s.substr(a, b - a + 1);
}

std::wstring Widen(const char* s, size_t n)
{
    std::wstring w;
    for (size_t i = 0; i < n && s[i]; ++i)
        w += static_cast<wchar_t>(static_cast<unsigned char>(s[i])); // the game's ini strings are ASCII
    return w;
}

// A binary ini (BINI, the vanilla game's DATA files): "BINI", version, the string table's offset; then sections
// {u16 name, u16 entries}, entries {u16 name, u8 values}, values {u8 type (1 int, 2 float, 3 string), 4 bytes}; names
// and string values are offsets into the string table. Only string values are kept (all fonts.ini needs).
bool ReadBini(const std::vector<char>& b, std::vector<Entry>& out)
{
    auto u16at = [&](size_t at) { return static_cast<unsigned>(static_cast<unsigned char>(b[at]) | (static_cast<unsigned char>(b[at + 1]) << 8)); };
    auto u32at = [&](size_t at) { return static_cast<size_t>(u16at(at) | (u16at(at + 2) << 16)); };
    if (b.size() < 12 || memcmp(b.data(), "BINI", 4) != 0)
        return false;
    const size_t strOff = u32at(8);
    if (strOff > b.size())
        return false;
    auto str = [&](size_t off) -> std::wstring
    {
        const size_t at = strOff + off;
        return at < b.size() ? Widen(&b[at], b.size() - at) : std::wstring();
    };
    size_t p = 12;
    while (p + 4 <= strOff)
    {
        const std::wstring sec = Lower(str(u16at(p)));
        const unsigned entries = u16at(p + 2);
        p += 4;
        for (unsigned e = 0; e < entries && p + 3 <= strOff; ++e)
        {
            const std::wstring key = Lower(str(u16at(p)));
            const unsigned vals = static_cast<unsigned char>(b[p + 2]);
            p += 3;
            for (unsigned v = 0; v < vals && p + 5 <= strOff; ++v, p += 5)
                if (b[p] == 3 && v == 0)
                    out.push_back({sec, key, Trim(str(u32at(p + 1)))});
        }
    }
    return true;
}

// A text ini: [section], key = value, ';' comments (a commented-out ";path = ..." is skipped).
void ReadText(const std::vector<char>& b, std::vector<Entry>& out)
{
    std::wstring sec;
    size_t i = 0;
    while (i < b.size())
    {
        size_t j = i;
        while (j < b.size() && b[j] != '\n')
            ++j;
        std::wstring line = Trim(Widen(&b[i], j - i));
        i = j + 1;
        const size_t sc = line.find(L';');
        if (sc != std::wstring::npos)
            line = Trim(line.substr(0, sc));
        if (line.empty())
            continue;
        if (line[0] == L'[')
        {
            const size_t e = line.find(L']');
            sec = Lower(Trim(line.substr(1, e == std::wstring::npos ? std::wstring::npos : e - 1)));
            continue;
        }
        const size_t eq = line.find(L'=');
        if (eq != std::wstring::npos)
            out.push_back({sec, Lower(Trim(line.substr(0, eq))), Trim(line.substr(eq + 1))});
    }
}

bool ReadAll(const std::wstring& path, std::vector<char>& b)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    const DWORD size = GetFileSize(h, nullptr);
    bool ok = size != INVALID_FILE_SIZE && size < (1u << 20);
    if (ok)
    {
        b.resize(size);
        DWORD got = 0;
        ok = !size || (::ReadFile(h, b.data(), size, &got, nullptr) && got == size);
    }
    CloseHandle(h);
    return ok;
}

int CALLBACK OnFamily(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM found)
{
    *reinterpret_cast<bool*>(found) = true;
    return 0;
}

// The face is available to this process (installed in Windows, or a private font this process loaded).
bool FaceAvailable(const std::wstring& face)
{
    if (face.empty() || face.size() >= LF_FACESIZE)
        return false;
    HDC dc = GetDC(nullptr);
    if (!dc)
        return false;
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, face.c_str());
    bool found = false;
    EnumFontFamiliesExW(dc, &lf, OnFamily, reinterpret_cast<LPARAM>(&found), 0);
    ReleaseDC(nullptr, dc);
    return found;
}

const char* SourceName(int s) { return s == 1 ? "the game's DATA\\FONTS files" : s == 2 ? "Windows' installed fonts" : "not found: the fallback"; }
} // namespace

bool Load()
{
    if (g_state >= 0)
        return g_state == 1;
    g_state = 0;
    wchar_t exe[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, exe, MAX_PATH))
        return false;
    std::wstring dir = exe;
    dir = dir.substr(0, dir.find_last_of(L"\\/")); // ...\EXE
    dir = dir.substr(0, dir.find_last_of(L"\\/")) + L"\\DATA\\";
    std::vector<char> raw;
    std::vector<Entry> ini;
    const char* format = "missing";
    if (ReadAll(dir + L"FONTS\\fonts.ini", raw))
    {
        if (ReadBini(raw, ini))
            format = "BINI";
        else
            ReadText(raw, ini), format = "text";
    }
    // the faces' names: [TrueType] nickname -> font (HudSmall = the HUD's labels, ListText = the lists' body text)
    std::wstring labelName = L"Agency FB", textName = L"Arial Unicode MS", nick;
    for (const Entry& e : ini)
        if (e.section == L"truetype")
        {
            if (e.key == L"nickname")
                nick = Lower(e.value);
            else if (e.key == L"font" && !e.value.empty())
            {
                if (nick == L"hudsmall")
                    labelName = e.value;
                else if (nick == L"listtext")
                    textName = e.value;
            }
        }
    const bool labelBefore = FaceAvailable(labelName), textBefore = FaceAvailable(textName);
    int files = 0, failed = 0;
    for (const Entry& e : ini)
        if (e.section == L"fontfiles" && e.key == L"path" && !e.value.empty())
        {
            std::wstring file = dir + e.value;
            for (wchar_t& c : file)
                if (c == L'/')
                    c = L'\\';
            if (AddFontResourceExW(file.c_str(), FR_PRIVATE, nullptr) > 0)
                ++files;
            else
                ++failed;
        }
    // a face counts as the game's files' when it appeared with them, or when Windows has it too and the game's files
    // loaded (the private copy is what the game itself loaded); Windows' when only installed there; else the fallback
    auto source = [&](const std::wstring& face, bool before)
    {
        if (!before)
            return files > 0 && FaceAvailable(face) ? 1 : 0;
        return files > 0 ? 1 : 2;
    };
    g_labelSrc = source(labelName, labelBefore);
    g_textSrc = source(textName, textBefore);
    g_label = g_labelSrc ? labelName : L"Segoe UI";
    g_text = g_textSrc ? textName : L"Segoe UI";
    Log("gamefont: fonts.ini %s, %d game font files loaded (%d failed) from %lsFONTS; label face \"%ls\" from %s, body "
        "face \"%ls\" from %s",
        format, files, failed, dir.c_str(), labelName.c_str(), SourceName(g_labelSrc), textName.c_str(),
        SourceName(g_textSrc));
    g_state = g_labelSrc ? 1 : 0;
    return g_state == 1;
}

const wchar_t* LabelFace() { return Load(), g_label.c_str(); }
int LabelWeight() { return Load() ? 700 : 600; }
const wchar_t* TextFace() { return Load(), g_text.c_str(); }
int TextWeight() { return 400; }
int LabelSource() { return Load(), g_labelSrc; }
int TextSource() { return Load(), g_textSrc; }
} // namespace gamefont
