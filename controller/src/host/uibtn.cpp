// FLvibed's native buttons (uibtn.h): the maneuver bar's fifth button, the Options screen's FLVIBED page button and
// its page switch, the pause menu's sixth button. Facts: docs\FREELANCER_RE.md "Options screen" (the factory's
// arguments, the record layouts, the page switch), "Main menu, load screens, pause menu" (NN_Pause).
#include "uibtn.h"

#include "../api.h"
#include "flexe.h"
#include "hook.h"
#include "log.h"

namespace uibtn
{
// ------------------------------------------------------------------ the game's types (core\flexe.h, verified call sites)
typedef u32(__thiscall* CreateIconButtonFn)(u32 parent, const char* name, const char* mesh, const char* mesh2,
                                            const char* clickSound, const char* action, const float* pos,
                                            const char* hardpoint, const float* mouseHalf, int noMeshRender,
                                            int actionButton, int noColourChange, int noZEnable, u32 tipIds, u32 tip2,
                                            u32 command);
typedef u32(__thiscall* CreateTextControlFn)(u32 parent, const char* name, const char* font, const float* pos, int a4,
                                             float width, float height, float scale, u32 ids, int align, int a10,
                                             int a11);
typedef u32(__thiscall* CreateWireControlFn)(u32 parent, const char* name, const char* sound, const float* pos,
                                             const float* dims, const char* hardpoint, int a6, u32 hintIds, int a8,
                                             int a9);
typedef int(__thiscall* SetPropertyFn)(u32 ctl, int id, const void* value, int a3);
typedef void(__thiscall* ShowPageFn)(u32 prefs, int hide);
typedef void(__thiscall* SetFrameFn)(u32 ctl, const float* translation);
typedef u32(__fastcall* MessageFn)(u32 self, void* edx, u32 msg, u32 id, u32 param);
typedef int(__fastcall* InitElementsFn)(u32 self, void* edx, u32 a1, u32 a2);
typedef void(__fastcall* ManeuverLayoutFn)(u32 part, void* edx);
typedef void(__fastcall* FrameSizeFn)(u32 part, void* edx, int count);

// FlControl layout (docs\FREELANCER_RE.md "UI")
constexpr u32 kParent = 0x04, kName = 0x18, kState = 0x6C, kPos = 0xA8, kBox = 0xB4;
constexpr u32 kFrameT = 0x60;    // the frame translation (the 4x3 at +0x3C; docs\FREELANCER_RE.md "Control placement")
constexpr u32 kBtnFrame = 0x2E0; // the icon button's copy of its frame translation (vt[0x14] IconButtonSetFrame)
// the icon button (vt 0x5E15F4): +0xD0 mesh name (char[]), +0x330/+0x331 flag bytes (0x331 bit 1 SET = clickable: the
// mouse handler 0x55B950 ignores the button while it is clear - the current page's button is the one with it clear, so
// it also gives the selected page's look; corrected 26 Oct 2026, it was read the other way round and FLVIBED took no
// mouse clicks), +0x335 the page-button mode (0x55BFC0), +0x364 the click sound std::string's buffer, +0x374 its sound id
constexpr u32 kBtnMesh = 0xD0, kBtnFlags1 = 0x331, kBtnMode = 0x335, kBtnSoundPtr = 0x364;
constexpr u32 kTextColour = 0x348; // text controls: the caption colour
// HUD_Maneuvers window (vt exe::VtManeuvers, at the HUD part + 0x38)
constexpr u32 kMbWindowOff = 0x38, kMbButtons = 0x334, kMbFrame = 0x380;
// NN_Preferences (vt exe::VtPrefs): +0x32D in game (else the main menu), the page buttons, the title, InterfaceText
constexpr u32 kPrefInGame = 0x32D, kPrefTitle = 0x344, kPrefKeymap = 0x34C, kPrefInterface = 0x350, kPrefAudio = 0x358,
              kPrefCredits = 0x35C, kPrefInterfaceText = 0x368, kPrefKeymapText = 0x364, kPrefPage = 0x7F4;
// NN_Pause (vt exe::VtPauseMenu): PauseButtonBox0..4 and PauseButtonText0..4
constexpr u32 kPauseBoxes = 0x338, kPauseTexts = 0x34C, kPauseLastFlag = 0x35D;

struct Btn
{
    const char* what;
    int id;
    u32 ctl = 0;    // our control (0 = none yet / gone)
    u32 text = 0;   // our caption text control (0 = none)
    u32 parent = 0; // the game's window it lives in (the lifetime)
    u32 failedIn = 0; // a window we could not create in (no retry per window)
    bool shown = true, lit = false, enabled = true; // the logic's state (SetUiButton); shown until it says otherwise
    int created = 0;
    u32 sentCtl = 0;     // the control the shown message last went to,
    int sentShown = -1;  // and its value (-1 = never)
};
static Btn g_bar = {"bar button", kUiBtnBar}, g_opt = {"options page button", kUiBtnOptionsPage},
           g_pause = {"pause menu button", kUiBtnPauseFlvibed};
static bool g_optShown = false; // our page replaces the game's page right now (OptionsPageState 2)
static bool g_optPlaced = true; // the Options button and caption re-placed after the window moved (see CreateOptionsButton)
static u32 g_barPart = 0;       // the HUD_Maneuvers part whose layout we observe (its window = g_bar.parent)
static char g_lastPlace[160] = "-";

// ------------------------------------------------------------------ guarded memory
static bool Read(u32 addr, void* out, size_t n)
{
    __try
    {
        memcpy(out, reinterpret_cast<const void*>(addr), n);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
static bool Write(u32 addr, const void* in, size_t n)
{
    __try
    {
        memcpy(reinterpret_cast<void*>(addr), in, n);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
static u32 R32(u32 addr)
{
    u32 v = 0;
    return Read(addr, &v, 4) ? v : 0;
}
static u8 R8(u32 addr)
{
    u8 v = 0;
    return Read(addr, &v, 1) ? v : 0;
}
static bool ReadVec(u32 ctl, u32 off, float* v) { return ctl && Read(ctl + off, v, 12); }
static u32 Vt(u32 obj) { return R32(obj); }

// Is the control still in the game's flat control list (node {+0 prev, +4 next, +8 FlControl*})?
static bool InList(u32 ctl)
{
    if (!ctl || !exe::UiControls.ok())
        return false;
    __try
    {
        const u32 head = Mem<u32>(exe::UiControls);
        u32 node = head ? Mem<u32>(head + 4) : 0;
        for (int guard = 0; node && node != head && guard < 8000; ++guard, node = Mem<u32>(node + 4))
            if (Mem<u32>(node + 8) == ctl)
                return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return false;
}

static bool KnownSetter(uintptr_t fn)
{
    return exe::ControlSetProperty.is(fn) || exe::ImageSetProperty.is(fn) || exe::IconButtonSetProperty.is(fn) ||
           exe::TextSetProperty.is(fn) || exe::TextCtlSetProperty.is(fn) || exe::TextButtonSetProperty.is(fn);
}

// The game's own setter of a control class whose vt[0xA8] another mod took over, from Freelancer.exe's file (0 = none
// known). FLUF (HudManager::CreateControlDetour, FLUF Source\Client\Ui\Vanilla\HudManager.cpp 313-335) writes its
// PerformDetour into vt[0xA8] of every control class it sees created, after recording the slot's original per vtable;
// PerformDetour (301-311) calls that original for the object's own vtable unless FLUF suppressed the control.
static uintptr_t OriginalSetter(void* const* vt)
{
    uintptr_t orig = 0;
    return hk::OriginalPointer(reinterpret_cast<uintptr_t>(&vt[0xA8 / 4]), &orig) && KnownSetter(orig) ? orig : 0;
}

// A control's property through its own vt[0xA8], as the game sets them. The slot must hold one of the known setters,
// or a restore-style mod's forwarder (FLUF's PerformDetour) in a game class whose original slot (the exe's file) is one
// of them: calling the live slot then runs FLUF's check and the game's setter (and nothing when FLUF suppressed the
// control, as for the game's own calls). Anything else is refused (fails closed).
static bool SetProp(u32 ctl, int id, const void* value, int a3 = 0)
{
    __try
    {
        void* const* vt = *reinterpret_cast<void* const* const*>(ctl);
        const uintptr_t fn = reinterpret_cast<uintptr_t>(vt[0xA8 / 4]);
        if (!KnownSetter(fn) && !(hk::IsRestoreStyle(fn) && OriginalSetter(vt)))
            return false;
        reinterpret_cast<SetPropertyFn>(fn)(ctl, id, value, a3);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
static bool SetPosition(u32 ctl, const float* pos) { return SetProp(ctl, 6, pos); }
static bool SetShownMsg(u32 ctl, bool shown) { return SetProp(ctl, 1, reinterpret_cast<const void*>(shown ? 1 : 0)); }
static bool SetLitMsg(u32 ctl, bool lit) { return SetProp(ctl, 0xC, reinterpret_cast<const void*>(lit ? 1 : 0)); }
static bool SetText(u32 ctl, const wchar_t* text) { return SetProp(ctl, 0x1D, text); }

// The state byte +0x6C: bits 0|1 shown, bit 3 enabled (hit-testable).
static void SetStateBits(u32 ctl, bool shown, bool enabled)
{
    u8 s = R8(ctl + kState);
    s = static_cast<u8>(shown ? (s | 3) : (s & ~3));
    s = static_cast<u8>(enabled ? (s | 8) : (s & ~8));
    Write(ctl + kState, &s, 1);
}

// ------------------------------------------------------------------ projection (UI camera: pixel = c + k * xy / -z)
static bool Project(u32 ctl, float* cx, float* cy, float* hw, float* hh)
{
    if (!exe::UiCamera.ok())
        return false;
    float ax = 0, ay = 0, az = 0, box[3] = {}, frame[3] = {};
    // the control's frame translation (+0x60, stamped by its window: the Options screen's page buttons carry
    // (slide x, 0, -1.0) and slide through it) + its own position; without a frame, the parents' position chain
    if (ReadVec(ctl, kFrameT, frame) && (frame[0] != 0 || frame[1] != 0 || frame[2] != 0))
    {
        float p[3];
        if (!ReadVec(ctl, kPos, p))
            return false;
        ax = frame[0] + p[0], ay = frame[1] + p[1], az = frame[2] + p[2];
    }
    else
    {
        u32 c = ctl;
        for (int guard = 0; c && guard < 40; ++guard)
        {
            float p[3];
            if (!ReadVec(c, kPos, p))
                return false;
            ax += p[0], ay += p[1], az += p[2];
            c = R32(c + kParent);
        }
    }
    if (az > -0.01f || !ReadVec(ctl, kBox, box))
        return false;
    const float kx = Mem<float>(exe::UiCamera, 0x78), ky = Mem<float>(exe::UiCamera, 0x7C);
    if (cx)
        *cx = Mem<float>(exe::UiCamera, 0x68) + kx * (ax / -az);
    if (cy)
        *cy = Mem<float>(exe::UiCamera, 0x6C) + ky * (ay / -az);
    if (hw)
        *hw = fabsf(kx) * box[0] / -az;
    if (hh)
        *hh = fabsf(ky) * box[1] / -az;
    return true;
}

// ------------------------------------------------------------------ the maneuver bar
static ManeuverLayoutFn g_origLayout = nullptr;
static FrameSizeFn g_origFrameSize = nullptr;
static MessageFn g_origMbMessage = nullptr;
static bool g_barWantFrame = false; // our button counted in the frame's width this frame
struct ShiftedPos
{
    u32 ctl = 0;
    float base = 0, written = 0;
    bool have = false;
};
static ShiftedPos g_frameShift;

static void ApplyBarState()
{
    if (!g_bar.ctl)
        return;
    SetShownMsg(g_bar.ctl, g_bar.shown);
    SetLitMsg(g_bar.ctl, g_bar.lit);
    SetStateBits(g_bar.ctl, g_bar.shown, g_bar.enabled);
}

// The button's mesh (UI_PLAN 2, decision 2): FLvibed's own icon EXE\FLvibedutopilot.3db (tools\mk3db.py) when it is
// installed, named relative to DATA as the game names its meshes (the loader resolves "..\EXE\..." from there: verified
// 25 Oct 2026 with a copy of a game icon); without the file the button has no mesh and the logic draws the glyph.
static char g_barMesh[MAX_PATH] = {};
static bool g_barMeshChosen = false;
static void ChooseBarMesh()
{
    if (g_barMeshChosen)
        return;
    g_barMeshChosen = true;
    const std::string file = DllDir() + "FLvibed\\autopilot.3db";
    if (GetFileAttributesA(file.c_str()) != INVALID_FILE_ATTRIBUTES)
        strcpy_s(g_barMesh, "..\\EXE\\FLvibed\\autopilot.3db");
    Log("bar button: mesh %s", g_barMesh[0] ? g_barMesh : "none (EXE\\FLvibed\\autopilot.3db not installed: the logic draws the glyph)");
}
void SetBarMesh(const char* path)
{
    g_barMeshChosen = true;
    if (path && *path)
        strncpy_s(g_barMesh, path, _TRUNCATE);
    else
        g_barMesh[0] = 0;
    g_bar.failedIn = 0;
    Log("bar button: mesh '%s' from the next HUD build on", g_barMesh);
}

static void CreateBarButton(u32 window)
{
    g_bar.failedIn = window;
    ChooseBarMesh();
    const u32 sibling = R32(window + kMbButtons);
    if (!sibling || !exe::VtIconButton.is(Vt(sibling)) || !exe::CreateIconButton.ok())
    {
        Log("bar button: no ManeuverButton0 (%08X) or factory - off", sibling);
        return;
    }
    float pos[3], half[3];
    if (!ReadVec(sibling, kPos, pos) || !ReadVec(sibling, kBox, half))
        return;
    const char* sound = reinterpret_cast<const char*>(R32(sibling + kBtnSoundPtr)); // "ui_select_item"
    char soundCopy[64] = {};
    if (!sound || !Read(reinterpret_cast<u32>(sound), soundCopy, 16))
        soundCopy[0] = 0;
    // No mesh (the logic draws the icon over UiButtonRect), the siblings' click sound and hit size, an action button
    // (as the game's four) with no command: its action sends 0x26 (its own id) to HUD_Maneuvers, whose handler finds no
    // record for it and does nothing; ManeuversMessageHook takes it first.
    const u32 ctl = exe::CreateIconButton.fn<CreateIconButtonFn>()(window, "FLvibedAutopilotButton",
                                                                     g_barMesh[0] ? g_barMesh : nullptr, nullptr,
                                                                     soundCopy[0] ? soundCopy : nullptr, nullptr, pos,
                                                                     nullptr, half, 0, 1, 0, 0, 0, 0, 0);
    if (!ctl)
    {
        Log("bar button: the factory returned null - off");
        return;
    }
    g_bar.ctl = ctl;
    g_bar.parent = window;
    g_bar.failedIn = 0;
    ++g_bar.created;
    g_frameShift = ShiftedPos();
    ApplyBarState();
    Log("bar button: created %08X in HUD_Maneuvers %08X (sound '%s', half %.4f x %.4f) #%d", ctl, window, soundCopy,
        half[0], half[1], g_bar.created);
}

// After the game's (and the subtitles module's) pass: our button after the last shown button, the frame widened.
static void BarAfterLayout(u32 part)
{
    const u32 window = part + kMbWindowOff;
    if (!exe::VtManeuvers.is(Vt(window)))
    {
        static bool logged = false;
        if (!logged)
            Log("bar button: HUD_Maneuvers window %08X has an unexpected vtable %08X - off", window, Vt(window)),
                logged = true;
        return;
    }
    g_barPart = part;
    if (g_bar.parent != window)
        g_bar.ctl = 0, g_bar.text = 0, g_bar.parent = 0; // a new HUD: the old control died with its window
    if (!g_bar.ctl && g_bar.failedIn != window && g_origMbMessage) // no click route (unresolved) = no dead control
        CreateBarButton(window);
    if (!g_bar.ctl)
        return;
    // the shown game buttons (+0x6C bit 1 is what the layout set this frame)
    int n = 0;
    float minX = 0, maxX = 0, y = 0, z = 0;
    for (int i = 0; i < 4; ++i)
    {
        const u32 b = R32(window + kMbButtons + 4 * i);
        float p[3];
        if (!b || !(R8(b + kState) & 2) || !ReadVec(b, kPos, p))
            continue;
        if (!n || p[0] < minX)
            minX = p[0];
        if (!n || p[0] > maxX)
            maxX = p[0], y = p[1], z = p[2];
        ++n;
    }
    const bool shown = g_bar.shown && n > 0;
    float pitch = Mem<float>(exe::ManeuverPitch);
    if (n >= 2 && maxX > minX)
        pitch = (maxX - minX) / (n - 1); // the bar as it is (the subtitles' narrow-screen scale included)
    if (pitch <= 0)
        pitch = 0.04f;
    SetShownMsg(g_bar.ctl, shown);
    SetStateBits(g_bar.ctl, shown, g_bar.enabled);
    if (exe::UiColourSelected.ok()) // the layout's msg 0x33 (the colour's address, flag 1 = grey) before the lit state
        SetProp(g_bar.ctl, 0x33, reinterpret_cast<const void*>(exe::UiColourSelected.get()), g_bar.enabled ? 0 : 1);
    SetLitMsg(g_bar.ctl, g_bar.lit);
    if (shown)
    {
        const float pos[3] = {maxX + pitch, y, z};
        SetPosition(g_bar.ctl, pos);
    }
    g_barWantFrame = shown;
    // the frame: one mesh wider (the sizer detour) and moved right by half a pitch, so its left end stays where the
    // game (HudShift, the subtitles' shift) put it; both at depth 0.5, so the pitch is the same in both units
    const u32 frame = R32(window + kMbFrame);
    float fp[3];
    if (frame && ReadVec(frame, kPos, fp))
    {
        if (g_frameShift.ctl != frame)
            g_frameShift = ShiftedPos(), g_frameShift.ctl = frame;
        if (!g_frameShift.have || fp[0] != g_frameShift.written)
            g_frameShift.base = fp[0], g_frameShift.have = true;
        const float want = g_frameShift.base + (shown ? pitch * 0.5f : 0.0f);
        if (want != fp[0])
        {
            // what the frame holds after the call counts as ours, not what we asked for: a setter that drops the call
            // leaves our last position there (FLUF suppresses ManeuverFrame and its later siblings while its palette is
            // open; the owner's attended run 26 Oct 2026: each palette open read our own shift back as the game's new
            // base and the frame crept right by half a pitch per open, the free-flight button left of it)
            const float np[3] = {want, fp[1], fp[2]};
            float after[3];
            if (SetPosition(frame, np) && ReadVec(frame, kPos, after))
                g_frameShift.written = after[0];
        }
        else
            g_frameShift.written = want;
    }
    sprintf_s(g_lastPlace, "shown=%d game=%d x=%.4f..%.4f pitch=%.4f ours=%.4f frame=%.4f->%.4f", shown, n, minX,
              maxX, pitch, maxX + pitch, g_frameShift.base, g_frameShift.written);
}

static void __fastcall ManeuverLayoutHook(u32 part, void* edx)
{
    g_origLayout(part, edx);
    __try
    {
        BarAfterLayout(part);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        static int logged = 0;
        if (logged < 3 && ++logged)
            Log("bar button: exception 0x%08X in the layout observer", GetExceptionCode());
    }
}

static void __fastcall ManeuverFrameSizeHook(u32 part, void* edx, int count)
{
    if (count >= 0 && part == g_barPart && g_bar.ctl && g_barWantFrame)
        ++count;
    g_origFrameSize(part, edx, count);
}

static u32 __fastcall ManeuversMessageHook(u32 self, void* edx, u32 msg, u32 id, u32 param)
{
    if (g_bar.ctl && id == g_bar.ctl && self == g_bar.parent && (msg == 0x26 || msg == 0x27))
    {
        UiButtonClicked(kUiBtnBar);
        return 0;
    }
    return g_origMbMessage(self, edx, msg, id, param);
}

// ------------------------------------------------------------------ the Options screen (NN_Preferences)
static InitElementsFn g_origPrefsInit = nullptr;
static MessageFn g_origPrefsMessage = nullptr;

static void ApplyOptState()
{
    if (!g_opt.ctl)
        return;
    // the shown message only when it changes: the logic re-applies the state every frame while the Options screen is
    // open, and a shown message each frame kept the game from ever hovering the button - no highlight, no mouse click
    // (the owner on the disc install, 26 Oct 2026; the harness's native clicks never needed the hover). The game sends
    // its own page buttons none per frame.
    if (g_opt.sentCtl != g_opt.ctl || g_opt.sentShown != static_cast<int>(g_opt.shown))
    {
        SetShownMsg(g_opt.ctl, g_opt.shown);
        g_opt.sentCtl = g_opt.ctl, g_opt.sentShown = g_opt.shown ? 1 : 0;
    }
    SetStateBits(g_opt.ctl, g_opt.shown, g_opt.enabled);
    const bool lit = g_opt.lit || g_optShown;
    u8 f = R8(g_opt.ctl + kBtnFlags1);
    f = static_cast<u8>(lit ? (f & ~2) : (f | 2)); // clear only while our page is the current one (as the game's own)
    Write(g_opt.ctl + kBtnFlags1, &f, 1);
    if (g_opt.text)
    {
        SetStateBits(g_opt.text, g_opt.shown, true);
        if (exe::UiColourSelected.ok() && exe::UiColourNormal.ok())
        {
            const u32 colour = lit ? Mem<u32>(exe::UiColourSelected) : Mem<u32>(exe::UiColourNormal);
            Write(g_opt.text + kTextColour, &colour, 4);
        }
    }
}

static void CreateOptionsButton(u32 prefs)
{
    g_opt.failedIn = prefs;
    if (!exe::CreateIconButton.ok() || !exe::CreateTextControl.ok())
        return;
    const bool inGame = R8(prefs + kPrefInGame) != 0;
    const u32 bInterface = R32(prefs + kPrefInterface), bKeymap = R32(prefs + kPrefKeymap);
    const u32 anchor = inGame ? R32(prefs + kPrefAudio) : R32(prefs + kPrefCredits); // the free slot comes after it
    const u32 tInterface = R32(prefs + kPrefInterfaceText);
    if (!bInterface || !bKeymap || !anchor || !exe::VtIconButton.is(Vt(bInterface)) ||
        !exe::VtIconButton.is(Vt(bKeymap)) || !exe::VtIconButton.is(Vt(anchor)))
    {
        Log("options page button: the page buttons are not where expected (%08X %08X %08X) - off", bInterface, bKeymap,
            anchor);
        return;
    }
    float pi[3], pk[3], pa[3], half[3];
    if (!ReadVec(bInterface, kPos, pi) || !ReadVec(bKeymap, kPos, pk) || !ReadVec(anchor, kPos, pa) ||
        !ReadVec(bInterface, kBox, half))
        return;
    const float pitch = pi[1] - pk[1];
    if (pitch <= 0.0001f)
    {
        Log("options page button: no pitch between GENERAL and CONTROLS (%.4f) - off", pitch);
        return;
    }
    char mesh[128] = {}, sound[64] = {};
    Read(bInterface + kBtnMesh, mesh, sizeof(mesh) - 1);
    const u32 soundPtr = R32(bInterface + kBtnSoundPtr);
    if (!soundPtr || !Read(soundPtr, sound, 16))
        sound[0] = 0;
    const float pos[3] = {pi[0], pa[1] - pitch, pi[2]};
    // the siblings' mesh, sound and hit size; a page button (actionButton 0 as theirs), no command / tip
    const u32 ctl = exe::CreateIconButton.fn<CreateIconButtonFn>()(prefs, "FLvibedOptionsButton", mesh[0] ? mesh : nullptr,
                                                                     nullptr, sound[0] ? sound : nullptr, nullptr, pos,
                                                                     nullptr, half, 0, 0, 0, 0, 0, 0, 0);
    if (!ctl)
    {
        Log("options page button: the factory returned null - off");
        return;
    }
    const u8 mode = 1; // 0x55BFC0(1) after each page button
    Write(ctl + kBtnMode, &mode, 1);
    g_opt.ctl = ctl;
    g_opt.parent = prefs;
    g_opt.failedIn = 0;
    ++g_opt.created;
    // the caption: a text control like the siblings' (InterfaceText: "ButtonText" font, the game's constants), raw text.
    // The captions' units differ from the buttons' (their own depth): the slot pitch is measured on two captions
    // (InterfaceText - keymapText) and ours sits that many slots below InterfaceText.
    float pt[3] = {pi[0], pi[1], pi[2]}, pk2[3] = {pk[0], pk[1], pk[2]};
    const u32 tKeymap = R32(prefs + kPrefKeymapText);
    const bool texts = tInterface && tKeymap && (exe::VtTitleText.is(Vt(tInterface)) || exe::VtText.is(Vt(tInterface))) &&
                       ReadVec(tInterface, kPos, pt) && ReadVec(tKeymap, kPos, pk2);
    const float slots = (pi[1] - pos[1]) / pitch;                                   // how many slots below GENERAL
    const float tpitch = texts && pt[1] > pk2[1] ? pt[1] - pk2[1] : pitch;         // the captions' own slot pitch
    const float tpos[3] = {pt[0], pt[1] - slots * tpitch, pt[2]};
    // A native caption did not draw after four tries (25 Oct 2026: created like InterfaceText, raw text through msg
    // 0x1D, its init fields copied from the sibling, re-placed after the window moved, the active flag +0x340 kept set
    // each pass - the control exists and holds the text, the game never draws it; the pause menu's caption, made the
    // same way, draws). Off until that difference is found: the logic draws "FLVIBED" over UiButtonRect.
    constexpr bool kNativeOptionsCaption = false;
    const u32 text = kNativeOptionsCaption
                         ? exe::CreateTextControl.fn<CreateTextControlFn>()(prefs, "FLvibedOptionsText", "ButtonText",
                                                                             tpos, 0, 0.258f, 0.03f, 1.25f, 0, 1, 0, 1)
                         : 0;
    if (text)
    {
        g_opt.text = text;
        if (!SetText(text, L"FLVIBED"))
            Log("options page button: caption text not set (setter unknown)");
        // The game's captions differ from a fresh text in fields the factory's arguments do not reach the same way (read
        // on InterfaceText, 25 Oct 2026): +0x324 (-1.0) and +0x330 (the height 0.03) - copied from the sibling caption,
        // which is exactly the look wanted. Its draw transform (+0x60) is final only once the window has been placed
        // (the caller moves NN_Preferences after InitElements), so the caption is re-placed on the first frame, and the
        // game clears the active flag +0x340 of texts it does not own every frame: it is set again at every UI pass.
        if (texts)
            for (const u32 off : {0x324u, 0x330u})
            {
                u32 v = 0;
                if (Read(tInterface + off, &v, 4))
                    Write(text + off, &v, 4);
            }
        SetStateBits(text, true, true);
        g_optPlaced = false;
    }
    ApplyOptState();
    Log("options page button: created %08X (text %08X) in NN_Preferences %08X, %s, at (%.4f, %.4f) mesh '%s' #%d", ctl,
        text, prefs, inGame ? "in game (after AUDIO)" : "main menu (after CREDITS)", pos[0], pos[1], mesh,
        g_opt.created);
}

static int __fastcall PrefsInitElementsHook(u32 self, void* edx, u32 a1, u32 a2)
{
    const int r = g_origPrefsInit(self, edx, a1, a2);
    __try
    {
        if (exe::VtPrefs.is(Vt(self)))
        {
            g_opt.ctl = g_opt.text = 0, g_opt.parent = 0;
            g_optShown = false;
            CreateOptionsButton(self);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log("options page button: exception 0x%08X creating it", GetExceptionCode());
    }
    return r;
}

static bool PrefsAlive() { return g_opt.parent && g_opt.ctl && InList(g_opt.ctl); }

// The screen's own furniture stays while FLvibed's page shows: the background and box, the page buttons and their
// captions, the close X (the title is handled apart); everything else under NN_Preferences is the current page's
// content (opt_* rows, the Controls page's key table, video_mode_*), which the game's PrefsShowPage(1) hides only in
// part (the toggles, sliders, DEFAULT / CANCEL; the labels and section headers stayed, 25 Oct 2026).
// (the panel box is named "big box" (vt 0x5E4D7C) - "OptionsBox" was a guess; INT-PAGE 25 Oct 2026: the main menu's page
// had no box behind it)
static const char* const kPrefsKeep[] = {"Options_BlackPlane", "OptionsBox",     "big box",                "PreferencesCloseButton", "Screen Title",
                                         "interfaceButton",    "keymapButton",   "VideoButton",            "AudioButton",
                                         "CreditsButton",      "MainMenuButton", "InterfaceText",          "keymapText",
                                         "VideoText",          "AudioText",      "mainMenuText",           "FLvibedOptionsButton",
                                         "FLvibedOptionsText"};
static std::vector<u32> g_prefsHidden; // the page content we hid (restored on the way back)

static bool UnderWindow(u32 ctl, u32 window)
{
    for (int guard = 0; ctl && guard < 40; ++guard)
    {
        ctl = R32(ctl + kParent);
        if (ctl == window)
            return true;
    }
    return false;
}

// `append`: a re-scan while FLvibed's page shows - only the controls that became visible since are hidden and added
// (the game shows its page's content again at the end of its page-switch slide: a switch to FLvibed's page during the
// slide left the GENERAL page under ours, 25 Oct 2026 - its labels were not visible yet when the first scan ran, so the
// per-frame re-hide of BeforeUiDraw did not know them). Returns how many were hidden.
static unsigned HidePrefsContent(u32 prefs, bool append = false)
{
    if (!append)
        g_prefsHidden.clear();
    unsigned hidden = 0;
    if (!exe::UiControls.ok())
        return 0;
    __try
    {
        const u32 head = Mem<u32>(exe::UiControls);
        u32 node = head ? Mem<u32>(head + 4) : 0;
        for (int guard = 0; node && node != head && guard < 8000; ++guard, node = Mem<u32>(node + 4))
        {
            const u32 ctl = Mem<u32>(node + 8);
            if (!ctl || ctl == prefs || !(Mem<u8>(ctl + kState) & 1) || !UnderWindow(ctl, prefs))
                continue;
            char name[33] = {};
            memcpy(name, reinterpret_cast<const char*>(ctl + kName), 32);
            bool keep = strstr(name, "redits") != nullptr; // the main menu's CreditsButton / its caption
            for (const char* k : kPrefsKeep)
                keep = keep || strcmp(name, k) == 0;
            if (keep)
                continue;
            Mem<u8>(ctl + kState) &= static_cast<u8>(~3); // bit 1 is what the window's draw tests (bit 0 alone left the labels on screen)
            if (!append || std::find(g_prefsHidden.begin(), g_prefsHidden.end(), ctl) == g_prefsHidden.end())
                g_prefsHidden.push_back(ctl);
            ++hidden;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return hidden;
}

// The game shows the current page's labels again every frame (the texts came back within a frame, 25 Oct 2026): while
// FLvibed's page is up they are hidden again at the start of every UI pass, before any control is drawn.
void BeforeUiDraw()
{
    if (g_opt.text && g_opt.ctl)
    {
        __try
        {
            Mem<u32>(g_opt.text + 0x340) = g_opt.shown ? 1 : 0; // the caption's active flag, as the page's captions get it
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
    if (!g_optShown || !g_opt.parent)
        return;
    __try
    {
        for (u32 ctl : g_prefsHidden)
            Mem<u8>(ctl + kState) &= static_cast<u8>(~3);
        if (const u32 title = R32(g_opt.parent + kPrefTitle))
            Mem<u8>(title + kState) &= static_cast<u8>(~3);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    // content the game showed again since (the end of its page-switch slide): a re-scan every 100 ms
    static ULONGLONG lastScan = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - lastScan >= 100)
    {
        lastScan = now;
        if (const unsigned n = HidePrefsContent(g_opt.parent, true))
            Log("options page: %u content control(s) shown again by the game hidden (%u known)", n,
                static_cast<unsigned>(g_prefsHidden.size()));
    }
}

static void RestorePrefsContent(u32 prefs)
{
    for (u32 ctl : g_prefsHidden)
        if (InList(ctl) && UnderWindow(ctl, prefs))
        {
            u8 s = R8(ctl + kState) | 3;
            Write(ctl + kState, &s, 1);
        }
    g_prefsHidden.clear();
}

bool ShowOptionsPage(bool on)
{
    if (!PrefsAlive() || !exe::PrefsShowPage.ok())
        return false;
    if (on == g_optShown)
        return true;
    __try
    {
        unsigned content = 0;
        if (on)
        {
            exe::PrefsShowPage.fn<ShowPageFn>()(g_opt.parent, 1); // the game's own hide of its page's controls
            HidePrefsContent(g_opt.parent);                        // and the rest of the page (labels, headers)
            content = static_cast<unsigned>(g_prefsHidden.size());
        }
        else
        {
            content = static_cast<unsigned>(g_prefsHidden.size());
            RestorePrefsContent(g_opt.parent);
            exe::PrefsShowPage.fn<ShowPageFn>()(g_opt.parent, 0);
        }
        if (const u32 title = R32(g_opt.parent + kPrefTitle))
            SetStateBits(title, !on, false);
        g_optShown = on;
        ApplyOptState();
        Log("options page: %s (game page %u, %u content controls %s)",
            on ? "FLvibed's page shown, the game's hidden" : "the game's page back", R32(g_opt.parent + kPrefPage),
            content, on ? "hidden" : "restored");
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log("options page: exception 0x%08X in the page switch", GetExceptionCode());
        return false;
    }
}

int OptionsPageState()
{
    if (!PrefsAlive())
        return 0;
    return g_optShown ? 2 : 1;
}

static u32 __fastcall PrefsMessageHook(u32 self, void* edx, u32 msg, u32 id, u32 param)
{
    if (g_opt.ctl && self == g_opt.parent)
    {
        if ((msg == 0x26 || msg == 0x27) && id == g_opt.ctl)
        {
            UiButtonClicked(kUiBtnOptionsPage);
            return 1;
        }
        if (msg == 0x26 && g_optShown) // any other button (a page, RETURN / MAIN MENU, the X): the game's page back first
            ShowOptionsPage(false);
    }
    return g_origPrefsMessage(self, edx, msg, id, param);
}

// ------------------------------------------------------------------ the pause menu (NN_Pause)
static InitElementsFn g_origPauseInit = nullptr;
static MessageFn g_origPauseMessage = nullptr;

static void ApplyPauseState()
{
    if (!g_pause.ctl)
        return;
    SetStateBits(g_pause.ctl, g_pause.shown, g_pause.enabled);
    if (g_pause.text)
        SetStateBits(g_pause.text, g_pause.shown, false);
}

static void CreatePauseButton(u32 pause)
{
    g_pause.failedIn = pause;
    if (!exe::CreateWireControl.ok() || !exe::CreateTextControl.ok())
        return;
    int n = 0;
    u32 boxes[5] = {}, texts[5] = {};
    for (int i = 0; i < 5; ++i)
    {
        boxes[i] = R32(pause + kPauseBoxes + 4 * i);
        texts[i] = R32(pause + kPauseTexts + 4 * i);
        if (boxes[i])
            n = i + 1;
    }
    if (n < 2 || !texts[n - 1])
    {
        Log("pause menu button: %d boxes - off", n);
        return;
    }
    float last[3], prev[3], dims[3], tlast[3];
    if (!ReadVec(boxes[n - 1], kPos, last) || !ReadVec(boxes[n - 2], kPos, prev) || !ReadVec(boxes[n - 1], kBox, dims) ||
        !ReadVec(texts[n - 1], kPos, tlast))
        return;
    const float pitch = prev[1] - last[1];
    if (pitch <= 0.0001f)
    {
        Log("pause menu button: no pitch (%.4f) - off", pitch);
        return;
    }
    // Six slots inside the same frame: the game lays 4 boxes out at (first y 0.1284, pitch 0.0963) and 5 at (0.149,
    // 0.0825) - one more box moves the first up by 0.0206 and tightens the pitch by 0.0138 (0x4A6D5C..0x4A6D76). The
    // same step once more gives the 6-slot layout (a box simply appended fell below the frame, 25 Oct 2026). The
    // game's boxes and captions move through their own setters; ours takes the last slot.
    float first[3], tfirst[3];
    if (!ReadVec(boxes[0], kPos, first) || !ReadVec(texts[0], kPos, tfirst))
        return;
    const float textOff = tfirst[1] - first[1];
    const float base6 = first[1] + 0.0206f, pitch6 = pitch - 0.0138f;
    bool moved = true;
    for (int i = 0; i < n && moved; ++i)
    {
        float bp[3], tp[3];
        if (!ReadVec(boxes[i], kPos, bp) || !ReadVec(texts[i], kPos, tp))
            moved = false;
        else
        {
            const float nb[3] = {bp[0], base6 - i * pitch6, bp[2]}, nt[3] = {tp[0], base6 - i * pitch6 + textOff, tp[2]};
            moved = SetPosition(boxes[i], nb) && SetPosition(texts[i], nt);
        }
    }
    if (!moved)
        Log("pause menu button: the game's boxes could not be re-pitched (a setter unknown): ours below the last one");
    const float pos[3] = {last[0], moved ? base6 - n * pitch6 : last[1] - pitch, last[2]};
    const float textY = moved ? pos[1] + textOff : tlast[1] - pitch;
    // as the game creates PauseButtonBox%d: no sound, the siblings' dimensions, no hardpoint / hint
    const u32 ctl = exe::CreateWireControl.fn<CreateWireControlFn>()(pause, "PauseButtonBox5", nullptr, pos, dims,
                                                                       nullptr, 0, 0, 0, 0);
    if (!ctl)
    {
        Log("pause menu button: the factory returned null - off");
        return;
    }
    // the "last box" flag moves from the game's last box to ours
    u8 f = R8(boxes[n - 1] + kPauseLastFlag);
    f &= ~1;
    Write(boxes[n - 1] + kPauseLastFlag, &f, 1);
    f = R8(ctl + kPauseLastFlag) | 1;
    Write(ctl + kPauseLastFlag, &f, 1);
    g_pause.ctl = ctl;
    g_pause.parent = pause;
    g_pause.failedIn = 0;
    ++g_pause.created;
    const float tpos[3] = {tlast[0], textY, tlast[2]};
    const u32 text = exe::CreateTextControl.fn<CreateTextControlFn>()(pause, "PauseButtonText5", "ButtonText", tpos, 0,
                                                                        0.27f, 0.015f, 1.5f, 0, 1, 0, 0);
    if (text)
    {
        g_pause.text = text;
        if (!SetText(text, L"FLVIBED"))
            Log("pause menu button: caption text not set (setter unknown)");
    }
    ApplyPauseState();
    Log("pause menu button: created %08X (text %08X) in NN_Pause %08X under %d boxes at y %.4f (pitch %.4f) #%d", ctl,
        text, pause, n, pos[1], pitch, g_pause.created);
}

static int __fastcall PauseInitElementsHook(u32 self, void* edx, u32 a1, u32 a2)
{
    const int r = g_origPauseInit(self, edx, a1, a2);
    __try
    {
        if (exe::VtPauseMenu.is(Vt(self)))
        {
            g_pause.ctl = g_pause.text = 0, g_pause.parent = 0;
            CreatePauseButton(self);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log("pause menu button: exception 0x%08X creating it", GetExceptionCode());
    }
    return r;
}

static u32 __fastcall PauseMessageHook(u32 self, void* edx, u32 msg, u32 id, u32 param)
{
    if (g_pause.ctl && self == g_pause.parent && msg == 0x26 && id == g_pause.ctl)
    {
        UiButtonClicked(kUiBtnPauseFlvibed);
        return 0;
    }
    return g_origPauseMessage(self, edx, msg, id, param);
}

// ------------------------------------------------------------------ HUD 2's hotspots (HostApi v22)
// While HUD 2 replaces the game's flight HUD every HUD root is hidden (hudvis), and a control under a hidden root is
// neither drawn nor hit (the hit test 0x59DD80 wants bits 0, 1 and 3 on the control and bit 1 on every ancestor). So the
// hotspots get a root of their own: an icon button made by the same factory with no parent (`this` = 0: CreateControl
// 0x59DB20 -> the class's vt[0x24] 0x55A6B0 -> SetParent(ctl, 0) 0x59DD00 + the flat list 0x59DBF0 - a top-level control
// like the HUD roots), no mesh (draws nothing), a zero hit box and bit 3 clear (never hit itself), bits 0|1 set (its
// children are hit-testable). Its children are icon buttons as the bar's (no mesh, an action button with no command):
// the game's mouse handler (0x55B950) plays the click sound and on the release runs vt[0xB4] (exe::IconButtonActivate:
// no command -> msg 0x26 to the owner) or, after a double click, sends 0x27 to the owner. The owner is the box, whose
// message handler (vt[0xA8]) is ours through a private copy of the icon button's vtable (the game's code and other
// buttons untouched); the copy's slot 0 (the scalar deleting destructor) tells us when the game frees the box.
typedef void*(__fastcall* DtorFn)(u32 self, void* edx, int flags);
constexpr int kHot = kUiBtnHud2Count;
constexpr int kVtSlots = 46;              // the icon button's vtable 0x5E15F4: slots 0..45 (45 = vt[0xB4] Activate)
constexpr int kVtMessage = 0xA8 / 4, kVtActivate = 0xB4 / 4;
static u32 g_hotBox = 0;                  // the box (0 = none)
static u32 g_hot[kHot] = {};              // its buttons
static HudHotspot g_hotSet[kHot] = {};    // what each button shows now (the last applied set)
static float g_hotCam[4] = {};            // the UI camera the set was placed with (centre, scale)
static void* g_hotVt[kVtSlots + 1] = {};  // the box's vtable: [0] = the RTTI slot (vt[-1]), then the slots
static MessageFn g_hotOrigMessage = nullptr;
static DtorFn g_hotOrigDtor = nullptr;
static bool g_hotFailed = false;          // creation failed: no retry (fails closed)
static unsigned g_hotFrame = 0, g_hotCalled = 0, g_hotApplied = 0, g_hotClicks = 0;
static bool g_hotAny = false;             // a button is shown
static ULONGLONG g_hotChecked = 0;

static void HotForget(const char* why)
{
    if (g_hotBox)
        Log("HUD 2 hotspots: box %08X gone (%s)", g_hotBox, why);
    g_hotBox = 0;
    memset(g_hot, 0, sizeof(g_hot));
    memset(g_hotSet, 0, sizeof(g_hotSet));
    g_hotAny = false;
}

static u32 __fastcall HotBoxMessage(u32 self, void* edx, u32 msg, u32 id, u32 param)
{
    if (self == g_hotBox && g_hotBox && id)
    {
        int slot = -1;
        for (int i = 0; i < kHot; ++i)
            if (g_hot[i] == id)
                slot = i;
        if (slot >= 0)
        {
            // 0x26 the action (a click's release), 0x27 the release after a double click: both a click here
            if ((msg == 0x26 || msg == 0x27) && g_hotSet[slot].shown)
            {
                ++g_hotClicks;
                UiButtonClicked(kUiBtnHud2 + slot);
            }
            return 0; // ours, also the hover / press / capture notes (0x25, 0x28, 0x29, 0x2C, 0x4F): nothing to do
        }
    }
    return g_hotOrigMessage(self, edx, msg, id, param);
}

static void* __fastcall HotBoxDtor(u32 self, void* edx, int flags)
{
    if (self == g_hotBox)
    {
        HotForget("freed by the game");
        __try
        {
            Mem<u32>(self) = exe::VtIconButton.get(); // its own class again for the destructor chain
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
    return g_hotOrigDtor(self, edx, flags);
}

static bool CreateHotspots()
{
    if (g_hotFailed)
        return false;
    g_hotFailed = true; // until it succeeds
    if (!exe::CreateIconButton.ok() || !exe::VtIconButton.ok() || !exe::IconButtonSetProperty.ok() ||
        !exe::IconButtonActivate.ok() || !exe::UiCamera.ok() || !exe::UiControls.ok())
    {
        Log("HUD 2 hotspots: a symbol is unresolved - off (HUD 2 has no clicks)");
        return false;
    }
    // the box's vtable: the icon button's, with our message handler and destructor
    const u32 vt = exe::VtIconButton.get();
    // The message slot may be FLUF's PerformDetour (OriginalSetter): it finds the original by the object's vtable,
    // and our box's vtable is this private copy, which FLUF never recorded - so the box forwards to the game's own
    // setter (the exe file's slot) instead; FLUF never suppresses this control (not one of its HUD's).
    uintptr_t message = Read(vt - 4, g_hotVt, sizeof(g_hotVt)) ? reinterpret_cast<uintptr_t>(g_hotVt[1 + kVtMessage]) : 0;
    if (message && !exe::IconButtonSetProperty.is(message) && hk::IsRestoreStyle(message))
    {
        const uintptr_t orig = OriginalSetter(reinterpret_cast<void* const*>(vt));
        message = exe::IconButtonSetProperty.is(orig) ? orig : 0;
        if (message)
            Log("HUD 2 hotspots: the icon button's message slot is FLUF's forwarder - the box forwards to the game's "
                "setter");
    }
    if (!message || !exe::IconButtonSetProperty.is(message) ||
        !exe::IconButtonActivate.is(reinterpret_cast<uintptr_t>(g_hotVt[1 + kVtActivate])))
    {
        Log("HUD 2 hotspots: the icon button's vtable is not as expected - off");
        return false;
    }
    g_hotOrigMessage = reinterpret_cast<MessageFn>(message);
    g_hotOrigDtor = reinterpret_cast<DtorFn>(g_hotVt[1]);
    g_hotVt[1 + kVtMessage] = reinterpret_cast<void*>(HotBoxMessage);
    g_hotVt[1] = reinterpret_cast<void*>(HotBoxDtor);
    const float zero[3] = {0, 0, 0};
    const auto create = exe::CreateIconButton.fn<CreateIconButtonFn>();
    // the bar button's arguments (an action button, no command); +0x338 is cleared below before the first draw
    const u32 box = create(0, "FLvibedHud2Hotspots", nullptr, nullptr, nullptr, nullptr, zero, nullptr, zero, 0, 1, 0, 0,
                           0, 0, 0);
    if (!box || Vt(box) != vt || R32(box + kParent) != 0)
    {
        Log("HUD 2 hotspots: the box was not created as a top-level icon button (%08X) - off", box);
        return false;
    }
    // +0x338 = the button's mesh-table entry: the Init (0x55AC30) sets it only when a mesh is named (0x55AC69 / 0x55ACEC
    // sit inside `if (mesh)`), so a mesh-less button keeps the allocation's garbage there and its draw (0x559AA0 reads
    // [+0x338]+0x20) faults - measured on the first pass, 25 Oct 2026. No mesh = 0, the value the draw tests for.
    const u32 kBtnMeshEntry = 0x338, noEntry = 0;
    Write(box + kBtnMeshEntry, &noEntry, 4);
    Mem<u32>(box) = reinterpret_cast<u32>(&g_hotVt[1]);
    SetStateBits(box, true, false); // drawn (its children's ancestor bit), never hit itself
    g_hotBox = box;
    int made = 0;
    for (int i = 0; i < kHot; ++i)
    {
        char name[32];
        sprintf_s(name, "FLvibedHud2Hot%02d", i);
        const float pos[3] = {0, 0, -1}, half[3] = {0.001f, 0.001f, 0};
        // as the bar button: no mesh, the HUD buttons' click sound, an action button with no command
        g_hot[i] = create(box, name, nullptr, nullptr, "ui_select_item", nullptr, pos, nullptr, half, 0, 1, 0, 0, 0, 0, 0);
        if (!g_hot[i] || Vt(g_hot[i]) != vt)
        {
            g_hot[i] = 0;
            continue;
        }
        Write(g_hot[i] + kBtnMeshEntry, &noEntry, 4);
        SetShownMsg(g_hot[i], false);
        SetStateBits(g_hot[i], false, false);
        ++made;
    }
    if (!made)
    {
        Log("HUD 2 hotspots: no button created - off");
        return false;
    }
    g_hotFailed = false;
    g_hotChecked = GetTickCount64();
    Log("HUD 2 hotspots: box %08X (top-level, own vtable) with %d buttons", box, made);
    return true;
}

static void HotApply(int i, const HudHotspot& s)
{
    const u32 c = g_hot[i];
    if (!c)
        return;
    if (!s.shown || s.x1 <= s.x0 || s.y1 <= s.y0)
    {
        SetShownMsg(c, false);
        SetStateBits(c, false, false);
        return;
    }
    // pixels -> the UI camera's units at depth 1 (pixel = centre + scale x xy / -z; the box sits at the origin)
    const float kx = g_hotCam[2], ky = g_hotCam[3];
    const float pcx = (s.x0 + s.x1) * 0.5f, pcy = (s.y0 + s.y1) * 0.5f;
    const float pos[3] = {(pcx - g_hotCam[0]) / kx, (pcy - g_hotCam[1]) / ky, -1.0f};
    const float half[3] = {(s.x1 - s.x0) * 0.5f / fabsf(kx), (s.y1 - s.y0) * 0.5f / fabsf(ky), 0};
    SetPosition(c, pos);
    Write(c + kBox, half, sizeof(half));
    SetShownMsg(c, true);
    SetStateBits(c, true, true);
}

int SetHudHotspots(const HudHotspot* spots, int count)
{
    g_hotCalled = g_hotFrame;
    bool want = false;
    for (int i = 0; spots && i < count && i < kHot && !want; ++i)
        want = spots[i].shown != 0;
    if (!g_hotBox && (!want || !CreateHotspots()))
        return g_hotFailed ? 0 : kHot;
    float cam[4] = {};
    __try
    {
        cam[0] = Mem<float>(exe::UiCamera, 0x68), cam[1] = Mem<float>(exe::UiCamera, 0x6C);
        cam[2] = Mem<float>(exe::UiCamera, 0x78), cam[3] = Mem<float>(exe::UiCamera, 0x7C);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
    if (fabsf(cam[2]) < 1 || fabsf(cam[3]) < 1)
        return kHot; // no UI camera yet: nothing placed this frame
    const bool camChanged = memcmp(cam, g_hotCam, sizeof(cam)) != 0;
    memcpy(g_hotCam, cam, sizeof(cam));
    g_hotAny = false;
    for (int i = 0; i < kHot; ++i)
    {
        HudHotspot s = {};
        if (spots && i < count)
            s = spots[i];
        s.shown = s.shown ? 1 : 0;
        if (camChanged || memcmp(&s, &g_hotSet[i], sizeof(s)) != 0)
        {
            HotApply(i, s);
            g_hotSet[i] = s;
            ++g_hotApplied;
        }
        g_hotAny |= s.shown != 0;
    }
    return kHot;
}

int HudHotspotHovered()
{
    if (!g_hotBox || !g_hotAny || !exe::HoveredControl.ok())
        return 0;
    const u32 h = R32(exe::HoveredControl.get());
    for (int i = 0; h && i < kHot; ++i)
        if (g_hot[i] == h && g_hotSet[i].shown)
            return kUiBtnHud2 + i;
    return 0;
}

// Once per frame (OnFrame): a set the logic stopped renewing is hidden; every 2 s the box is checked against the game's
// control list (the destructor slot normally tells first).
static void HotFrame()
{
    ++g_hotFrame;
    if (!g_hotBox)
        return;
    const ULONGLONG now = GetTickCount64();
    if (now - g_hotChecked >= 2000)
    {
        g_hotChecked = now;
        if (!InList(g_hotBox))
        {
            HotForget("not in the control list");
            return;
        }
    }
    if (g_hotAny && g_hotFrame - g_hotCalled > 2)
    {
        for (int i = 0; i < kHot; ++i)
            if (g_hotSet[i].shown)
                HotApply(i, HudHotspot{}), g_hotSet[i] = HudHotspot{};
        g_hotAny = false;
        Log("HUD 2 hotspots: hidden (the logic stopped renewing them)");
    }
}

// Debug `uibtn click <100 + slot>`: the game's own action of that button (vt[0xB4] -> 0x26 to the box -> the logic).
static bool HotDebugClick(int slot)
{
    if (slot < 0 || slot >= kHot || !g_hotBox || !g_hot[slot] || !g_hotSet[slot].shown)
        return false;
    __try
    {
        void* const* vt = *reinterpret_cast<void* const* const*>(g_hot[slot]);
        if (!exe::IconButtonActivate.is(reinterpret_cast<uintptr_t>(vt[kVtActivate])))
            return false;
        reinterpret_cast<void(__thiscall*)(u32)>(vt[kVtActivate])(g_hot[slot]);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static std::string HotStatus()
{
    int shown = 0;
    for (int i = 0; i < kHot; ++i)
        shown += g_hotSet[i].shown;
    char b[256];
    sprintf_s(b, "hud2 hotspots: box=%08X failed=%d shown=%d/%d applied=%u clicks=%u hovered=%d renewed %u frame(s) ago\n",
              g_hotBox, g_hotFailed, shown, kHot, g_hotApplied, g_hotClicks, HudHotspotHovered(),
              g_hotFrame - g_hotCalled);
    std::string s = b;
    for (int i = 0; i < kHot; ++i)
        if (g_hotSet[i].shown)
        {
            float cx = 0, cy = 0, hw = 0, hh = 0;
            const bool p = Project(g_hot[i], &cx, &cy, &hw, &hh);
            sprintf_s(b, "  %2d %08X want (%.0f,%.0f)-(%.0f,%.0f) placed %s(%.0f,%.0f)-(%.0f,%.0f)\n", i, g_hot[i],
                      g_hotSet[i].x0, g_hotSet[i].y0, g_hotSet[i].x1, g_hotSet[i].y1, p ? "" : "?", cx - hw, cy - hh,
                      cx + hw, cy + hh);
            s += b;
        }
    return s;
}

// ------------------------------------------------------------------ the API
static Btn* ById(int id)
{
    switch (id)
    {
    case kUiBtnBar: return &g_bar;
    case kUiBtnOptionsPage: return &g_opt;
    case kUiBtnPauseFlvibed: return &g_pause;
    }
    return nullptr;
}

void SetUiButton(int id, bool shown, bool lit, bool enabled)
{
    Btn* b = ById(id);
    if (!b)
        return;
    b->shown = shown, b->lit = lit, b->enabled = enabled;
    if (!b->ctl)
        return;
    if (b == &g_bar)
        ApplyBarState(); // re-applied after the game's layout each frame
    else if (b == &g_opt)
        ApplyOptState();
    else
        ApplyPauseState();
}

bool UiButtonRect(int id, float* cx, float* cy, float* hw, float* hh)
{
    const Btn* b = ById(id);
    if (!b || !b->ctl || !b->shown)
        return false;
    if (b == &g_bar && !g_barWantFrame)
        return false; // hidden with the bar (no game button shown)
    if (!(R8(b->ctl + kState) & 1))
        return false;
    return Project(b->ctl, cx, cy, hw, hh);
}

void OnFrame()
{
    HotFrame();
    // the Options caption's draw transform follows the window only once it has been placed (the frame after
    // InitElements): re-place it then, outside the UI pass (a transform changed inside the pass crashed the game)
    if (!g_optPlaced && g_opt.text && InList(g_opt.text))
    {
        float p[3];
        if (ReadVec(g_opt.text, kPos, p))
            SetPosition(g_opt.text, p);
        g_optPlaced = true;
    }
    // a window the game destroyed took our control with it: forget it before anything uses the pointer this frame
    if (g_opt.ctl && !InList(g_opt.ctl))
        g_opt.ctl = g_opt.text = 0, g_opt.parent = 0, g_optShown = false;
    // the Options screen slides its page buttons in from the left (the main menu's entry, ~0.9 s): what it animates is
    // each button's frame translation (vt[0x14] = exe::IconButtonSetFrame with x running -1.256 -> 0 at z -1.0, +0xA8
    // untouched; traced 25 Oct 2026 with a write watch on +0x60), and only on the controls it made itself - ours kept
    // the final frame and sat still (INT-PAGE's screenshot); a plain write of the frame bytes moved nothing either (the
    // icon button places its mesh from its own copy at +0x2E0). Ours is given the GENERAL button's frame through the
    // same setter whenever it differs, so it slides with them.
    if (g_opt.ctl && g_opt.parent && exe::IconButtonSetFrame.ok())
    {
        const u32 sibling = R32(g_opt.parent + kPrefInterface);
        float fs[3], fo[3];
        if (sibling && exe::VtIconButton.is(Vt(sibling)) && ReadVec(sibling, kBtnFrame, fs) &&
            ReadVec(g_opt.ctl, kBtnFrame, fo) && memcmp(fs, fo, sizeof(fs)) != 0)
        {
            __try
            {
                void* const* vt = *reinterpret_cast<void* const* const*>(g_opt.ctl);
                if (exe::IconButtonSetFrame.is(reinterpret_cast<uintptr_t>(vt[0x14 / 4])))
                    reinterpret_cast<SetFrameFn>(vt[0x14 / 4])(g_opt.ctl, fs);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }
    if (g_pause.ctl && !InList(g_pause.ctl))
        g_pause.ctl = g_pause.text = 0, g_pause.parent = 0;
    if (g_bar.ctl && !InList(g_bar.ctl))
        g_bar.ctl = 0, g_bar.parent = 0, g_barPart = 0, g_barWantFrame = false;
}

bool DebugClick(int id)
{
    if (id >= kUiBtnHud2 && id < kUiBtnHud2 + kUiBtnHud2Count)
        return HotDebugClick(id - kUiBtnHud2);
    const Btn* b = ById(id);
    if (!b || !b->ctl)
        return false;
    UiButtonClicked(id);
    return true;
}

std::string Status()
{
    char buf[1024];
    auto line = [&](const Btn& b)
    {
        char l[256];
        float cx = 0, cy = 0, hw = 0, hh = 0;
        const bool rect = UiButtonRect(b.id, &cx, &cy, &hw, &hh);
        sprintf_s(l, "%s: ctl=%08X text=%08X parent=%08X shown=%d lit=%d enabled=%d created=%d rect=%s(%.0f,%.0f half %.0fx%.0f)\n",
                  b.what, b.ctl, b.text, b.parent, b.shown, b.lit, b.enabled, b.created, rect ? "" : "none",
                  cx, cy, hw, hh);
        return std::string(l);
    };
    std::string s = line(g_bar) + line(g_opt) + line(g_pause);
    sprintf_s(buf, "options page state=%d (ours shown=%d)\nbar layout: %s\nhooks: layout=%p framesize=%p mbmsg=%p prefsinit=%p prefsmsg=%p pauseinit=%p pausemsg=%p\n",
              OptionsPageState(), g_optShown, g_lastPlace, g_origLayout, g_origFrameSize, g_origMbMessage,
              g_origPrefsInit, g_origPrefsMessage, g_origPauseInit, g_origPauseMessage);
    return s + buf + HotStatus();
}

void Install()
{
    // ManeuverLayout: sub esp,34h; push ebp; push esi; mov ebp,ecx (7 bytes); chained after the subtitles host's hook
    g_origLayout = static_cast<ManeuverLayoutFn>(
        hk::Detour(exe::ManeuverLayout, reinterpret_cast<void*>(ManeuverLayoutHook), 7, "the bar's autopilot button"));
    // ManeuverFrameSize: mov eax,[esp+4]; sub esp,10h (7 bytes)
    g_origFrameSize = static_cast<FrameSizeFn>(hk::Detour(exe::ManeuverFrameSize,
                                                          reinterpret_cast<void*>(ManeuverFrameSizeHook), 7,
                                                          "the bar's frame widened for the autopilot button"));
    // ManeuversMessage: cmp dword ptr [esp+4],26h (5 bytes)
    g_origMbMessage = static_cast<MessageFn>(hk::Detour(exe::ManeuversMessage, reinterpret_cast<void*>(ManeuversMessageHook),
                                                        5, "the autopilot button's click"));
    // PrefsInitElements / PrefsMessage / PauseInitElements: mov eax,fs:[0] / sub esp,144h (6 bytes)
    g_origPrefsInit = static_cast<InitElementsFn>(hk::Detour(exe::PrefsInitElements,
                                                             reinterpret_cast<void*>(PrefsInitElementsHook), 6,
                                                             "the FLVIBED page button of the Options screen"));
    g_origPrefsMessage = static_cast<MessageFn>(hk::Detour(exe::PrefsMessage, reinterpret_cast<void*>(PrefsMessageHook), 6,
                                                           "the FLVIBED page button's click and page switch"));
    g_origPauseInit = static_cast<InitElementsFn>(hk::Detour(exe::PauseInitElements,
                                                             reinterpret_cast<void*>(PauseInitElementsHook), 6,
                                                             "the pause menu's FLVIBED button"));
    // PauseMessage: mov eax,[esp+4]; cmp eax,26h (7 bytes)
    g_origPauseMessage = static_cast<MessageFn>(hk::Detour(exe::PauseMessage, reinterpret_cast<void*>(PauseMessageHook), 7,
                                                           "the pause menu's FLVIBED button's click"));
    Log("UI buttons: layout=%p framesize=%p mbmsg=%p prefsinit=%p prefsmsg=%p pauseinit=%p pausemsg=%p", g_origLayout,
        g_origFrameSize, g_origMbMessage, g_origPrefsInit, g_origPrefsMessage, g_origPauseInit, g_origPauseMessage);
}
} // namespace uibtn
