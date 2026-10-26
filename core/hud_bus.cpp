// FLvibed - the HUD bus (core\hud_bus.h): the one instance in FLvibed.dll, compiled plain beside core\flvibed.cpp
// (build.cmd :link). Two plain records copied in and out; the game's thread only.
#include "pre.h"

#include "hud_bus.h"

#include "fluf.h"

namespace
{
HudLayout g_layout{};
bool g_haveLayout = false;
HudSubtitles g_subs{};
bool g_haveSubs = false;
u32 g_frame = 0;

void PublishLayout(const HudLayout* l)
{
    if (!l)
        return;
    g_layout = *l;
    g_haveLayout = true;
    ++g_frame;
}

bool ReadLayout(HudLayout* out)
{
    if (!out || !g_haveLayout)
        return false;
    *out = g_layout;
    return true;
}

void PublishSubtitles(const HudSubtitles* s)
{
    if (!s)
        return;
    g_subs = *s;
    g_haveSubs = true;
}

bool ReadSubtitles(HudSubtitles* out)
{
    if (!out || !g_haveSubs)
        return false;
    *out = g_subs;
    return true;
}

u32 Frame() { return g_frame; }

HudLane g_lane{};
bool g_haveLane = false;
u32 g_laneFrame = 0;

void PublishLane(const HudLane* l)
{
    if (!l)
        return;
    g_lane = *l;
    g_haveLane = true;
    ++g_laneFrame;
}

bool ReadLane(HudLane* out)
{
    if (!out || !g_haveLane)
        return false;
    *out = g_lane;
    return true;
}

u32 LaneFrame() { return g_laneFrame; }

HudFloor g_floor{};
bool g_haveFloor = false;
u32 g_floorFrame = 0;
HudDockedSubs g_dsubs{};
bool g_haveDsubs = false;
u32 g_dsubsFrame = 0;

void PublishFloor(const HudFloor* f)
{
    if (!f)
        return;
    g_floor = *f;
    g_haveFloor = true;
    ++g_floorFrame;
}

bool ReadFloor(HudFloor* out)
{
    if (!out || !g_haveFloor)
        return false;
    *out = g_floor;
    return true;
}

u32 FloorFrame() { return g_floorFrame; }

void PublishDockedSubs(const HudDockedSubs* s)
{
    if (!s)
        return;
    g_dsubs = *s;
    g_haveDsubs = true;
    ++g_dsubsFrame;
}

bool ReadDockedSubs(HudDockedSubs* out)
{
    if (!out || !g_haveDsubs)
        return false;
    *out = g_dsubs;
    return true;
}

u32 DockedSubsFrame() { return g_dsubsFrame; }

// version 4: FLUF's windows from the one probe in FLvibed.dll (core\fluf.*), refreshed lazily by whoever asks first
bool ReadFluf(HudFluf* out) { return fluf::Read(out); }
void SetFlufPolicy(int policy) { fluf::SetPolicy(policy); }
// version 5: the same snapshot's per-window extras
bool ReadFlufExtra(HudFlufExtra* out) { return fluf::ReadExtra(out); }

const HudBus g_bus = {5,          PublishLayout, ReadLayout,        PublishSubtitles, ReadSubtitles,
                      Frame,       PublishLane,   ReadLane,          LaneFrame,        PublishFloor,
                      ReadFloor,   FloorFrame,    PublishDockedSubs, ReadDockedSubs,   DockedSubsFrame,
                      ReadFluf,    SetFlufPolicy, ReadFlufExtra};
} // namespace

extern "C" const HudBus* flv_hud_bus() { return &g_bus; }
