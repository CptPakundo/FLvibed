// Access to Freelancer's own UI controls (FlControl tree) for gamepad focus navigation.
#pragma once
#include "common.h"

namespace ui
{
struct Item
{
    uintptr_t ctl = 0;
    uintptr_t root = 0;   // top-level window this control belongs to
    float x = 0, y = 0;   // screen center (pixels)
    float hw = 0, hh = 0; // half extents (pixels)
    float depth = 0;      // camera distance (smaller = closer to the player)
    char name[33] = {};
};

// Project a point in UI camera space to screen pixels. Returns false if behind the camera.
bool Project(float x, float y, float z, float& sx, float& sy);

// All visible, enabled, clickable controls with their screen rectangles.
std::vector<Item> Collect();

// Control currently under Freelancer's cursor (null if none).
uintptr_t Hovered();

// Root window that currently holds Freelancer's keyboard focus (modal windows such as the
// pause menu or the save dialog), if it is one of the roots in `items`; otherwise 0.
uintptr_t ModalRoot(const std::vector<Item>& items);

// Index of the best item in direction (dx, dy) from point (fx, fy), or -1.
int FindInDirection(const std::vector<Item>& items, float fx, float fy, float dx, float dy, uintptr_t exclude);

// Index of the item containing (x, y), or -1.
int FindAt(const std::vector<Item>& items, float x, float y);

const char* ControlName(uintptr_t ctl);
uintptr_t FindControl(const char* name);

// Ask Freelancer which control would be hovered with the cursor at screen pixel (x, y).
uintptr_t HitTest(float x, float y);

// Discover clickable controls by probing Freelancer's own hit test on a screen grid
// (works for every UI class, whatever its internal layout). `step` is in pixels.
std::vector<Item> Probe(float step);

// Clickable controls from both the control tree and hit-test probing, without duplicates.
std::vector<Item> CollectAll();
// CollectAll for per-frame readers, shared: the hit-test probe inside it is ~2800 game hit tests (2.5 ms in a room,
// 7 ms on a dealer at 4K: perf 25 Oct 2026, research\experience\runs\perf_pad_2026-10-25), so it runs again only when
// the tree walk's clickable set (Collect, ~12 us: controls, roots, rectangles) changed - at most every 50 ms then, the
// old refresh rate - else every 2 s (what only the probe sees: front-end lists, covered tree items). A reader that acts
// on the controls right now (a D-pad step, a click) takes CollectAll.
const std::vector<Item>& CollectAllCached();

// True for scrolling list controls (navigated row by row).
bool IsList(uintptr_t ctl);
// Row height and first-row center (screen pixels) of a list control; false if not a list.
bool ListRows(uintptr_t ctl, float& rowH, float& firstRowY);
// The row under the game's cursor as the list itself holds it (+0x7C on both list classes: the data index its click
// handler selects, the scroll position included); -1 when none or not a list.
int ListHoveredRow(uintptr_t ctl);

// Visible top-level windows (roots of the control tree).
std::vector<uintptr_t> VisibleRoots();

// One control's screen rectangle by the same arithmetic Collect uses (its parents' +0xA8 chain, or a parent's / its own
// +0x3C frame), whatever its class and state: the passive panels Collect skips (the Options screen's "big box") can be
// measured. False when the control cannot be read or projects behind the camera.
bool ControlRect(uintptr_t ctl, Item& out);
} // namespace ui
