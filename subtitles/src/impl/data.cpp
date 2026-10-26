#include "data.h"

#include "log.h"

#include <fstream>
#include <unordered_map>
#include <vector>

namespace data
{
static std::vector<Line> g_lines;
static std::unordered_map<unsigned long long, size_t> g_byVoiceMsg;
static std::unordered_map<u32, size_t> g_byMsg;
static std::unordered_map<u32, std::wstring> g_voiceNames;

u32 CreateID(const char* s)
{
    static u32 table[256];
    static bool init = false;
    if (!init)
    {
        for (u32 i = 0; i < 256; ++i)
        {
            u32 x = i;
            for (int b = 0; b < 8; ++b)
                x = (x & 1) ? (x >> 1) ^ (0xA001u << 14) : x >> 1;
            table[i] = x;
        }
        init = true;
    }
    u32 h = 0;
    for (; *s; ++s)
    {
        const u8 c = static_cast<u8>(tolower(static_cast<u8>(*s)));
        h = (h >> 8) ^ table[static_cast<u8>(h) ^ c];
    }
    h = (h >> 24) | ((h >> 8) & 0xFF00) | ((h << 8) & 0xFF0000) | (h << 24);
    return (h >> 2) | 0x80000000;
}

std::wstring Widen(const std::string& s)
{
    if (s.empty())
        return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string Narrow(const std::wstring& w)
{
    if (w.empty())
        return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

std::vector<std::string> SplitTsv(const std::string& l)
{
    std::vector<std::string> out;
    size_t a = 0;
    for (;;)
    {
        const size_t b = l.find('\t', a);
        out.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos)
            return out;
        a = b + 1;
    }
}

static Kind ParseKind(const std::string& k)
{
    if (k == "story")
        return Kind::Story;
    if (k == "chunk")
        return Kind::Chunk;
    if (k == "nn")
        return Kind::Nn;
    return Kind::Unique;
}

bool Load(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        Log("data: cannot open %s", path.c_str());
        return false;
    }
    g_lines.clear();
    g_byVoiceMsg.clear();
    g_byMsg.clear();
    g_voiceNames.clear();
    std::vector<std::string> head;
    int col[7] = {-1, -1, -1, -1, -1, -1, -1};
    const char* names[7] = {"id", "voice", "kind", "source", "sec", "speaker", "text"};
    std::string l;
    while (std::getline(in, l))
    {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (l.empty() || l[0] == '#')
            continue;
        std::vector<std::string> f = SplitTsv(l);
        if (head.empty())
        {
            head = f;
            for (int c = 0; c < 7; ++c)
                for (size_t i = 0; i < head.size(); ++i)
                    if (head[i] == names[c])
                        col[c] = static_cast<int>(i);
            if (col[0] < 0 || col[1] < 0 || col[6] < 0)
            {
                Log("data: unexpected header in %s", path.c_str());
                return false;
            }
            continue;
        }
        auto get = [&](int c)
        { return col[c] >= 0 && col[c] < static_cast<int>(f.size()) ? f[col[c]] : std::string(); };
        Line ln;
        ln.id = get(0);
        ln.voice = get(1);
        ln.kind = ParseKind(get(2));
        ln.sec = static_cast<float>(atof(get(4).c_str()));
        ln.speaker = Widen(get(5));
        const std::string text = get(6);
        ln.text = Widen(text);
        u32 h = 0x811C9DC5;
        for (const char ch : text)
            h = (h ^ static_cast<u8>(ch)) * 0x01000193;
        ln.textFnv = h;
        const u32 msg = CreateID(ln.id.c_str()), voice = CreateID(ln.voice.c_str());
        g_byVoiceMsg[(static_cast<unsigned long long>(voice) << 32) | msg] = g_lines.size();
        g_byMsg.emplace(msg, g_lines.size());
        if (!ln.speaker.empty() && ln.kind != Kind::Chunk)
            g_voiceNames.emplace(voice, ln.speaker);
        g_lines.push_back(std::move(ln));
    }
    Log("data: %zu lines from %s", g_lines.size(), path.c_str());
    return true;
}

void LoadWordTimes(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        Log("data: no word timings (%s)", path.c_str());
        return;
    }
    size_t timed = 0, stale = 0;
    std::string l;
    bool head = false;
    while (std::getline(in, l))
    {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (l.empty() || l[0] == '#')
            continue;
        if (!head) // id voice text_fnv audio_sha1 end_ms starts_ms
        {
            head = true;
            continue;
        }
        const std::vector<std::string> f = SplitTsv(l);
        if (f.size() < 6)
            continue;
        const u32 msg = CreateID(f[0].c_str()), voice = CreateID(f[1].c_str());
        const auto it = g_byVoiceMsg.find((static_cast<unsigned long long>(voice) << 32) | msg);
        if (it == g_byVoiceMsg.end())
            continue;
        Line& ln = g_lines[it->second];
        std::vector<float> starts;
        for (size_t a = 0; a < f[5].size();)
        {
            size_t b = f[5].find(',', a);
            if (b == std::string::npos)
                b = f[5].size();
            starts.push_back(static_cast<float>(atoi(f[5].substr(a, b - a).c_str())) / 1000.0f);
            a = b + 1;
        }
        size_t words = 0;
        for (size_t i = 0; i < ln.text.size(); ++i)
            words += !iswspace(ln.text[i]) && (i == 0 || iswspace(ln.text[i - 1])) ? 1 : 0;
        if (strtoul(f[2].c_str(), nullptr, 16) != ln.textFnv || starts.size() != words)
        {
            ++stale; // the text changed after its alignment: the estimate
            continue;
        }
        ln.wordStarts = std::move(starts);
        ln.speechEnd = static_cast<float>(atoi(f[4].c_str())) / 1000.0f;
        ++timed;
    }
    Log("data: word timings for %zu lines (%zu stale, ignored) from %s", timed, stale, path.c_str());
}

size_t Count() { return g_lines.size(); }

const std::vector<Line>& Lines() { return g_lines; }

const Line* Find(u32 voice, u32 msg)
{
    auto it = g_byVoiceMsg.find((static_cast<unsigned long long>(voice) << 32) | msg);
    if (it != g_byVoiceMsg.end())
        return &g_lines[it->second];
    auto m = g_byMsg.find(msg);
    return m != g_byMsg.end() ? &g_lines[m->second] : nullptr;
}

const std::wstring& VoiceName(u32 voice)
{
    static const std::wstring empty;
    auto it = g_voiceNames.find(voice);
    return it != g_voiceNames.end() ? it->second : empty;
}
} // namespace data
