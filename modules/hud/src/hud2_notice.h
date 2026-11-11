// HUD 2's flight notice (GitHub issues 70 and 69; modules\hud\docs\UI_HUD2.md 5.43): a short card that appears on an
// event, holds a few seconds and fades - the credits a mission paid, the loot the tractor took in. Nothing permanent: the
// owner's B9 (31 Oct 2026) keeps credits and the hold off the flight HUD as standing elements; a notice is new
// information that comes and goes (the owner, 11 Nov 2026). This file is the pure half: what the card holds, how a
// second event merges into it, its time and fade, and the credits' gain watch. hud2.cpp / hud2_data.cpp feed it,
// hud2_parts.cpp DrawNotice draws it, hud2_layout.h PlaceNotice places it. No game call, no system headers:
// tests\hud2_notice_test.cpp runs the same code.
#pragma once
#include "common.h"

namespace hud2notice
{
constexpr float kHoldS = 4.0f;     // shown, from the last event that fed it (the old hold strip's wake: hud_wake_s)
constexpr float kFadeInS = 0.15f;  // the alert card's ease in
constexpr float kFadeOutS = 0.4f;  // a calm fade out (not urgent: it leaves slower than it came)
constexpr int kMaxItems = 4;       // item lines; more kinds than that collapse into one "+n MORE" line
constexpr int kMaxExtraKinds = 32; // the extra kinds it remembers to count each once

struct Item
{
    u32 key = 0;       // the item's kind (its good / equipment archetype id; 0 = unknown, merged by name)
    std::wstring name; // as the game names it
    int count = 0;
};

// One card. A new event while it shows (or fades) feeds the same card and restarts its time; once faded out it empties.
struct Notice
{
    bool credits = false;   // the credits line shows
    long long cash = 0;     // the balance after the payout
    long long gain = 0;     // what came in while the card showed (several payouts add up)
    Item items[kMaxItems];  // the loot lines, in the order the kinds first came
    int nItems = 0;
    u32 extra[kMaxExtraKinds] = {}; // the kinds past kMaxItems (counted once each in the "+n MORE" line)
    int nExtra = 0;
    float left = 0;  // seconds left to hold
    float alpha = 0; // 0..1 as faded

    bool Up() const { return alpha > 0.0f || left > 0.0f; }
    bool Loot() const { return nItems > 0; }
    bool Empty() const { return !credits && nItems == 0; }

    // A payout: the balance now and what it rose by.
    void Credit(long long now, long long rise)
    {
        if (rise <= 0)
            return;
        gain = credits ? gain + rise : rise;
        cash = now, credits = true, left = kHoldS;
    }
    // `count` units of an item came into the hold: its line's count grows, or a new line (or the "+n MORE" line).
    void Add(u32 key, const std::wstring& name, int count)
    {
        if (count <= 0)
            return;
        left = kHoldS;
        for (int i = 0; i < nItems; ++i)
            if (key ? items[i].key == key : (items[i].key == 0 && items[i].name == name))
            {
                items[i].count += count;
                return;
            }
        if (nItems < kMaxItems)
        {
            items[nItems].key = key, items[nItems].name = name, items[nItems].count = count;
            ++nItems;
            return;
        }
        for (int i = 0; i < nExtra; ++i)
            if (extra[i] == key)
                return;
        if (nExtra < kMaxExtraKinds)
            extra[nExtra++] = key;
    }
    // Once a frame. `motion` = skin_motion (0 full, 1 fades, 2 none: on and off at once). An emptied card stays empty.
    void Step(float dt, int motion)
    {
        dt = dt > 0 ? dt : 0.0f;
        left = left > dt ? left - dt : 0.0f;
        const bool on = left > 0 && !Empty();
        if (motion >= 2)
            alpha = on ? 1.0f : 0.0f;
        else if (on)
            alpha = alpha + dt / kFadeInS < 1.0f ? alpha + dt / kFadeInS : 1.0f;
        else
            alpha = alpha - dt / kFadeOutS > 0.0f ? alpha - dt / kFadeOutS : 0.0f;
        if (!on && alpha <= 0.0f)
            *this = Notice();
    }
    // Leaving the flight (a dock, a load, the menu): nothing carries over to the next one.
    void Clear() { *this = Notice(); }
};

// The credits' gain watch: a rise of the balance between two reads in flight is a payout. Purchases, repairs and sales
// happen docked, where the watch forgets the balance (Forget), so the first read after a launch only primes it; a load
// forgets it too (LoadWatch), wherever it goes. A fall (the game has no fine in space; a bribe is paid docked) shows nothing.
struct CashWatch
{
    long long last = -1;
    // The rise since the last read (0 none); `armed` false (the first moments in flight, the balance unread: < 0) only primes.
    long long Step(long long now, bool armed)
    {
        if (now < 0)
            return 0;
        const long long was = last;
        last = now;
        return armed && was >= 0 && now > was ? now - was : 0;
    }
    void Forget() { last = -1; }
};

// A game load (a save, New Game, the death screen's LOAD AUTOSAVE / LOAD GAME: each loads a fresh Content.dll, core's
// GameLoads count, the log's "event game-load") ends the flight the notice watched, also when it goes from space into
// space and the HUD never leaves the flight: a richer save loaded from the pause menu in space showed its balance as a
// payout (0.4.6's final check). Step is true once per new load; the first read only primes.
struct LoadWatch
{
    u32 seen = 0;
    bool primed = false;
    bool Step(u32 loads)
    {
        const bool load = primed && loads != seen;
        seen = loads, primed = true;
        return load;
    }
};

// "$50,000": the docked strip's money (uistrip.cpp Money; the dealers' and INVENTORY's alike).
inline std::wstring Money(long long v)
{
    std::wstring d = std::to_wstring(v < 0 ? -v : v), out;
    for (size_t i = 0; i < d.size(); ++i)
    {
        if (i && (d.size() - i) % 3 == 0)
            out += L',';
        out += d[i];
    }
    return (v < 0 ? L"-$" : L"$") + out;
}
} // namespace hud2notice
