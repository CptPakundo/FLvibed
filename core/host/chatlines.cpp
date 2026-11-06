// The chat window's lines as they arrive, for the multiplayer rule reader (chatlines.h).
#include "all.h"

#include "mprules_tag.h"

namespace chatlines
{
// v30 ChatLineAt: the chat window's lines as they arrive (exe::ChatFlush: every line the chat log gets - a server's
// or a player's message in multiplayer, the game's own and FLvibed's chatter lines), sender and the RDL's text nodes:
// a server says what it allows FLvibed through it (docs\MULTIPLAYER.md "Server permission").
typedef void(__cdecl* ChatFlushFn)(int sender, const void* rdl, u32 size);
static ChatFlushFn g_origChatFlush = nullptr;
struct ChatLineRec
{
    u32 sender;
    wchar_t text[256];
};
static ChatLineRec g_chat[32];
static u32 g_chatSerial = 0; // lines seen; line n (1-based) is g_chat[(n - 1) % 32]

// v31 ServerTag: the multiplayer session's last tag-only server line (core\mprules_tag.h), kept apart from the ring - a
// logic reload re-reads the ring from the start, and the tag line (sent once, at login) had scrolled out of its 32
// lines: the server's word fell back to the silent-server default until the next login. Forgotten when
// the mode changes (a new session), as the logic forgets it.
static ChatLineRec g_tag = {};
static u32 g_tagSerial = 0; // the tag line's chat serial, 0 = none this session
typedef bool (*SinglePlayerFn)();

static SinglePlayerFn SinglePlayerExport()
{
    static const SinglePlayerFn sp = reinterpret_cast<SinglePlayerFn>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?SinglePlayer@@YA_NXZ"));
    return sp;
}
bool Multiplayer()
{
    const SinglePlayerFn sp = SinglePlayerExport();
    __try
    {
        return sp && !sp();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
void ModeCheck() // per frame and per line: a mode change starts a new session
{
    static bool mp = false;
    const bool now = Multiplayer();
    if (now != mp)
        mp = now, g_tagSerial = 0;
}
static void KeepServerTag(const ChatLineRec& r) // (no __try here: ParseMpTag builds strings)
{
    ModeCheck();
    int v = 0;
    if ((r.sender == 0 || r.sender >= 0x10000) && Multiplayer() && ParseMpTag(r.text, v))
        g_tag = r, g_tagSerial = g_chatSerial;
}

static void RecordChat(int sender, const u8* rdl, u32 size)
{
    ChatLineRec& r = g_chat[g_chatSerial % 32];
    r.sender = static_cast<u32>(sender);
    r.text[0] = 0;
    size_t at = 0;
    __try
    {
        for (u32 off = 0; rdl && off + 8 <= size && off < 0x4000;)
        {
            const u32 type = *reinterpret_cast<const u32*>(rdl + off), len = *reinterpret_cast<const u32*>(rdl + off + 4);
            if (len > size - off - 8)
                break;
            if (type == 2) // text: UTF-16, NUL-terminated
                for (const wchar_t* p = reinterpret_cast<const wchar_t*>(rdl + off + 8);
                     *p && at + 1 < 256 && reinterpret_cast<const u8*>(p) < rdl + off + 8 + len; ++p)
                    r.text[at++] = *p;
            off += 8 + len; // nodes are packed (modules\subtitles\src\impl\chat.cpp builds them so)
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    r.text[at] = 0;
    ++g_chatSerial;
    KeepServerTag(r);
}

u32 ServerTag(u32* sender, wchar_t* text, int max)
{
    ModeCheck();
    if (!g_tagSerial)
        return 0;
    if (sender)
        *sender = g_tag.sender;
    if (text && max > 0)
        wcsncpy_s(text, max, g_tag.text, _TRUNCATE);
    return g_tagSerial;
}

static void __cdecl ChatFlushHook(int sender, const void* rdl, u32 size)
{
    RecordChat(sender, static_cast<const u8*>(rdl), size);
    // a server line that is only FLvibed's tag (the FLHook plugin server\flhook sends one at login) is for FLvibed
    // alone: read above, not shown (a line with other text around the tag is shown as the server wrote it)
    const ChatLineRec& r = g_chat[(g_chatSerial - 1) % 32];
    const size_t n = wcslen(r.text);
    if ((sender == 0 || static_cast<u32>(sender) >= 0x10000) && n > 20 && r.text[n - 1] == L']' &&
        _wcsnicmp(r.text, L"[flvibed autopilot=", 19) == 0 && !wcschr(r.text + 1, L'['))
        return;
    g_origChatFlush(sender, rdl, size);
}

u32 LineAt(u32 n, u32* sender, wchar_t* text, int max)
{
    if (n == 0 || n > g_chatSerial || g_chatSerial - n >= 32 || !text || max < 1)
        return g_chatSerial;
    const ChatLineRec& r = g_chat[(n - 1) % 32];
    if (sender)
        *sender = r.sender;
    wcsncpy_s(text, max, r.text, _TRUNCATE);
    return g_chatSerial;
}

void Install()
{
    // ChatFlush: push ecx; mov eax,[esp+8] (5 bytes)
    g_origChatFlush = static_cast<ChatFlushFn>(
        hk::Detour(exe::ChatFlush, ChatFlushHook, 5, "chat lines (a multiplayer server's permission for FLvibed)"));
}
} // namespace chatlines
