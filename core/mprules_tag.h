// The multiplayer server's permission tag in a chat line (controller\src\impl\mprules.*, docs\MULTIPLAYER.md "Server
// permission"). Pure: unit-tested in tests\mprules_tag_test.cpp.
//
// The tag counts only as the WHOLE line (spaces around it allowed): "[flvibed autopilot=on|full|all|travel|off]",
// case-insensitive. Why the whole line (28 Oct 2026, two clients on HDE's FLHook server): FLHook relays a player's chat
// to every client as a SERVER line (sender 0x10000, the server's own id) with the player's name first - "FLvTest22:
// [flvibed autopilot=off]" switched both clients off while the rule was "the tag anywhere in a server line". A relayed
// line always carries the "Name: " prefix and a non-empty text, so it can never be the bare tag; the FLHook plugin
// (server\flhook) and an admin's own banner line send the tag alone.
#pragma once
#include <string>

// 0 off, 1 travel, 2 everything; false = no tag (or not the whole line, or an unknown value)
inline bool ParseMpTag(const std::wstring& text, int& out)
{
    size_t b = 0, e = text.size();
    while (b < e && (text[b] == L' ' || text[b] == L'\t' || text[b] == L'\r' || text[b] == L'\n'))
        ++b;
    while (e > b && (text[e - 1] == L' ' || text[e - 1] == L'\t' || text[e - 1] == L'\r' || text[e - 1] == L'\n'))
        --e;
    std::wstring t;
    for (size_t i = b; i < e; ++i) // the tag is ASCII: lower case ASCII letters only
        t += text[i] >= L'A' && text[i] <= L'Z' ? static_cast<wchar_t>(text[i] + 32) : text[i];
    static const std::wstring head = L"[flvibed autopilot=";
    if (t.size() < head.size() + 2 || t.compare(0, head.size(), head) != 0 || t.back() != L']')
        return false;
    const std::wstring v = t.substr(head.size(), t.size() - head.size() - 1);
    if (v == L"on" || v == L"full" || v == L"all")
        out = 2;
    else if (v == L"travel")
        out = 1;
    else if (v == L"off")
        out = 0;
    else
        return false;
    return true;
}
