// The HUD module's entry: its table for the logic's slots (core\game\modslots.h) and its places in core's frame (core\ui\views.h HudView), published at Attach.
// Off, every place is empty: the game's own HUD, the classic look, and what the pad's prompts need from core\ui\winplates.*.
#include "game/hostcalls.h"
#include "game/hudwake.h"
#include "game/modslots.h"
#include "gametext.h"
#include "hudvis.h"
#include "hud2.h"
#include "reticlecue.h"
#include "shieldcue.h"
#include "ui/flufaware.h"
#include "ui/hud_rows.h"
#include "ui/options_page.h"
#include "ui/uiopt.h"
#include "ui/views.h"
#include "uiskin.h"
#include "uistrip.h"
#include "windowtext.h"

#include <sstream>

static void Cues(float dt, bool flying, bool autopilot)
{
    shieldcue::Update(dt, flying);
    reticlecue::Update(dt, flying, autopilot);
}

static void GameText(bool menu) { gametext::Frame(hudrows::g.gameTextSize, menu); }

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
    const bool activity = padOwns ? !known
                                  : !known || mx != mxWas || my != myWas || b != 0 || b != bWas || Mem<u8>(exe::MouseFlightEngaged) != 0;
    mxWas = mx, myWas = my, bWas = b;
    // (the fail-safe: every non-flight state shows it the same frame - the pause menu by the game's own paused flag too, a mode change at once, before the
    // mode's 3-frame debounce settles)
    const bool eligible = flyingFree && hud2::CursorIdleEligible() && onlyHudOpen() && !options_page::IsOpen() && !flufaware::InputWindowOpen();
    const int fade = uiopt::Get().hudFade <= 0 ? 0 : 1;
    const bool hide = hudwake::CursorStep(cs, fade, eligible, activity, dt);
    hostcalls::CursorHide(hostcalls::kCursorHideIdle, hide);
    hud2::NoteCursorHidden(hide);
}

static void Skin(float dt, bool padOwns, bool enabled, bool docked)
{
    const hudrows::Values& h = hudrows::g;
    const int uiStyle = h.hudStyle != 0 ? h.uiStyle : 0;
    uiskin::Context sc;
    sc.style = uiStyle;
    sc.pad = padOwns;
    sc.enabled = enabled;
    uiskin::Update(dt, sc);
    uistrip::Update(dt, enabled && uiStyle == 1 && docked);
    windowtext::Frame(h.windowText && enabled && uiStyle == 1 && h.gameTextSize == 100);
}

static bool UnderBegin(void* device, bool flying)
{
    const bool menuOver = flying && uiskin::ModalOpen();
    hud2::SetMenuOver(menuOver);
    // the native HUD switch and the game's 3D views in HUD 2's windows: before the UI pass draws the controls (any mode: restores)
    hud2::RenderUnderHud(device);
    return menuOver;
}

static void UnderEnd(void* device, bool shieldCue)
{
    uiskin::RenderUnder(device);
    if (shieldCue)
        shieldcue::Render(device);
}

static void Overlay(void* device, bool menuOver, bool noUnderPass)
{
    if (noUnderPass)
        shieldcue::Render(device);
    if (!menuOver)
        reticlecue::Render(device);
    hud2::Render(device); // HUD 2 (its panels; the flight modals draw over it)
}

static const std::vector<views::HudRect>* StripDrawn() { return &uistrip::Drawn(); }

static void Attach()
{
    LogJoin(hostcalls::g.LogWrite);
    using modslots::kHud;
#define G(fn) modslots::Guard<kHud, fn>::Call
    views::skin = {G(uiskin::On)};
    views::hud = {G(hud2::TargetCardAnchor), G(hud2::BottomPanelsTop), G(uistrip::StripRight), G(uistrip::StripBottom), G(hud2::Active), G(hud2::RingRadius),
                  G(hud2::CycleListFilter), G(hud2::LockNotice), G(hud2::NoteAction), G(hud2::OnHotspot), G(uistrip::Active), G(StripDrawn), G(uiskin::DrawOver), G(Cues), G(GameText), G(hud2::Update),
                  G(Cursor), G(Skin), G(UnderBegin), G(UnderEnd), G(Overlay), G(uistrip::Draw), G(hud2::OnDeviceLost)};
#undef G
}

static void Detach()
{
    views::skin = {};
    views::hud = {};
}

// Render.cpp's D3D textures go with the logic: every owner frees its own, and the game's panels, windows and colours go back.
static void Shutdown()
{
    shieldcue::Shutdown();
    reticlecue::Shutdown();
    hud2::Shutdown(); // the game's panels back, the textures freed
    uiskin::Shutdown(); // the windows' art and colours back
    uistrip::Shutdown();
}

// A fault: the game's panels first, the cursor and the click box; the windows' art after (its list is on the heap), and the state blocks, which would fail the
// next device Reset under d3d8to9: a stopped module no longer hears the device.
static void Fault(int phase)
{
    if (phase == 0)
    {
        hudvis::GiveBack();
        hostcalls::CursorHide(hostcalls::kCursorHideIdle, false);
        if (hostcalls::g.SetHudHotspots)
            hostcalls::g.SetHudHotspots(nullptr, 0);
    }
    else
    {
        uiskin::GiveBack();
        hud2::OnDeviceLost();
        windowtext::Frame(false); // every text's own font and box; game_text_size's table stays until the game reads it again
    }
}

// Its debug verbs.
static std::string Verb(const std::string& op, std::istream& in)
{
    if (op == "cue") // cue status | cue hit|break|hull [damage] [munition arch] | cue dir <screen deg>|off: shield cue
    {
        std::string what;
        in >> what;
        if (what == "hullpts") // test aid: the damage edge acts on these hull points until `cue hullpts off`
        {
            std::string v;
            in >> v;
            return shieldcue::TestHull(v == "off" ? -1.0f : static_cast<float>(std::atof(v.c_str())));
        }
        if (what == "dir") // test aid: hits come from this screen direction (0 = right, 90 = down) until `cue dir off`
        {
            std::string v;
            in >> v;
            return shieldcue::TestDir(v != "off", v == "off" ? 0.0f : static_cast<float>(std::atof(v.c_str())));
        }
        float dmg = 50;
        u32 mun = 0;
        in >> dmg >> mun;
        if (what == "hit")
            return shieldcue::Test(0, dmg, mun);
        if (what == "break")
            return shieldcue::Test(1, dmg, mun);
        if (what == "hull")
            return shieldcue::Test(2, dmg, mun);
        if (what == "ring") // cue ring 0..3: engage, hand-back, nanobots, batteries
        {
            reticlecue::Pulse(static_cast<reticlecue::Kind>(std::clamp(static_cast<int>(dmg), 0, 3)));
            return reticlecue::Status();
        }
        return shieldcue::Status() + "\n" + reticlecue::Status();
    }
    // ---- HUD 2 (the HUD2 track): hud2 on|off|auto|state|sim <combat|travel|cruise|dock|lane|off>
    if (op == "hud2")
        return hud2::Command(in);
    // ---- Extended HUD 2 (the SKIN track): uiskin state | windows | restore | rescan
    if (op == "uiskin")
        return uiskin::Command(in);
    if (op == "uistrip") // Extended HUD 2's docked overlay (the STRIP track): uistrip [state] | data | research ... | hover <i>
        return uistrip::Command(in);
    if (op == "gametext") // gametext [rich <percent>]: the size as applied (gametext.h); rich: that table alone, now (research)
    {
        std::string what;
        int pct = 100;
        in >> what >> pct;
        if (what == "rich")
            gametext::RichNow(pct);
        if (what == "window")
            return windowtext::Status();
        if (what == "texts")
            return windowtext::Texts();
        return gametext::Status();
    }
    return std::string();
}

static bool Command(const std::string& op, std::istream& in, std::string& reply)
{
    if (op != "hud2" && op != "uiskin" && op != "uistrip" && op != "gametext" && op != "cue")
        return false;
    reply = Verb(op, in);
    return true;
}

static const modslots::Module kModule = {nullptr, Shutdown, nullptr, nullptr, Command, nullptr, Attach, Detach, Fault};

extern "C" const modslots::Module* flv_hud_module() { return &kModule; }
