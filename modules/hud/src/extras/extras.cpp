// The extras that run under both interfaces (extras.h): their places in core's frame and passes, their give-back and their verbs.
#include "extras.h"

#include "dimouse.h"
#include "game/fl.h"
#include "game/hostcalls.h"
#include "game/hudsize.h"
#include "game/hudwake.h"
#include "game/modslots.h"
#include "gametext.h"
#include "reticlecue.h"
#include "shieldcue.h"
#include "ui/flufaware.h"
#include "ui/hud_rows.h"
#include "ui/options_page.h"
#include "ui/ui.h"
#include "ui/uiopt.h"
#include "ui/views.h"

#include <sstream>

namespace extras
{
static CursorFeed g_feed = {};
void SetCursorFeed(const CursorFeed& feed) { g_feed = feed; }

// The classic HUD is selected: HUD 2 sets the feed only while it is (the door's Enter to its Leave).
static bool ClassicHud() { return !g_feed.eligible; }
// The cues run: always with HUD 2 (each by its own switch), on the classic HUD only with classic_cues (the owner's answer to card Q4 of audit 102_:
// "Keep, off by default"). Off, they run as out of flight: nothing kept, nothing drawn, a fresh read when they come back.
static bool CuesHere() { return !ClassicHud() || hudrows::classicCues; }

static void Cues(float dt, bool flying, bool autopilot)
{
    const bool here = CuesHere();
    shieldcue::Update(dt, flying && here);
    reticlecue::Update(dt, flying && here, autopilot);
}

static int GameTextPercent() { return hudsize::GameTextPercent(hudrows::g.gameTextSize, hudsize::g_preset, ClassicHud()); }
static void GameText(bool menu) { gametext::Frame(GameTextPercent(), menu); }

static struct
{
    bool classic, eligible, hidden;
    int mx, my;
} g_idle = {}; // the last frame's idle cursor, for the verb `idlecursor`

// The game's mouse cursor under hud_fade 1 (core\game\hudwake.h CursorStep): in flight with the keyboard and mouse it hides after 3 s without mouse movement
// or a button and shows again the frame either comes; shown in every other context (a menu, the map, a dialog / popup = any non-HUD window, the options page,
// a FLUF window, the pad's cursor, mouse flight engaged) and whenever a symbol it reads is unresolved.
static void Cursor(float dt, bool padOwns, bool flyingFree, bool (*onlyHudOpen)())
{
    static hudwake::CursorState cs;
    static int mxWas = -1, myWas = -1;
    static u32 bWas = 0;
    const bool known = exe::MouseX.ok() && exe::MouseY.ok() && exe::MouseButtons.ok() && exe::MouseFlightEngaged.ok();
    const int mx = known ? fl::MouseX() : 0, my = known ? fl::MouseY() : 0;
    const u32 b = known ? (fl::MouseButtons() & 7u) : 0;
    // the pad in control: its own aim moves the game's cursor and its steering engages mouse flight, so neither counts - a real mouse hands the device back to
    // the keyboard and mouse (padOwns false: the K&M rule at once)
    // ... and with the pad the cursor is its aim: never idle-hidden (the owner, 7 Nov 2026: it vanished with the target centred, the stick at rest 3 s)
    const bool activity = padOwns ? true
                                  : !known || mx != mxWas || my != myWas || b != 0 || b != bWas || Mem<u8>(exe::MouseFlightEngaged) != 0;
    mxWas = mx, myWas = my, bWas = b;
    // (the fail-safe: every non-flight state shows it the same frame - the pause menu by the game's own paused flag too, a mode change at once, before the
    // mode's 3-frame debounce settles)
    // HUD 2 selected: its feed says when its flight HUD rests. The classic HUD (no feed: the owner's decision 62, 13 Nov 2026): the game's HUD does not fade,
    // so free flight with only the HUD up is the rest, but for an in-flight scene (the letterbox; unread: no hide)
    const bool classic = !g_feed.eligible;
    const bool hudRests = classic ? exe::LetterboxAmount.ok() && Mem<float>(exe::LetterboxAmount) <= 0.0f : g_feed.eligible();
    const bool eligible = flyingFree && hudRests && onlyHudOpen() && !options_page::IsOpen() && !flufaware::InputWindowOpen();
    // idle_cursor (CUES AND TEXT, both HUDs) switches it; with HUD 2 hud_fade ALWAYS keeps it shown too (the HUD never rests)
    const int fade = hudrows::idleCursorHide && (classic || uiopt::Get().hudFade > 0) ? 1 : 0;
    const bool hide = hudwake::CursorStep(cs, fade, eligible, activity, dt);
    g_idle = {classic, eligible, hide, mx, my};
    // a Neural Net tab on its way with no window up yet: the game still shows its flight cursor, the reticle's centre mark, over the cover; it goes until the
    // window takes the mouse (the game's arrow)
    const bool cover = g_feed.cover && g_feed.cover() && _stricmp(fl::CursorName(), "arrow") != 0;
    hostcalls::CursorHide(hostcalls::kCursorHideIdle, hide || cover);
    if (g_feed.hidden)
        g_feed.hidden(hide);
}

// The game's hover help - the mouse-over label (Rollover: "Rogues - Ken Norris" beside the cursor) and the mouse-over box (ToolTip: "NeuroNet", "Police
// Base / Fort Bush") - only for an intentional hover: in flight after a real mouse move (motion, wheel, buttons: the host's physical-mouse tick) within the
// last 3 s, not for a cursor resting on a HUD button while the stick, the autopilot or nothing flies. The NeuroNet windows, the bases, the menus, a modal
// menu over the flight and the paused game (the pause / load / save menus) keep the game's own. Otherwise, at the UI pass's entry (after the game's update
// started it, before anything draws), the shown help is hidden the game's own way (ui::HideHoverHelp, its shown and update bits: its hover timer starts
// over instead of showing it again the next frame): it never reaches the screen, and the game shows it again on the next real hover. HUD 2 on both
// devices; the classic HUD with the pad only (core\ui\hud_door.cpp UnderBegin: with keyboard and mouse the classic HUD keeps the game's own).
static u32 g_hoverHides = 0;
static bool g_hoverAllowed = true;
static bool g_hoverAid = false; // `hoverhelp hover <x> <y>`: the game's hover at that pixel each frame (a background game computes none)
static float g_hoverX = 0, g_hoverY = 0;
void HoverHelp(bool space, bool menuOver)
{
    if (g_hoverAid)
        ui::HitTest(g_hoverX, g_hoverY, false);
    const u32 mt = dimouse::LastRealTick();
    g_hoverAllowed = !space || menuOver || fl::GamePaused() || (mt != 0 && GetTickCount() - mt < 3000);
    if (!g_hoverAllowed)
        g_hoverHides += static_cast<u32>(ui::HideHoverHelp(3));
}

// Under the game's UI pass, after HUD 2's glass: the shield cue (screen-edge effects under the HUD).
static void Under(void* device, bool shieldCue)
{
    if (shieldCue && CuesHere())
        shieldcue::Render(device);
}

// Over the flight, before HUD 2's panels: the shield cue when the under pass did not run, the reticle cue unless a modal menu holds the flight HUD.
static void Over(void* device, bool menuOver, bool noUnderPass)
{
    if (!CuesHere())
        return;
    if (noUnderPass)
        shieldcue::Render(device);
    if (!menuOver)
        reticlecue::Render(device);
}

void Attach()
{
    using modslots::kHud;
#define G(fn) modslots::Guard<kHud, fn>::Call
    views::extras = {G(Cues), G(GameText), G(Cursor), G(HoverHelp), G(Under), G(Over)};
#undef G
}

void Detach() { views::extras = {}; }

void Shutdown()
{
    shieldcue::Shutdown();
    reticlecue::Shutdown();
}

void Fault(int phase)
{
    if (phase == 0)
        hostcalls::CursorHide(hostcalls::kCursorHideIdle, false);
}

bool Command(const std::string& op, std::istream& in, std::string& reply)
{
    if (op == "cue") // cue status | cue hit|break|hull [damage] [munition arch] | cue dir <screen deg>|off | cue hullpts <pts>|off | cue ring 0..3
    {
        std::string what;
        in >> what;
        if (what == "hullpts") // test aid: the damage edge acts on these hull points until `cue hullpts off`
        {
            std::string v;
            in >> v;
            reply = shieldcue::TestHull(v == "off" ? -1.0f : static_cast<float>(std::atof(v.c_str())));
            return true;
        }
        if (what == "dir") // test aid: hits come from this screen direction (0 = right, 90 = down) until `cue dir off`
        {
            std::string v;
            in >> v;
            reply = shieldcue::TestDir(v != "off", v == "off" ? 0.0f : static_cast<float>(std::atof(v.c_str())));
            return true;
        }
        float dmg = 50;
        u32 mun = 0;
        in >> dmg >> mun;
        if (what == "hit")
            reply = shieldcue::Test(0, dmg, mun);
        else if (what == "break")
            reply = shieldcue::Test(1, dmg, mun);
        else if (what == "hull")
            reply = shieldcue::Test(2, dmg, mun);
        else if (what == "ring") // cue ring 0..3: engage, hand-back, nanobots, batteries
        {
            reticlecue::Pulse(static_cast<reticlecue::Kind>(std::clamp(static_cast<int>(dmg), 0, 3)));
            reply = reticlecue::Status();
        }
        else
            reply = shieldcue::Status() + "\n" + reticlecue::Status();
        // (where the cues run: HUD 2, or the classic HUD with classic_cues; the game text's percent as the extras apply it)
        reply += std::string("\nextras hud=") + (ClassicHud() ? "classic" : "hud2") + " classic_cues=" + (hudrows::classicCues ? "1" : "0") +
                 " cues=" + (CuesHere() ? "run" : "off") + " game_text=" + std::to_string(GameTextPercent());
        return true;
    }
    if (op == "idlecursor") // idlecursor: the idle cursor's hide as the last frame decided it
    {
        std::ostringstream o;
        o << "idlecursor rule=" << (g_idle.classic ? "classic" : "hud2") << " eligible=" << (g_idle.eligible ? 1 : 0) << " hidden=" << (g_idle.hidden ? 1 : 0)
          << " hud_fade=" << uiopt::Get().hudFade << " idle_cursor=" << (hudrows::idleCursorHide ? 1 : 0) << " mouse=" << g_idle.mx << "," << g_idle.my
          << " cursor=" << fl::CursorName()
          << " hide=" << hostcalls::g_cursorHide; // (the game's cursor by name, and who hides it: hostcalls kCursorHide* bits)
        reply = o.str();
        return true;
    }
    if (op == "hoverhelp") // hoverhelp [state] | hover <x> <y> | hover off (TEST ONLY: the game's hover at a pixel each frame)
    {
        std::string what, x;
        in >> what >> x;
        if (what == "hover")
        {
            g_hoverAid = x != "off" && !x.empty();
            if (g_hoverAid)
            {
                float y = 0;
                in >> y;
                g_hoverX = static_cast<float>(std::atof(x.c_str())), g_hoverY = y;
            }
        }
        std::ostringstream o;
        o << "hoverhelp " << (g_hoverAllowed ? "allowed" : "suppressed") << " hides=" << g_hoverHides << " aid=" << (g_hoverAid ? 1 : 0);
        if (g_hoverAid)
            o << " at=" << g_hoverX << "," << g_hoverY;
        reply = o.str();
        return true;
    }
    return false;
}

std::string GameTextVerb(const std::string& what, int pct)
{
    if (what == "rich")
        gametext::RichNow(pct);
    return gametext::Status();
}
} // namespace extras
