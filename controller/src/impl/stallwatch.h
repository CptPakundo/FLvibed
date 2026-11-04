// The stall rule of the NPC stall recorder (docs\research\npc-ai.md; live part stallrec.*), pure: unit test
// tests\stallwatch_test.cpp. A ship stalled when it stayed within kMoveM of one point for kStallSec of game time; it is
// over when it left that point, or when the ship is gone. The owner, 4 Nov 2026: enemies sitting at 0 speed, not every run.
#pragma once
#include <map>
#include <vector>

namespace stallwatch
{
constexpr float kMoveM = 2.0f, kStallSec = 3.0f;
constexpr double kMaxGapSec = 1.5; // a longer gap between rounds (a load, a pause) starts over

struct Sample
{
    unsigned id = 0;
    float pos[3] = {};
};
struct Event
{
    unsigned id = 0;
    bool began = false;  // true: it just stalled; false: the stall is over
    bool gone = false;   // over because the ship is no longer sampled (destroyed, out of range)
    double seconds = 0;  // the stall's length so far
};

class Watch
{
    struct Track
    {
        float anchor[3] = {};
        double since = 0;
        bool stalled = false;
        bool seen = false;
    };
    std::map<unsigned, Track> m_ships;
    double m_last = -1;

public:
    void Reset()
    {
        m_ships.clear();
        m_last = -1;
    }
    // One round at game time `now`: every ship in scope. Events are appended to `out`.
    void Update(double now, const std::vector<Sample>& ships, std::vector<Event>& out)
    {
        if (m_last >= 0 && now - m_last > kMaxGapSec)
            m_ships.clear();
        m_last = now;
        for (auto& e : m_ships)
            e.second.seen = false;
        for (const Sample& s : ships)
        {
            auto it = m_ships.find(s.id);
            if (it == m_ships.end())
            {
                Track t;
                for (int i = 0; i < 3; ++i)
                    t.anchor[i] = s.pos[i];
                t.since = now;
                t.seen = true;
                m_ships[s.id] = t;
                continue;
            }
            Track& t = it->second;
            t.seen = true;
            float d2 = 0;
            for (int i = 0; i < 3; ++i)
                d2 += (s.pos[i] - t.anchor[i]) * (s.pos[i] - t.anchor[i]);
            if (d2 > kMoveM * kMoveM)
            {
                if (t.stalled)
                    out.push_back({s.id, false, false, now - t.since});
                for (int i = 0; i < 3; ++i)
                    t.anchor[i] = s.pos[i];
                t.since = now;
                t.stalled = false;
            }
            else if (!t.stalled && now - t.since >= kStallSec)
            {
                t.stalled = true;
                out.push_back({s.id, true, false, now - t.since});
            }
        }
        for (auto it = m_ships.begin(); it != m_ships.end();)
        {
            if (it->second.seen)
            {
                ++it;
                continue;
            }
            if (it->second.stalled)
                out.push_back({it->first, false, true, now - it->second.since});
            it = m_ships.erase(it);
        }
    }
};
} // namespace stallwatch
