// What the flight modals of phase D share (docs\UI_SPEC.json `hud` states picker / wheel / goals; docs\UI_BUILD.md):
// the pad's button edges with D-pad repeat, the game commands as taps, the names and distances of contacts, the colours
// (the HUD's red / white / green attitudes, its chrome), the prompt bar bottom right (confirm first, at most five
// glyphs, `prompt_bar`: the shared promptbar.* draws it since the integration of 25 Oct 2026), and the fonts
// (uikit::FontsFor at `text_size`). The modals (picker.*, wheel.*, goals.*) are FLvibed-drawn screens over the HUD that
// step aside for nothing: the sticks keep flying, RT keeps firing; they own only the buttons they name (controller.cpp
// hides those from the flight dispatch while one is open). Header-only.
#pragma once
#include "common.h"
#include "../fl.h"
#include "aim.h"
#include "game/hudinfo.h"
#include "game/render.h"
#include "game/space.h"
#include "game/uidraw.h"
#include "glyphs.h"
#include "promptbar.h"
#include "text.h"
#include "uikit.h"
#include "uiopt.h"

#include <string>
#include <vector>

namespace flightmodal
{
constexpr u32 kHud = 0x82C9F3;      // the HUD's chrome (130, 201, 243)
constexpr u32 kDim = 0x6E8EA8;      // secondary text
constexpr u32 kWhite = 0xF0F4F8;    // neutral
constexpr u32 kHostile = 0xFF5A48;  // the HUD's hostile red
constexpr u32 kFriendly = 0x6EE07A; // the HUD's friendly green
constexpr u32 kSelected = 0xFCF344; // the game's selected caption yellow
constexpr u32 kGrey = 0x76767B;     // disabled
constexpr wchar_t kDot = static_cast<wchar_t>(0x00B7); // the middle dot separator

inline u32 Argb(u32 rgb, float a)
{
    return (static_cast<u32>(Clampf(a, 0, 1) * 255 + 0.5f) << 24) | (rgb & 0xFFFFFF);
}

// Button edges over the frame's down[] (glyphs::Id order), with auto-repeat on the D-pad's up / down / left / right
// while held (0.4 s, then every 0.1 s). One per modal (each keeps its own previous frame).
struct Edges
{
    bool pressed[glyphs::COUNT] = {};
    bool down[glyphs::COUNT] = {};
    bool prev[glyphs::COUNT] = {};
    float repeat[glyphs::COUNT] = {};
    bool fresh = true; // the first Update after Reset primes: the press that opened the modal is no edge inside it
    void Reset()
    {
        memset(pressed, 0, sizeof(pressed));
        memset(down, 0, sizeof(down));
        memset(prev, 0, sizeof(prev));
        fresh = true;
    }
    // `in` = the pad's buttons this frame; after it, pressed[] holds the edges (and the repeats of the D-pad).
    void Update(float dt, const bool in[glyphs::COUNT])
    {
        if (fresh)
        {
            memcpy(prev, in, sizeof(prev));
            fresh = false;
        }
        for (int i = 0; i < glyphs::COUNT; ++i)
        {
            down[i] = in[i];
            pressed[i] = in[i] && !prev[i];
            const bool dpad = i == glyphs::DU || i == glyphs::DD || i == glyphs::DL || i == glyphs::DR;
            if (dpad)
            {
                if (pressed[i])
                    repeat[i] = 0.4f;
                else if (in[i])
                {
                    repeat[i] -= dt;
                    if (repeat[i] <= 0)
                        pressed[i] = true, repeat[i] = 0.1f;
                }
            }
            prev[i] = in[i];
        }
    }
};

// A game command as a bound key's press and release would send it (its keymap event mask; controller.cpp's Tap).
inline void Tap(int cmd)
{
    const u32 flags = fl::CommandFlags(cmd);
    if (flags & fl::CMDF_KEYDOWN)
        fl::FireCommand(cmd, true);
    if (flags & fl::CMDF_KEYUP)
        fl::FireCommand(cmd, false);
}

inline std::wstring Upper(std::wstring s)
{
    for (wchar_t& c : s)
        c = static_cast<wchar_t>(towupper(c));
    return s;
}

inline std::wstring Dist(float m)
{
    wchar_t buf[32];
    if (m >= 1000)
        swprintf_s(buf, L"%.1f KM", m / 1000);
    else
        swprintf_s(buf, L"%.0f M", m);
    return buf;
}

inline std::wstring Sep() { return std::wstring(L" ") + kDot + L" "; }

// A contact's name as the HUD shows it: a ship's pilot (its reputation's name), else the object's own name; a space id
// when it has neither.
inline std::wstring ContactName(const space::Contact& c)
{
    // as the game's own target panel / contact list names it: an unvisited gate / hole / station reads "Unknown ..." (no
    // knowledge leak: the picker's NAVIGATION rows named "NEW YORK -> COLORADO" before a visit), a lane ring its lane
    std::wstring n = hudinfo::DisplayName(c.id);
    if (n.empty())
        n = c.ship ? space::SpeakerName(c.id) : text::Ids(c.nameIds);
    if (n.empty())
        n = space::SpeakerName(c.id);
    if (n.empty())
    {
        wchar_t buf[24];
        swprintf_s(buf, L"#%u", c.id);
        n = buf;
    }
    return Upper(n);
}

// The HUD's attitude colour: hostile red, friendly green, else white (aim::kHostileAttitude and its mirror).
inline u32 AttitudeColour(float att, bool known)
{
    if (!known)
        return kWhite;
    if (att <= aim::kHostileAttitude)
        return kHostile;
    if (att >= -aim::kHostileAttitude)
        return kFriendly;
    return kWhite;
}

// A dockable thing: a base / station / planet docking ring / gate / hole the game lets the player dock with, or a
// lane ring (the dock maneuver rides it).
inline bool Dockable(const space::Contact& c)
{
    return c.dock || (c.type & (space::kTradelaneRing | space::kJumpGate | space::kJumpHole | space::kDockingRing)) != 0;
}

inline std::wstring GlyphToken(glyphs::Id id)
{
    std::wstring s = L"{";
    for (const char* p = glyphs::Name(id); *p; ++p)
        s += static_cast<wchar_t>(*p);
    return s + L"}";
}

// The screen and the fonts for this frame (fractions of H x text_size); false without a device / screen.
struct Screen
{
    float W = 0, H = 0, ts = 1;
    const uikit::Fonts* f = nullptr;
};
inline bool BeginScreen(void* device, Screen& s)
{
    if (!device)
        return false;
    render::SetDevice(device);
    glyphs::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return false;
    s.W = static_cast<float>(sw);
    s.H = static_cast<float>(sh);
    s.ts = uiopt::Get().textSize / 100.0f;
    s.f = &uikit::FontsFor(sh, uiopt::Get().textSize);
    return s.f->ok && render::Begin(s.f->label);
}

// The prompt bar bottom right (UI_PLAN 6.5: confirm first, at most five glyphs): items as "{A} TARGET" ("{LB}{RB}
// CATEGORY" = two glyphs for one label). Since 25 Oct 2026 (the integration) nothing is drawn here: the items go to the
// shared bar (promptbar::Set, priority 5, pending = a modal awaits a choice, so the contextual rule keeps it up while
// the modal is open; `prompt_bar` OFF is its business), which draws them in the overlay pass after the modals. Returns
// the bar's top edge (the panel above keeps clear of it). `screen` = the modal's id for the bar's contextual memory.
struct Prompt
{
    std::wstring text; // with glyph tokens
    bool enabled = true;
};
inline float PromptBar(const Screen& s, const std::vector<Prompt>& items, const char* screen = "flight.modal")
{
    const float top = promptbar::Top(s.H, s.f->label) - s.H * 0.01f;
    if (items.empty())
        return top;
    promptbar::Prompt out[5];
    std::wstring labels[5]; // the labels live here until Set has copied them
    int n = 0;
    for (const Prompt& p : items)
    {
        if (n == 5)
            break;
        glyphs::Id btn = glyphs::COUNT, btn2 = glyphs::COUNT;
        std::wstring rest = p.text;
        int got = 0;
        while (!rest.empty() && rest[0] == L'{' && got < 2) // the leading glyph tokens (at most two)
        {
            const size_t close = rest.find(L'}');
            if (close == std::wstring::npos)
                break;
            std::string name;
            for (size_t i = 1; i < close; ++i)
                name += static_cast<char>(rest[i]);
            const glyphs::Id id = glyphs::FromName(name.c_str());
            if (id != glyphs::COUNT)
                (got == 0 ? btn : btn2) = id, ++got;
            rest.erase(0, close + 1);
            while (!rest.empty() && rest[0] == L' ')
                rest.erase(0, 1);
        }
        if (btn == glyphs::COUNT)
            continue; // a prompt without a glyph has no place in the bar
        labels[n] = rest;
        out[n] = promptbar::Prompt{btn, labels[n].c_str(), false, btn2, 0, p.enabled};
        ++n;
    }
    if (n)
        promptbar::Set(screen, out, n, true, 5);
    return top;
}

// Text trimmed with an ellipsis to fit `maxW` in the current font.
inline std::wstring Fit(const render::Font& font, std::wstring s, float maxW)
{
    float w = static_cast<float>(render::TextWidth(font, s));
    while (w > maxW && s.size() > 3)
    {
        s = s.substr(0, s.size() - 2) + L"\x2026";
        w = static_cast<float>(render::TextWidth(font, s));
    }
    return s;
}
} // namespace flightmodal
