// Speedrun mode's clock, split detector and LiveSplit client (docs\UI_PLAN.md 6.6; docs\TESTING.md "Speedrun splits").
// The clock: game time since the New Game click of a speedrun-mode session, loads and menus excluded (the mode `menu`),
// carried per save through newgame's sidecar. The detector: the story's checkpoints of docs\checkpoints.tsv
// (checkpoints_gen.h, the same list and conditions tools\splits.py matches in the logs), reached in order from the live
// game state - the objective step (HostObjectiveNow: obj:<id>; text id 0 = the mission's result, `success`), the mode
// transitions (control / land / launch / cutscene), the marked solars that vanish (kill:<n>); one split per checkpoint,
// never twice, a missed objective checkpoint skipped when a later one's objective shows. The client: a non-blocking TCP
// connection to LiveSplit Server (127.0.0.1:<livesplit_port>, commands line-terminated with \r\n: starttimer on the New
// Game click, split per checkpoint and at the story's end, reset on the main menu, pausegametime / unpausegametime
// around loads when livesplit_gametime is on), reconnecting with a backoff, never stalling a frame; off with
// `livesplit` 0. Plus `FLvibedSplits`, a stable memory struct for community autosplitters (docs\livesplit\README.md).
#pragma once
#include "common.h"

#include <istream>
#include <string>

namespace livesplit
{
void Init();
void Shutdown();
// Every frame (ctl::OnFrame, early, whatever the device): `modeName` = ctl's ModeName(g_mode).
void Update(float dt, const char* modeName);
// The New Game click of a speedrun session (newgame.cpp): the clock starts, LiveSplit gets starttimer.
void OnNewGame(bool speedrun);

// For the HUD timer (runtimer.cpp) and the debug commands.
bool Running();            // a speedrun session's clock runs (game time, outside the menu)
u32 RunMs();               // game time since the New Game click
int CheckpointIndex();     // the last checkpoint reached (-1 none); the next expected is +1
int SplitSeq();            // grows with every split
u32 LastSplitMs();         // the clock at the last split
u32 LastSegmentMs();       // the last split's segment (its time minus the previous split's)
const char* CheckpointName(int index); // "M01a.wing" (nullptr out of range)
const char* SegmentOf(int index);      // what happens in that segment
bool Connected();

// Debug: livesplit state|test|connect|reset
std::string Command(std::istream& in);
std::string Status();
} // namespace livesplit
