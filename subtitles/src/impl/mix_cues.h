// Scene mix cues (pure; tests\mix_cues_test.cpp). A scene mix (gf_sounds.ini mix_*: a whole room scene's dialogue as
// one track, started by the scene's THN SOUND entity) has its lines in data/mixes.tsv, each a window on the mix's own
// clock (seconds from the sound's start). mixcues.cpp starts a mix's track when the game starts that sound and calls
// these each frame: which lines are due, and whether the track is over.
#pragma once
#include <vector>

namespace mixcues
{
struct Window
{
    float start, end; // seconds on the mix clock
};

// The lines of a track (sorted by start) that start by t, from `next` on; `next` moves past them. A line already over
// at t (a late first frame, a hitch) is passed over, not shown late.
inline void Due(const std::vector<Window>& lines, size_t& next, double t, std::vector<size_t>& show)
{
    show.clear();
    for (; next < lines.size() && lines[next].start <= t; ++next)
        if (lines[next].end > t)
            show.push_back(next);
}

// The end of a track: its last line is over (+1 s), or the scene, once seen, has been gone for 0.5 s - it ended or the
// player skipped it (Escape: USER_CANCEL ends the scene and its sound). `scene` = room::InScene (the game's cinematic
// letterbox, or - with no bars, which vanilla and mods allow (the owner, 29 Oct 2026) - every room button hidden while
// the NavBar shows); a track that never sees a scene ends with its lines (or on mixcues' other signals: the base, space,
// the menu, another mix). `skipped`: the scene went before the last line was over (lines on screen are retired at once).
struct Track
{
    bool sawScene = false;
    double goneSince = -1;
    static constexpr double kGone = 0.5; // a scene state that flickers for a frame or two is not an end
    bool Ended(double t, float lastEnd, bool scene, bool& skipped)
    {
        skipped = false;
        if (scene)
            sawScene = true, goneSince = -1;
        else if (sawScene)
        {
            if (goneSince < 0)
                goneSince = t;
            else if (t - goneSince >= kGone)
            {
                skipped = t < lastEnd;
                return true;
            }
        }
        return t > lastEnd + 1.0;
    }
};
} // namespace mixcues
