// The pad in menus: D-pad focus navigation, the free cursor, the pad on FLUF's windows.
#include "controller.h"

#include "dimouse.h"
#include "game/events.h"
#include "game/aim.h"
#include "game/flightio.h"
#include "game/pursuit.h"
#include "settings.h"
#include "game/fl.h"
#include "game/ai.h"
#include "game/deckmode.h"
#include "game/flight.h"
#include "game/gameclock.h"
#include "game/geom.h"
#include "game/mission.h" // ForgetGame at a game load (host v34)
#include "game/render.h" // OnDeviceLost: the shared renderer's state block before a Reset
#include "game/room.h"
#include "game/space.h"
#include "game/spawn.h"
#include "log.h"
#include "../shm.h"
#include "device.h"
#include "gamepad.h"
#include "ui/glyphs.h"
#include "haptics.h" // the pad's vibration: events -> effect table -> mixer -> the two motors (IDEAS U30)
// phase D flight modals (the D-FLIGHT track): the goal panel is the Autopilot module's (core\ui\views.h),
#include "picker.h" // the target picker,
#include "wheel.h"  // the action wheel
#include "host.h"
#include "padhud.h"
#include "enabled_gate.h"
#include "game/coreframe.h"
#include "game/gamemode.h"
#include "game/mprules.h"
#include "ui/flufaware.h" // FLUF's windows: keep-outs, the pad on them, the keys' yield (docs\MOD_COMPATIBILITY.md FLUF)
// D-MENUS: the menu screens on the pad, the New Game prompt, speedrun mode (clock, splits, LiveSplit)
// and the HUD timer (padmenus.h / newgame.h; the clock and the timer: modules\speedrun)
#include "game/target_cycle.h"   // the targeting press's enemy rule
#include "newgame.h"
#include "padmenus.h"
#include "padbases.h" // phase D bases / dealers / NeuroNet windows (the D-BASES track),
#include "padmap.h"   // the nav map,
#include "wgroups.h"  // the weapon group editor,
#include "game/hudinfo.h"
#include "game/modslots.h" // the cut modules' slots
#include "promptbar.h" // the shared prompt bar (D-MENUS)
#include "ui/presets.h"
#include "press_gate.h" // press ownership between the pad's consumers (the attended run's B leak)
#include "ui/options_page.h"
#include "ui/ui.h"
#include "ui/uiopt.h"
#include "ui/hud_rows.h"
#include "ui/views.h"

#include <deque>
#include <map>
#include <sstream>

#include "controller_internal.h"

namespace ctl
{
static float g_wheelAccum = 0;
// ---------------------------------------------------------------- UI focus navigation
static float g_navRepeat = 0; // auto-repeat timer for held D-pad directions
static u32 g_navHeldDir = 0;
static float g_snapHold = 0; // keep driving the cursor to a snapped control briefly
std::vector<uintptr_t> g_prevRoots;
// Windows that make up the flight HUD (recorded while flying). When another window opens over
// them (pause menu, nav map, dialogs) they are background: not new, not navigable.
static bool g_navPrimed = false; // roots from the first frame after loading are not "new"

static void SnapTo(const ui::Item& it)
{
    float x = it.x, y = it.y;
    // Enter lists at their first row: the control spans the whole list box, whose
    // middle or bottom rows are often empty.
    float rowH, firstRow;
    if (ui::ListRows(it.ctl, rowH, firstRow))
        y = firstRow;
    dimouse::MoveTo(x, y);
    g_snapHold = 0.12f;
    Log("ui: focus %s (%.0f,%.0f)", it.name, x, y);
}

static bool IsAffirmative(const char* name)
{
    char low[40];
    strncpy_s(low, name, _TRUNCATE);
    _strlwr_s(low);
    return strstr(low, "accept") || strstr(low, "yes") || strstr(low, "okbutton") || strstr(low, "confirm") ||
           strstr(low, "continue") || strstr(low, "loadautosave");
}

void UpdateUiNav(float dt)
{
    if (g_snapHold > 0)
    {
        g_snapHold -= dt;
        if (g_snapHold <= 0)
            dimouse::ReleaseTarget();
    }

    // Full item discovery (control tree + hit-test probing) is not cheap (the probe: 2.5-7 ms at 4K): fresh when the
    // player navigates (a press, or a held direction's repeat step due this frame), else the shared cache that probes
    // again only when the tree's clickable set changed (ui::CollectAllCached). Was: every 50 ms plus "g_navRepeat <=
    // dt", which held from the session's start until its first D-pad step (the timer starts at 0) and after a release
    // that left it under a frame: a full probe every frame in every pad screen, OnFrame 2.5 ms in a room and 7 ms on a
    // dealer at 4K (perf 25 Oct 2026, research\experience\runs\perf_pad_2026-10-25).
    const bool navPressed = g_pad.Pressed(XINPUT_GAMEPAD_DPAD_UP) || g_pad.Pressed(XINPUT_GAMEPAD_DPAD_DOWN) ||
                            g_pad.Pressed(XINPUT_GAMEPAD_DPAD_LEFT) || g_pad.Pressed(XINPUT_GAMEPAD_DPAD_RIGHT);
    const bool repeatDue = g_navHeldDir && g_pad.Down(g_navHeldDir) && g_navRepeat <= dt;
    std::vector<ui::Item> items = navPressed || repeatDue ? ui::CollectAll() : ui::CollectAllCached();
    const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());

    // A window holding keyboard focus (pause menu, save dialog, message boxes...) is modal:
    // keep D-pad focus inside it. So is a menu's modal loop without the keyboard focus (padmenus::ModalRoot: the
    // multiplayer LAN / INTERNET prompt).
    items.erase(std::remove_if(items.begin(), items.end(), [](const ui::Item& it) { return padbases::SkipsFocus(it.name); }), items.end());
    uintptr_t modal = ui::ModalRoot(items);
    if (!modal)
        modal = padmenus::ModalRoot();
    if (modal)
        items.erase(
            std::remove_if(items.begin(), items.end(), [modal](const ui::Item& it) { return it.root != modal; }),
            items.end());

    // In space, a window opened over the flight HUD (pause menu, nav map, load dialog...) takes
    // the D-pad; the HUD behind it is only navigable when nothing else is open.
    auto isHud = [](const ui::Item& it)
    { return std::find(g_hudRoots.begin(), g_hudRoots.end(), it.root) != g_hudRoots.end(); };
    if (g_mode == Mode::SpaceUi &&
        std::any_of(items.begin(), items.end(), [&](const ui::Item& it) { return !isHud(it); }))
        items.erase(std::remove_if(items.begin(), items.end(), isHud), items.end());

    // Auto-focus: when new windows appear and the cursor is not already on one of their
    // controls, put it on the most likely choice: an affirmative button (OK / Accept /
    // Continue) if there is one, otherwise the new control closest to the screen center.
    std::vector<uintptr_t> roots;
    for (const ui::Item& it : items)
        if (std::find(roots.begin(), roots.end(), it.root) == roots.end())
            roots.push_back(it.root);
    auto isNew = [](uintptr_t r) { return std::find(g_prevRoots.begin(), g_prevRoots.end(), r) == g_prevRoots.end(); };
    const bool anyNew = g_navPrimed && std::any_of(roots.begin(), roots.end(), isNew);
    // The left stick moves the cursor this frame: the player points, no auto-focus (the owner's attended run 26 Oct 2026:
    // the stick's cursor move zeroed the snap's hold every frame, so the new window stayed "new" and the snap fired again
    // each frame - `ui: focus ManeuverCruiseButton` ten times in 50 ms, the cursor pulled back against the stick)
    const PadState& ps = g_pad.Cur();
    const bool stickPoints = (ps.lx != 0 || ps.ly != 0) && padbases::CursorAllowed();
    // (While a snap is still travelling to its target the cursor position is stale.)
    if (anyNew && g_snapHold <= 0 && !stickPoints && !padbases::EntryFocus())
    {
        const int under = ui::FindAt(items, mx, my);
        if (under < 0 || !isNew(items[under].root))
        {
            const float cx = fl::ScreenWidth() * 0.5f, cy = fl::ScreenHeight() * 0.5f;
            int best = -1;
            float bestD = 1e30f;
            for (size_t i = 0; i < items.size(); ++i)
            {
                if (!isNew(items[i].root))
                    continue;
                float d = (items[i].x - cx) * (items[i].x - cx) + (items[i].y - cy) * (items[i].y - cy);
                if (IsAffirmative(items[i].name))
                    d *= 0.01f;
                if (d < bestD)
                {
                    bestD = d;
                    best = static_cast<int>(i);
                }
            }
            if (best >= 0)
                SnapTo(items[best]);
        }
    }
    if (!(anyNew && g_snapHold > 0) || stickPoints) // keep new windows "new" until we could look at them
        g_prevRoots = roots;
    g_navPrimed = true;

    // D-pad moves focus between clickable controls, with auto-repeat when held.
    u32 dir = 0;
    const u32 dirs[4] = {XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT,
                         XINPUT_GAMEPAD_DPAD_RIGHT};
    for (u32 d : dirs)
    {
        if (g_pad.Pressed(d) && !padbases::Owns(d)) // (D-BASES: the bar's character strip / the dealers' side take L / R)
        {
            dir = d;
            g_navHeldDir = d;
            g_navRepeat = 0.4f;
        }
    }
    if (!dir && g_navHeldDir && g_pad.Down(g_navHeldDir))
    {
        g_navRepeat -= dt;
        if (g_navRepeat <= 0)
        {
            dir = g_navHeldDir;
            g_navRepeat = 0.11f;
        }
    }
    if (dir)
    {
        float dx = 0, dy = 0;
        if (dir == XINPUT_GAMEPAD_DPAD_UP)
            dy = -1;
        if (dir == XINPUT_GAMEPAD_DPAD_DOWN)
            dy = 1;
        if (dir == XINPUT_GAMEPAD_DPAD_LEFT)
            dx = -1;
        if (dir == XINPUT_GAMEPAD_DPAD_RIGHT)
            dx = 1;
        const int cur = ui::FindAt(items, mx, my);
        // Inside a list, up/down step row by row until the list's edge.
        float rowH, firstRow;
        if (cur >= 0 && dy != 0 && ui::ListRows(items[cur].ctl, rowH, firstRow))
        {
            // Snap to row centers so repeated steps cannot drift: the row the cursor is on comes from the list itself
            // (ui::ListHoveredRow, the data index minus the scroll bar's first visible row) when it knows, else from the
            // geometry (the front list's assumed pitch drifted a row after a dozen steps).
            const ui::Item& list = items[cur];
            int row = static_cast<int>(lroundf((my - firstRow) / rowH));
            const int hovered = ui::ListHoveredRow(list.ctl);
            if (hovered >= 0)
            {
                const u32 bar = uiclick::FindVisible(exe::VtFrontList.is(Mem<u32>(list.ctl)) ? "CharScroll" : "LoadSave_Scroll");
                const int first = bar && exe::ScrollBarClick.is(reinterpret_cast<u32>((*reinterpret_cast<void***>(bar))[0x4C / 4]))
                                      ? static_cast<int>(Mem<float>(bar + 0x3C8) + 0.5f)
                                      : 0;
                if (hovered >= first)
                    row = hovered - first;
            }
            row += static_cast<int>(dy);
            const float ny = firstRow + row * rowH;
            if (row >= 0 && ny < list.y + list.hh)
            {
                dimouse::MoveTo(mx, ny);
                g_snapHold = 0.12f;
                return;
            }
        }
        const uintptr_t exclude = cur >= 0 ? items[cur].ctl : 0;
        int next = ui::FindInDirection(items, mx, my, dx, dy, exclude);
        // Lists made of row controls with their own scroll bar (the dealers' ItemButton/ItemView rows + Inventory_Scroll,
        // all children of the dealer list): up/down past the edge row scrolls the list one row, natively
        // (uiclick::Scroll), and the cursor stays on the edge row, which now shows the next item; once the bar stops
        // moving, focus leaves the list as usual. Was: the D-pad could not reach dealer rows below the first five.
        if (cur >= 0 && dy != 0)
        {
            const u32 parent = Mem<u32>(items[cur].ctl + 4);
            const auto sibling = [&](int i) { return i >= 0 && Mem<u32>(items[i].ctl + 4) == parent; };
            const auto isBar = [&](int i)
            {
                return exe::ScrollBarClick.is(
                    reinterpret_cast<u32>((*reinterpret_cast<void***>(items[i].ctl))[0x4C / 4]));
            };
            int bar = -1;
            for (int i = 0; parent && i < static_cast<int>(items.size()) && bar < 0; ++i)
                if (i != cur && sibling(i) && isBar(i))
                    bar = i;
            // the edge row: no row of this list (a sibling that is not the bar) lies further in that direction (was: the
            // control spatial navigation picked next - going up that could be another control, and Up scrolled a
            // row early)
            bool edge = bar >= 0 && !isBar(cur);
            for (int i = 0; edge && i < static_cast<int>(items.size()); ++i)
                if (i != cur && sibling(i) && !isBar(i) && (items[i].y - my) * dy > 4.0f)
                    edge = false;
            if (edge)
            {
                const float before = Mem<float>(items[bar].ctl + 0x3C8);
                if (uiclick::Scroll(items[bar].ctl, dy, false, nullptr, nullptr) != before)
                    return;
            }
        }
        if (next < 0 && cur < 0 && !items.empty())
            next = 0; // nothing focused yet: start somewhere
        if (next >= 0)
        {
            SnapTo(items[next]);
        }
    }
}

// ---------------------------------------------------------------- free cursor (menus, bases, windows)
void UpdateFreeCursor(float dt)
{
    bool in[glyphs::COUNT];
    PadInputs(in, g_menuPad);
    g_gateUi.Take(in); // a press another consumer owns (flight's held A, the B that closed a pad menu) is no click here
    const PadState& p = g_pad.Cur();
    const float mag = in[glyphs::LS] ? sqrtf(p.lx * p.lx + p.ly * p.ly) : 0.0f;
    if (mag > 0 && padbases::CursorAllowed()) // (D-BASES: pad_cursor 0 = focus only)
    {
        // Quadratic response: precise near the center, fast at full deflection.
        const float speed = g_opt.cursorSpeed * fl::ScreenHeight() * mag * mag * (padmap::IsOpen() ? padmap::kCursorScale : 1.0f); // (the full-screen map is a longer way across than a menu)
        float dx = p.lx / mag * speed * dt, dy = -p.ly / mag * speed * dt;
        padbases::ShapeCursor(dx, dy); // cursor_magnet: slows over and pulls toward the nearest clickable element
        dimouse::MoveBy(dx, dy);
        g_snapHold = 0;
    }

    // (D-BASES: a button the pad screen consumes this frame is not also a mouse button / the Escape key)
    dimouse::SetButton(0, in[glyphs::A] && !padbases::Owns(XINPUT_GAMEPAD_A));
    dimouse::SetButton(1, in[glyphs::X] && !padbases::Owns(XINPUT_GAMEPAD_X));

    // Right stick vertical scrolls lists (on the map it pans).
    if (fabsf(p.ry) > 0 && in[glyphs::RS] && !padmap::OwnsRightStick())
    {
        g_wheelAccum += p.ry * g_opt.wheelRate * dt;
        const int notches = static_cast<int>(g_wheelAccum);
        if (notches)
        {
            dimouse::Wheel(notches);
            g_wheelAccum -= notches;
        }
    }
    else
        g_wheelAccum = 0;

    if (g_menuPad.Pressed(XINPUT_GAMEPAD_B) && !padbases::Owns(XINPUT_GAMEPAD_B))
        fl::TapKey(VK_ESCAPE);
}

// ---------------------------------------------------------------- FLUF's windows on the pad (flufaware.h)
// FLUF's ImGui reads the game's own cursor position and buttons (fluf.md "FLUF in play" B), so the pad drives its
// windows the way it drives the game's: the left stick moves the cursor (whatever pad_cursor says: FLUF's windows have no
// D-pad focus), A / X are the mouse buttons, the right stick the wheel, B the Escape key (FLUF closes its topmost window
// on it; the palette clears its text first). The palette is a list under a text box: the D-pad steps through it and A
// runs the selected command (its own keys, sent through the game's key handler, which FLUF feeds to ImGui); typing stays
// with the keyboard. The prompt bar says what the buttons do. Nothing of FLvibed's flight / menu dispatch runs meanwhile.
static bool g_flufPad = false;
// B on a window FLUF's Escape does not reach (a command's or a module's ImGui window: ImGui::Begin with a close button;
// FLUF's UiManager::ProcessEscapeKey closes only its own FlWindows - the owner's attended run 26 Oct 2026, a palette
// command's window stayed open under B): the game's cursor FLUF reads goes to the window's close button, the left button
// goes down and up there (ImGui's CloseButton acts on the release over it), the cursor stays. Every step capped in frames.
struct FlufClose
{
    int step = 0; // 0 idle, 1 moving, 2 down, 3 up (waiting for the window to go)
    int frames = 0;
    float x = 0, y = 0;
    std::string name;
};
static FlufClose g_flufClose;
static void FlufCloseStep()
{
    FlufClose& c = g_flufClose;
    if (!c.step)
        return;
    ++c.frames;
    switch (c.step)
    {
    case 1: // the cursor on the button (exact at the game's next mouse poll; ImGui sees it at FLUF's next frame)
        dimouse::MoveTo(c.x, c.y);
        if ((fabsf(fl::MouseX() - c.x) < 2 && fabsf(fl::MouseY() - c.y) < 2 && c.frames >= 2) || c.frames > 30)
            c.step = 2, c.frames = 0;
        break;
    case 2: // held two frames (one FLUF ImGui frame sees it down)
        dimouse::SetButton(0, true);
        if (c.frames >= 3)
            c.step = 3, c.frames = 0;
        break;
    case 3:
        dimouse::SetButton(0, false);
        if (!flufaware::WindowOpen(c.name) || c.frames > 30)
        {
            const bool closed = !flufaware::WindowOpen(c.name);
            Log("fluf: B closed '%s' by its close button: %s", c.name.c_str(), closed ? "closed" : "still open");
            events::Push("fluf close %s %s", c.name.c_str(), closed ? "ok" : "failed");
            dimouse::ReleaseTarget();
            c = FlufClose();
        }
        break;
    }
}
void UpdateFlufPad(float dt)
{
    g_flufPad = true;
    bool in[glyphs::COUNT];
    PadInputs(in, g_menuPad);
    g_gateFluf.Take(in); // a press flight / a popup owns (A held for formation, the stick flying) is not FLUF's
    if (g_flufClose.step) // a close by the button runs: the pad waits for it (a few frames)
    {
        FlufCloseStep();
        return;
    }
    const PadState& p = g_pad.Cur();
    const float mag = in[glyphs::LS] ? sqrtf(p.lx * p.lx + p.ly * p.ly) : 0.0f;
    if (mag > 0)
    {
        const float speed = g_opt.cursorSpeed * fl::ScreenHeight() * mag * mag;
        dimouse::MoveBy(p.lx / mag * speed * dt, -p.ly / mag * speed * dt);
    }
    const bool palette = flufaware::PaletteOpen() && flufaware::TextInput();
    if (palette)
    {
        if (g_pad.Pressed(XINPUT_GAMEPAD_DPAD_UP))
            fl::TapKey(VK_UP);
        if (g_pad.Pressed(XINPUT_GAMEPAD_DPAD_DOWN))
            fl::TapKey(VK_DOWN);
        if (g_menuPad.Pressed(XINPUT_GAMEPAD_A))
            fl::TapKey(VK_RETURN);
        dimouse::SetButton(0, false);
    }
    else
        dimouse::SetButton(0, in[glyphs::A]);
    dimouse::SetButton(1, in[glyphs::X]);
    if (fabsf(p.ry) > 0 && in[glyphs::RS])
    {
        g_wheelAccum += p.ry * g_opt.wheelRate * dt;
        const int notches = static_cast<int>(g_wheelAccum);
        if (notches)
        {
            dimouse::Wheel(notches);
            g_wheelAccum -= notches;
        }
    }
    else
        g_wheelAccum = 0;
    if (g_menuPad.Pressed(XINPUT_GAMEPAD_B))
    {
        bool byEscape = true;
        float fx = -1, fy = -1;
        std::string name;
        if (flufaware::CloseTarget(byEscape, fx, fy, name) && !byEscape)
        {
            g_flufClose.step = 1, g_flufClose.frames = 0;
            g_flufClose.x = fx * fl::ScreenWidth(), g_flufClose.y = fy * fl::ScreenHeight();
            g_flufClose.name = name;
            dimouse::SetButton(0, false);
            FlufCloseStep();
        }
        else
            fl::TapKey(VK_ESCAPE); // FLUF's own windows: its Escape closes the topmost (the palette clears first)
    }
    if (palette)
    {
        const promptbar::Prompt items[] = {{glyphs::A, L"RUN"}, {glyphs::DU, L"SELECT", false, glyphs::DD},
                                           {glyphs::B, L"CLEAR / CLOSE"}};
        promptbar::Set("fluf.palette", items, 3, true, 10);
    }
    else
    {
        const promptbar::Prompt items[] = {{glyphs::A, L"CLICK"}, {glyphs::LS, L"CURSOR"}, {glyphs::B, L"CLOSE"}};
        promptbar::Set("fluf.window", items, 3, true, 10);
    }
}

// The pad leaves FLUF's windows: no mouse button stays down.
void EndFlufPad()
{
    if (!g_flufPad)
        return;
    g_flufPad = false;
    dimouse::SetButton(0, false);
    dimouse::SetButton(1, false);
    if (g_flufClose.step) // the window went (its close took): the sequence ends here
    {
        Log("fluf: B closed '%s' by its close button: closed", g_flufClose.name.c_str());
        events::Push("fluf close %s ok", g_flufClose.name.c_str());
        dimouse::ReleaseTarget();
        g_flufClose = FlufClose();
    }
}
} // namespace ctl
