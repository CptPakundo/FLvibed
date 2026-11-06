// What hud2.cpp (the frame) and hud2_data.cpp (the model) share. Private to those two files; hud2_impl.h is for the parts.
#pragma once

namespace hud2
{
// hud2.cpp: the frame's state the model reads and writes
extern Context g_ctx;
extern Model g_m;
extern Layout g_L;
extern std::string g_sim; // debug: a mock-up state painted over the live data (its name: hud2_sim.h)

// hud2_data.cpp: the model's state the frame reads, and its steps
extern long long g_creditsTest; // debug `hud2 credits <n>`: the ship panel's number (-1: the player's)
extern u32 g_laneKey[2], g_laneFinal;
extern std::vector<u32> g_laneRings;
extern u32 g_objSeq;
extern std::vector<u32> g_objRefs;
extern bool g_combat;
extern double g_spaceSince; // when the ship last entered space (a load, a launch): the alerts arm after it

std::wstring Upper(std::wstring s);
bool FireKeyPressed();
void Alert(const wchar_t* title, u32 col, int side = 0);
void ClearAlerts(); // out of space: an alert's timer stands still there, and it would come back stale on the next flight
void SlideAlerts(float dt);
void AlertNone(const wchar_t* s);
void AlertPrompt(presets::Action a, const wchar_t* label);
bool AlertTractor(int reason); // core\game\tractorfail.h
void Fast(float dt);
void ProjectTarget(Model& m); // the target on the screen, at the moment of the call
void Slow();
void Simulate(Model& m);
} // namespace hud2
