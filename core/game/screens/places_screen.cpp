// The NeuroNet's left column's reads: places_screen.h. nnpage.cpp's Here (its reads), ReadPlaces and Offers, moved unchanged
// (audit 70b F4a).
#include "places_screen.h"

#include "game/baseinfo.h"
#include "game/fl.h"
#include "game/hudinfo.h"
#include "game/space.h"

#include <algorithm>

namespace screens
{
Here ReadHere()
{
    Here h;
    h.docked = fl::PlayerBase() != 0;
    h.system = h.docked ? baseinfo::SystemName() : hudinfo::SystemName();
    h.place = h.docked ? baseinfo::BaseName() : hudinfo::ZoneName();
    return h;
}

std::wstring Offers(const gamedata::BaseFacts& b)
{
    std::wstring s;
    const auto add = [&](bool on, const wchar_t* word)
    {
        if (on)
            s += (s.empty() ? L"" : L", "), s += word;
    };
    add(b.ships, L"SHIPS");
    add(b.equipment, L"EQUIPMENT");
    add(b.trader, L"TRADE");
    add(b.bar, L"BAR");
    return s;
}

static std::wstring Upper(std::wstring s)
{
    for (wchar_t& c : s)
        c = towupper(c);
    return s;
}

void ReadPlaces(std::vector<Place>& places, std::vector<Place>& ships)
{
    places.clear(), ships.clear();
    if (fl::PlayerBase())
        return;
    for (const space::Contact& c : space::Contacts())
    {
        if (c.ship && c.id && !c.player)
        {
            Place p;
            p.name = hudinfo::DisplayName(c.id);
            p.what = hudinfo::FactionName(c.id);
            p.distance = c.distance, p.attitude = c.attitude;
            if (!p.name.empty())
                ships.push_back(p);
        }
        if (c.ship || !c.dock || !c.id || hudinfo::IsLaneRing(c.id) || hudinfo::Undiscovered(c.id))
            continue;
        Place p;
        p.name = hudinfo::DisplayName(c.id);
        p.what = hudinfo::ClassName(c.id);
        p.distance = c.distance;
        // what the data says of a place the player has found: where a gate leads, what a base offers
        for (const gamedata::SolarFact& s : gamedata::System(fl::PlayerSystem()).solars)
            if (s.id == c.id)
            {
                if (!s.goesToName.empty())
                    p.what += L" TO " + Upper(s.goesToName);
                else if (s.base)
                {
                    const std::wstring o = Offers(gamedata::Base(s.base));
                    if (!o.empty())
                        p.what += L": " + o;
                }
            }
        if (!p.name.empty())
            places.push_back(p);
    }
    const auto nearest = [](const Place& a, const Place& b) { return a.distance < b.distance; };
    std::sort(places.begin(), places.end(), nearest);
    std::sort(ships.begin(), ships.end(), nearest);
}
} // namespace screens
