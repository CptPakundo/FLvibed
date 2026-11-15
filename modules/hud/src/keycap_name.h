// The name a punctuation key's keycap shows in the keyboard-and-mouse form (hud2_data.cpp VkName): the character the key
// types on the player's DEFAULT input layout, never the layout the game's thread holds at that moment. The game's own
// names (its controls screen, Freelancer-Decomp menu_keymap.cpp KeyMapFormatBindingName = 0x577F50) take
// MapVirtualKey + GetKeyNameText, which follow the thread's current layout: with two layouts installed (here English US
// and Hebrew) the same VK_OEM_3 (192, the game's free flight) read "`" in one start and ";" in another, and the maneuver
// row's first cap and the lists under it moved between starts (audit 75_ item 2, 133_ item 8). Only the OEM keys
// (VK_OEM_1..8, VK_OEM_102, the plus / comma / minus / period keys) type a different character per layout; letters,
// digits and named keys are named as before. Win32 only, no game: tests\keycap_name_test.cpp.
#pragma once
#include <windows.h>
#include <string>

namespace keycapname
{
// The player's default input layout (Settings > Time & language > the first language); the thread's own when unread.
inline HKL DefaultLayout()
{
    HKL h = nullptr;
    if (SystemParametersInfoW(SPI_GETDEFAULTINPUTLANG, 0, &h, 0) && h)
        return h;
    return GetKeyboardLayout(0);
}

inline bool IsOemKey(int vk) { return (vk >= 0xBA && vk <= 0xC0) || (vk >= 0xDB && vk <= 0xDF) || vk == 0xE2; }

// A key whose name is a lone small mark reads as an EMPTY keycap at the HUD's size: the game's free flight
// (USER_MANEUVER_FREE_FLIGHT, key 192 = VK_OEM_3 in SAVE\UserKeyMap.ini) is "`" on a US layout - ABORT DOCK / STOP / the
// K&M maneuver row's first key showed a ~0.004 H tick in an empty cap (the owner's frames, 31 Oct 2026). The grave key
// shows its larger mark "~" (the same 1-character cap: "TILDE" widened LEAVE FORMATION's pill to 0.28 H, past the K&M
// loadout's edge - the context line fell back above the panels into zone S1); the rarer marks get their word; nullptr:
// the character stands as it is (a letter-like one, a German layout's "\x00D6", in capitals).
inline const wchar_t* MarkWord(wchar_t c)
{
    switch (c)
    {
    case L'`':
    case L'~': return L"~";
    case 0x00B4: return L"ACUTE";
    case L'^': return L"CARET";
    case L'\'': return L"QUOTE";
    case L',': return L"COMMA";
    case L'.': return L"PERIOD";
    case L'-': return L"MINUS";
    case 0x00A8: return L"DIAERESIS";
    default: return nullptr;
    }
}

// An OEM key's cap by the layout given: its unshifted character (a dead key's too) as MarkWord spells it, else in
// capitals; "" when the layout types nothing on it (the caller names it the old way).
inline std::wstring OemName(int vk, HKL layout)
{
    if (!IsOemKey(vk) || !layout)
        return L"";
    const wchar_t c = static_cast<wchar_t>(MapVirtualKeyExW(static_cast<UINT>(vk), MAPVK_VK_TO_CHAR, layout) & 0xFFFF);
    if (c <= L' ')
        return L"";
    if (const wchar_t* w = MarkWord(c))
        return w;
    wchar_t s[2] = {c, 0};
    CharUpperBuffW(s, 1);
    return s;
}
} // namespace keycapname
