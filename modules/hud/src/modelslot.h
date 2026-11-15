// The model slot (audit 70b F7, from the model probe of 48_): a page asks "draw this ship's or item's 3D model in this
// rectangle", each frame it wants it, and the slot does the rest: its own mesh holder, camera and lights, the draw in HUD 2's
// pass under the game's UI (so the page's own text and frames draw over it), only inside the rectangle, nothing while the
// model is not loaded, and every holder freed when the page stops asking, over a save load, a base or system change, a device
// Reset, HUD 2 leaving, the module's stop and the logic's reload. Up to 12 models at once. No page type in here: a page
// knows only Model, its key and Status. How a page asks: modules\hud\docs\UI_HUD2_EXTENDED.md section 18.
#pragma once
#include "modelslot_rules.h"

#include <string>

namespace modelslot
{
// What a page asks for, each frame it wants the model drawn.
struct Model
{
    const char* file = nullptr; // the model's file under DATA, as the game's ini names it (a ship's or an item's DA_archetype)
    Rect rect;                  // back-buffer pixels; drawn only when it lies wholly inside the screen and `clip`
    Rect clip;                  // the page's own clip (a list's area); w 0 = the screen only
    float yaw = 0, pitch = 0;   // radians; pitch about the screen's horizontal axis (its sign not yet judged in game)
    float spin = 0;             // radians a second added to yaw while it shows (0.8: the experiment's turn, about 46 degrees a second)
    float fill = 0.6f;          // the bounds' radius over half the rectangle's smaller side (0.6 fits a whole ship: 48_ 4)
    float fov = 25.0f;          // the camera's vertical half angle, degrees (the ship view's +0x4B4)
    unsigned flags = 0x300;     // the renderer's flags (the ship view's)
    int ambient = 0;            // the ambient light 0..255 (the ship view's is black: its three lights light the model)
};

// A page's ask for `key`'s model this frame (KeyOf(page, row)), from any pass of the game's thread: drawn at the next pass.
// A key not asked for half a second gives its holder and its place back. False: no file, or all 12 places taken.
bool Show(unsigned key, const Model& m);
// The page closes: the key's holder freed at the next pass.
void Drop(unsigned key);

enum class Status
{
    None,    // not asked (or given back)
    Pending, // asked, not loaded yet: the page draws its flat fallback (an icon or text: 48_ 6)
    Ready,   // drawn at the last pass
    Hidden,  // loaded, but its rectangle was off the screen or out of the clip at the last pass
    Failed,  // the file did not load (or a draw faulted): the flat fallback stays
};
Status StatusOf(unsigned key);

// HUD 2's pass under the UI (hud2_entry UnderBegin): loads, draws and frees what was asked. Nothing at all when nothing is.
void Pass();
// Every holder freed now, the asks kept (HUD 2 leaving, a fault's give-back): `why` goes to the log.
void Release(const char* why);
// The module stops or the logic reloads: every holder freed, the lights out of the engine's list, the asks forgotten.
void Shutdown();

// Tests (the model probe's state line): what the slots hold and what the last passes cost.
struct Stats
{
    int used = 0, held = 0, drawn = 0, faults = 0;
    unsigned loads = 0, frees = 0, failures = 0, refused = 0, passes = 0, releases = 0;
    double usLast = 0, usAvg = 0, usMax = 0; // the pass's CPU time while it drew, microseconds
    long frames = 0;                         // passes that drew since the last ResetCost
    bool symbols = false;                    // every signed symbol resolved
    bool vpSame = true;                      // the viewport read back as before every pass
    int vpBefore[4] = {}, vpAfter[4] = {}, d3dAfter[4] = {};
    const char* lastWhy = "-"; // the last release's reason
    std::string why;           // the last refusal or fault
};
const Stats& GetStats();
void ResetCost();
// One slot's load: its handle (-1 none), its load time and its bounds (Engine vt[0x88] A, B); false: not asked.
bool SlotInfo(unsigned key, int* handle, double* loadMs, float boundsA[4], float boundsB[4], float* distance);
} // namespace modelslot
