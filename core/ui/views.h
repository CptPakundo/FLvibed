// FLvibed - read-only views across the module boundary: what core's UI asks of a module that may be off. A module fills its view
// at its Init and clears it at its Shutdown; a null entry means no publisher, and the caller behaves as without that module: no
// pad reader (the keyboard and mouse drive the page), no prompt bar, the classic look. One logic, one instance: no bus needed. A module publishes its entries
// through modslots::Guard, so a fault in one stops that module and the view goes empty with it.
#pragma once
#include "common.h"
#include "game/apmode.h"
#include "game/fl.h"
#include "game/flightio.h"
#include "game/gamemode.h"
#include "game/hostcalls.h"
#include "game/input_rules.h"
#include "game/render.h"
#include "game/space.h"
#include "settings.h"
#include "ui/presets.h"
#include "ui/uikit.h"

namespace views
{
// The pad, for the row kit and the prompt bar (the Controller module: device_view.cpp). A reader is the module's: each one keeps
// its own edges, and it is read when the kit polls, in the same place of the frame as ever.
struct DeviceView
{
    void* (*NewPad)();
    void (*Poll)(void* pad, float dt, uikit::PadRead& out);
    void (*Extra)(const void* pad, uikit::Input::PadExtra& out);
    bool (*Connected)(const void* pad);
    void (*NoteFrame)(bool ours); // the game's arrow on this pad screen
    void (*SetPrompts)(const char* screen, const prompts::Prompt* items, int count, bool pending, int priority);
    float (*PromptTop)(float H, const render::Font& label);
    bool (*PadActive)(); // the player is on the pad
    bool (*LaneWanted)(); // the prompt lane is in use this frame
    bool (*OskOpen)();    // the on-screen keyboard
    bool (*RearView)();   // the wheel's rear view is on
    bool (*PromptVisible)();
    const wchar_t* (*HoveredOffer)(); // the pad's offer word for the hovered room character
    bool (*HoveredIsStory)();
};
inline DeviceView device = {};

// The skin over the game's windows (HUD: Extended HUD 2).
struct SkinView
{
    bool (*On)();
    const wchar_t* (*DialogAction)(); // the open dialog's verbs for the prompt lane; null: the game's own YES / NO
    const wchar_t* (*DialogSafe)();
    bool (*DialogFocusAction)();      // nothing is at risk: the pad's focus starts on the action
    bool (*ContinueOffered)();        // the main menu shows CONTINUE (a save to load, a place for it)
    void (*ContinueFocus)(bool on);   // the pad's focus stands on it
    void (*ContinueGo)();             // loads the latest save, by the game's load screen
};
inline SkinView skin = {};

// The autopilot, read only (the Autopilot module: autopilot_view.cpp).
struct AutopilotView
{
    const wchar_t* (*WaitReason)(); // what the objective run waits for, in the HUD's words; null when it does not
    bool (*ObjectiveOn)();
    u32 (*Target)();
    bool (*Active)();
    int (*MissionObjects)(u32* out, int max, const std::vector<space::Contact>& all); // what the mission points at
    apmode::Mode (*Mode)();
    bool (*OwnDock)();
    // what it acts on
    bool (*Fly)(float dt);                // the frame's slot: true while it flies the ship
    void (*DockSkip)(float dt, bool cutscene);
    void (*Stop)(const char* why);
    void (*StartSmart)();
    void (*TracePlayer)(float dt);
    void (*NoteMenu)();
    void (*ForgetGame)();
    std::string (*Command)(std::istream& in, const std::string& iniPath);
    // its settings: the [Autopilot] section of FLController.ini
    const settings::Def* (*Defs)(size_t& count);
    void (*SettingApplied)(const settings::Def* def);
    void (*Load)(const std::string& iniPath);
    void (*WriteCommented)(FILE* f);
    std::string (*SettingsReference)();
    // the goal panel
    void (*PanelClose)();
    bool (*PanelOpen)();
    void (*PanelToggle)();
    bool (*PanelOwnsButton)(glyphs::Id b);
    void (*PanelUpdate)(float dt, const bool* down);
    void (*PanelTick)(float dt);
    void (*PanelRender)(void* device);
    bool (*PanelKey)(int vk, bool down, bool textField);
    bool (*PanelOwnsHotspots)();
    void (*PanelHotspot)(int slot);
    // the maneuver bar's button
    void (*BarUpdate)();
    void (*BarDraw)();
    void (*BarClick)();
    bool (*BarKey)(int vk, bool down, bool textField);
    void (*BarShutdown)();
    const char* (*KeyName)(); // the autopilot key as armed: the row may not be on the bus with the controller off
};
inline AutopilotView autopilot = {};

// The pad as the autopilot's flight meets it (the Controller module). Without it nothing merges: no sticks to hand back, no held actions.
struct PadFlightView
{
    void (*Sticks)(inputrules::Sticks& out);
    bool (*Held)(inputrules::Channel c);
    void (*TorpedoSlot)();
    void (*FireGroups)();
    bool (*TurretLook)(float rx, float ry); // true: turret view, the cursor placed
    void (*MarkApUsed)();
    void (*Raw)(float out[6], u32* buttons); // lx ly rx ry lt rt, unfiltered, for the watchdog's snapshot
    u32 (*LastPadTick)();
    u32 (*LastKbmTick)();
};
inline PadFlightView padflight = {};

// What HUD 2 is told each frame.
struct HudContext
{
    bool space = false;     // Mode::Space (flying, the HUD up)
    bool spaceUi = false;   // Mode::SpaceUi (a NeuroNet window over the HUD)
    bool cutscene = false;  // Mode::Cutscene (an in-flight cutscene: a docking, a mission's scene)
    bool pad = false;       // the pad is the input device
    bool bot = false;       // the test bot flies
    presets::Table table;   // the effective bindings (flight or flight_ap)
    bool autopilot = false; // the Advanced Autopilot flies
    bool turret = false;    // turret view (the pad's toggle)
    int weaponGroup = 1;    // the active group as the pad last set it (1..3)
    bool apKnown = false;   // the autopilot's prompts may show (used once / ap_hints)
    bool modal = false;     // the picker / wheel / goal panel is open (its prompt bar: the context line steps aside)
    float steer = 0;        // the pad's steering this frame (0..~1.4: |x, y|): CONTEXTUAL's steer trigger (hudwake.h)
    bool firing = false;    // the pad holds fire / a launcher: CONTEXTUAL's fire trigger
};
// What uistrip drew this frame (pixels): the prompt lane's keep-outs.
struct HudRect
{
    float x0, y0, x1, y1;
    int part = 0; // 0 the strip, 1 the character card, 2 the decision words (each dims under FLUF's windows on its own)
};

// One chip on the nav map (pixels; the HUD's navskin): `stepper`: the D-pad's left and right step it where they would walk off it.
struct MapChip
{
    float x0, y0, x1, y1;
    int id;
    bool enabled, stepper;
};

// The HUD (modules\hud): its reads, and its places in core's frame and passes (core\game\coreframe.cpp). Null with the module off: the classic HUD.
struct HudView
{
    bool (*TargetCardAnchor)(float& right, float& bottom);
    bool (*BottomPanelsTop)(float& y);
    float (*StripRight)();  // uistrip's strip this frame: its right edge (pixels), 0 = none
    float (*StripBottom)(); // its bottom edge (fractions of H)
    bool (*Active)();       // HUD 2 replaces the game's HUD this frame: the pad HUD layer's badges and lines stand down
    float (*RingRadius)();
    bool (*CycleListFilter)(std::wstring& now);
    bool (*LockNotice)();
    void (*NoteAction)(unsigned wake);
    void (*Hotspot)(int slot);
    bool (*StripOn)();
    const std::vector<HudRect>* (*StripDrawn)();
    void (*DrawOver)(void* device); // the skin's marks over the game's UI, from the prompt bar's pass
    // the frame
    void (*Cues)(float dt, bool flying, bool autopilot);
    void (*GameText)(bool menu);
    void (*Flight)(float dt, const HudContext& c);
    void (*Cursor)(float dt, bool padOwns, bool flyingFree, bool (*onlyHudOpen)());
    void (*Skin)(float dt, bool padOwns, bool enabled, bool docked);
    // the passes
    bool (*UnderBegin)(void* device, bool flying); // true: a modal menu holds the flight HUD under its scrim
    void (*UnderEnd)(void* device, bool shieldCue);
    void (*Overlay)(void* device, bool menuOver, bool noUnderPass);
    void (*Strip)(void* device);
    void (*DeviceLost)();
    void (*Top)(void* device); // last of the overlay: over the options page and the prompts (the cursor, a tooltip)
    // the strip of words on the NeuroNet windows: its rectangle this frame, pixels x0 y0 x1 y1; false while it does not stand
    bool (*NNStrip)(float* rect);
    // the nav map's chips, for the pad's panel zone: what stands this frame, a press by the chip's own action, a stepper's step, the focus the HUD draws (-1 none)
    int (*MapChips)(MapChip* out, int max);
    void (*MapChipPress)(int id);
    void (*MapChipStep)(int id, int dir);
    void (*MapChipFocus)(int id);
};
inline HudView hud = {};

// The pad's places in core's frame and passes (the Controller module: controller.cpp). All null with the module off.
struct PadFrameView
{
    void (*Read)(float dt);
    void (*Feel)();
    void (*Options)();
    void (*Device)(float dt, bool space);
    void (*Watch)(u32 now);
    bool (*Owns)(); // the pad is the device
    void (*Died)();
    void (*ModeChange)(gamemode::Mode from, gamemode::Mode to);
    void (*FillHud)(HudContext& c);
    bool (*MenuButtonDown)();
    void (*Prompts)(float dt, bool padOwns);
    bool (*BotOn)();
    void (*Fly)(float dt);
    void (*DeviceRow)();
    void (*Rest)(float dt);
    bool (*Command)(const std::string& op, std::istream& in, std::string& reply); // its debug verbs
    void (*DrawFlight)(void* device, bool menuOver);
    void (*DrawGroups)(void* device);
    void (*DrawBases)(void* device);
    void (*DrawMenus)();
    void (*DrawPrompts)();
    void (*Fault)(int phase); // modslots::Module::Fault's place: the motors off
    bool (*NoHoverHelp)();    // a pad window with the arrow hidden: no cursor for the game's hover help to follow
};
inline PadFrameView padframe = {};

inline const wchar_t* ApWaitReason() { return autopilot.WaitReason ? autopilot.WaitReason() : nullptr; }
inline bool ApActive() { return autopilot.Active && autopilot.Active(); }
inline bool DevicePad() { return device.PadActive && device.PadActive(); }
inline bool HudTargetCardAnchor(float& right, float& bottom) { return hud.TargetCardAnchor && hud.TargetCardAnchor(right, bottom); }
inline bool HudBottomPanelsTop(float& y) { return hud.BottomPanelsTop && hud.BottomPanelsTop(y); }
inline bool HudActive() { return hud.Active && hud.Active(); }
// The reticle ring's radius in pixels: without the HUD, its size at scale 100.
inline float HudRingRadius() { return hud.RingRadius ? hud.RingRadius() : fl::ScreenHeight() * 0.10f; }
inline bool HudCycleListFilter(std::wstring& now) { return hud.CycleListFilter && hud.CycleListFilter(now); }
inline bool HudLockNotice() { return hud.LockNotice && hud.LockNotice(); }
inline void HudNoteAction(unsigned wake)
{
    if (hud.NoteAction)
        hud.NoteAction(wake);
}
inline void HudHotspot(int slot)
{
    if (hud.Hotspot)
        hud.Hotspot(slot);
}
inline bool HudStripOn() { return hud.StripOn && hud.StripOn(); }
inline const std::vector<HudRect>& HudStripDrawn()
{
    static const std::vector<HudRect> none;
    const std::vector<HudRect>* drawn = hud.StripDrawn ? hud.StripDrawn() : nullptr; // (null: a fault in it)
    return drawn ? *drawn : none;
}
inline void HudDrawOver(void* device)
{
    if (hud.DrawOver)
        hud.DrawOver(device);
}
inline bool HudUnderBegin(void* device, bool flying) { return hud.UnderBegin && hud.UnderBegin(device, flying); }
inline bool HudPresent() { return hud.Flight != nullptr; }
inline bool DeviceRearView() { return device.RearView && device.RearView(); }
inline bool DevicePromptVisible() { return device.PromptVisible && device.PromptVisible(); }
inline const wchar_t* DeviceHoveredOffer() { return device.HoveredOffer ? device.HoveredOffer() : nullptr; }
inline bool DeviceHoveredIsStory() { return device.HoveredIsStory && device.HoveredIsStory(); }
// The strip of words stands this frame: the bumpers step the tabs. False with the HUD off or on the classic look: the pad keeps its older bumpers.
inline bool HudNNStrip()
{
    float r[4];
    return hud.NNStrip && hud.NNStrip(r);
}
// The map's chips: none with the HUD off or the old map: the pad's map has one zone, as it always had.
inline int HudMapChips(MapChip* out, int max) { return hud.MapChips ? hud.MapChips(out, max) : 0; }
inline void HudMapChipPress(int id)
{
    if (hud.MapChipPress)
        hud.MapChipPress(id);
}
inline void HudMapChipStep(int id, int dir)
{
    if (hud.MapChipStep)
        hud.MapChipStep(id, dir);
}
inline void HudMapChipFocus(int id)
{
    if (hud.MapChipFocus)
        hud.MapChipFocus(id);
}
inline float HudStripRight() { return hud.StripRight ? hud.StripRight() : 0; }
inline float HudStripBottom() { return hud.StripBottom ? hud.StripBottom() : 0; }
inline bool LaneWanted() { return device.LaneWanted && device.LaneWanted(); }
inline bool OskOpen() { return device.OskOpen && device.OskOpen(); }
inline bool ApPresent() { return autopilot.Fly != nullptr; }
inline apmode::Mode ApMode() { return autopilot.Mode ? autopilot.Mode() : apmode::Mode::Off; }
inline bool ApOwnDock() { return autopilot.OwnDock && autopilot.OwnDock(); }
inline bool ApFly(float dt) { return autopilot.Fly && autopilot.Fly(dt); }
inline void ApDockSkip(float dt, bool cutscene)
{
    if (autopilot.DockSkip)
        autopilot.DockSkip(dt, cutscene);
}
inline void ApStop(const char* why)
{
    if (autopilot.Stop)
        autopilot.Stop(why);
}
inline void ApStartSmart()
{
    if (autopilot.StartSmart)
        autopilot.StartSmart();
}
inline void ApTracePlayer(float dt)
{
    if (autopilot.TracePlayer)
        autopilot.TracePlayer(dt);
}
inline void ApNoteMenu()
{
    if (autopilot.NoteMenu)
        autopilot.NoteMenu();
}
inline void ApForgetGame()
{
    if (autopilot.ForgetGame)
        autopilot.ForgetGame();
}
inline std::string ApCommand(std::istream& in, const std::string& iniPath)
{
    return autopilot.Command ? autopilot.Command(in, iniPath) : std::string("the autopilot module is off");
}
inline const char* ApKeyName() { return autopilot.KeyName ? autopilot.KeyName() : ""; }
inline void PanelClose()
{
    if (autopilot.PanelClose)
        autopilot.PanelClose();
}
inline bool PanelOpen() { return autopilot.PanelOpen && autopilot.PanelOpen(); }
inline void PanelToggle()
{
    if (autopilot.PanelToggle)
        autopilot.PanelToggle();
}
inline bool PanelOwnsButton(glyphs::Id b) { return autopilot.PanelOwnsButton && autopilot.PanelOwnsButton(b); }
inline void PanelUpdate(float dt, const bool* down)
{
    if (autopilot.PanelUpdate)
        autopilot.PanelUpdate(dt, down);
}
inline void PanelTick(float dt)
{
    if (autopilot.PanelTick)
        autopilot.PanelTick(dt);
}
inline void PanelRender(void* device)
{
    if (autopilot.PanelRender)
        autopilot.PanelRender(device);
}
inline bool PanelKey(int vk, bool down, bool textField) { return autopilot.PanelKey && autopilot.PanelKey(vk, down, textField); }
inline bool PanelOwnsHotspots() { return autopilot.PanelOwnsHotspots && autopilot.PanelOwnsHotspots(); }
inline void PanelHotspot(int slot)
{
    if (autopilot.PanelHotspot)
        autopilot.PanelHotspot(slot);
}
inline void BarUpdate()
{
    if (autopilot.BarUpdate)
        autopilot.BarUpdate();
}
inline void BarDraw()
{
    if (autopilot.BarDraw)
        autopilot.BarDraw();
}
inline void BarClick()
{
    if (autopilot.BarClick)
        autopilot.BarClick();
}
inline bool BarKey(int vk, bool down, bool textField) { return autopilot.BarKey && autopilot.BarKey(vk, down, textField); }
inline void BarShutdown()
{
    if (autopilot.BarShutdown)
        autopilot.BarShutdown();
}
inline void PadSticks(inputrules::Sticks& s)
{
    s = {};
    if (padflight.Sticks && !flightio::UnderWindow()) // (a window's pad is the window's)
        padflight.Sticks(s);
}
inline bool PadHeld(inputrules::Channel c) { return padflight.Held && !flightio::UnderWindow() && padflight.Held(c); }
inline void PadTorpedoSlot()
{
    if (padflight.TorpedoSlot && !flightio::UnderWindow())
        padflight.TorpedoSlot();
}
inline void PadFireGroups()
{
    if (padflight.FireGroups && !flightio::UnderWindow())
        padflight.FireGroups();
}
// (the game's turret camera without the pad: the guns' aim follows it for a keyboard player too)
// (under a window the look is the window's stick and the cursor the window's: the camera rests)
inline bool PadTurretLook(float rx, float ry) { return padflight.TurretLook && !flightio::UnderWindow() ? padflight.TurretLook(rx, ry) : fl::TurretView(); }
inline void PadMarkApUsed()
{
    if (padflight.MarkApUsed)
        padflight.MarkApUsed();
}
inline bool ApObjectiveOn() { return autopilot.ObjectiveOn && autopilot.ObjectiveOn(); }
inline u32 ApTarget() { return autopilot.Target ? autopilot.Target() : 0; }
// With the Autopilot off the marks and the objective's objects are still the game's: the picker and HUD 2 keep them (the script's marks need the autopilot's read)
inline int ApMissionObjects(u32* out, int max, const std::vector<space::Contact>& all)
{
    if (autopilot.MissionObjects)
        return autopilot.MissionObjects(out, max, all);
    int n = hostcalls::g.MissionMarks ? hostcalls::g.MissionMarks(out, max) : 0;
    if (n < 0)
        return 0;
    u32 refs[16];
    const int r = hostcalls::g.ObjectiveRefs ? hostcalls::g.ObjectiveRefs(refs, 16) : 0;
    for (int i = 0; i < r && n < max; ++i)
        if (const u32 id = space::ObjectiveObject(refs[i], all))
            if (std::find(out, out + n, id) == out + n)
                out[n++] = id;
    return n;
}

inline bool SkinOn() { return skin.On && skin.On(); }
inline const wchar_t* SkinDialogAction() { return skin.DialogAction ? skin.DialogAction() : nullptr; }
inline const wchar_t* SkinDialogSafe() { return skin.DialogSafe ? skin.DialogSafe() : nullptr; }
inline bool SkinDialogFocusAction() { return skin.DialogFocusAction && skin.DialogFocusAction(); }
inline bool SkinContinueOffered() { return skin.ContinueOffered && skin.ContinueOffered(); }
inline void SkinContinueFocus(bool on)
{
    if (skin.ContinueFocus)
        skin.ContinueFocus(on);
}
inline void SkinContinueGo()
{
    if (skin.ContinueGo)
        skin.ContinueGo();
}
inline void NoteFrame(bool ours)
{
    if (device.NoteFrame)
        device.NoteFrame(ours);
}
inline void SetPrompts(const char* screen, const prompts::Prompt* items, int count, bool pending = false, int priority = 0)
{
    if (device.SetPrompts)
        device.SetPrompts(screen, items, count, pending, priority);
}
// The bottom lane's top edge: 0.910 H without the bar too, so a panel above it keeps the same place.
inline float PromptTop(float H, const render::Font& label) { return device.PromptTop ? device.PromptTop(H, label) : H * 0.910f; }
} // namespace views
