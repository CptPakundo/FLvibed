// The door between core and HUD 2 (hud_door.h).
#include "ui/hud_door.h"

#include "game/hostcalls.h"
#include "game/modslots.h"
#include "ui/hud_rows.h"
#include "ui/winplates.h"

namespace huddoor
{
// The door's entries, for the counter's report (the extras first: they are not HUD 2's).
enum Entry : int
{
    kCues,
    kGameText,
    kCursor,
    kHoverHelp,
    kExtraUnder,
    kExtraOver,
    kExtras, // (the first of HUD 2's)
    kFlight = kExtras,
    kSkin,
    kUnderBegin,
    kUnderEnd,
    kOverlay,
    kStrip,
    kTop,
    kHotspot,
    kLockNotice,
    kNoteAction,
    kPageOn,
    kSkinOn,
    kNNStrip,
    kStripRight,
    kStripBottom,
    kEntries
};
static const char* const kNames[kEntries] = {"Cues",     "GameText",   "Cursor",     "HoverHelp", "Under",      "Over",
                                             "Flight",   "Skin",       "UnderBegin", "UnderEnd",  "Overlay",    "Strip",
                                             "Top",      "Hotspot",    "LockNotice", "NoteAction", "PageOn",    "SkinOn",
                                             "NNStrip",  "StripRight", "StripBottom"};

static Hud2Entry g_entry = {};
static bool g_entered = false; // HUD 2 selected: between its Enter and its Leave
static bool g_leaving = false; // its Leave not done yet
static u32 g_leaveFrames = 0, g_enters = 0, g_leaves = 0;

// While the classic HUD is selected: the door's calls per entry, and every call into the HUD module through its views (core's and the other
// modules', modslots::g_viewCalls) less the extras' and the door's own Enter / Leave.
static u32 g_door[kEntries] = {};
static u32 g_extrasSince = 0;   // the extras' calls since the last frame step
static u32 g_lastCalls = 0;     // the module's view calls at the last frame step
static bool g_wasClassic = false;
static u32 g_classicFrames = 0;
static unsigned long long g_classicModule = 0; // the HUD module's calls other than the extras, over the frames classic at both ends

// A load runs inside one game frame (core's frame, where Frame decides, does not run in it), so each of its frames asks hud_style itself: a style written
// during the load (the owner's decision 63: "HUD 2 and classic HUD must be separate") gives the game's own screen from the next load frame on, HUD 2's
// screen given back at once; the rest of the leave follows at core's next frame. Once the game's screen showed, that load stays the game's to its end (no
// frame of HUD 2 after a classic one). A switch to HUD 2 during a load is HUD 2's after the load: core's frame runs inside a load too (seen 13 Nov 2026:
// a style written in a load entered HUD 2 before the load's last frame), so the door does not enter while load frames still come (Loading).
static u32 g_loadHud2 = 0, g_loadGame = 0, g_loadRefused = 0, g_loadSeq = 0;
static bool g_loadGameSeen = false, g_loadHud2Drew = false; // (this load's: cleared once it is over)
static DWORD g_loadAt = 0;   // the last load frame
static int g_loadStyle = -1; // the test aid: hud_style written at the 3rd load frame after `huddoor loadstyle`
// A load still runs: a load frame within the last second (its frames came every few tens of ms; a gap of a whole second is no load's). The game's own
// "world loading" byte is not used: who clears it is not known (core\flexe.h WorldLoading).
static bool Loading() { return g_loadAt && GetTickCount() - g_loadAt < 1000; }
static void LoadDraw()
{
    g_loadAt = GetTickCount();
    if (!g_loadAt)
        g_loadAt = 1;
    if (g_loadStyle >= 0 && ++g_loadSeq == 3) // (a load drew 7 to 11 frames, 13 Nov 2026)
        hudrows::g.hudStyle = hudrows::StyleOf(g_loadStyle), g_loadStyle = -1;
    const bool hud2 = g_entered && Style() != 0;
    if (hud2 && g_loadGameSeen)
        ++g_loadRefused; // (HUD 2 selected again inside a load: the game's screen stays)
    g_loadGameSeen |= !hud2;
    const bool draw = hud2 && !g_loadGameSeen;
    ++(draw ? g_loadHud2 : g_loadGame);
    // HUD 2 is called only while it draws, and once more to give its screen back; on the classic HUD the game's screen alone
    if (g_entry.LoadDraw && (draw || g_loadHud2Drew))
        g_entry.LoadDraw(draw);
    g_loadHud2Drew = draw;
}

void Register(const Hud2Entry& entry)
{
    // (a Detach: the module's own Shutdown or Fault gives back; the door forgets it was entered)
    g_entry = entry;
    // the load's frames are the door's to decide while the module runs (the host's load hook; core\host\router.cpp SetLoadDraw)
    if (hostcalls::g.SetLoadDraw)
        hostcalls::g.SetLoadDraw(entry.LoadDraw ? LoadDraw : nullptr);
    if (!entry.Enter)
        g_entered = g_leaving = false;
}

bool Present() { return modslots::g[modslots::kHud] != nullptr; }
int Style() { return hudrows::StyleOf(hudrows::g.hudStyle); }
bool Classic() { return !g_entered; }

// One call through the door: counted while the classic HUD is selected.
static void Note(Entry e)
{
    if (e < kExtras)
        ++g_extrasSince;
    if (Classic())
        ++g_door[e];
}

void Frame()
{
    const bool loading = Loading();
    if (!loading)
        g_loadGameSeen = g_loadHud2Drew = false;
    const u32 calls = modslots::g_viewCalls[modslots::kHud];
    if (Classic() && g_wasClassic)
    {
        const u32 since = calls - g_lastCalls;
        g_classicModule += since > g_extrasSince ? since - g_extrasSince : 0;
        ++g_classicFrames;
    }
    // the decision, once a frame: HUD 2 selected = the module runs and hud_style is not 0; a leave at once, an enter not inside a load (decision 63)
    const bool want = Present() && g_entry.Enter && Style() != 0;
    if (want && !g_entered && !loading)
    {
        g_leaving = false; // (back before a page's close: HUD 2 takes its window up again)
        // HUD 2's own hooks (the loading screen, the target label's mute) at its first selection in the session, never on the classic HUD
        // before it; once in, they pass through while classic (the host keeps them: idempotent)
        if (hostcalls::g.InstallHud2Hooks)
            hostcalls::g.InstallHud2Hooks();
        g_entry.Enter();
        g_entered = true, ++g_enters;
    }
    else if (!want && g_entered)
        g_entered = false, g_leaving = true, g_leaveFrames = 0, ++g_leaves;
    // the leave runs until it answers done; a window that never stops sliding gives it up after 300 frames (5 s at 60 Hz)
    if (g_leaving)
        g_leaving = g_entry.Leave && !g_entry.Leave() && ++g_leaveFrames < 300;
    g_lastCalls = modslots::g_viewCalls[modslots::kHud]; // (after Enter / Leave: the door's own calls are not counted)
    g_extrasSince = 0, g_wasClassic = Classic();
}

// The extras (views::extras): not HUD 2's.
void Cues(float dt, bool flying, bool autopilot)
{
    if (views::extras.Cues)
        Note(kCues), views::extras.Cues(dt, flying, autopilot);
}
void GameText(bool menu)
{
    if (views::extras.GameText)
        Note(kGameText), views::extras.GameText(menu);
}
void Cursor(float dt, bool padOwns, bool flyingFree, bool (*onlyHudOpen)())
{
    if (views::extras.Cursor)
        Note(kCursor), views::extras.Cursor(dt, padOwns, flyingFree, onlyHudOpen);
}

void Flight(float dt, const views::HudContext& c)
{
    if (views::hud.Flight)
        Note(kFlight), views::hud.Flight(dt, c);
}
void Skin(float dt, bool padOwns, bool enabled, bool docked)
{
    if (views::hud.Skin)
        Note(kSkin), views::hud.Skin(dt, padOwns, enabled, docked);
}
// HUD 2's under pass calls the hover help's rule itself, where its frame knows a NeuroNet window on its way and its modal menus; the classic HUD has
// neither (the skin is HUD 2's), so the door hands it free flight - with the pad only: with keyboard and mouse the classic HUD shows the game's
// hover help as the game does (the owner's answer of 13 Nov 2026 to card Q5 of audit 102_, decision 77). With the pad the rule stays: its aim moves
// the game's cursor over the HUD's buttons, and their help is no hover the player meant. (Not in flight the rule allows anyway: called every frame,
// so `hoverhelp` reports this frame's answer.)
bool UnderBegin(void* device, bool flying, bool space, bool padOwns)
{
    if (!views::hud.UnderBegin)
    {
        if (views::extras.HoverHelp)
            Note(kHoverHelp), views::extras.HoverHelp(space && padOwns, false);
        return false;
    }
    Note(kUnderBegin);
    return views::hud.UnderBegin(device, flying);
}
// The under pass's end: HUD 2's glass or the classic pass, then the extras' shield cue over it.
void UnderEnd(void* device, bool shieldCue, bool enabled, bool padOwns)
{
    if (views::hud.UnderEnd)
        Note(kUnderEnd), views::hud.UnderEnd(device);
    else
        winplates::ClassicPass(device, enabled, padOwns, true);
    if (views::extras.Under)
        Note(kExtraUnder), views::extras.Under(device, shieldCue);
}
// The overlay: the extras' cues first, HUD 2's panels over them.
void Overlay(void* device, bool menuOver, bool noUnderPass)
{
    if (views::extras.Over)
        Note(kExtraOver), views::extras.Over(device, menuOver, noUnderPass);
    if (views::hud.Overlay)
        Note(kOverlay), views::hud.Overlay(device);
}
void Strip(void* device)
{
    if (views::hud.Strip)
        Note(kStrip), views::hud.Strip(device);
}
void Top(void* device)
{
    if (views::hud.Top)
        Note(kTop), views::hud.Top(device);
}
void Hotspot(int slot)
{
    if (views::hud.Hotspot)
        Note(kHotspot), views::hud.Hotspot(slot);
}
bool LockNotice()
{
    if (!views::hud.LockNotice)
        return false;
    Note(kLockNotice);
    return views::hud.LockNotice();
}
void NoteAction(unsigned wake)
{
    if (views::hud.NoteAction)
        Note(kNoteAction), views::hud.NoteAction(wake);
}

bool PageOn()
{
    if (!views::hud.PageOn)
        return false;
    Note(kPageOn);
    return views::hud.PageOn();
}
bool SkinOn()
{
    if (!views::skin.On)
        return false;
    Note(kSkinOn);
    return views::skin.On();
}
bool NNStrip()
{
    if (!views::hud.NNStrip)
        return false;
    Note(kNNStrip);
    float r[4];
    return views::hud.NNStrip(r);
}
float StripRight()
{
    if (!views::hud.StripRight)
        return 0;
    Note(kStripRight);
    return views::hud.StripRight();
}
float StripBottom()
{
    if (!views::hud.StripBottom)
        return 0;
    Note(kStripBottom);
    return views::hud.StripBottom();
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "reset")
    {
        std::fill(std::begin(g_door), std::end(g_door), 0u);
        g_classicModule = 0, g_classicFrames = 0;
        g_loadHud2 = g_loadGame = g_loadRefused = 0;
    }
    else if (what == "loadstyle") // TEST ONLY
    {
        int s = -1;
        in >> s;
        g_loadStyle = s >= 0 && s <= 2 ? s : -1, g_loadSeq = 0;
    }
    std::ostringstream o;
    u32 door = 0, extras = 0;
    for (int e = 0; e < kEntries; ++e)
        (e < kExtras ? extras : door) += g_door[e];
    o << "huddoor selected=" << (Classic() ? "classic" : "hud2") << " present=" << (Present() ? 1 : 0) << " hud_style=" << Style()
      << " enters=" << g_enters << " leaves=" << g_leaves << " leaving=" << (g_leaving ? 1 : 0) << " views=" << (views::hud.Flight ? 1 : 0)
      << "\nload frames hud2=" << g_loadHud2 << " game=" << g_loadGame << " refused=" << g_loadRefused << " loadstyle=" << g_loadStyle
      << "\nclassic frames=" << g_classicFrames << " hud2_calls=" << g_classicModule << " door_hud2=" << door << " door_extras=" << extras << "\n";
    for (int e = 0; e < kEntries; ++e)
        if (g_door[e])
            o << "  " << kNames[e] << "=" << g_door[e] << (e < kExtras ? " (extra)" : "") << "\n";
    return o.str();
}
} // namespace huddoor
