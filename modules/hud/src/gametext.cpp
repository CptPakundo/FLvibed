// The game's own text, larger - see gametext.h, core\game\fontscale.h and docs\FREELANCER_RE.md "The game's text size is its
// font's". Fails closed: a file not found or a table that does not match it changes nothing and says so in the log.
#include "gametext.h"

#include "flexe.h"
#include "game/fontscale.h"
#include "hook.h"
#include "log.h"
#include "safemem.h"

#include <sstream>

namespace gametext
{
namespace
{
int g_applied = 100; // the size the tables hold
int g_fixedSet = 0, g_richSet = 0;
std::string g_note = "not applied";

std::string ReadFile(const char* name)
{
    char path[MAX_PATH * 2] = {};
    if (!GetModuleFileNameA(nullptr, path, MAX_PATH))
        return "";
    char* slash = strrchr(path, '\\');
    if (!slash)
        return "";
    *slash = 0;
    strcat_s(path, "\\..\\DATA\\FONTS\\");
    strcat_s(path, name);
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") || !f)
        return "";
    std::string s;
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        s.append(buf, n);
    fclose(f);
    return s;
}

// The fonts.ini table in memory: a list, its head at exe::FontList, an entry at node + 8 (nickname + 8, fixed_height + 0xD0).
int WriteFixed(const std::vector<fontscale::Fixed>& fonts, int percent)
{
    if (!exe::FontList.ok())
        return 0;
    const u32 head = Mem<u32>(exe::FontList);
    u32 node = 0;
    int set = 0;
    if (!head || !safemem::Read(head, &node, 4))
        return 0;
    for (int guard = 0; node && node != head && guard < 256; ++guard)
    {
        char nick[64] = {};
        if (!safemem::Read(node + 8 + 8, nick, sizeof(nick) - 1))
            break;
        for (const fontscale::Fixed& f : fonts)
            if (f.nickname == nick)
            {
                const float h = fontscale::Scaled(f.height, percent);
                if (hk::Write(node + 8 + 0xD0, &h, sizeof(h)))
                    ++set;
                break;
            }
        if (!safemem::Read(node, &node, 4))
            break;
    }
    return set;
}

// Common.dll's exports, by name: no address of that DLL is kept
struct FontDesc
{
    const char* face;
    int size;
    u8 bold, italic, underline, pad;
};
using SetStandardFontFn = void(__cdecl*)(int, const FontDesc*);
using ReinitializeFn = void(__cdecl*)();

bool CallSet(SetStandardFontFn fn, int id, const FontDesc* d)
{
    __try
    {
        fn(id, d);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
bool CallVoid(ReinitializeFn fn)
{
    __try
    {
        fn();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

int WriteRich(const std::vector<fontscale::Rich>& fonts, int percent)
{
    const HMODULE common = GetModuleHandleA("common.dll");
    if (!common)
        return 0;
    const auto set = reinterpret_cast<SetStandardFontFn>(GetProcAddress(common, "?SetStandardFont@@YAXHABUFontDesc@TextRenderContext@@@Z"));
    const auto reinit = reinterpret_cast<ReinitializeFn>(GetProcAddress(common, "?ReinitializeRichTextFonts@@YAXXZ"));
    if (!set || !reinit)
        return 0;
    int done = 0;
    for (const fontscale::Rich& f : fonts)
    {
        if (f.id < 0 || f.id >= 32)
            continue;
        const FontDesc d = {f.face.c_str(), fontscale::Scaled(f.size, percent), 0, 0, 0, 0};
        done += CallSet(set, f.id, &d) ? 1 : 0;
    }
    return done && CallVoid(reinit) ? done : 0;
}

void Apply(int percent)
{
    const std::vector<fontscale::Fixed> fixed = fontscale::ParseFixed(ReadFile("fonts.ini"));
    const std::vector<fontscale::Rich> rich = fontscale::ParseRich(ReadFile("rich_fonts.ini"));
    g_applied = percent; // (one try per value, whatever comes of it: never every frame)
    if (fixed.empty() || rich.empty() || !exe::FontFlush.ok())
    {
        g_note = "refused: font files or the flush not found";
        Log("gametext: %s", g_note.c_str());
        return;
    }
    g_fixedSet = WriteFixed(fixed, percent);
    g_richSet = WriteRich(rich, percent);
    const auto flush = reinterpret_cast<ReinitializeFn>(exe::FontFlush.get());
    const bool flushed = CallVoid(flush);
    g_note = flushed ? "applied" : "flush faulted";
    Log("gametext: %d %% - %d of %zu fonts, %d of %zu rich fonts, %s", percent, g_fixedSet, fixed.size(), g_richSet, rich.size(),
        g_note.c_str());
}
} // namespace

void Frame(int percent, bool menu)
{
    if (menu && percent != g_applied)
        Apply(percent);
}

int Applied() { return g_applied; }

void RichNow(int percent)
{
    g_richSet = WriteRich(fontscale::ParseRich(ReadFile("rich_fonts.ini")), percent);
    Log("gametext: rich %d %% now - %d fonts", percent, g_richSet);
}

void RichSet(int percent)
{
    static std::vector<fontscale::Rich> rich = fontscale::ParseRich(ReadFile("rich_fonts.ini"));
    g_richSet = WriteRich(rich, percent);
}

std::vector<Font> Fonts()
{
    std::vector<Font> out;
    if (!exe::FontList.ok())
        return out;
    const std::vector<fontscale::Fixed> fixed = fontscale::ParseFixed(ReadFile("fonts.ini"));
    const u32 head = Mem<u32>(exe::FontList);
    u32 node = 0;
    if (!head || !safemem::Read(head, &node, 4))
        return out;
    for (int guard = 0; node && node != head && guard < 256; ++guard)
    {
        char nick[64] = {};
        float height = 0;
        if (!safemem::Read(node + 8 + 8, nick, sizeof(nick) - 1) || !safemem::Read(node + 8 + 0xD0, &height, 4))
            break;
        for (const fontscale::Fixed& f : fixed)
            if (f.nickname == nick)
            {
                Font font;
                font.nickname = f.nickname, font.face = f.face, font.height = height, font.entry = node + 8;
                out.push_back(font);
                break;
            }
        if (!safemem::Read(node, &node, 4))
            break;
    }
    return out;
}

std::string Status()
{
    std::ostringstream o;
    o << "gametext applied=" << g_applied << " fonts=" << g_fixedSet << " rich=" << g_richSet << " " << g_note;
    return o.str();
}
} // namespace gametext
