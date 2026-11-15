// HUD 2's entry in the HUD module (hud2_entry.h): its places in core's frame and passes, published between the door's Enter and Leave, its one give-back list
// and its verbs.
#include "hud2_entry.h"

#include "extras/extras.h"
#include "game/hostcalls.h"
#include "game/modslots.h"
#include "hudvis.h"
#include "hud2.h"
#include "modelprobe.h"
#include "modelslot.h"
#include "npcicons.h"
#include "mapcells.h"
#include "navnew.h"
#include "navskin.h"
#include "basepage.h"
#include "nnpage.h"
#include "pages/pagerun.h"
#include "nnstrip.h"
#include "skindialog.h"
#include "skinlists.h"
#include "skinload.h"
#include "skinmenu.h"
#include "skinover.h"
#include "skinchip.h"
#include "ui/hud_door.h"
#include "ui/hud_rows.h"
#include "ui/views.h"
#include "uiskin.h"
#include "uistrip.h"
#include "windowtext.h"
#include "ui/winplates.h"

#include <sstream>

namespace hud2entry
{
// HUD 2's frame. A Neural Net tab open or on its way (nnstrip::Wanted: the game's window manager holds one, the scrim and the page's cover stand) is a
// window over the flight, also where the frame's mode reads flight: between two tabs (the old window's close, the new one's open) and on a first open no
// window root is shown yet, and the reticle's ring and mark faded in over the cover and stood on the next page's first frames (issue 62's gap).
static void Flight(float dt, const views::HudContext& c)
{
    if (c.space && nnstrip::Wanted())
    {
        views::HudContext window = c;
        window.space = false, window.spaceUi = true;
        hud2::Update(dt, window);
        return;
    }
    hud2::Update(dt, c);
}

static void Skin(float dt, bool padOwns, bool enabled, bool docked)
{
    const hudrows::Values& h = hudrows::g;
    // HUD 2's look over the menus and rooms comes with HUD 2 (ui_style 0, the game's own menus inside HUD 2, retired 13 Nov 2026: audit 113_); the
    // classic HUD's frames never reach here, but a frame of a leave can
    const bool skin = h.hudStyle != 0;
    uiskin::Context sc;
    sc.skin = skin;
    sc.pad = padOwns;
    sc.enabled = enabled;
    {
        perf::Scope ps("hud.uiskinUpdate");
        uiskin::Update(dt, sc);
    }
    skinmenu::Frame(dt, !padOwns);
    nnstrip::Update(dt, padOwns);
    {
        perf::Scope ps("hud.nnpage");
        nnpage::Update(dt, padOwns);
    }
    {
        perf::Scope ps("hud.basepage");
        basepage::Update(dt, padOwns);
    }
    {
        perf::Scope ps("hud.uistrip");
        uistrip::Update(dt, enabled && skin && docked);
    }
    skinlists::Frame(enabled && skin); // the save lists: the rows that fit their panel at HUD 2's text size
    winplates::SetMapOver(enabled && skin && navnew::Grown());
    // a NeuroNet page places the game's card through it whatever window_text says: with it off the card stood at the game's place, its title and X over the
    // page and the faction names gone (Shockwave's Steam Deck, 7 Nov 2026)
    perf::Scope ps("hud.windowtext");
    windowtext::Frame((h.windowText || pagerun::Standing()) && enabled && skin);
}

static bool UnderBegin(void* device, bool flying)
{
    const bool menuOver = flying && uiskin::ModalOpen();
    hud2::SetMenuOver(menuOver);
    // the native HUD switch and the game's 3D views in HUD 2's windows: before the UI pass draws the controls (any mode: restores)
    hud2::RenderUnderHud(device);
    windowtext::ApplyCuts();
    nnpage::Under(); // (after the game's update, before its UI pass: what the game shows again each frame)
    uiskin::TipUnder(); // (the same for the room character's name card)
    modelprobe::Ask();  // the model probe's squares (a test verb; nothing unless it was asked)
    modelslot::Pass();  // the 3D models the pages and the probe asked for, under the UI (nothing when none was)
    return menuOver;
}

// The under pass's end: the skin's glass (the extras' shield cue follows it, through the door).
static void UnderEnd(void* device) { uiskin::RenderUnder(device); }

// The overlay: HUD 2's panels (the extras' cues come first, through the door; the flight modals draw over it).
static void Overlay(void* device) { hud2::Render(device); }

static const std::vector<views::HudRect>* StripDrawn() { return &uistrip::Drawn(); }

// Before the prompt lane: the docked overlay, then the dialogs over the game's UI, the lane staying above them
static void Strip(void* device)
{
    uistrip::Draw(device);
    uiskin::DrawMap(device);
    uiskin::DrawModal(device);
}

// Last of the overlay
static void Top(void* device)
{
    uiskin::GiveHelpBack(); // (after the game's UI pass: its hover help was hidden for that pass alone)
    skinover::Draw(device, uiskin::On(), uiskin::PadOwns());
}

// One click box: the strip and the map's chips hold it while a NeuroNet window stands, HUD 2's keyboard form otherwise.
static void Hotspot(int slot)
{
    if (!skinchip::OnClick(slot))
        hud2::OnHotspot(slot);
}

// HUD 2 selected (the door's Enter, core\ui\hud_door.h): its places in core's frame and the idle cursor's feed (a load's frames come through the door: LoadDraw).
// The door enters
// on the first frame hud_style is not 0 and leaves on the first frame it is: in between nothing of this runs on the classic HUD.
static int g_leaveFrames = 0;
static void Enter()
{
    using modslots::kHud;
#define G(fn) modslots::Guard<kHud, fn>::Call
    views::skin = {G(uiskin::On), G(skindialog::ActionWord), G(skindialog::SafeWord), G(skindialog::FocusAction),
                   G(skinmenu::Offered), G(skinmenu::Focus), G(skinmenu::Go)};
    views::hud = {G(hud2::TargetCardAnchor), G(hud2::BottomPanelsTop), G(uistrip::StripRight), G(uistrip::StripBottom), G(hud2::Active), G(hud2::RingRadius),
                  G(hud2::CycleListFilter), G(hud2::LockNotice), G(hud2::NoteAction), G(Hotspot), G(uistrip::Active), G(StripDrawn), G(uiskin::DrawOver), G(Flight),
                  G(Skin), G(UnderBegin), G(UnderEnd), G(Overlay), G(Strip), G(Top), G(nnstrip::Rect),
                  G(navskin::Chips), G(navskin::ChipPress), G(navskin::ChipStep), G(navskin::ChipFocus), G(navskin::ListOpen),
                  G(nnpage::On), G(nnpage::Prompts), G(nnpage::Pad), G(hud2::FlightAnswers)};
    // HUD 2 decides when the flight HUD rests, so when the idle cursor may hide (the extras' Cursor); guarded and counted as the views are
    extras::SetCursorFeed({G(hud2::CursorIdleEligible), G(hud2::NoteCursorHidden), G(nnstrip::Wanted)});
#undef G
    g_leaveFrames = 0;
}

// Its places withdrawn: every view answers as with the module off.
static void Withdraw()
{
    views::skin = {};
    views::hud = {};
    extras::SetCursorFeed({});
}

// The one give-back list: HUD 2 leaving (Leave), the module's stop (Shutdown) and a fault (Fault), so the three cannot drift apart. Phase 0 touches no heap (a
// fault's first step): the game's panels, the cursor, the click box. Phase 1 the rest, in the order the frame gave it back while HUD 2's
// entries still ran under the classic HUD: the pages and the strip, then windowtext's rows (while the windows' controls still stand: a base page's close
// comes after, basepage::Leave), the skin's art and glass, the cursor set, the flight HUD's own state. The draw kit is core's (coreframe Stop and
// OnDeviceLost).
static void GiveBack(int phase)
{
    if (phase == 0)
    {
        hudvis::GiveBack();
        hostcalls::CursorHide(hostcalls::kCursorHideSkin, false);
        if (hostcalls::g.SetHudHotspots)
            hostcalls::g.SetHudHotspots(nullptr, 0);
        return;
    }
    extras::SetCursorFeed({});
    modelslot::Release("hud2 gives back"); // the 3D models' holders back to the engine (the classic HUD draws none)
    npcicons::Shutdown();      // the outline worker joined, the ships' atlas freed, the kit's source cleared (built again at HUD 2's next ask)
    skinload::Ended();         // the load screen's word back
    skinlists::GiveBack();     // an open save list's box, rows and bar as the game made them
    skinmenu::Drop();          // a CONTINUE not yet at the game's load screen
    nnstrip::Shutdown();
    nnpage::Shutdown();        // INFO's window back (the page runtime's park: pagerun::Shutdown)
    uistrip::Update(0, false); // windowtext's parked refusal back, the skin's items gone
    uistrip::Shutdown();
    winplates::SetMapOver(false);
    windowtext::Frame(false);  // every text's own font, box and name; game_text_size's table stays until the game reads it again
    navnew::Shutdown();
    mapcells::Shutdown();      // the influence cells' worker joined, the cells dropped (computed again at the map's next ask)
    uiskin::Shutdown();        // the windows' art and colours back, the skin off (winplates::g_on)
    skinover::Shutdown();      // the game's cursor back
    hud2::GiveBack();          // the game's panels and chat, the click box, a layout with no HUD 2 on the bus, the comm window, its own state
}

// The door's Leave, once a frame until true: the list on the first frame; a base page standing over its dealer closes its window on a later one, after
// windowtext gave the window's rows back (audit 95_). A window that never stops sliding gives it up after 4 s, the page forgotten.
static bool Leave()
{
    const bool first = g_leaveFrames++ == 0;
    if (first)
    {
        Withdraw();
        GiveBack(0);
        GiveBack(1);
    }
    if (!basepage::Leave(first) && g_leaveFrames < 240)
        return false;
    basepage::Shutdown();
    g_leaveFrames = 0;
    return true;
}

// A load frame while HUD 2 is selected (the door decides per frame of the load: the owner's decision 63): HUD 2's loading screen, or the game's own with
// HUD 2's word given back.
static void LoadDraw(bool hud2)
{
    if (hud2)
        skinload::Draw();
    else
        skinload::Ended();
}

void Attach()
{
    using modslots::kHud;
    huddoor::Register({modslots::Guard<kHud, Enter>::Call, modslots::Guard<kHud, Leave>::Call, modslots::Guard<kHud, LoadDraw>::Call});
}

void Detach()
{
    huddoor::Register({});
    Withdraw();
}

// Render.cpp's D3D textures go with the logic: every owner frees its own, and the game's panels, windows and colours go back. (A base page's window stays
// where the page put it: a close pressed at the stop has no frame after it to settle in.)
void Shutdown()
{
    modelslot::Shutdown(); // the 3D models' holders back to the engine, its lights out of the engine's list (before the logic unloads)
    GiveBack(0);
    GiveBack(1);
    basepage::Shutdown();
}

// A fault: the same list, phase by phase (modslots.h Fault); its views went at the fault (Detach).
void Fault(int phase)
{
    GiveBack(phase);
    if (phase == 1)
        basepage::Shutdown();
}

bool Command(const std::string& op, std::istream& in, std::string& reply)
{
    // ---- HUD 2 (the HUD2 track): hud2 on|off|auto|state|sim <combat|travel|cruise|dock|lane|off>
    if (op == "hud2")
        reply = hud2::Command(in);
    // ---- Extended HUD 2 (the SKIN track): uiskin state | windows | restore | rescan
    else if (op == "uiskin")
        reply = uiskin::Command(in);
    else if (op == "uistrip") // Extended HUD 2's docked overlay (the STRIP track): uistrip [state] | data | research ... | hover <i>
        reply = uistrip::Command(in);
    else if (op == "nnstrip") // the strip of words on the NeuroNet windows: nnstrip [state] | go <tab> | step <dir> | close | slide <seconds>
        reply = nnstrip::Command(in);
    else if (op == "nnpage") // the NeuroNet's full-screen pages: nnpage [state] | press <button>
        reply = nnpage::Command(in);
    else if (op == "page") // the page runtime (audit 70b F2a): page [state] | model | game | press <button>
        reply = pagerun::Command(in);
    else if (op == "modelprobe") // the model probe (audit R3b): modelprobe [state] | on [file] [x y] [size] [count] | off | set <k> <v>
        reply = modelprobe::Command(in);
    else if (op == "npcicons") // the NPC icons (GitHub 84 + 85): npcicons [state] - outlines, bakes, their cost
        reply = npcicons::Command(in);
    else if (op == "basepage") // the docked windows' pages: basepage [state] | press <button> | card | probe
        reply = basepage::Command(in);
    else if (op == "navskin") // the map's frame and words: navskin [state] | native on|off | press <id>
        reply = navskin::Command(in);
    else if (op == "navnew") // the new map, off until its checks pass: navnew [state] | on | off
        reply = navnew::Command(in);
    else
        return false;
    return true;
}

bool Key(int vk, bool down) { return skindialog::EnterKey(vk, down) || skindialog::ArrowKey(vk, down); }

bool GameTextVerb(const std::string& what, int pct, std::string& reply)
{
    if (what == "window")
        reply = windowtext::Status();
    else if (what == "frame") // gametext frame 0|1: NN_Inventory on the one frame (R8), until probe 18 clears it
    {
        windowtext::SetInventoryFrame(pct != 0);
        reply = windowtext::Status();
    }
    else if (what == "texts")
        reply = windowtext::Texts();
    else if (what == "bar")
        reply = winplates::BarReport();
    else if (what == "fit")
        reply = winplates::FitReport() + windowtext::Overflows();
    else
        return false;
    return true;
}
} // namespace hud2entry
