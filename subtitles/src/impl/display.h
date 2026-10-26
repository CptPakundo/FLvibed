// Subtitle presentation: settings, timing, line breaking, speaker row, colours, and drawing
// (design and the standards behind it: docs/DISPLAY_PLAN.md).
#pragma once
#include "common.h"
#include "settings.h"

#include <string>
#include <vector>

namespace display
{
enum class Category
{
    Story,  // cutscenes, room scenes
    Comms,  // mission comms and lines addressed to the player
    Bar,    // bar / base speech
    Nn,     // Neural Net
    Chatter // ambient NPC chatter not addressed to the player
};

enum class Relation
{
    Ally,
    Neutral,
    Hostile,
    Computer,
    Player
};

struct Settings
{
    bool enabled = true;
    int size = 1;       // 0 small, 1 medium, 2 large, 3 extra large, 4 huge
    int bgOpacity = 60; // percent
    // In flight: 1 = the space HUD's panel look (user 21 Oct 2026): its violet-navy base, frame line and bevel, the
    // animated scanline sweeping down every 4 s (hud_shipinfo.cmp anim_hud_scanline + refreshtest2.tga, measured in
    // game: research\experience\runs\hud_scanline_1021); 0 = the soft navy panel. Elsewhere always the soft navy panel
    // (user: "when not in space, they can keep the original background").
    int bgStyle = 1;
    bool shadow = true;
    int textColor = 0; // 0 white, 1 yellow
    int nameMode = 0;  // 0 own row, 1 inline "Name: text", 2 off
    bool story = true, comms = true, bar = true, nn = true;
    int chatter = 0;      // 0 chat window, 1 subtitles, 2 off
    bool chatAll = false; // also copy story / mission / Neural Net lines into the chat window (history)
    // Bottom edge of the lowest row, as a fraction of the screen height from the bottom, per context:
    // bases and menus; in space above the HUD's bottom panels; in cutscenes inside the lower letterbox bar.
    // Bases: room scenes are letterboxed like cutscenes and rooms have no bottom UI, so the same low spot.
    float bottomBase = 0.025f, bottomSpace = 0.305f, bottomCutscene = 0.025f;
    float bottomMovie = 0.025f; // movies: bottom centre, inside the lower letterbox bar of the intro
    // In flight: 2 (default since 21 Oct 2026, user) a HUD panel at the very top centre that slides in from the top edge
    // while lines show; the maneuver bar moves to the top left for it (at the next game load; until it has moved, 1
    // applies). 1 the top third below the maneuver bar; 0 between the bottom HUD panels.
    int spacePosition = 2;
    // Narrow screens (4:3, 5:4: the room between the moved bar and the NeuroNet box holds < 26 characters a row) with
    // space_position 2 (user, 21 Oct 2026: "move the bar left ... the best compromise between keeping the subtitles at
    // the top and giving them the breathing room they need"): 1 the panel at the very top in that room; 2 the same
    // with the small font; 3 the panel just below the top HUD row at full width; 4 the bar scaled down to mirror the
    // NeuroNet box (user's idea, narrow screens only) and the panel at the very top in the wider room; 5 the tab layout
    // (user's idea): the first name in a tab at the top between bar and NeuroNet box, the rows in a body below the top
    // HUD row, wide under the bar's lower right; 0 the bar stays centred (position 1).
    int spaceNarrow = 5; // the user's pick (21 Oct 2026, from the 4:3 comparison of layouts 0-5)
    float linger =
        1.0f; // seconds a line stays after its audio (0.6 until 21 Oct 2026: user, "fade away a bit too quickly")
    // seconds a line pushed out by a newer speaker (the two-speaker cap) stays on, at least until it has been up for its
    // reading minimum (0.3 s per word, at least 1.25 s, rounded up to 0.25 s) and never past its own end; 0 = at once. 1.25 since 22 Oct 2026 (user: "just
    // a tiny bit more time for subtitles that get kicked away by a new one"; 1.0 before)
    float retireDelay = 1.25f;
    // letterboxed scenes (one speaker at a time): a line kicked up by the next fades at once, over 0.3 s + this (user,
    // 22 Oct 2026: 0 after the Donau cutscene, then "a tiny bit more time": 0.35, then "disappeared a tiny bit too
    // quickly": 0.6, rounded to the 250 ms steps: 0.75 - the fade then 0.5 + 0.75 = 1.25 s)
    float retireDelayCinematic = 0.75f;
    int font = 0;           // 0 the game's own fonts (DATA\FONTS: names Agency FB, text Arial Unicode MS), 1 Segoe UI
    bool motion = true;     // lines fade in / out and the stack glides (TLOU2 / Star Wars Outlaws style); 0 = instant
    bool nnSeverity = true; // Neural Net lines coded by severity: colour + brightness pulses (danger 2, warning 1)
    // Letterboxed scenes (room scenes, space cutscenes; the bar is read from the game's own letterbox math, so no bars -
    // a mod, a future HDE option - means no special placement): 0 = inside the lower bar with inline names and no
    // backing (the black bar is the contrast), 1 = resting on the bar's top edge inside the picture with the name row
    // and the soft backing (Star Wars Outlaws). docs\SUBTITLE_PRACTICES.md.
    int letterboxMode = 0;
    bool cinematicSingle = true; // letterboxed scenes show one speaker at a time (a new speaker retires the previous)
    // 1 = a speaker hostile to the player gets the HUD contact list's red triangle before the name (21 Oct 2026). Off
    // by default since 22 Oct 2026 (user: keep hostility to the name colour, "without adding that triangle. It may have
    // confusing connotations"). DISPLAY_PLAN.md "Speaker colours".
    bool hostileMarker = false;
    // In flight the off-screen target arrows (current target, non-selected, waypoint, loot, player) are pushed in from
    // the screen edge, along their direction from the centre, until clear of the HUD's panels, the maneuver bar, the
    // NeuroNet box and the subtitles (user, 21 Oct 2026: "dynamically confined between the static HUD elements"); 0 =
    // where the game puts them.
    bool hudArrows = true;
    // In flight with the top panel: the game's comm window (the speaker's live head, or its generic radio-tower icon) is
    // placed in a slot at the panel's left - in the name tab on narrow screens - while the game shows it (user, 21 Oct
    // 2026: "have the portrait moved INSIDE the subtitle box"); the game's caption under it is hidden (the panel names the
    // speaker). 0 = where the game puts it (below the moved maneuver bar).
    bool commPortrait = true;
    // In flight with the portrait in the panel: when the game closes its comm window (the voice is done), the line it
    // was opened for fades out right after the channel-closed animation (never before its reading minimum) instead of
    // lingering (user, 21 Oct 2026: "make it work like that for all space dialogue"). 0 = the usual linger.
    bool fadeWithChannel = true;
    // The word being spoken grows slightly (lines with word timings; user, 22 Oct 2026: an optional "pop", "visually
    // consistent with the game"). 0 = still text.
    bool wordPop = true;
    // Pages turn on the speaker's words (the review's forced alignment, word_times.tsv). 0 = pages spread over the time
    // on screen (the estimate), and no pop.
    bool wordTiming = true;
    // The chat log's NPC lines ("name · faction") on narrow screens (4:3, 5:4): 1 = a header row with the name and the
    // faction, the dialogue under it at full width (user, 22 Oct 2026: kept after an in-game comparison with the column); 0 = the column
    // layout as elsewhere (the name over the faction, the dialogue beside them).
    int chatLayoutNarrow = 1;
    // the same choice on wider screens (16:10 and wider): 1 = the header row (default since the user's 16:9 comparison,
    // 22 Oct 2026), 0 = the column
    int chatLayoutWide = 1;
    // The UI build (25 Oct 2026, docs\UI_SPEC.json; the defaults = the spec's `def`). Story speakers' names in their own
    // colours (data\speaker_colors.tsv); 0 = every name by the speaker's relation to the player only.
    bool speakerColors = true;
    // Movies (movie.cpp): the picture fitted to the screen (aspect kept, the encoded black bars zoomed out of view);
    // the startup movies skipped natively (1) or played (0); which intro plays (0 as installed; 1 HD, 2 the original
    // 2003, 3 the full Bonus DVD intro - DATA\MOVIES files through the host's intro override, movie.cpp ApplyIntro: a
    // missing file falls back to 0); subtitles over them.
    bool movieFit = true;
    int movieSkip = 0;
    int movieIntro = 0;
    bool movieSubtitles = true;
};

// Neural Net message severity, from the line's id (language-neutral): 0 info, 1 warning, 2 danger.
int NnSeverity(const std::string& id);
// A Neural Net line HUD 2 shows as its own alert card while it shows with hud_alerts on (the HUD bus's `alerts`): the
// line is left out of the Neural Net column - one place per event (shield failed / destroyed, hull breach, incoming
// missile / torpedo, cruise disrupted). Every other Neural Net line shows as before; HUD 2 absent = all of them.
bool Hud2ShowsAlert(const std::string& id);

Settings& Get();
void LoadSettings(const std::string& iniPath);
void SaveSettings(const std::string& iniPath);
std::string Describe();
// Every setting with its value and help (debug `settings`; the registry core\settings.h, display.cpp kDefs).
std::string SettingsReference();
// "key value" -> applies one setting; false if unknown.
bool Set(const std::string& key, const std::string& value);
// The registry rows (kDefs) for the settings bus (core\settings_bus.h; the options page lists and changes them).
const settings::Def* Defs(size_t& count);
// A row was changed from outside (the bus): the reactions Set has (a font rebuild on font / word_pop / size).
void OnSettingApplied(const settings::Def* def);

// Shows a line now for `seconds` of audio (the reading-speed minimum is applied).
// `severity`: Neural Net lines (NnSeverity). `portrait`: a comm the game shows with its comm window (to the player, with
// a costume): the panel's portrait slot lines up with this line. `wordStarts`: when each whitespace word of `text` is
// spoken (seconds from now, forced alignment): pages turn as their first word is spoken; null = estimated.
void Show(Category cat, const std::wstring& speaker, Relation rel, const std::wstring& text, float seconds,
          int severity = 0, bool portrait = false, const std::vector<float>* wordStarts = nullptr);
void Clear();
// Adds characters to the font atlases' set (ASCII is always in), so a line using them never triggers a mid-game GDI
// rebuild: called at data load with every character of the shipped data and of the runtime's own punctuation. A
// character still unseen rebuilds on its first line as before.
void SeedGlyphs(const std::wstring& chars);
// Debug `pages`: each shown line's pages (t0-t1 from its start, word-timed or estimated, rows) and its age.
std::string PagesReport();
// A scene is over (a new bar conversation started): its dialogue lines fade out now (Neural Net lines stay).
void EndScene();
void Update(double dt);
// ownScene: true from Present (outside the game's scene), false from the UI pass.
void Draw(bool ownScene);
// Draws into premultiplied BGRA pixels (w x h) for a movie (movie.cpp: a child window of the game window, else a layered
// window over it). Only the blocks' rectangles are written (the caller clears the last frame's rectangles).
void DrawMovie(u32* pixels, int w, int h);
// What the last DrawMovie drew: each block's rectangle (its backing with the feather and the shadow; pixels), a key of
// everything drawn (texts, positions, opacities: unchanged = the same picture) and whether a transition runs (a fade,
// a glide: redraw at the display's rate).
struct MovieFrame
{
    std::vector<RECT> rects;  // per line: everything it drew
    std::vector<RECT> boxes;  // per line: its backing box's rectangle (drawn or not)
    std::vector<double> ids;  // per line: its id (start time) for SetMovieBox
    std::vector<float> box;   // per line: the backing drawn (0 none .. 1 full)
    unsigned long long key = 0;
    bool animating = false;
};
const MovieFrame& LastMovieFrame();
// The movie's picture in the drawing's pixels (empty: the whole frame) and whether the target blends a translucent box
// over it (the layered window: yes; the child window: no - a box is then on or off, a hard cut). A line whose box lies
// wholly outside the picture (on the letterbox bars) gets no box.
void SetMoviePicture(const RECT& picture, bool translucent);
// A line inside the picture: its box from how busy the picture under it is (0 none .. 1 the full box; movie.cpp samples
// the video). Unset = 1 (legibility first).
void SetMovieBox(double id, float target);
// a movie line inside the picture with no SetMovieBox target: 0 no box (the default since 26 Oct 2026), 1 the full box
void SetMovieBoxDefault(float target);
// Something to show right now.
bool Active();
void OnDeviceLost();
// In Present, before any overlay: the comm portrait's picture while it is in the panel (held when the game closes it).
void CapturePortrait();
// Puts the HUD controls the logic changed (the scaled maneuver bar) back: before the logic unloads.
void RestoreHud();
// The game has just laid the maneuver bar out (every frame in flight): rescales it for space_narrow 4.
void OnManeuverLayout();
std::string ActiveText();
// The geometry of the last frame drawn (debug `layout`): screen, font and row metrics, the placement context and
// anchor, and every block's top / height / widest row in pixels - for tests across display modes.
std::string LayoutReport();
// The flight HUD's off-screen arrows kept clear of its panels and of the subtitles (hud_arrows): a target indicator
// part about to be drawn (exe::HudDrawPart, `this` = callerEbp); true with `out` = the position to draw it at.
bool ConfineHudArrow(u32 callerEbp, const void* part, const float* pos, float* out);
// Debug `perf`: the module's CPU time per frame per entry point since the last report (and resets it).
std::string PerfReport();
// Debug `arrowtrace start | dump`: the current-target arrow's screen point per frame (game / drawn) into `file`.
std::string ArrowTrace(const std::string& arg, const std::string& file);
// Speaker colour for a relation as 0xRRGGBB (for the chat window).
u32 NameRgb(Relation r);
// A speaker's name colour as 0xRRGGBB: Trent amber, a named story character's own colour, else by relation.
u32 SpeakerRgb(const std::wstring& speaker, Relation rel);
// Loads the game's own font files (DATA\FONTS\fonts.ini) for this process, once; false if none loaded.
bool GameFonts();
// HUD 2 (core\hud_bus.h; display.cpp "HUD 2"): the chat window's row width in px (the lower-left panels' width, or
// HUD 2's chat band while it shows).
float ChatRowMax(int sw, int sh);
// HUD 2's comm feed: an ambient chatter line drawn in HUD 2's chat band instead of the game's chat window (false: HUD 2
// does not show - the caller prints it to the chat window as before).
bool Hud2Feed(const std::wstring& speaker, u32 rgb, const std::wstring& text, float seconds);
// Debug `hud2sim on|off`: publish the mock-up's HUD 2 layout on the bus (a real publisher wins).
std::string Hud2Sim(const std::string& arg);
} // namespace display
