#include "display.h"

#include "flexe.h"
#include "log.h"
#include "settings.h"
#include "chat.h"
#include "data.h"
#include "feed.h"
#include "speakers.h"
#include "game/gamefont.h"
#include "game/render.h"
#include "game/room.h"
#include "game/textfit.h"
#include "game/uidraw.h"

#include "../api.h"
#include "hud_bus.h"

#include <deque>
#include <sstream>
#include <vector>

extern const HostApi* g_host;

namespace display
{
static Settings g_set;
static render::Font g_font;     // the text
static render::Font g_nameFont; // speaker names on their own row
// The spoken word's pop (word_pop; user, 22 Oct 2026: "subtly 'popping' words being said ... keep it as visually
// consistent with the game as possible"): with the line's word timings the word being spoken grows to kPopScale over
// kPopIn from its start and settles over kPopOut after the next word starts (smoothstep), centred on its own place (the
// row never reflows). The size is continuous: the text font rasterised at kPopScale, drawn scaled down to the moment's
// size with linear filtering (render::TextScaled). Tried first, 22 Oct 2026 (grabs of King's M01 line): a crossfade of
// two sizes read as a grey double image; four solid size steps "still looked a bit choppy" (user). The game's HUD text
// has no brighter state than its white, so size is the cue; the yellow text colour (text_color 1) also lifts toward white.
static constexpr float kPopScale = 1.08f, kPopIn = 0.12f, kPopOut = 0.20f;
static render::Font g_popFont; // the text font at kPopScale
static std::wstring g_chars;   // every character the font must hold

// Font em height as a share of the screen height per size option (docs/DISPLAY_PLAN.md).
static const float kSizes[] = {0.030f, 0.036f, 0.044f, 0.054f, 0.072f};
static const char* kSizeNames[] = {"small", "medium", "large", "extra-large", "huge"};
static const int kMaxChars = 42; // line length limit (XAG 104: ~40; BBC 37-38)

Settings& Get() { return g_set; }

// ------------------------------------------------------------------ settings file
// Every [Subtitles] key declared once (core\settings.h): name, type, field, range, help. The order is the ini's and
// `config`'s. The why of each default is with its field in display.h.
using settings::Bool;
using settings::Float;
using settings::Int;

static const settings::Def kDefs[] = {
    {"enabled", Bool, &g_set.enabled, 0, 0, "subtitles on"},
    {"size", Int, &g_set.size, 0, 4, "text size: 0 small, 1 medium, 2 large, 3 extra-large, 4 huge"},
    {"background_opacity", Int, &g_set.bgOpacity, 0, 100, "backing opacity in percent"},
    {"background_style", Int, &g_set.bgStyle, 0, 1,
     "in flight: 1 the space HUD's panel look, 0 the soft navy panel (used everywhere else)"},
    {"shadow", Bool, &g_set.shadow, 0, 0, "text shadow"},
    {"text_color", Int, &g_set.textColor, 0, 1, "0 white, 1 yellow"},
    {"speaker_names", Int, &g_set.nameMode, 0, 2, "0 the name on its own row, 1 inline 'Name: text', 2 off"},
    {"story", Bool, &g_set.story, 0, 0, "story cutscene lines"},
    {"comms", Bool, &g_set.comms, 0, 0, "mission comms"},
    {"bar", Bool, &g_set.bar, 0, 0, "bar and base conversations"},
    {"neural_net", Bool, &g_set.nn, 0, 0, "Neural Net lines"},
    {"chatter", Int, &g_set.chatter, 0, 2,
     "NPC lines without their comm portrait (chatter, broadcasts): 0 to the chat window, 1 as subtitles where the dialogue "
     "leaves room, 2 off"},
    {"chat_all_dialogue", Bool, &g_set.chatAll, 0, 0,
     "also copy every story / mission / Neural Net line into the chat window (its log holds every spoken line)"},
    {"bottom_base", Float, &g_set.bottomBase, 0, 0.9f,
     "bases and menus: the lowest row's bottom edge, as a share of the screen height from the bottom"},
    {"bottom_space", Float, &g_set.bottomSpace, 0, 0.9f,
     "in space (space_position 0): the same, above the HUD's bottom panels"},
    {"bottom_cutscene", Float, &g_set.bottomCutscene, 0, 0.9f, "cutscenes: the same, inside the lower letterbox bar"},
    {"space_position", Int, &g_set.spacePosition, 0, 2,
     "in flight: 2 a HUD panel at the top centre (the maneuver bar moves left), 1 the top third, 0 between the bottom "
     "panels"},
    {"font", Int, &g_set.font, 0, 1, "0 the game's own fonts, 1 Segoe UI"},
    {"motion", Bool, &g_set.motion, 0, 0, "lines fade in / out and the stack glides; 0 = instant"},
    {"neural_net_severity", Bool, &g_set.nnSeverity, 0, 0,
     "Neural Net lines coded by severity (colour, brightness pulses)"},
    {"bottom_movie", Float, &g_set.bottomMovie, 0, 0.9f,
     "movies: the lowest row's bottom edge, inside the lower letterbox bar"},
    {"linger", Float, &g_set.linger, 0, 5, "seconds a line stays after its audio"},
    {"retire_delay", Float, &g_set.retireDelay, 0, 5,
     "seconds a line pushed out by a newer speaker stays (never below its reading minimum); 0 = at once"},
    {"retire_delay_cinematic", Float, &g_set.retireDelayCinematic, 0, 5,
     "letterboxed scenes: the extra fade delay of a line kicked up by the next (s)"},
    {"letterbox_mode", Int, &g_set.letterboxMode, 0, 1,
     "letterboxed scenes: 0 inside the lower bar (inline name, no backing), 1 on the bar's top edge in the picture"},
    {"cinematic_single_speaker", Bool, &g_set.cinematicSingle, 0, 0, "letterboxed scenes show one speaker at a time"},
    {"hostile_marker", Bool, &g_set.hostileMarker, 0, 0,
     "a hostile speaker gets the HUD's red triangle before the name"},
    {"hud_arrows", Bool, &g_set.hudArrows, 0, 0,
     "in flight the off-screen target arrows are kept clear of the HUD panels and the subtitles"},
    {"space_narrow", Int, &g_set.spaceNarrow, 0, 5,
     "narrow screens (4:3, 5:4) with the top panel: 0-5 layouts, 5 = the name tab with the rows below the top HUD row"},
    {"comm_portrait", Bool, &g_set.commPortrait, 0, 0,
     "in flight the game's comm window sits in a slot inside the subtitle panel"},
    {"fade_with_channel", Bool, &g_set.fadeWithChannel, 0, 0,
     "a comm's line fades right after the game closes its comm window (never before its reading minimum)"},
    {"word_pop", Bool, &g_set.wordPop, 0, 0, "the word being spoken grows slightly (lines with word timings)"},
    {"word_timing", Bool, &g_set.wordTiming, 0, 0,
     "pages turn on the speaker's words (forced alignment); 0 = spread over the time on screen"},
    {"chat_layout_narrow", Int, &g_set.chatLayoutNarrow, 0, 1,
     "chat log NPC lines on narrow screens: 1 a header row with name and faction, 0 the column layout"},
    {"chat_layout_wide", Int, &g_set.chatLayoutWide, 0, 1, "the same on wider screens"},
    // The UI build's rows (docs\UI_SPEC.json SUBTITLES / MOVIES; the page shows them with their words, core\ui_meta.h)
    {"speaker_colors", Bool, &g_set.speakerColors, 0, 0,
     "story speakers' names in their own colours; 0 = every name by the speaker's relation to you"},
    {"movie_fit", Bool, &g_set.movieFit, 0, 0,
     "movies keep their aspect ratio and the encoded black bars are zoomed out of view"},
    {"movie_skip", Int, &g_set.movieSkip, 0, 1, "the startup movies: 0 play, 1 skip (natively, as a finished movie)"},
    {"movie_intro", Int, &g_set.movieIntro, 0, 3,
     "which intro plays: 0 as installed, 1 HD (fl_intro.wmv), 2 original 2003 (fl_intro_vanilla.wmv), 3 full Bonus "
     "DVD (fl_intro_full.wmv), from DATA\\MOVIES - a file the install lacks falls back to 0"},
    {"movie_subtitles", Bool, &g_set.movieSubtitles, 0, 0, "subtitles over the intros and startup movies"},
};
static const size_t kDefCount = sizeof(kDefs) / sizeof(kDefs[0]);

const settings::Def* Defs(size_t& count)
{
    count = kDefCount;
    return kDefs;
}

void LoadSettings(const std::string& ini)
{
    g_set = Settings(); // the defaults, then what the file says (a `config load` undoes live changes)
    settings::Load("Subtitles", ini, kDefs, kDefCount);
}

void SaveSettings(const std::string& ini) { settings::Save("Subtitles", ini, kDefs, kDefCount); }

std::string Describe()
{
    std::ostringstream o;
    o << settings::Describe(kDefs, kDefCount) << "\n(size " << kSizeNames[g_set.size] << ", font " << g_font.px
      << " px " << data::Narrow(g_font.face) << " / " << data::Narrow(g_nameFont.face) << ")";
    return o.str();
}

std::string SettingsReference() { return settings::Reference(kDefs, kDefCount); }

void OnSettingApplied(const settings::Def* d)
{
    if (!d)
        return;
    if (strcmp(d->name, "font") == 0 || strcmp(d->name, "word_pop") == 0)
        g_font.px = -1; // rebuild the fonts (the face / the pop font); a size change rebuilds by its px on its own
}

bool Set(const std::string& k, const std::string& v)
{
    const settings::Def* d = settings::Find(kDefs, kDefCount, k);
    if (!d || !settings::Apply(*d, v))
        return false;
    OnSettingApplied(d);
    return true;
}

// ------------------------------------------------------------------ active subtitles
struct Page
{
    std::vector<std::wstring> rows;
    std::vector<std::vector<int>> rowWords; // per row, per word: its word of the line's text (-1: the inline name)
    float t0, t1;                           // seconds relative to the block start
};

struct Block
{
    std::wstring speaker;
    Relation rel;
    u32 nameArgb = 0;     // the speaker's colour (SpeakerColor)
    bool hostile = false; // the hostile marker before the name (hostile_marker)
    std::wstring text;
    double start, end;
    std::vector<Page> pages; // laid out lazily (font size dependent)
    int layoutPx = 0;
    bool nn = false;       // a Neural Net line: its own corner stack in flight (user, 19 Oct 2026)
    double leaving = -1;   // when it began to fade out (-1: still on); it keeps its slot until gone
    double retireAt = -1;  // pushed out by a newer speaker: fades at this time (-1: not pushed out; retire_delay)
    double readMin = 1.25; // its reading minimum (s)
    double audioEnd = 0;   // when its audio ends if played through
    float y = 0;           // drawn position (top edge), glides to its slot
    bool placed = false;   // y has been set (a new block appears in its slot, fading in)
    int severity = 0;      // Neural Net: 0 info, 1 warning, 2 danger (NnSeverity)
    bool portrait = false; // the game shows this comm with its comm window (the panel's portrait slot lines up here)
    double fadeOut = 0.50; // its fade-out (s): kFadeOut, longer for a line kicked up in a letterboxed scene
    bool snap = false;     // moves to its slot at once next frame (kicked up: no glide through the new line)
    int keepW = 0;         // the row width it was laid out with beside the portrait (kept after the slot goes)
    std::vector<float>
        wordStarts;    // per word of `text`, seconds from start (forced alignment; < 0 unknown); empty: estimated
    bool comm = false; // came through the comm player (comms may overlap: the game plays them at once)
    bool chatter = false; // ambient chatter shown as a subtitle (chatter 1): never displaces a dialogue line
    float movieBox = 1; // a movie line's backing (0 none .. 1 the full box): SetMovieBox, eased (movie.cpp decides)
};

static std::deque<Block> g_blocks;
static void Insert(Block&& b); // a line onto the stack (the speaker cap)

// Motion (studied 19 Oct 2026 at 5 fps in The Last of Us Part II and Star Wars Outlaws, docs/DISPLAY_PLAN.md "Look
// and motion"): a line fades in over ~0.2 s, an ended one fades out over ~0.3 s while keeping its slot, then the rest
// glides into place (eased, settled in ~0.3 s); at most two speakers - a third retires the oldest.
// Every timing in 250 ms steps (user, 22 Oct 2026: "I'd rather we do timings in 250ms increments, so round everything up";
// fade-in 0.18 and fade-out 0.30 before).
static const double kFadeIn = 0.125, kFadeOut = 0.50; // fade-in 125 ms: the channel's opening (user, 22 Oct 2026)
static double Quarter(double s) { return std::ceil(s * 4.0 - 1e-9) / 4.0; } // rounded up to the next 0.25 s
static const float kGlideTau = 0.07f; // exponential ease time constant (s): ~95 % in 0.2 s

static void Retire(Block& b, double now)
{
    if (b.leaving < 0)
        b.leaving = g_set.motion ? now : now - b.fadeOut;
}

static double Now()
{
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) / freq.QuadPart;
}

// The module's own CPU time per frame (debug `perf`; user, 21 Oct 2026: "what are the game performance ramifications?"):
// seconds spent in each entry point since the last report, and the frames counted by Update.
static struct
{
    double update = 0, draw = 0, arrows = 0, layout = 0, worstFrame = 0, frameAcc = 0;
    long frames = 0, arrowCalls = 0;
    double since = 0;
} g_perf;

struct PerfScope
{
    double& acc;
    double t0;
    explicit PerfScope(double& a) : acc(a), t0(Now()) {}
    ~PerfScope()
    {
        const double d = Now() - t0;
        acc += d, g_perf.frameAcc += d;
    }
};

std::string PerfReport()
{
    const double span = Now() - g_perf.since, f = std::max<long>(1, g_perf.frames);
    char s[400];
    sprintf_s(s,
              "frames %ld over %.1f s (%.0f fps)\nper frame (us): update %.1f, draw %.1f, arrows %.1f (%ld calls), bar "
              "layout %.1f, "
              "total %.1f; worst frame %.1f us\n",
              g_perf.frames, span, g_perf.frames / std::max(0.001, span), g_perf.update / f * 1e6,
              g_perf.draw / f * 1e6, g_perf.arrows / f * 1e6, g_perf.arrowCalls, g_perf.layout / f * 1e6,
              (g_perf.update + g_perf.draw + g_perf.arrows + g_perf.layout) / f * 1e6, g_perf.worstFrame * 1e6);
    const double now = Now();
    g_perf = {};
    g_perf.since = now;
    return s;
}

static int Words(const std::wstring& s)
{
    int n = 0;
    bool in = false;
    for (wchar_t c : s)
    {
        const bool w = !iswspace(c);
        n += w && !in;
        in = w;
    }
    return n;
}

static u32 NameColor(Relation r)
{
    switch (r)
    {
    // The HUD contact list's relation colours (measured on the 2020 footage: friendly 61A75A, hostile 8F1F2C - dimmed by
    // the video), lifted to subtitle lightness: generic speakers read as the game shows them (user, 21 Oct 2026). Friendly
    // was light blue until then, next to Juni's sky blue.
    case Relation::Ally: return 0xFF8DDB85;     // friendly green
    case Relation::Hostile: return 0xFFFF7A6B;  // red
    case Relation::Computer: return 0xFF8CF5A8; // green
    case Relation::Player: return 0xFFFFD27A;   // amber
    default: return 0xFFF2E3B3;                 // neutral: warm white
    }
}

u32 NameRgb(Relation r) { return NameColor(r) & 0xFFFFFF; }

// One colour per character (user, 21 Oct 2026): Trent always the player's amber, whichever path his line comes through
// (the M01a Donau cutscene showed him in the ally colour: comm lines); every story speaker its colour from
// data\speaker_colors.tsv (speakers.cpp; a station / ship turned hostile the hostile red); everyone else (open-world
// chatter, factions, the Neural Net) by relation.
static u32 SpeakerColor(const std::wstring& speaker, Relation rel)
{
    if (!g_set.speakerColors) // speaker_colors 0: the relation colour only (the UI build's row)
        return NameColor(rel);
    if (speaker == L"Trent")
        return NameColor(Relation::Player);
    if (rel != Relation::Computer)
        if (const u32 rgb = speakers::Rgb(speaker, rel == Relation::Hostile))
            return 0xFF000000 | rgb;
    return NameColor(rel);
}

u32 SpeakerRgb(const std::wstring& speaker, Relation rel) { return SpeakerColor(speaker, rel) & 0xFFFFFF; }

// Neural Net severity by the line's id tokens (user, 19 Oct 2026 - HUD principles: pre-attentive cues, one meaning each,
// never colour alone): danger = threats to the ship or the mission now; warning = refusals and things to correct;
// info = everything else (navigation, dealers, confirmations). Ids are the same in every language.
int NnSeverity(const std::string& raw)
{
    std::string id = raw;
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    auto has = [&](const char* t) { return id.find(t) != std::string::npos; };
    static const char* kDanger[] = {
        "incoming",   "breach",         "critical",   "destroyed",        "shield_failed", "danger",
        "hostiles",   "mines_detected", "corona",     "atmosphere",       "radiation",     "disrupted",
        "msn_failed", "mission_failed", "obj_failed", "objective_failed", "_lost"};
    static const char* kWarning[] = {"denied",      "disallowed",   "cannot",     "nag_",    "warning",
                                     "out_of_area", "insufficient", "not_enough", "expired", "failure",
                                     "exhausted",   "out_of_range", "drag",       "gases",   "sensor_efficiency",
                                     "anomaly",     "must_have",    "no_place"};
    if (has("restored") || has("activated") || has("granted"))
        return 0;
    // an objective destroyed / completed / reached is the player's success ("info_obj_destroyed": the target)
    if ((has("obj_") || has("objective_")) && !has("failed"))
        return 0;
    if (has("formation_lost"))
        return 1;
    if (has("cmsn_failed")) // "CARGO HAS EXPIRED"
        return 1;
    for (const char* t : kDanger)
        if (has(t))
            return 2;
    for (const char* t : kWarning)
        if (has(t))
            return 1;
    return 0;
}

static bool Cinematic();        // placement section below
static void PlaceManeuverBar(); // the top panel's room, below

void SeedGlyphs(const std::wstring& chars)
{
    bool added = false;
    for (const wchar_t c : chars)
        if (c >= 32 && g_chars.find(c) == std::wstring::npos)
            g_chars += c, added = true;
    if (added)
        g_font.px = -1; // (the next frame builds the atlases with them)
}

void Show(Category cat, const std::wstring& speaker, Relation rel, const std::wstring& text, float seconds,
          int severity, bool portrait, const std::vector<float>* wordStarts)
{
    if (!g_set.enabled || text.empty())
        return;
    const bool on = cat == Category::Story   ? g_set.story
                    : cat == Category::Comms ? g_set.comms
                    : cat == Category::Bar   ? g_set.bar
                    : cat == Category::Nn    ? g_set.nn
                                             : g_set.chatter == 1;
    if (!on)
        return;
    if (cat == Category::Chatter)
    {
        // chatter as subtitles (chatter 1) takes only free room (the owner, 26 Oct 2026: chatter never replaces or covers
        // a story line): with the dialogue stack full it goes to the chat window instead - nothing lost
        size_t dialogue = 0;
        for (const Block& x : g_blocks)
            dialogue += !x.nn && !x.chatter && x.leaving < 0 ? 1 : 0;
        if (dialogue >= (g_set.cinematicSingle && Cinematic() ? 1u : 2u))
        {
            chat::Print(speaker, SpeakerRgb(speaker, rel), text);
            return;
        }
    }
    static const char* kCat[] = {"story", "comms", "bar", "nn", "chatter"};
    feed::Push("sub", kCat[static_cast<int>(cat)], speaker, text);
    if (g_set.chatAll && cat != Category::Chatter)
        chat::Print(speaker, SpeakerRgb(speaker, rel), text);
    for (wchar_t c : speaker + text)
        if (g_chars.find(c) == std::wstring::npos)
        {
            g_chars += c;
            g_font.px = -1; // rebuild with the new character
        }
    Block b;
    b.speaker = speaker;
    b.rel = rel;
    b.nameArgb = SpeakerColor(speaker, rel);
    b.hostile = g_set.hostileMarker && rel == Relation::Hostile && !speaker.empty();
    b.text = text;
    b.start = Now();
    // On screen for the audio plus a short linger, but at least 0.3 s per word (BBC reading speed).
    const double shown = std::max<double>(seconds + g_set.linger, 0.3 * Words(text));
    b.end = b.start + Quarter(std::max(1.25, shown));
    b.readMin = Quarter(std::max(1.25, 0.3 * Words(text)));
    b.audioEnd = b.start + seconds;
    b.nn = cat == Category::Nn;
    b.severity = b.nn ? std::clamp(severity, 0, 2) : 0;
    b.portrait = portrait && !b.nn;
    if (wordStarts && g_set.wordTiming)
        b.wordStarts = *wordStarts;
    b.comm = cat == Category::Comms || cat == Category::Nn;
    b.chatter = cat == Category::Chatter;
    Insert(std::move(b));
}

static void Insert(Block&& nb)
{
    const bool nn = nb.nn;
    // The same speaker going on replaces their previous line in place at once (user, 22 Oct 2026, from a recorded
    // session: "when the same character says another line ... the new line is delayed because of the previous line,
    // and does not show up immediately" - a leaving line keeps its slot while it fades, so the new one sat under it):
    // no grace, no fade, no held slot; different speakers keep the push-out below. The same speaker = the same name
    // before " · " (a bar character is introduced as "Name · Faction", then named alone).
    auto who = [](const std::wstring& s) { return s.substr(0, s.find(L" \x00B7 ")); };
    // Chatter never replaces a dialogue line of the same name (a story character's own chatter voice), and a comm whose
    // voice is still heard is not replaced either - it holds its place until its audio ends (the owner, 26 Oct 2026),
    // then the push-out below treats it as any other line.
    const bool newChatter = nb.chatter;
    const double t = Now();
    if (!nn && !nb.speaker.empty())
        for (auto it = g_blocks.begin(); it != g_blocks.end();)
            it = !it->nn && who(it->speaker) == who(nb.speaker) && !(newChatter && !it->chatter) &&
                         !(it->comm && it->leaving < 0 && t < it->audioEnd - 0.15)
                     ? g_blocks.erase(it)
                     : it + 1;
    g_blocks.push_back(std::move(nb));
    // at most two speakers on screen per stack (dialogue, Neural Net): the oldest still shown fades out; in a
    // letterboxed scene one speaker at a time (cinematic convention: Outlaws, TLOU2 cutscenes), the new line's speaker
    // A pushed-out line is not cut at once (user, 21 Oct 2026: short lines of a quick exchange "appear to move too
    // fast"): it stays retire_delay longer, at least until its reading minimum, never past its own end; one such grace
    // at a time (an earlier pushed-out line still waiting goes now), so at most one block over the cap. A line whose
    // audio the new one cut short (the player skipping through a scene: the next line starts while this one still
    // plays) goes at once, as before (user, 21 Oct 2026: subtitles stayed while spam-skipping).
    // In a letterboxed scene the grace is short (user, 22 Oct 2026, the Donau cutscene: "some subtitles linger even when
    // the next line has started to be spoken ... not expected"; then "just a tiny bit more time for subtitles that get
    // kicked away by a new one": retire_delay_cinematic, 0.35 s): the pushed-out line goes shortly after the new one
    // starts, and never before its reading minimum (over the picture: DrawImpl).
    const double now = Now();
    const bool cinematic = Cinematic();
    const size_t keep = !nn && g_set.cinematicSingle && cinematic ? 1 : 2;
    const double grace = !nn && cinematic ? g_set.retireDelayCinematic : g_set.retireDelay;
    size_t onScreen = 0;
    for (const Block& x : g_blocks)
        onScreen += x.leaving < 0 && x.retireAt < 0 && x.nn == nn ? 1 : 0;
    // chatter shown as subtitles goes first; a chatter line never pushes out a dialogue line (pass 1 only)
    for (int pass = 0; pass < (newChatter ? 1 : 2); ++pass)
    for (Block& x : g_blocks)
        if (onScreen > keep && x.leaving < 0 && x.retireAt < 0 && x.nn == nn && x.chatter == (pass == 0))
        {
            for (Block& y : g_blocks)
                if (y.leaving < 0 && y.retireAt >= 0 && y.nn == nn && !(y.comm && now < y.audioEnd - 0.15))
                    Retire(y, now);
            // a line whose audio still plays: the player skipped ahead (a scene line - the next one stops it), or the game
            // plays two comms at once (user, 22 Oct 2026, the Donau's fall: Newark Station's "Red Alert" to King under the
            // Donau's own line - removed at once, its second page never shown). A comm stays, kicked up, while its voice
            // is heard (pages and all), then fades.
            const bool overlapping = now < x.audioEnd - 0.15;
            if (overlapping && x.comm)
            {
                x.retireAt = x.audioEnd + 0.25;
                --onScreen;
                continue;
            }
            const bool cutShort = overlapping;
            if (!nn && cinematic)
            {
                // a letterboxed scene (user, 22 Oct 2026, footage of the M01a launch: the old line stayed in place at
                // full strength for its grace, the new one under it read as late - "doesn't allow a subtitle to kick the
                // previous one up"): the old line is kicked up at once - gliding up, as the stack glides (a jump read
                // as abrupt: user, same day) - and fades from now, slower by retire_delay_cinematic - unless it has not
                // been readable yet: then it is held up there until it has (the reading minimum), and fades as usual
                if (cutShort || now >= x.start + x.readMin)
                {
                    if (!cutShort)
                        x.fadeOut = kFadeOut + grace;
                    Retire(x, now);
                }
                else
                    x.retireAt = std::min(x.end, x.start + x.readMin);
                --onScreen;
                continue;
            }
            const double at = std::min(x.end, std::max(now + grace, x.start + x.readMin));
            if (cutShort || at <= now)
                Retire(x, now);
            else
                x.retireAt = at;
            --onScreen;
        }
}

void Clear() { g_blocks.clear(); }

std::string PagesReport()
{
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(2);
    const double now = Now();
    for (const Block& b : g_blocks)
    {
        o << data::Narrow(b.speaker) << " age=" << now - b.start << " span=" << b.end - b.start
          << (b.wordStarts.empty() ? " estimated" : " word-timed") << (b.leaving >= 0 ? " LEAVING" : "")
          << (b.retireAt >= 0 ? " retire-in=" + std::to_string(b.retireAt - now) : std::string()) << "\n";
        const float age = static_cast<float>(now - b.start);
        for (const Page& p : b.pages)
        {
            o << (age >= p.t0 && age < p.t1 ? "> " : "  ") << p.t0 << "-" << p.t1 << " ";
            for (const std::wstring& r : p.rows)
                o << " | " << data::Narrow(r);
            o << "\n";
        }
    }
    return o.str();
}

void EndScene()
{
    const double now = Now();
    for (Block& b : g_blocks)
        if (!b.nn && b.leaving < 0)
            Retire(b, now);
}

static void UpdateBanner();    // the top panel's room, below
static void PlaceCommWindow(); // the comm portrait in the top panel, below
static void ReleaseHeldPortrait();
static void Hud2Update();  // HUD 2 on the bus: read once per frame (and the hud2sim publisher), below
static void Hud2Publish(); // where the dialogue panel and the chat column landed, back onto the bus, below

void Update(double)
{
    // a frame's total (the previous frame's entry points) for the worst-frame figure, then this frame's update
    g_perf.worstFrame = std::max(g_perf.worstFrame, g_perf.frameAcc);
    g_perf.frameAcc = 0;
    ++g_perf.frames;
    PerfScope perf(g_perf.update);
    Hud2Update();
    Hud2Publish();
    PlaceManeuverBar();
    UpdateBanner();
    PlaceCommWindow();
    const double now = Now();
    for (Block& b : g_blocks)
        if (b.end < now || (b.retireAt >= 0 && b.retireAt <= now))
            Retire(b, now);
    for (auto it = g_blocks.begin(); it != g_blocks.end();)
        it = it->leaving >= 0 && now - it->leaving >= it->fadeOut ? g_blocks.erase(it) : it + 1;
    if (g_blocks.empty()) // a held portrait's line is gone (the draw no longer runs to notice)
        ReleaseHeldPortrait();
}

std::string ActiveText()
{
    std::string s;
    for (const Block& b : g_blocks)
        if (b.leaving < 0) // fading out: already gone for the reader
            s += data::Narrow(b.speaker) + ": " + data::Narrow(b.text) + "\n";
    return s;
}

// ------------------------------------------------------------------ line breaking
static bool WeakWord(const std::wstring& w)
{
    static const wchar_t* weak[] = {L"a",     L"an",   L"the", L"to",   L"of", L"in",   L"on",  L"at",  L"for",
                                    L"and",   L"with", L"by",  L"from", L"my", L"your", L"our", L"his", L"her",
                                    L"their", L"its",  L"is",  L"be",   L"or", L"but",  L"as",  L"if",  L"no"};
    std::wstring l;
    for (wchar_t c : w)
        l += static_cast<wchar_t>(towlower(c));
    for (const wchar_t* x : weak)
        if (l == x)
            return true;
    return false;
}

static std::vector<std::wstring> SplitWords(const std::wstring& s)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    for (wchar_t c : s)
    {
        if (c == L' ')
        {
            if (!cur.empty())
                out.push_back(cur), cur.clear();
        }
        else
            cur += c;
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

static std::wstring Join(const std::vector<std::wstring>& w, size_t a, size_t b)
{
    std::wstring s;
    for (size_t i = a; i < b; ++i)
        s += (i > a ? L" " : L"") + w[i];
    return s;
}

// Words of an inline "Name:" prefix at the start of the block being laid out (0 = none): no row or page cut
// falls inside it or right after it (the colon would otherwise win as a clause end and leave the name alone
// on a short first row).
static size_t s_nameWords = 0;

static bool InName(size_t k) { return k <= s_nameWords; }

// Best two-row split of words[a,b): both rows fit, the top row a little shorter (bottom-heavy pyramid),
// no row ending in an article/preposition, a clause boundary preferred.
static size_t BestBreak(const std::vector<std::wstring>& w, size_t a, size_t b, int maxW)
{
    size_t best = 0;
    double bestCost = 1e30;
    for (size_t k = a + 1; k < b; ++k)
    {
        // never inside the name; right after it ("Name:" alone on the first row) only when nothing else fits (user, 22
        // Oct 2026: a long "Name · Faction" with a short line had no valid break, and the page was cut after the
        // name's first word - "Agent" alone, the rest uncoloured)
        if (InName(k) && k != s_nameWords)
            continue;
        const int w1 = render::TextWidth(g_font, Join(w, a, k)), w2 = render::TextWidth(g_font, Join(w, k, b));
        if (w1 > maxW || w2 > maxW)
            continue;
        double cost = std::abs(w1 - w2) + (w1 > w2 ? (w1 - w2) * 0.5 : 0);
        if (k == s_nameWords)
            cost += maxW * 4.0;
        if (WeakWord(w[k - 1]))
            cost += maxW * 0.5;
        const wchar_t last = w[k - 1].back();
        if (last == L'.' || last == L'?' || last == L'!' || last == L'\x2026')
            cost -= maxW * 0.7; // a sentence per row reads best
        else if (last == L',' || last == L';' || last == L':' || last == L'\x2013')
            cost -= maxW * 0.3;
        // never split a name ("Fort / Bush", "Planet / Manhattan")
        if (iswupper(w[k - 1][0]) && iswupper(w[k][0]) && iswalpha(last))
            cost += maxW * 0.6;
        if (cost < bestCost)
            bestCost = cost, best = k;
    }
    return best;
}

static bool EndsClause(const std::wstring& word, bool sentenceOnly)
{
    const wchar_t c = word.back();
    if (c == L'.' || c == L'?' || c == L'!' || c == L'\x2026')
        return true;
    return !sentenceOnly && (c == L',' || c == L';' || c == L':' || c == L'\x2013');
}

// Words [a,b) fit on one page: one row, or two rows with a valid break.
static bool FitsPage(const std::vector<std::wstring>& w, size_t a, size_t b, int maxW)
{
    return render::TextWidth(g_font, Join(w, a, b)) <= maxW || BestBreak(w, a, b, maxW) != 0;
}

// Splits words [a,b) (too long for one page) into the fewest pages of about equal length, cutting at
// clause boundaries where possible.
static void SplitEven(const std::vector<std::wstring>& w, size_t a, size_t b, int maxW,
                      std::vector<std::pair<size_t, size_t>>& out)
{
    while (a < b)
    {
        if (FitsPage(w, a, b, maxW))
        {
            out.push_back({a, b});
            return;
        }
        const int total = render::TextWidth(g_font, Join(w, a, b));
        const int pages = std::max(2, (total + 2 * maxW * 9 / 10 - 1) / (2 * maxW * 9 / 10));
        const int target = total / pages;
        // the cut nearest the even share that still fits, preferring a clause end
        size_t best = 0;
        double bestCost = 1e30;
        for (size_t k = a + 1; k < b && (InName(k) || FitsPage(w, a, k, maxW)); ++k)
        {
            if (InName(k))
                continue;
            const int wk = render::TextWidth(g_font, Join(w, a, k));
            double cost = std::abs(wk - target);
            if (EndsClause(w[k - 1], false))
                cost -= maxW * 0.4;
            if (WeakWord(w[k - 1]))
                cost += maxW * 0.5;
            if (cost < bestCost)
                bestCost = cost, best = k;
        }
        if (!best) // nothing fits: one word per page, but the name whole
            best = std::max(a + 1, a < s_nameWords ? s_nameWords + 1 : 0);
        out.push_back({a, best});
        a = best;
    }
}

static void Layout(Block& b, int maxW, bool inlineName)
{
    b.pages.clear();
    std::wstring prefix;
    if ((g_set.nameMode == 1 || inlineName) && !b.speaker.empty())
        prefix = b.speaker + L": ";
    const std::wstring all = prefix + b.text;
    const std::vector<std::wstring> w = SplitWords(all);
    s_nameWords = prefix.empty() ? 0 : SplitWords(prefix).size();
    // Pages follow sentences: whole sentences share a page while they fit in two rows; a sentence too
    // long for one page is cut into even parts at clause boundaries.
    std::vector<std::pair<size_t, size_t>> spans;
    size_t a = 0;
    while (a < w.size())
    {
        size_t e = std::max(a + 1, s_nameWords + 1); // a name ending in "." ("Dr.") is no sentence end
        while (e < w.size() && !EndsClause(w[e - 1], true))
            ++e;
        if (!FitsPage(w, a, e, maxW))
        {
            SplitEven(w, a, e, maxW, spans);
            a = e;
            continue;
        }
        // extend with further whole sentences while they fit
        for (;;)
        {
            size_t f = e + 1;
            if (e >= w.size())
                break;
            while (f < w.size() && !EndsClause(w[f - 1], true))
                ++f;
            if (!FitsPage(w, a, f, maxW))
                break;
            e = f;
        }
        spans.push_back({a, e});
        a = e;
    }
    // Pages are spread over the time on screen (audio + linger) in proportion to their characters. (Timed over the audio
    // alone for a few hours on 22 Oct 2026: the next page then came before the speaker had finished the last one, and a
    // short first page - "Hello." - flashed by; the user preferred this. Exact page times would come from the clip's
    // pauses: HANDOFF open item.)
    const float span = static_cast<float>(b.end - b.start);
    const float total = static_cast<float>(std::max<size_t>(1, all.size()));
    float t = 0;
    // every page of an inline-named line starts with the name (user, 21 Oct 2026: keep the speaker on each part); a
    // later page that no longer fits two rows with it keeps its rows without it
    const std::vector<std::wstring> nameWords(w.begin(), w.begin() + s_nameWords);
    // a word of w as a word of the text (the name prefix: -1), for the spoken-word pop
    auto textWord = [&](size_t i) { return i < s_nameWords ? -1 : static_cast<int>(i - s_nameWords); };
    auto rowsOf = [&](const std::vector<std::wstring>& v, const std::vector<int>& vi, Page& p)
    {
        const auto add = [&](size_t a, size_t e)
        {
            p.rows.push_back(Join(v, a, e));
            p.rowWords.emplace_back(vi.begin() + a, vi.begin() + e);
        };
        if (render::TextWidth(g_font, Join(v, 0, v.size())) <= maxW)
        {
            add(0, v.size());
            return true;
        }
        const size_t k = BestBreak(v, 0, v.size(), maxW);
        add(0, k ? k : v.size());
        if (k)
            add(k, v.size());
        return k != 0;
    };
    for (const auto& sp : spans)
    {
        const size_t s = sp.first, e = sp.second;
        Page p;
        bool done = false;
        if (s > 0 && s_nameWords > 0)
        {
            std::vector<std::wstring> v = nameWords;
            v.insert(v.end(), w.begin() + s, w.begin() + e);
            std::vector<int> vi(s_nameWords, -1);
            for (size_t i = s; i < e; ++i)
                vi.push_back(textWord(i));
            done = rowsOf(v, vi, p);
            if (!done)
                p.rows.clear(), p.rowWords.clear();
        }
        if (!done)
        {
            std::vector<int> vi;
            for (size_t i = s; i < e; ++i)
                vi.push_back(textWord(i));
            const size_t keepName = s_nameWords;
            if (s > 0)
                s_nameWords = 0; // no name in this page: any break is allowed
            rowsOf(std::vector<std::wstring>(w.begin() + s, w.begin() + e), vi, p);
            s_nameWords = keepName;
        }
        float chars = 0;
        for (const std::wstring& x : p.rows)
            chars += static_cast<float>(x.size() + 1);
        p.t0 = t;
        t += span * chars / total;
        p.t1 = t;
        b.pages.push_back(std::move(p));
    }
    if (!b.pages.empty())
        b.pages.back().t1 = span;
    // With the line's word timings (the review's forced alignment, user 22 Oct 2026: "a forced alignment model we could
    // run on the sound files"), a page turns when its first word is spoken: King's 15 s briefing turns on his pauses.
    // The first page shows from the line's start, the last holds through the linger; a page that would last under
    // 0.25 s keeps the estimate's timing (a timing that does not fit the line is not used).
    if (b.pages.size() > 1 && b.wordStarts.size() + s_nameWords == w.size())
    {
        std::vector<float> t0(spans.size(), 0.0f);
        bool ok = true;
        for (size_t k = 1; k < spans.size() && ok; ++k)
        {
            const size_t i = std::max(spans[k].first, s_nameWords) - s_nameWords;
            t0[k] =
                b.wordStarts[i] >= 0 ? b.wordStarts[i] : b.pages[k].t0; // a word the aligner could not place: estimate
            ok = t0[k] >= t0[k - 1] + 0.25f && t0[k] <= span - 0.25f;
        }
        if (ok)
            for (size_t k = 0; k < spans.size(); ++k)
            {
                b.pages[k].t0 = t0[k];
                b.pages[k].t1 = k + 1 < spans.size() ? t0[k + 1] : span;
            }
    }
}

// ------------------------------------------------------------------ drawing

void OnDeviceLost() {}

// The space HUD's panel look (background_style 1) lives in core\game\uidraw.* since 25 Oct 2026 (the UI build: the
// options page shares it); its measurements stayed with it.
static void HudPanel(float x0, float y0, float w, float h, float alpha, float sh, double now)
{
    uidraw::HudPanel(x0, y0, w, h, alpha, sh, now);
}

// Where the subtitles go, from game state (docs/FREELANCER_RE.md "Frame / game state").
struct Placement
{
    float bottom;          // bottom edge of the lowest row, pixels from the top (or the top edge, see fromTop)
    float centerX;         // horizontal centre, pixels
    int maxW;              // row width limit, pixels
    bool fromTop = false;  // `bottom` is the top edge of the highest row: the block hangs down from it
    bool flight = false;   // flying with the HUD: Neural Net lines go to their own corner
    int bar = 0;           // the game's letterbox bar height now (px, 0 = no bars)
    bool inBar = false;    // the block sits inside the lower bar: inline names, no backing (letterbox_mode 0)
    bool topPanel = false; // flight, space_position 2: one HUD panel at the top centre holds the stack (TopPanel)
    bool handOver =
        false; // flight, space_position 2, before the top panel has its room (out of a cutscene: the HUD comes up)
    float panelTop = 0; // its top edge (0 = flush with the screen's top edge; space_narrow 3: below the top HUD row)
    float bodyTop = 0;  // space_narrow 5: the body's top below the top HUD row (the first name in a tab above it)
    float tabHalf = 0;  // space_narrow 5: the half width the tab may take (between the bar and the NeuroNet box)
    // space_narrow 5: the tab's room as it is (from the bar's right edge to the NeuroNet box: wider on the right) and
    // the body's (clear of the cruise box, inside the screen) - the panel moves off centre when its tab needs it
    float tabLeft = 0, tabRight = 0, bodyMinX = 0, bodyMaxX = 0;
    // HUD 2 (core\hud_bus.h) replaces the game's HUD: the room its layout leaves (px). freeL / freeR: the panel's outer
    // edges stay inside (it centres on the screen and moves aside as far as its width needs); limit: a top-anchored
    // stack ends above it (0 = kStackLimit); nn*: the Neural Net column (under HUD 2's target card)
    bool hud2 = false;
    float freeL = 0, freeR = 0, limit = 0, nnLeft = 0, nnTop = 0, nnRight = 0;
};

// The tab's width limit (space_narrow 5): the whole room between the bar and the NeuroNet box when known, else twice the
// narrower side.
static float TabRoom(const Placement& p) { return p.tabRight > p.tabLeft ? p.tabRight - p.tabLeft : 2.0f * p.tabHalf; }

// HUD 2 (section "HUD 2" below): its layout while it replaces the game's HUD (null = the game's HUD: today's placements)
static const HudLayout* Hud2Layout();
static bool Hud2Recent();             // HUD 2 went away within the last 0.3 s
static const HudLayout* Hud2Last();   // its last layout
static Placement PlaceHud2(const HudLayout& l, int sw, int sh, int maxW, int avgChar, float padX);
static bool Shown(const HudRect& r) { return r.x1 > r.x0 && r.y1 > r.y0; }
// The controller's prompt lane on a docked screen (bus version 2): its rectangle while fresh and in use (false = none)
static bool PadLane(HudRect& r, bool& column);
static const HudBus* Bus(); // (Place reads the windows' floor, bus v3)
// The last frame's block extent in a base (Place keeps it off the open window): its height (the extent, not the place:
// the blocks may still glide) and x range (pixels); lineH = the last row height on this screen height (0 unknown).
// False when the last frame drew nothing there.
static bool LastDockedExtent(int sh, float padX, float& h, float& x0, float& x1, float& lineH);

// The game is in a letterboxed scene: the cutscene camera (space story scenes) or the cinematic letterbox amount
// above zero (room scenes run with the base camera, FREELANCER_RE "Base rooms").
static bool Cinematic()
{
    const u32 camera = Mem<u32>(exe::CurrentCamera);
    if (exe::CamCutscene.is(camera))
        return true;
    return exe::IsPlayerInCutscene.ok() && hk::Call<int(__cdecl*)()>(exe::IsPlayerInCutscene) != 0;
}

// The letterbox bar's height in pixels, the way the game computes it this frame (0x41A207: the picture framed to the
// cinematic aspect K, blended by the letterbox amount; JFLP writes K per resolution, so this follows JFLP, a vanilla
// exe, and a mod that leaves the amount at 0 alike). 0 without bars or without the symbols.
static int LetterboxBar(int sw, int sh)
{
    if (!exe::LetterboxAmount.ok() || !exe::CinematicAspect.ok() || sw <= 0 || sh <= 0)
        return 0;
    const float lb = Mem<float>(exe::LetterboxAmount);
    if (!(lb > 0.0f))
        return 0;
    const float a = static_cast<float>(sw) / sh, k = Mem<float>(exe::CinematicAspect);
    const float framed = a + (k - a) * std::min(lb, 1.0f);
    if (!(framed > 0.0f))
        return 0;
    const int bar = static_cast<int>(std::floor((sh - sw / framed) * 0.5f + 0.5f));
    return std::clamp(bar, 0, sh / 2);
}

// The game's objective banner is up: its text control exists only while it shows (found 19 Oct 2026: created at
// 0x4BB270 with the fonts.ini "MissionObjective" font; ~3-6 s per objective step, after loads too). Looked up at most
// every 100 ms (a walk of the UI control list).
static bool BannerShown()
{
    static ULONGLONG at = 0;
    static bool shown = false;
    const ULONGLONG now = GetTickCount64();
    if (now - at >= 100)
    {
        at = now;
        shown = uiclick::FindVisible("MissionObjectiveText") != 0;
    }
    return shown;
}

// ------------------------------------------------------------------ the top panel's room (space_position 2)
// Measured on the flight HUD at 3840x2160 (21 Oct 2026, research\experience\runs\hud_scanline_1021), fractions of H:
static const float kMbMargin = 0.0102f;  // the NeuroNet box's gap to the right edge (22 px): the moved bar mirrors it
static const float kMbHalf = 0.2329f;    // half the maneuver frame's width (1006 px, wings included)
static const float kMbFrameOut = 0.042f; // the frame beyond ManeuverButton3's right side (91 px)
static float g_mbScale = 1.0f;           // the bar's scale in effect (space_narrow 4: ScaleManeuverBar); 1 = as built
static const float kNnFrameOut = 0.020f; // the NeuroNet box beyond its leftmost button's left side (44 px)
static const float kNnBottom = 0.032f;   // the NeuroNet box's lower edge (69 px)
static const float kPanelGap = 0.012f;   // between the panel and its neighbours
// The panel's frame, as the HUD panels draw theirs (FREELANCER_RE "HUD panels"): a ~4 px pale steel-blue line, a dark
// rim outside it, a bevel fading inwards over ~18 px; corners cut 22 x 45 px (wide x tall) where the panel faces the
// screen (the bottom panels' top corners; ours hang from the top edge: the bottom corners).
static const float kFrameLine = 0.00185f, kBevel = 0.0083f, kChamW = 0.0102f, kChamH = 0.0208f;
static const float kPanelTop = 0.016f;                  // the first row's top (the frame's top edge is off screen)
static const float kPanelBottom = 0.016f;               // below the last row, inside the frame line
static const float kPanelMinW = 0.30f;                  // never narrower (a HUD panel, not a tag)
static const float kSlideIn = 0.25f, kSlideOut = 0.25f; // seconds (slide-out 0.22 before the 250 ms steps)
static const float kTopMinHalf = 0.22f; // the least half room for the top panel: 26 characters a row (16:10 and wider)
static const float kCommTop = 0.090f;   // the comm window's portrait frame, its native top (195 px at 4K)
static const float kCommGap = 0.010f;   // the comm window's gap below the cruise box when the bar is moved
// More HUD geometry, fractions of H as well: measured at 4K (3840x2160) on the HDE HUD (Advanced Widescreen HUD); fixed
// until the shared screen-layout registry (DISPLAY_PLAN "Next"); the live control rectangles replace them where read.
static constexpr float kBarFrameBottom = 0.083f; // the maneuver bar's frame, lower edge: the top HUD row ends
static constexpr float kMbFrameIn = 0.040f; // the frame beyond ManeuverButton0's left side (kMbFrameOut: the right)
static constexpr float kBelowBar = 0.162f; // top of the stack (space_position 1) / the Neural Net corner: below the bar
static constexpr float kBelowBanner = 0.235f; // the same while the objective banner (0.17-0.225 H) is shown
static constexpr float kLowPanelsLeft =
    0.613f; // space_position 0: the bottom-left panels end this far from the left edge
static constexpr float kLowPanelsRight = 0.564f; // ... the bottom-right panels start this far from the right edge
static constexpr float kLowGapMargin = 0.02f;    // ... the gap between them is kept this much narrower
static constexpr float kLowBottom = 0.056f; // ... the stack's bottom edge above the screen bottom (the throttle bar)
static constexpr float kBarRestGap = 0.02f; // letterbox_mode 1: the block rests this far above the bar's top edge
static constexpr float kGaugeHalfW =
    0.215f; // the gauge bar between the bottom panels (HealthBar): half its keep-out width
static constexpr float kGaugeAbove = 0.03f;   // ... and the keep-out above its centre
static constexpr float kCruiseOutX = 0.02f;   // the cruise box beyond its left / right sides
static constexpr float kCruiseBelow = 0.012f; // ... and below it
static constexpr float kStackLimit =
    0.44f; // a top-anchored stack is trimmed (oldest first) before it passes this (the crosshair)

// A visible control's screen rectangle (centre, half extents) through the UI camera: its position is the sum of the
// positions up its parent chain (FlControl +0x04 parent, +0xA8 pos, +0xB4 half box; the controller's ui::Collect).
static bool ControlRect(const char* name, float& cx, float& cy, float& hw, float& hh)
{
    const u32 ctl = uiclick::FindVisible(name);
    if (!ctl || !exe::UiCamera.ok())
        return false;
    __try
    {
        float x = 0, y = 0, z = 0;
        u32 c = ctl;
        for (int i = 0; c && i < 24; ++i)
        {
            const float* p = reinterpret_cast<const float*>(c + 0xA8);
            x += p[0], y += p[1], z += p[2];
            c = *reinterpret_cast<const u32*>(c + 0x04);
        }
        if (z > -0.01f)
            return false;
        const float* box = reinterpret_cast<const float*>(ctl + 0xB4);
        const float kx = Mem<float>(exe::UiCamera, 0x78), ky = Mem<float>(exe::UiCamera, 0x7C);
        cx = Mem<float>(exe::UiCamera, 0x68) + kx * (x / -z);
        cy = Mem<float>(exe::UiCamera, 0x6C) + ky * (y / -z);
        hw = std::fabs(kx) * box[0] / -z, hh = std::fabs(ky) * box[1] / -z;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The half width the top panel may take around the screen centre: the room between the maneuver bar's right edge and
// the NeuroNet box's left edge as the HUD stands now (both read live, so an aspect ratio, a HudShift setup or a bar
// not yet moved - the shift applies when the HUD is built - all give the true room), less a gap each side. 0 = no
// room (the bar still at the centre). Looked up at most every 250 ms.
// The maneuver bar's live right edge (-1: not shown - auto-hidden, or no HUD): the frame beyond its rightmost shown button
// (the game lays out only the buttons it shows, centred: FREELANCER_RE "maneuver bar"), and FLvibed's own fifth button
// (the controller's FLvibedAutopilotButton, after Formation) where it reaches further (fix-text 4, 25 Oct 2026: the comm
// portrait in the top panel overlapped it - the room was measured from ManeuverButton3 alone).
// The frame's top right ear reaches 0.023 H beyond the fifth button's control box (fix-text 5, 25 Oct 2026: measured
// 27.6 px at 1600x1200, 47.5 px at 4K, research\experience\runs\fix_text5_2026-10-25\probe_*_nocomm; the 0.008 H ring
// taken before left the ear 0.015 H under the 4:3 name tab and the 4K panel's top left corner).
static constexpr float kBarButtonRim = 0.023f; // the frame (its ear) beyond the fifth button's control box
static float BarRight(float sh)
{
    float cx, cy, hw, hh, right = -1;
    for (const char* n : {"ManeuverButton0", "ManeuverButton1", "ManeuverButton2", "ManeuverButton3"})
        if (ControlRect(n, cx, cy, hw, hh))
            right = std::max(right, cx + hw + sh * kMbFrameOut * g_mbScale); // the frame scales with the bar
    if (right >= 0 && ControlRect("FLvibedAutopilotButton", cx, cy, hw, hh))
        right = std::max(right, cx + hw + sh * kBarButtonRim * g_mbScale);
    return right;
}
static float g_barRight = -1, g_nnLeft = 0;         // the last TopRoom's readings (debug `layout`)
static float g_cruiseRight = 0, g_cruiseBottom = 0; // the cruise box's right / bottom edge (0 = none)
static ULONGLONG g_topRoomAt = 0, g_keepOutsAt = 0; // the HUD reads' last times (0 = read at the next call: HUD 2 left)
static float TopRoom(int sw, int sh)
{
    static float room = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - g_topRoomAt < 250)
        return room;
    g_topRoomAt = now;
    float cx, cy, hw, hh, nnLeft = static_cast<float>(sw);
    const float barRight = BarRight(static_cast<float>(sh));
    for (const char* n : {"NNMissionLogButtonRoot", "NNNavmapButtonRoot", "NNInventoryButtonRoot",
                          "NNPlayerInfoButtonRoot", "NNHelpButtonRoot"})
        if (ControlRect(n, cx, cy, hw, hh))
            nnLeft = std::min(nnLeft, cx - hw - sh * kNnFrameOut);
    const float centre = sw * 0.5f, gap = sh * kPanelGap;
    room = barRight < 0 ? 0.0f : std::max(0.0f, std::min(centre - barRight, nnLeft - centre) - gap);
    g_barRight = barRight, g_nnLeft = nnLeft;
    // the cruise box under the bar's left end (space_narrow 3 keeps a panel below the top row clear of it)
    g_cruiseRight = 0, g_cruiseBottom = 0;
    if (ControlRect("ManeuverCruiseButton", cx, cy, hw, hh))
        g_cruiseRight = cx + hw + sh * kCruiseOutX, g_cruiseBottom = cy + hh + sh * kCruiseBelow;
    return room;
}

// ------------------------------------------------------------------ the comm portrait in the top panel (comm_portrait)
// The game's comm window (exe::HudCommInstance, a HUD_Receiver) is laid out every frame from its window's win_pos and
// win_size (FCommWindow at +0xEC: the head is drawn into the viewport between its corners, so a size scales the head),
// the generic icon (+0x5B4, a ButtonControl) from its control position, and the caption under them is a text control
// of the player's channel (+0xB4 +0x1BC). What it shows is the game's decision (decomp hud_comm.cpp Dispatch, Appearance,
// Update; checked in game 21 Oct 2026): a comm to the player with a costume opens the window with the live head, or the
// generic radio-tower icon when no body can be built or character detail is 0; a comm without a costume (all part ids
// 0), someone else's chatter, or a letterboxed scene opens none. So the slot shows exactly while the window is open.
// The draw asks for the slot (CommSlotRequest: a screen square), the frame update writes it (PlaceCommWindow); the
// natives are read while nothing is moved and written back the frame the slot is not asked for.
static const u32 kRecvWindow = 0xEC, kRecvIcon = 0x5B4, kRecvCaption = 0xB4 + 0x1BC;
static const u32 kWinPos = 0x24, kWinSize = 0x3C; // FCommWindow
static struct
{
    u32 recv = 0;
    float pos[3] = {}, size[2] = {}, iconPos[3] = {}; // natives (window centre + size in UI camera space, icon control)
    u32 icon = 0, caption = 0;                        // the controls moved / hidden (0 = none)
    bool have = false, moved = false;
    float x = 0, y = 0, s = 0; // the slot last written (where the game draws the window, px)
    float w = 0, h = 0;        // the window's size in it (smaller while it opens)
    float f = 0;               // its outline's size (px, square, centred)
} g_comm;
static struct
{
    float x = 0, y = 0, s = 0; // the slot's top-left and size (px)
    ULONGLONG at = 0;          // asked for at (the last draw that showed it)
    float sx = 1, sy = 1;      // the window's width / height in the slot (the opening animation: < 1, centred)
    float fs = 1;              // its outline's size in the slot (the opening animation: grows with the line)
} g_slot;
// The game's outline around the comm picture (FCommChannel::DrawFrame 0x4CC440): a square of 0.207 UI units around
// win_pos at its depth, the picture 0.200 - fixed sizes, so its size on screen follows the window's depth only.
static const float kCommFrameRatio = 0.207f / 0.200f;

static u32 CommReceiver() { return exe::HudCommInstance.ok() ? Mem<u32>(exe::HudCommInstance) : 0; }

// The game's comm window is open now (and its size on screen at its own size, px).
static bool CommWindowOpen(float* nativePx = nullptr)
{
    const u32 recv = CommReceiver();
    if (!recv || !exe::UiCamera.ok())
        return false;
    __try
    {
        if (!*reinterpret_cast<const u8*>(recv + kRecvWindow))
            return false;
        if (nativePx)
        {
            const float* pos = reinterpret_cast<const float*>(recv + kRecvWindow + kWinPos);
            const float* size = reinterpret_cast<const float*>(recv + kRecvWindow + kWinSize);
            const bool mine = g_comm.recv == recv && g_comm.have && g_comm.moved;
            const float z = mine ? g_comm.pos[2] : pos[2], h = mine ? g_comm.size[1] : size[1];
            *nativePx = z < -0.01f ? std::fabs(Mem<float>(exe::UiCamera, 0x7C)) * h / -z : 0.0f;
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void CommSlotRequest(float x, float y, float s, float sx = 1, float sy = 1, float fs = 1)
{
    g_slot = {x, y, s, GetTickCount64(), sx, sy, fs};
}

// A control's parents' summed position (FlControl +0x04 parent, +0xA8 position).
static void ParentOffset(u32 ctl, float* o)
{
    o[0] = o[1] = o[2] = 0;
    u32 c = *reinterpret_cast<const u32*>(ctl + 0x04);
    for (int i = 0; c && i < 24; ++i)
    {
        const float* p = reinterpret_cast<const float*>(c + 0xA8);
        o[0] += p[0], o[1] += p[1], o[2] += p[2];
        c = *reinterpret_cast<const u32*>(c + 0x04);
    }
}

// The generic icon is an icon button: its mesh instance is placed by the control's own position setter (a written +0xA8
// would not move it; decomp nn_button_control.cpp SetPosition). Only when the position differs.
static bool SetControlPosition(u32 ctl, const float* v); // the scaled maneuver bar's, below
static void MoveIcon(u32 icon, const float* want)
{
    const float* cur = reinterpret_cast<const float*>(icon + 0xA8);
    if (cur[0] != want[0] || cur[1] != want[1] || cur[2] != want[2])
        SetControlPosition(icon, want);
}

static void PlaceCommWindow()
{
    const u32 recv = CommReceiver();
    if (recv != g_comm.recv)
        g_comm = {}, g_comm.recv = recv; // a new HUD: its own natives (the old receiver is gone)
    if (!recv || !exe::UiCamera.ok())
        return;
    __try
    {
        float* pos = reinterpret_cast<float*>(recv + kRecvWindow + kWinPos);
        float* size = reinterpret_cast<float*>(recv + kRecvWindow + kWinSize);
        const u32 icon = *reinterpret_cast<const u32*>(recv + kRecvIcon);
        float* iconPos = icon ? reinterpret_cast<float*>(icon + 0xA8) : nullptr;
        const bool open = *reinterpret_cast<const u8*>(recv + kRecvWindow) != 0;
        const bool want =
            g_set.enabled && g_set.commPortrait && open && g_slot.s > 0 && GetTickCount64() - g_slot.at < 150;
        if (!g_comm.moved) // the natives as the game (or HudShift / ShiftCommWindow) set them
        {
            memcpy(g_comm.pos, pos, sizeof(g_comm.pos)), memcpy(g_comm.size, size, sizeof(g_comm.size));
            if (iconPos)
                memcpy(g_comm.iconPos, iconPos, sizeof(g_comm.iconPos));
            g_comm.have = true;
        }
        if (!want)
        {
            if (g_comm.moved)
            {
                memcpy(pos, g_comm.pos, sizeof(g_comm.pos)), memcpy(size, g_comm.size, sizeof(g_comm.size));
                if (iconPos && icon == g_comm.icon)
                    MoveIcon(icon, g_comm.iconPos);
                // the caption back while its comm still shows (the game hides it when the window closes)
                const u32 cap = *reinterpret_cast<const u32*>(recv + kRecvCaption);
                if (cap && cap == g_comm.caption && open)
                    *reinterpret_cast<u8*>(cap + 0x6C) |= 3;
                g_comm.moved = false, g_comm.icon = g_comm.caption = 0;
            }
            return;
        }
        const float cx0 = Mem<float>(exe::UiCamera, 0x68), cy0 = Mem<float>(exe::UiCamera, 0x6C);
        const float kx = Mem<float>(exe::UiCamera, 0x78), ky = Mem<float>(exe::UiCamera, 0x7C);
        // the depth sets the game's outline: native size around a native-size picture, so at the slot's size (the tab
        // shrinks it) and the opening animation's fraction it is placed deeper (smaller) or nearer
        const float z0 = g_comm.pos[2];
        if (z0 > -0.01f || std::fabs(kx) < 1.0f || std::fabs(ky) < 1.0f || g_slot.s < 1.0f)
            return;
        const float nativeWin = std::fabs(ky) * g_comm.size[1] / -z0, fs = std::max(0.02f, g_slot.fs);
        const float z = nativeWin > 1.0f ? z0 * nativeWin / (g_slot.s * fs) : z0;
        // the window: centred on the slot, as large as it (the viewport follows its corners) - during the opening
        // animation narrower / flatter (the head render squashes with its viewport), at least 2 px each way
        const float cx = g_slot.x + g_slot.s * 0.5f, cy = g_slot.y + g_slot.s * 0.5f;
        const float ww = std::max(2.0f, g_slot.s * g_slot.sx), wh = std::max(2.0f, g_slot.s * g_slot.sy);
        pos[0] = (cx - cx0) / kx * -z, pos[1] = (cy - cy0) / ky * -z, pos[2] = z;
        size[0] = ww * -z / std::fabs(kx), size[1] = wh * -z / std::fabs(ky);
        // the generic icon: the same offset from the window's centre and the same scale as natively (its size goes
        // with 1 / depth)
        if (iconPos)
        {
            float par[3];
            ParentOffset(icon, par);
            const float iz = g_comm.iconPos[2] + par[2], nativePx = nativeWin;
            if (iz < -0.01f && nativePx > 1.0f)
            {
                // (opening: it grows with the window's smaller side - a model scales one way only)
                const float scale = g_slot.s * std::max(0.02f, std::min(g_slot.sx, g_slot.sy)) / nativePx,
                            newZ = iz / scale;
                const float nwx = cx0 + kx * (g_comm.pos[0] / -z0), nwy = cy0 + ky * (g_comm.pos[1] / -z0);
                const float nix = cx0 + kx * ((g_comm.iconPos[0] + par[0]) / -iz),
                            niy = cy0 + ky * ((g_comm.iconPos[1] + par[1]) / -iz);
                const float ix = cx + (nix - nwx) * (g_slot.s / nativePx),
                            iy = cy + (niy - nwy) * (g_slot.s / nativePx);
                const float to[3] = {(ix - cx0) / kx * -newZ - par[0], (iy - cy0) / ky * -newZ - par[1], newZ - par[2]};
                MoveIcon(icon, to);
                g_comm.icon = icon;
            }
        }
        // the caption hidden: the panel's name row names the speaker
        const u32 cap = *reinterpret_cast<const u32*>(recv + kRecvCaption);
        if (cap)
            *reinterpret_cast<u8*>(cap + 0x6C) &= 0xFC;
        g_comm.caption = cap;
        g_comm.moved = true;
        g_comm.x = g_slot.x, g_comm.y = g_slot.y, g_comm.s = g_slot.s;
        g_comm.w = ww, g_comm.h = wh, g_comm.f = g_slot.s * fs * kCommFrameRatio;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_comm = {};
    }
}

// The portrait held after the game closes its window (user, 21 Oct 2026: "The portrait disappears as soon as the NPC
// stops talking, but the subtitles linger ... How do we make it look better while keeping it natural-feeling?"): the
// game closes the window when the voice ends (its own linger), our line stays for its reading time. While the window is
// in the slot its picture is captured from the back buffer (every 100 ms, in Present: the game has drawn it, the panel
// leaves its hole); when the window closes while its line still shows, the last picture stays in the slot - dimmed
// over 0.3 s, as a closed channel - and fades out with the line; the slot keeps its room until then. A new comm window
// takes over at once. Holding the game's window open instead would keep its comm channel busy (the next comm waits):
// game logic, not changed.
static struct
{
    std::vector<u32> px; // the slot's last capture (top-down ARGB, w x w)
    int w = 0;
    ULONGLONG at = 0;  // when (GetTickCount64)
    render::Image img; // built when a hold starts
    bool holding = false;
    double block = -1; // the line it belongs to (its start time)
    double since = 0;  // the hold's start (Now)
} g_held;
static double g_liveBlock = -1; // the live portrait's line (its start time), -1 none
// The channel's animations (s): closing = the picture folds into a line (0.12) that shrinks away (0.2); opening = its
// mirror, a line grows from the centre (0.2) and the live window unfolds from it (0.12).
static const double kFoldTime = 0.25, kLineTime = 0.25,
                    kChannelClose = kFoldTime + kLineTime; // 0.12 / 0.2 before the 250 ms steps
// The opening is quicker (user, 22 Oct 2026: "cut down channel open (both text and portrait) to 125ms"): the line
// 62.5 ms, the unfold 62.5 ms; the line's text fades in over the same 125 ms (kFadeIn).
static const double kOpenLine = 0.0625, kOpenUnfold = 0.0625, kChannelOpen = kOpenLine + kOpenUnfold;
static const u32 kScanBlue = 0xA0D1FF, kScanWhite = 0xE6F3FF; // the HUD scanline (refreshtest2) and its core

static void ReleaseHeldPortrait()
{
    g_held.holding = false;
    if (g_held.img.texture)
        render::FreeImage(g_held.img);
}

void CapturePortrait()
{
    if (!g_comm.moved || g_comm.s < 8 || g_comm.w < g_comm.s - 0.5f || g_comm.h < g_comm.s - 0.5f || !CommWindowOpen())
        return; // (not while it opens: the picture is not whole yet)
    if (!render::ReadbackSafe())
        return; // (under a D3D9 wrapper the readback breaks every later device Reset: no held picture there)
    const ULONGLONG now = GetTickCount64();
    if (now - g_held.at < 100)
        return;
    int sw = 0, sh = 0;
    const int s = static_cast<int>(g_comm.s + 0.5f), x = static_cast<int>(g_comm.x + 0.5f),
              y = static_cast<int>(g_comm.y + 0.5f);
    if (!render::ScreenSize(sw, sh) || x < 0 || y < 0 || x + s > sw || y + s > sh)
        return;
    std::vector<u32> px;
    if (!render::GrabRegion(x, y, s, s, px))
        return;
    for (u32& p : px)
        p |= 0xFF000000; // the back buffer's alpha is not the picture's
    g_held.px = std::move(px), g_held.w = s, g_held.at = now;
}

// The objective banner under the top panel: its native place (the "MissionObjectiveText" control, typed at 0.18-0.215 H)
// lies inside a panel with a name row and two rows (21 Oct 2026, M02 "START SCANNING NEARBY SHIPS" under King's line).
// While the panel is out, the banner's control moves down to clear it and glides back after: only its position is
// written (FlControl +0xA8, y in UI units: pixel = centre + scale * y / -z), the game draws and types it as always. The
// control exists only while the banner shows: looked up every frame (never a stale pointer), its native y kept per
// control. clearY < 0: no panel. The text's top lies 0.0125 H below the control's anchor (measured: anchor 363, text
// 390 px at 4K).
static void KeepBannerClear(float clearY, float sh)
{
    static u32 seen = 0;
    static float nativeY = 0, offPx = 0;
    if (!exe::UiCamera.ok() || (clearY < 0 && offPx == 0))
        return; // no panel and the banner where the game put it: no lookup (it cost a list walk each frame)
    const u32 ctl = uiclick::FindVisible("MissionObjectiveText");
    if (!ctl)
    {
        seen = 0, offPx = 0;
        return;
    }
    __try
    {
        float* pos = reinterpret_cast<float*>(ctl + 0xA8);
        if (ctl != seen)
            seen = ctl, nativeY = pos[1], offPx = 0;
        float py = 0, pz = 0; // the parents' offset
        u32 c = *reinterpret_cast<const u32*>(ctl + 0x04);
        for (int i = 0; c && i < 24; ++i)
        {
            const float* p = reinterpret_cast<const float*>(c + 0xA8);
            py += p[1], pz += p[2];
            c = *reinterpret_cast<const u32*>(c + 0x04);
        }
        const float z = pz + pos[2], ky = Mem<float>(exe::UiCamera, 0x7C);
        if (z > -0.01f || std::fabs(ky) < 1.0f)
            return;
        const float anchor = Mem<float>(exe::UiCamera, 0x6C) + ky * ((py + nativeY) / -z);
        // tracks the panel's displayed bottom directly: the panel glides and slides smoothly, so the banner rides along
        // (a glide of its own lagged a growing panel: a second speaker covered the banner for ~0.3 s, 21 Oct 2026)
        offPx = clearY >= 0 ? std::max(0.0f, clearY - (anchor + sh * 0.0125f)) : 0.0f;
        pos[1] = nativeY + offPx * -z / ky;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        seen = 0;
    }
}

// A screen too narrow for the top panel's rows between the moved bar and the NeuroNet box (4:3 at 1.33, 5:4): the half
// room left must hold kTopMinHalf (26 characters of the medium font and the panel's sides; 16:10 and wider do).
static bool NarrowScreen(int sw, int sh)
{
    return sw * 0.5f - sh * (kMbMargin + 2 * kMbHalf + kPanelGap) < sh * kTopMinHalf;
}

// Keeps the maneuver bar where the top panel needs it: at the top left, mirroring the NeuroNet box at the top right
// (space_position 2), else where the game / HudShift put it. The host owns the HUD constants (HostApi v4); every frame,
// cheap (three floats compared).
static bool g_mbShifted = false;
static void PlaceManeuverBar()
{
    if (!g_host || g_host->version < 4 || !g_host->ShiftManeuverBar)
        return;
    // HUD units: the flight UI camera's x scale, 1.2963 H pixels per unit (2800 at 2160; measured 21 Oct 2026: a shift
    // of -0.4982 moved the buttons 1395 px). Not the live camera: its scale differs between game states (2742 in the
    // menus) and the value that counts is the one present when the HUD is built.
    // Narrow screens (NarrowScreen) move it too unless space_narrow 0 (then the bar stays centred, the subtitles below
    // it: the room at the top is ~0.18 H there, rows of one or two words - display-subtitles, 21 Oct 2026).
    float dx = 0;
    const int sw = Mem<int>(exe::ScreenW), sh = Mem<int>(exe::ScreenH);
    if (g_set.enabled && g_set.spacePosition == 2 && sw > 0 && sh > 0 &&
        (g_set.spaceNarrow != 0 || !NarrowScreen(sw, sh)))
        dx = (sh * (kMbMargin + kMbHalf) - sw * 0.5f) / (sh * 1.2963f);
    g_mbShifted = g_host->ShiftManeuverBar(dx) && dx != 0;
    // The comm window (the speaker's portrait and its name lines, top left) under the moved bar (user, 21 Oct 2026: the
    // bar rendered over the portraits): down until its top clears the cruise box under the bar's left end (read live:
    // TopRoom; the scaled bar of space_narrow 4 has a smaller one), a gap below it. Its native top: 0.090 H (the
    // portrait's frame, measured at 4K 21 Oct 2026, 195 px; the cruise box ended at 335). HostApi v5.
    // HUD 2 (its nav block holds the top left; the bar stays shifted as configured - it is hidden, and returns with the
    // game's HUD): the comm window's native place goes below the nav, where it shows whenever the panel holds no slot
    // for it (comm_portrait 0, space_position 0 / 1, a panel too narrow). From the next comm on, as the shift always does.
    if (g_host->version >= 5 && g_host->ShiftCommWindow && sh > 0)
    {
        float dy = 0;
        const HudLayout* h2 = Hud2Layout();
        if (h2 && Shown(h2->nav))
            dy = -std::max(0.0f, h2->nav.y1 * sh + sh * kCommGap - sh * kCommTop) / (sh * 1.2963f);
        else if (dx != 0)
        {
            const float cruiseBottom = g_cruiseBottom > 0 ? g_cruiseBottom : sh * 0.155f;
            dy = -std::max(0.0f, cruiseBottom + sh * kCommGap - sh * kCommTop) / (sh * 1.2963f);
        }
        g_host->ShiftCommWindow(dy);
    }
}

// ------------------------------------------------------------------ the scaled maneuver bar (space_narrow 4)
// The user's idea for narrow screens (21 Oct 2026): the bar scaled down to mirror the NeuroNet box at the top right
// (its width 592 px over the frame's 1006 px at 4K), so the top panel gets the room. The bar's parts hang under
// HUD_Maneuvers (depth 1) and nest two ways (measured): the buttons and the cruise button add their depth to the root's
// (button 0.5 + 1 -> 0.5 deep), the frame and tab images use their own (0.5, 1.0). On screen a part's size is
// proportional to 1 / depth: each part's depth is divided by the scale and its position recomputed so the whole bar
// scales about the frame's top-left corner (flush in the corner, the cruise box still under its left end); hit boxes
// project the same way (clicks, pad navigation follow); the root's hover region (+0xC0 centre, +0xB4 half extents, at
// depth 1) scales with it. The original of each part is kept per control (a new HUD build has new controls); written
// only when it differs; restored before the logic unloads (RestoreHud), so a hot reload never scales twice.
static const float kMbScale = 0.588f;

struct ScaledPart
{
    u32 ctl = 0;
    float orig[4] = {}, written[4] = {};
    bool have = false;
};
static const char* const kMbParts[] = {"ManeuverFrame",   "ManeuverTab",     "ManeuverButton0",     "ManeuverButton1",
                                       "ManeuverButton2", "ManeuverButton3", "ManeuverCruiseButton"};
static ScaledPart g_mbPart[7], g_mbRegion, g_mbRegionCentre; // the parts; the root's hover region: half extents, centre

static bool WriteFloats(u32 addr, const float* v, int n)
{
    __try
    {
        memcpy(reinterpret_cast<void*>(addr), v, n * sizeof(float));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool ReadFloats(u32 addr, float* v, int n)
{
    __try
    {
        memcpy(v, reinterpret_cast<const void*>(addr), n * sizeof(float));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The part's original (taken when first seen, or when the game wrote something else since our last write).
static bool Original(ScaledPart& p, u32 ctl, u32 off, int n)
{
    float cur[4] = {};
    if (!ReadFloats(ctl + off, cur, n))
        return false;
    if (p.ctl != ctl)
        p = ScaledPart(), p.ctl = ctl;
    if (!p.have || memcmp(cur, p.written, n * sizeof(float)) != 0)
        memcpy(p.orig, cur, sizeof(cur)), memcpy(p.written, cur, sizeof(cur)), p.have = true;
    return true;
}

static void Put(ScaledPart& p, u32 off, const float* v, int n)
{
    if (memcmp(v, p.written, n * sizeof(float)) != 0 && WriteFloats(p.ctl + off, v, n))
        memcpy(p.written, v, n * sizeof(float));
}

// A control's position through its own property setter (vt[0xA8], id 6), as the game sets it: images cache their draw
// transform there (a direct write moved the buttons, not the frame and tab). The slot must hold one of the known
// setters (exe::ControlSetProperty and the image's and icon button's overrides of it).
static bool SetControlPosition(u32 ctl, const float* v)
{
    __try
    {
        void* const* vt = *reinterpret_cast<void* const* const*>(ctl);
        const uintptr_t fn = reinterpret_cast<uintptr_t>(vt[0xA8 / 4]);
        if (!exe::ControlSetProperty.is(fn) && !exe::ImageSetProperty.is(fn) && !exe::IconButtonSetProperty.is(fn))
            return false;
        typedef int(__thiscall * SetProp)(void*, int, const void*, int);
        reinterpret_cast<SetProp>(fn)(reinterpret_cast<void*>(ctl), 6, v, 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void PutPosition(ScaledPart& p, const float* v)
{
    if (memcmp(v, p.written, 3 * sizeof(float)) != 0 && SetControlPosition(p.ctl, v))
        memcpy(p.written, v, 3 * sizeof(float));
}

static const char* g_mbScaleWhy = "-"; // why the last OnManeuverLayout stopped (debug `layout`)

// Right after the game's own layout of the bar, every frame in flight (exe::ManeuverLayout, ImplApi v6): the game
// re-lays the buttons, tab and cruise button out each frame - a write between frames is undone (21 Oct 2026).
void OnManeuverLayout()
{
    PerfScope perf(g_perf.layout);
    const int sw = Mem<int>(exe::ScreenW), sh = Mem<int>(exe::ScreenH);
    const bool want = g_set.enabled && g_set.spacePosition == 2 && g_set.spaceNarrow == 4 && sw > 0 && sh > 0 &&
                      NarrowScreen(sw, sh) && g_mbShifted;
    const float s = want ? kMbScale : 1.0f;
    g_mbScaleWhy = want ? "applied" : "not wanted";
    if (!want && g_mbScale == 1.0f)
        return; // nothing scaled, nothing to do (measured: a control-list lookup each frame cost ~3 us for nothing)
    const u32 root = uiclick::FindVisible("HUD_Maneuvers");
    if (!root || !exe::UiCamera.ok())
    {
        g_mbScale = 1.0f, g_mbScaleWhy = "no HUD_Maneuvers";
        return;
    }
    const float cx = Mem<float>(exe::UiCamera, 0x68), cy = Mem<float>(exe::UiCamera, 0x6C);
    const float kx = Mem<float>(exe::UiCamera, 0x78), ky = Mem<float>(exe::UiCamera, 0x7C);
    float rp[3] = {};
    if (std::fabs(kx) < 1.0f || std::fabs(ky) < 1.0f || !ReadFloats(root + 0xA8, rp, 3))
    {
        g_mbScaleWhy = "no camera";
        return;
    }
    u32 ctl[7] = {};
    for (int i = 0; i < 7; ++i)
        if (!(ctl[i] = uiclick::FindVisible(kMbParts[i])) || !Original(g_mbPart[i], ctl[i], 0xA8, 3))
        {
            g_mbScaleWhy = kMbParts[i]; // all or nothing: this part was not found
            return;
        }
    if (!Original(g_mbRegion, root, 0xB4, 2) || !Original(g_mbRegionCentre, root, 0xC0, 2))
    {
        g_mbScaleWhy = "region";
        return;
    }
    // the anchor: the frame's original top-left corner on screen (its centre less half its width, the top edge)
    const ScaledPart& fr = g_mbPart[0];
    const float fd = -fr.orig[2];
    if (fd < 0.01f)
        return;
    const float ax = cx + kx * ((rp[0] + fr.orig[0]) / fd) - sh * kMbHalf, ay = 0.0f;
    for (int i = 0; i < 7; ++i)
    {
        ScaledPart& p = g_mbPart[i];
        const bool image = i < 2;
        const float d = image ? -p.orig[2] : -(rp[2] + p.orig[2]);
        if (d < 0.01f)
            continue;
        const float sx = cx + kx * ((rp[0] + p.orig[0]) / d), sy = cy + ky * ((rp[1] + p.orig[1]) / d);
        const float d2 = d / s, tx = ax + s * (sx - ax), ty = ay + s * (sy - ay);
        const float v[3] = {(tx - cx) * d2 / kx - rp[0], (ty - cy) * d2 / ky - rp[1], image ? -d2 : -d2 - rp[2]};
        PutPosition(p, s == 1.0f ? p.orig : v);
    }
    {
        const float* rc = g_mbRegionCentre.orig; // at depth 1
        const float sx = cx + kx * rc[0], sy = cy + ky * rc[1];
        const float c2[2] = {(ax + s * (sx - ax) - cx) / kx, (ay + s * (sy - ay) - cy) / ky};
        Put(g_mbRegionCentre, 0xC0, s == 1.0f ? rc : c2, 2);
        const float b2[2] = {g_mbRegion.orig[0] * s, g_mbRegion.orig[1] * s};
        Put(g_mbRegion, 0xB4, s == 1.0f ? g_mbRegion.orig : b2, 2);
    }
    g_mbScale = s;
}

// Puts every scaled part back (before the logic unloads: the next logic would take the scaled values for originals).
void RestoreHud()
{
    for (ScaledPart& p : g_mbPart)
        if (p.have && p.ctl)
            PutPosition(p, p.orig);
    if (g_mbRegion.have && g_mbRegion.ctl)
        Put(g_mbRegion, 0xB4, g_mbRegion.orig, 2);
    if (g_mbRegionCentre.have && g_mbRegionCentre.ctl)
        Put(g_mbRegionCentre, 0xC0, g_mbRegionCentre.orig, 2);
    g_mbScale = 1.0f;
    g_slot = {}; // the comm window back where the game put it
    PlaceCommWindow();
}

// The panel's state: it slides in from the top edge (fading in) while lines show and slides back out after the last
// one (user, 21 Oct 2026); its size glides as speakers come and go. s 0..1 (hidden..in place).
static struct
{
    float s = 0;
    float x = 0, w = 0, h = 0;   // displayed: centre x, width, height from the screen's top edge
    float top = 0;               // its top edge (0 = flush with the screen edge, which hides the top frame line)
    float tabW = 0, bodyTop = 0; // the tab layout (space_narrow 5): the name tab's width, the body's top (0 = no tab)
    bool sized = false;
    bool tabNoRoom = false; // the tab layout's panel + tab would reach the bar: below the top row until the panel hides
    double last = 0;
    float yoff = 0, alpha = 0; // this frame: the slide offset (<= 0) and the panel's opacity factor
} g_panel;

static render::Image g_white; // 1x1 white: solid quads (chamfers, mitred frame bands)

static void SolidQuad(const float xy[8], u32 c0, u32 c1, u32 c2, u32 c3)
{
    static const float uv[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    const u32 c[4] = {c0, c1, c2, c3};
    render::Quad(g_white, xy, uv, c, false);
}

// The panel's outline: a polygon, clockwise on screen (y down), its inside to the right of each edge. The plain panel
// has 6 corners (top two beyond the screen edge, chamfered lower corners); the tab layout (space_narrow 5) 14 - a name
// tab at the top, a step out at the body's top, chamfered outer corners and a chamfered neck (concave).
struct Poly
{
    float v[32];
    int n = 0;
    void add(float x, float y) { v[n * 2] = x, v[n * 2 + 1] = y, ++n; }
};

static void InwardNormal(const Poly& p, int i, float& nx, float& ny) // of the edge from corner i to corner i + 1
{
    const int j = (i + 1) % p.n;
    const float dx = p.v[j * 2] - p.v[i * 2], dy = p.v[j * 2 + 1] - p.v[i * 2 + 1], l = std::sqrt(dx * dx + dy * dy);
    nx = l > 0 ? -dy / l : 0, ny = l > 0 ? dx / l : 0;
}

// The outline offset inwards by d (mitred corners; concave corners mitre outwards the same way).
static Poly Offset(const Poly& p, float d)
{
    Poly o;
    for (int i = 0; i < p.n; ++i)
    {
        float n1x, n1y, n2x, n2y;
        InwardNormal(p, (i + p.n - 1) % p.n, n1x, n1y);
        InwardNormal(p, i, n2x, n2y);
        const float k = d / std::max(0.2f, 1.0f + n1x * n2x + n1y * n2y);
        o.add(p.v[i * 2] + (n1x + n2x) * k, p.v[i * 2 + 1] + (n1y + n2y) * k);
    }
    return o;
}

// A rectangle the panel's fill leaves out: the comm portrait (the game draws it before the overlay; under the panel's
// base it showed dimmed, 21 Oct 2026). Set per frame by DrawImpl.
struct Hole
{
    bool on = false;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
static Hole g_hole, g_closingHole; // g_closingHole: the closing animation's outline, drawn last frame

// A horizontal band [ya, yb] filled inside the polygon (any shape), colour ca at ya to cb at yb: slabs between the
// corners' heights, each split into the spans the outline crosses at its middle (and around g_hole).
static void PolyBand(const Poly& p, float ya, float yb, u32 ca, u32 cb)
{
    ya = std::max(ya, 0.0f);
    if (yb <= ya)
        return;
    float ys[30];
    int ny = 0;
    ys[ny++] = ya;
    for (int i = 0; i < p.n; ++i)
        if (p.v[i * 2 + 1] > ya && p.v[i * 2 + 1] < yb)
            ys[ny++] = p.v[i * 2 + 1];
    ys[ny++] = yb;
    // the comm portrait's hole (g_hole): its top and bottom split the bands too
    const bool hole = g_hole.on && g_hole.y1 > ya && g_hole.y0 < yb && ny + 2 <= 30;
    if (hole)
        ys[ny++] = std::max(ya, g_hole.y0), ys[ny++] = std::min(yb, g_hole.y1);
    std::sort(ys, ys + ny);
    auto mix = [&](float y)
    {
        const float t = (y - ya) / (yb - ya);
        u32 o = 0;
        for (int s = 0; s < 32; s += 8)
            o |= static_cast<u32>(((ca >> s) & 0xFF) + (static_cast<float>((cb >> s) & 0xFF) - ((ca >> s) & 0xFF)) * t +
                                  0.5f)
                 << s;
        return o;
    };
    for (int k = 0; k + 1 < ny; ++k)
    {
        const float y0 = ys[k], y1 = ys[k + 1];
        if (y1 - y0 < 0.01f)
            continue;
        const float ym = (y0 + y1) * 0.5f;
        struct Cross
        {
            float x;
            int e;
        } xs[12];
        int nx = 0;
        for (int i = 0; i < p.n && nx < 12; ++i)
        {
            const int j = (i + 1) % p.n;
            const float ay = p.v[i * 2 + 1], by = p.v[j * 2 + 1];
            if ((ay <= ym) == (by <= ym))
                continue; // horizontal or not crossing
            xs[nx++] = {p.v[i * 2] + (ym - ay) * (p.v[j * 2] - p.v[i * 2]) / (by - ay), i};
        }
        std::sort(xs, xs + nx, [](const Cross& a, const Cross& b) { return a.x < b.x; });
        auto xAt = [&](int e, float y)
        {
            const int j = (e + 1) % p.n;
            const float ay = p.v[e * 2 + 1], by = p.v[j * 2 + 1];
            return p.v[e * 2] + (y - ay) * (p.v[j * 2] - p.v[e * 2]) / (by - ay);
        };
        const u32 c0 = mix(y0), c1 = mix(y1);
        const bool inHole = hole && ym > g_hole.y0 && ym < g_hole.y1;
        for (int s = 0; s + 1 < nx; s += 2)
        {
            const float la = xAt(xs[s].e, y0), lb = xAt(xs[s].e, y1), ra = xAt(xs[s + 1].e, y0),
                        rb = xAt(xs[s + 1].e, y1);
            if (inHole && g_hole.x0 > std::max(la, lb) && g_hole.x1 < std::min(ra, rb))
            {
                // left of the hole, right of it (the hole lies inside the span)
                const float l[8] = {la, y0, g_hole.x0, y0, g_hole.x0, y1, lb, y1};
                const float r[8] = {g_hole.x1, y0, ra, y0, rb, y1, g_hole.x1, y1};
                SolidQuad(l, c0, c0, c1, c1), SolidQuad(r, c0, c0, c1, c1);
                continue;
            }
            const float xy[8] = {la, y0, ra, y0, rb, y1, lb, y1};
            SolidQuad(xy, c0, c0, c1, c1);
        }
    }
}

// The top panel: the HUD panel look hanging from the top edge (frame line, rim, bevel, chamfered lower corners, the
// scanline sweeping down every 4 s, the frame breathing over 2 s as the HUD's edge pulse does), drawn in its displayed
// place (g_panel) with the slide offset. alpha = the panel's opacity factor (slide and background_opacity).
static void DrawTopPanel(float sh, float opacity, double now)
{
    const float a = g_panel.alpha;
    if (a <= 0.005f || g_panel.w <= 0 || g_panel.h <= 0)
        return;
    const float x0 = g_panel.x - g_panel.w * 0.5f, x1 = g_panel.x + g_panel.w * 0.5f;
    // the top beyond the screen edge, or (space_narrow 3) the panel's own top edge with its frame line drawn
    const bool floating = g_panel.top > 0;
    const float bot = g_panel.h + g_panel.yoff;
    const float top = floating ? g_panel.top + g_panel.yoff : std::min(0.0f, bot - g_panel.h) - sh * 0.01f;
    const float cw = sh * kChamW, ch = sh * kChamH;
    Poly poly;
    const float tx0 = g_panel.x - g_panel.tabW * 0.5f, tx1 = g_panel.x + g_panel.tabW * 0.5f;
    if (g_panel.bodyTop > 0 && g_panel.tabW > 0 && tx0 > x0 + cw && g_panel.bodyTop + g_panel.yoff + ch < bot - ch)
    {
        // the tab layout (space_narrow 5): the name tab from the top edge, the body stepping out below the top HUD row
        // the neck where the tab meets the body chamfered like the outer corners (user, 21 Oct 2026), when both have room
        const float yb = g_panel.bodyTop + g_panel.yoff;
        const bool neck = yb - ch > top + ch && tx1 + cw < x1 - cw && tx0 - cw > x0 + cw;
        poly.add(tx0, top), poly.add(tx1, top);
        if (neck)
            poly.add(tx1, yb - ch), poly.add(tx1 + cw, yb);
        else
            poly.add(tx1, yb);
        poly.add(x1 - cw, yb), poly.add(x1, yb + ch);
        poly.add(x1, bot - ch), poly.add(x1 - cw, bot), poly.add(x0 + cw, bot), poly.add(x0, bot - ch);
        poly.add(x0, yb + ch), poly.add(x0 + cw, yb);
        if (neck)
            poly.add(tx0 - cw, yb), poly.add(tx0, yb - ch);
        else
            poly.add(tx0, yb);
    }
    else
    {
        poly.add(x0, top), poly.add(x1, top), poly.add(x1, bot - ch), poly.add(x1 - cw, bot), poly.add(x0 + cw, bot);
        poly.add(x0, bot - ch);
    }
    auto argb = [&](u32 rgb, float al)
    { return (static_cast<u32>(std::clamp(al * a, 0.0f, 1.0f) * 255.0f) << 24) | rgb; };
    // the base: translucent violet-navy (13, 11, 26 over dark space), at background_opacity
    PolyBand(poly, top, bot, argb(0x0D0B1A, opacity), argb(0x0D0B1A, opacity));
    // the scanline (as HudPanel: refreshtest2's pale blue line, its glow trailing above), inside the outline
    {
        const float h = bot - top, trail = 0.22f * std::min(h, sh * 0.3f), thick = std::max(2.0f, sh * 0.0028f);
        const float phase = static_cast<float>(std::fmod(now, 4.0) / 4.0);
        const float y = top + phase * (h + trail);
        const float peak = 50.0f / 255.0f * 1.6f;
        PolyBand(poly, std::max(top, y - trail), std::min(bot, y), argb(0xA0D1FF, 0.0f), argb(0xA0D1FF, peak * 0.35f));
        PolyBand(poly, std::max(top, y - thick * 0.5f), std::min(bot, y + thick * 0.5f), argb(0xA0D1FF, peak),
                 argb(0xA0D1FF, peak));
    }
    // the frame: a soft shoulder outside, the line, the bevel inside - mitred bands along every visible edge (those wholly
    // above the screen edge are skipped). Profile across the bottom panels' edge at 4K: ~2 px rising, the line (118, 151,
    // 180) ~3 px, then the bevel from (70, 77, 98) down to the base over ~16 px. The edges facing down (their inside
    // above them) breathe, as the HUD's edges facing the screen do.
    const float line = std::max(1.5f, sh * kFrameLine), bevel = sh * kBevel, rim = std::max(1.0f, line * 0.5f);
    const float breath = 1.0f + 0.06f * std::sin(static_cast<float>(now) * 3.14159265f); // 2 s period, +-6 %
    const Poly o0 = Offset(poly, -rim), o1 = Offset(poly, 0.0f), o2 = Offset(poly, line),
               o3 = Offset(poly, line + bevel);
    for (int i = 0; i < poly.n; ++i)
    {
        const int j = (i + 1) % poly.n;
        if (poly.v[i * 2 + 1] <= 0.5f && poly.v[j * 2 + 1] <= 0.5f)
            continue; // beyond the top edge
        float nx, ny;
        InwardNormal(poly, i, nx, ny);
        const float facing = ny < -0.5f ? breath : 1.0f;
        auto band = [&](const Poly& a, const Poly& b, u32 ca, u32 cb)
        {
            const float xy[8] = {a.v[i * 2], a.v[i * 2 + 1], a.v[j * 2], a.v[j * 2 + 1],
                                 b.v[j * 2], b.v[j * 2 + 1], b.v[i * 2], b.v[i * 2 + 1]};
            SolidQuad(xy, ca, ca, cb, cb);
        };
        band(o0, o1, argb(0x7697B4, 0.0f), argb(0x7697B4, 0.45f * facing)); // the shoulder: rises into the line
        const u32 lc = argb(0x7697B4, std::min(1.0f, 0.94f * facing));      // headroom for the breathing
        band(o1, o2, lc, lc);
        band(o2, o3, argb(0x464D62, 0.95f * facing), argb(0x464D62, 0.0f));
    }
}

// The flight HUD scales with the screen height and is anchored to the bottom corners: the bottom-left
// panels end 0.613 H from the left edge, the bottom-right ones start 0.564 H from the right edge, the
// throttle bar between them is 0.044 H tall, the panels 0.30 H (HD Edition with Advanced Widescreen HUD,
// measured at 3840x2160). Subtitles go into that gap when it holds ~28 characters a row, otherwise above
// the panels; the chat log grows upwards from the panels on the left, so the gap also keeps clear of it.
// Letterboxed scenes (cutscene camera, room scenes with the letterbox up): letterbox_mode 0 puts the block inside the
// lower bar (inline name, no backing), 1 rests it on the bar's top edge inside the picture (Outlaws); without bars
// (amount 0: a plain room, a mod that removed them) the plain bottom placement applies.
static Placement Letterboxed(int sw, int sh, int maxW)
{
    Placement p{sh * (1.0f - g_set.bottomCutscene), sw * 0.5f, maxW};
    p.bar = LetterboxBar(sw, sh);
    if (p.bar <= 0)
        return p;
    if (g_set.letterboxMode == 0)
        p.inBar = true;
    else
        p.bottom = static_cast<float>(sh - p.bar) - sh * kBarRestGap;
    return p;
}

static Placement PlaceBase(int sw, int sh, int maxW, int avgChar, float padX)
{
    const u32 camera = Mem<u32>(exe::CurrentCamera);
    if (exe::CamCutscene.is(camera)) // cutscene camera: letterboxed
        return Letterboxed(sw, sh, maxW);
    if (camera && hk::Call<void*(__cdecl*)()>(exe::GetPlayerIObj)) // flying, HUD shown
    {
        // HUD 2 replaces the game's HUD: its room (section "HUD 2"); held for up to 0.3 s after it goes while the game's
        // HUD is not readable yet (the switch: one re-place, no detour through the fallback below)
        const HudLayout* h2 = Hud2Layout();
        if (!h2 && Hud2Recent() && g_set.spacePosition == 2)
        {
            g_topRoomAt = 0; // read the game's HUD every frame until it is back
            if (TopRoom(sw, sh) <= 0)
                h2 = Hud2Last();
        }
        if (h2)
            return PlaceHud2(*h2, sw, sh, maxW, avgChar, padX);
        // centred on the screen (under the crosshair), as wide as the gap allows on both sides
        // top: just below the maneuver bar and the small box hanging under its left end (to 0.153 H; the bar itself
        // ends 0.086 H), as close to the top as the HUD allows (user, 19 Oct 2026); while the game shows its objective
        // banner (the "MissionObjectiveText" control, Agency FB 0.06 H
        // at 0.17-0.225 H, e.g. "FLY TO RESEARCH STATION WILLARD") the stack moves below it and back after (it
        // glides: DrawImpl). The comm portrait sits top left, clear of the centre.
        // space_position 2: the top panel, when the bar has moved out of the way (else as 1 below)
        if (g_set.spacePosition == 2)
        {
            const float room = TopRoom(sw, sh);
            const float side = std::max(padX, sh * (kChamW + kPanelGap)); // text clear of the chamfers
            const int w = static_cast<int>(2 * (room - side));
            // readable rows; narrow screens per space_narrow (1 / 2: in the room at the top, 2 with the small font -
            // DrawImpl - down to 12 characters; 3 below); room 0 = the bar has not moved (yet, or space_narrow 0)
            const bool narrow = NarrowScreen(sw, sh);
            if (room > 0 && (w >= avgChar * 26 ||
                             (narrow && (g_set.spaceNarrow == 1 || g_set.spaceNarrow == 2) && w >= avgChar * 12)))
            {
                Placement p{sh * kPanelTop, sw * 0.5f, std::min(maxW, w), true, true};
                p.topPanel = true;
                return p;
            }
            // the tab layout needs the room for its smallest tab (0.16 H) between the bar and the NeuroNet box, and its
            // panel + tab clear of the bar (DrawImpl's keep-out test, g_panel.tabNoRoom); else below the top row (as 3)
            const float tabGap = sh * kPanelGap;
            const bool tabFits = g_nnLeft - g_barRight - 2 * tabGap >= sh * 0.16f && !g_panel.tabNoRoom;
            if (room > 0 && narrow && g_set.spaceNarrow == 5 && g_set.nameMode == 0 && tabFits)
            {
                // the tab layout (user's idea, 21 Oct 2026): the first speaker's name in a tab at the very top between
                // the bar and the NeuroNet box, the rows in a body below the top HUD row, as wide as the space under the
                // bar's lower right and the NeuroNet box allows (clear of the cruise box under the bar's left end)
                const float bodyTop = sh * (kBarFrameBottom + kPanelGap);
                float half = sw * 0.5f - sh * kPanelGap;
                if (g_cruiseBottom > bodyTop)
                    half = std::min(half, sw * 0.5f - g_cruiseRight - sh * kPanelGap);
                Placement p{sh * kPanelTop, sw * 0.5f, std::min(maxW, static_cast<int>(2 * (half - side))), true, true};
                p.topPanel = true;
                p.bodyTop = bodyTop;
                p.tabHalf = room;
                const float gap = sh * kPanelGap;
                p.tabLeft = g_barRight + gap, p.tabRight = g_nnLeft - gap;
                p.bodyMinX = (g_cruiseBottom > bodyTop ? g_cruiseRight : 0.0f) + gap, p.bodyMaxX = sw - gap;
                return p;
            }
            if (room > 0 && narrow && (g_set.spaceNarrow == 3 || (g_set.spaceNarrow == 5 && g_set.nameMode == 0)))
            {
                // just below the top HUD row (the bar's frame ends 0.083 H, the NeuroNet box 0.032 H), as wide as the
                // rows need, clear of the cruise box hanging under the bar's left end
                const float top = sh * (kBarFrameBottom + kPanelGap);
                float half = sw * 0.5f - sh * kPanelGap;
                if (g_cruiseBottom > top)
                    half = std::min(half, sw * 0.5f - g_cruiseRight - sh * kPanelGap);
                Placement p{top + sh * kPanelTop, sw * 0.5f, std::min(maxW, static_cast<int>(2 * (half - side))), true,
                            true};
                p.topPanel = true;
                p.panelTop = top;
                return p;
            }
        }
        if (g_set.spacePosition >= 1)
        {
            Placement p{sh * (BannerShown() ? kBelowBanner : kBelowBar), sw * 0.5f, maxW, true, true};
            p.handOver = g_set.spacePosition == 2; // the top panel's room is not there yet (the HUD is coming up)
            return p;
        }
        const float left = sh * kLowPanelsLeft, right = sw - sh * kLowPanelsRight;
        const float half = std::min(sw * 0.5f - left, right - sw * 0.5f);
        const int gap = static_cast<int>(2 * half - 2 * padX - sh * kLowGapMargin);
        if (g_set.bottomSpace < 0.3f || gap < avgChar * 28)
            return {sh * (1.0f - g_set.bottomSpace), sw * 0.5f, maxW, false, true};
        return {sh * (1.0f - kLowBottom), sw * 0.5f, std::min(maxW, gap), false, true};
    }
    // a base: a room scene letterboxes the room (the base camera stays), a plain room has no bars
    Placement p = Letterboxed(sw, sh, maxW);
    if (p.bar <= 0)
        p.bottom = sh * (1.0f - g_set.bottomBase);
    // the pad's prompt lane (docs\UI_HUD2_EXTENDED.md 13; core\hud_bus.h v2): on a docked screen whose prompt pills sit
    // at the bottom, the lowest row ends 0.010 H above the lane - the bottom row always (one bottom band, the subtitles
    // over the pills), a column right of a window only where the block can reach its x range (a block inside the
    // letterbox bar that the lane cuts into rests above it instead). Stable per screen: the controller publishes the
    // lane whether its pills show or have faded. No lane (keyboard and mouse, an older controller logic) = as before.
    HudRect lane{};
    bool column = false;
    const bool haveLane = PadLane(lane, column);
    const float base = p.bottom; // (before the lane)
    if (haveLane && (!column || (lane.x0 * sw < p.centerX + p.maxW * 0.5f && lane.x1 * sw > p.centerX - p.maxW * 0.5f)))
    {
        const float limit = lane.y0 * sh - sh * 0.010f;
        if (p.inBar && limit < static_cast<float>(sh - p.bar))
            p.inBar = false, p.bottom = limit;
        else if (!p.inBar)
            p.bottom = std::min(p.bottom, limit);
    }
    // The open window (critique 4: at 21:9 the block lifted above the lane covered the offer's ACCEPT / DECLINE): the
    // block's top also stays 0.010 H under the windows' floor in its x range (bus v3 HudFloor: the lowest plate / keep-out
    // edge per slice, published by the controller while the pad is the device; none = as before). The block's extent is
    // the last frame's (its rows do not depend on the bottom; a new block: name + two rows). Where it does not fit
    // between the window and the lane: (a) the lane counts only where it reaches the block's x range (a row beside a
    // centred block leaves the base bottom free); else (b) the lane yields - the block hangs right under the window and
    // the pills keep clear of it (it is published on the bus: HudDockedSubs, a keep-out of the prompt bar) - the
    // subtitles never over a window's buttons, the pills never under the subtitles. Where even (b) would end below the
    // 0.95 H safe margin (a window reaching the screen's foot: the dealers, the news window's trimmed plate) nothing keeps
    // off it: the block rests at its base bottom (bottom_base: the least it covers of the window) and the lane yields.
    if (!p.inBar && !p.fromTop)
    {
        static u32 floorSeen = 0;
        static ULONGLONG floorAt = 0;
        HudFloor fl{};
        bool haveFloor = false;
        const HudBus* bus = Bus();
        if (bus && bus->version >= 3 && bus->FloorFrame && bus->ReadFloor)
        {
            const u32 f = bus->FloorFrame();
            const ULONGLONG t = GetTickCount64();
            if (f != floorSeen)
                floorSeen = f, floorAt = t;
            haveFloor = t - floorAt < 100 && bus->ReadFloor(&fl) && fl.count > 0 && fl.count <= kHudFloorSlices;
        }
        // the block's extent: the last frame's blocks in a base (fresh), else a name and two rows
        float stackH = 0, x0 = p.centerX, x1 = p.centerX, lineH = 0;
        if (!LastDockedExtent(sh, padX, stackH, x0, x1, lineH) || stackH <= 0)
        {
            stackH = (lineH > 0 ? lineH : sh * 0.032f) * 3.2f;
            x0 = p.centerX - p.maxW * 0.5f, x1 = p.centerX + p.maxW * 0.5f;
        }
        stackH += sh * 0.006f; // (the backing's padding)
        float floorY = 0;
        if (haveFloor)
            for (int i = 0; i < fl.count; ++i)
            {
                const float s0 = static_cast<float>(sw) * i / fl.count, s1 = static_cast<float>(sw) * (i + 1) / fl.count;
                if (s1 > x0 && s0 < x1)
                    floorY = std::max(floorY, fl.bottom[i] * sh);
            }
        const float gap = sh * 0.010f, margin = sh * 0.012f;
        int how = 0; // 0 clear of the window, 1 between it and the lane, 2 beside the lane, 3 the lane yields, 4 no room
        if (floorY > 0 && p.bottom - stackH < floorY + gap)
        {
            const bool laneHits = haveLane && lane.x0 * sw < x1 + margin && lane.x1 * sw > x0 - margin;
            const float b = laneHits ? std::min(base, lane.y0 * sh - gap) : base;
            const float hang = floorY + gap + stackH;
            if (b - stackH >= floorY + gap)
                p.bottom = b, how = laneHits ? 1 : 2;
            else if (hang <= sh * 0.95f)
                p.bottom = hang, how = 3;
            else
                p.bottom = std::max(p.bottom, base), how = 4; // (as low as the block may go: the least of the window)
        }
        static int lastHow = 0;
        if (how != lastHow && how != 0)
        {
            static const char* kHow[] = {"", "between the window and the lane", "beside the lane, under the window",
                                         "under the window (the lane yields)", "over the window's foot at the base bottom (no room; the lane yields)"};
            Log("display: docked block %s (floor %.0f px, block %.0f px tall, bottom %.0f)", kHow[how], floorY, stackH,
                p.bottom);
        }
        lastHow = how;
        if (bus && bus->version >= 3 && bus->PublishDockedSubs)
        {
            static u32 n = 0;
            HudDockedSubs s{};
            s.frame = ++n;
            s.rect = {x0 / sw, (p.bottom - stackH) / sh, x1 / sw, p.bottom / sh};
            bus->PublishDockedSubs(&s);
        }
    }
    return p;
}

// The game's own fonts, loaded once for this process (core\game\gamefont.* since 25 Oct 2026, shared with the controller
// module's UI): the subtitles then share the game's typefaces - Agency FB for names, Arial Unicode MS for the text -
// from the game's DATA\FONTS files, else the same face installed in Windows (vanilla), else Segoe UI (gamefont.h).
bool GameFonts() { return gamefont::Load(); }

// Text and name fonts for the em height px: the game's (setting font 0, when its files load and the faces resolve) or
// Segoe UI. Agency FB is condensed with a small x-height: its names are drawn 1.2x larger to read as the same size.
// With HUD 2 (agency): the text in Agency FB too (the round 3 critique of the built HUD 2: speaker names and lines in
// two families beside HUD 2's Agency FB throughout), 1.2x as the names for the same reading size.
static int g_fontsPx = 0;          // the em px the fonts were last built for (Agency FB is built larger)
static bool g_fontsAgency = false; // built with Agency FB text (HUD 2 showed)
static bool BuildFonts(int px, const std::wstring& chars, bool agency = false)
{
    const bool game = g_set.font == 0 && GameFonts();
    g_fontsPx = px, g_fontsAgency = agency; // (what was asked for: a missing face does not rebuild every frame)
    const bool useAgency = agency && game;
    const bool gameText =
        game && (useAgency ? render::BuildFont(g_font, gamefont::LabelFace(), static_cast<int>(px * 1.2f + 0.5f), 400, chars)
                               : render::BuildFont(g_font, gamefont::TextFace(), px, 400, chars));
    const bool text = gameText || render::BuildFont(g_font, L"Segoe UI", px, 600, chars);
    if (!text)
        return false;
    // the spoken word's pop (word_pop): the text face at the pop's full size, drawn scaled down to each moment's size
    const int popPx = static_cast<int>(px * kPopScale + 0.5f);
    if (!(g_set.wordPop && (gameText ? render::BuildFont(g_popFont, useAgency ? gamefont::LabelFace() : gamefont::TextFace(),
                                                         useAgency ? static_cast<int>(popPx * 1.2f + 0.5f) : popPx, 400, chars)
                                     : render::BuildFont(g_popFont, L"Segoe UI", popPx, 600, chars))))
        render::FreeFont(g_popFont);
    if (!(game && render::BuildFont(g_nameFont, gamefont::LabelFace(), static_cast<int>(px * 1.2f + 0.5f), 700, chars)) &&
        !render::BuildFont(g_nameFont, L"Segoe UI", px, 700, chars))
        render::FreeFont(g_nameFont); // names then fall back to the text font
    return true;
}

static u32 Fade(u32 argb, float a)
{
    return (static_cast<u32>(static_cast<float>(argb >> 24) * std::clamp(a, 0.0f, 1.0f) + 0.5f) << 24) |
           (argb & 0xFFFFFF);
}

// soft: draw into these premultiplied BGRA pixels (sw x sh) instead of Direct3D (movies).
// What the last frame drew (debug `layout`; tests across display modes read it instead of measuring pixels)
struct LayoutBlock
{
    std::string speaker;
    float y, h, w;
    bool corner;
    float x = 0;          // the widest row's left edge (the arrows keep clear of the blocks)
    u32 name = 0;         // the speaker's name colour (0xAARRGGBB)
    bool hostile = false; // the hostile marker drawn
};
static struct
{
    int sw = 0, sh = 0, px = 0, maxW = 0;
    int vpX = 0, vpY = 0, vpW = 0, vpH = 0; // the device viewport at draw time (UI pass / Present)
    int bar = 0;                            // the game's letterbox bar (px) and whether the block sits inside it
    bool inBar = false;
    float lineH = 0, nameH = 0, gap = 0, bottom = 0, centerX = 0;
    bool fromTop = false, flight = false, cutscene = false, letterbox = false, movie = false;
    bool topPanel = false; // the top panel (space_position 2) and its displayed state
    float panelX = 0, panelW = 0, panelH = 0, panelYoff = 0, panelS = 0;
    float portraitX = 0, portraitY = 0, portraitS = 0; // the comm portrait's slot (0 = none)
    bool hud2 = false; // placed in HUD 2's room: the band, the stack's limit, the Neural Net column (px)
    float freeL = 0, freeR = 0, limit = 0, nnLeft = 0, nnTop = 0;
    std::vector<LayoutBlock> blocks;
    ULONGLONG at = 0;
} g_layout;

static bool LastDockedExtent(int sh, float padX, float& h, float& x0, float& x1, float& lineH)
{
    lineH = g_layout.sh == sh ? g_layout.lineH : 0;
    if (g_layout.flight || g_layout.movie || g_layout.fromTop || g_layout.blocks.empty() || g_layout.sh != sh ||
        GetTickCount64() - g_layout.at >= 250)
        return false;
    float top = 1e9f, bot = -1e9f;
    for (const LayoutBlock& b : g_layout.blocks)
        top = std::min(top, b.y), bot = std::max(bot, b.y + b.h), x0 = std::min(x0, b.x - padX),
        x1 = std::max(x1, b.x + b.w + padX);
    h = bot - top;
    return true;
}

// The objective banner stays clear below the top panel while it is out, and glides back after: every frame (Update),
// also once the panel is gone and nothing is drawn.
static void UpdateBanner()
{
    const float sh = static_cast<float>(Mem<int>(exe::ScreenH));
    KeepBannerClear(g_layout.topPanel && g_panel.s > 0 ? g_panel.h + g_panel.yoff + sh * kPanelGap : -1.0f, sh);
}

// ------------------------------------------------------------------ HUD 2 (docs\UI_HUD2.md; core\hud_bus.h)
// The controller logic's HUD 2 replaces the game's flight HUD (the maneuver bar, the NeuroNet box, the target / contact /
// status panels and the gauges are hidden; the comm window, the objective banner, the target indicators stay native) and
// publishes on the HUD bus, every frame, where its parts are. While a fresh layout (the bus advanced within our last 2
// frames) says `active`, every flight placement takes HUD 2's room instead of the game's controls - the top panel in the
// band between its nav and its target card, the Neural Net column under the target card, the off-screen arrows clear of
// its parts, the chat rows inside its chat band, the comm window's own place below the nav - and the module answers on
// the bus where its dialogue panel and chat column landed. A stale or inactive layout = the game's HUD: today's
// placements, unchanged. The bus is read once per frame (one plain copy); nothing walks the controls while HUD 2 shows.
// Debug `hud2sim on|off` publishes the mock-up's layout (iteration 3) so all of it runs before HUD 2 exists; a real
// publisher wins (the sim yields while anyone else publishes).
static struct
{
    bool on = false;              // HUD 2 replaces the game's HUD this frame
    HudLayout l{};                // its layout (read this frame while on; the last one after)
    u32 frame = 0;                // our frame counter (Update)
    u32 busFrame = 0, seenAt = 0; // the bus's publish counter as last seen, and our frame when it last moved
    double offAt = -1;            // when HUD 2 last went away (-1: on, or long gone)
    bool sim = false;             // debug hud2sim
    bool simArmed = false;        // simSeen holds a reading
    u32 simSeen = 0;              // the bus counter as the sim last left it (moved on by someone else: the sim yields)
    int simYield = 0;             // frames the sim still yields
    u32 published = 0;            // HudSubtitles published
    HudSubtitles last{};          // ... the last of them (debug `layout`)
    // the pad's prompt lane (bus v2): the lane counter as last seen, our frame when it last moved, the fresh lane
    u32 laneBusFrame = 0, laneSeenAt = 0;
    bool laneOn = false;
    HudLane lane{};
} g_hud2;

static const HudBus* Bus()
{
    if (!g_host || g_host->version < 8 || !g_host->GetHudBus)
        return nullptr;
    const HudBus* bus = g_host->GetHudBus();
    return bus && bus->version >= 1 && bus->PublishLayout && bus->ReadLayout && bus->PublishSubtitles && bus->Frame
               ? bus
               : nullptr;
}

static const HudLayout* Hud2Layout() { return g_hud2.on ? &g_hud2.l : nullptr; }

static bool PadLane(HudRect& r, bool& column)
{
    if (!g_hud2.laneOn)
        return false;
    r = g_hud2.lane.rect;
    column = g_hud2.lane.column != 0;
    return true;
}

bool Hud2ShowsAlert(const std::string& raw)
{
    if (!g_hud2.on || !g_hud2.l.active || !g_hud2.l.alerts)
        return false;
    std::string id = raw;
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    // the ids of the events HUD 2's alert cards show (hud2.cpp Slow: SHIELD FAILING, HULL CRITICAL, MISSILE INCOMING,
    // CRUISE DISRUPTED); ids are the same in every language
    static const char* kIds[] = {"nnv_shield_failed", "nnv_shield_destroyed", "shield_destroyed", "hull_breach_imminent",
                                 "incoming_missile",  "incoming_torpedo",     "nnv_cruise_disrupted"};
    for (const char* k : kIds)
        if (id == k)
            return true;
    return false;
}
static bool Hud2Recent() { return !g_hud2.on && g_hud2.offAt >= 0 && Now() - g_hud2.offAt < 0.3; }
static const HudLayout* Hud2Last() { return &g_hud2.l; }

// The mock-up's layout (docs\UI_HUD2.md, iteration 3; fractions of the screen): the ring 0.13 H around the centre.
static HudLayout SimLayout()
{
    HudLayout s{};
    s.active = 1, s.alpha = 1.0f, s.scale = 1.0f;
    const int sw = Mem<int>(exe::ScreenW), sh = Mem<int>(exe::ScreenH);
    const float rx = sw > 0 && sh > 0 ? 0.13f * sh / sw : 0.073f;
    s.nav = {0.05f, 0.04f, 0.37f, 0.20f};
    s.target = {0.69f, 0.05f, 0.95f, 0.40f};
    s.ship = {0.05f, 0.78f, 0.27f, 0.95f};
    s.loadout = {0.66f, 0.78f, 0.95f, 0.95f};
    s.ring = {0.5f - rx, 0.37f, 0.5f + rx, 0.63f};
    s.context = {0.38f, 0.89f, 0.62f, 0.94f};
    s.topFree = {0.30f, 0.0f, 0.68f, 0.30f};
    s.chatFree = {0.03f, 0.30f, 0.32f, 0.72f};
    return s;
}

static void Hud2Update()
{
    ++g_hud2.frame;
    const bool was = g_hud2.on;
    g_hud2.on = false;
    if (const HudBus* bus = Bus())
    {
        if (g_hud2.sim)
        {
            const u32 f = bus->Frame();
            if (g_hud2.simArmed && f != g_hud2.simSeen)
                g_hud2.simYield = 10; // someone else published since: HUD 2 itself wins
            g_hud2.simSeen = f, g_hud2.simArmed = true;
            if (g_hud2.simYield > 0)
                --g_hud2.simYield;
            else
            {
                HudLayout s = SimLayout();
                s.frame = g_hud2.frame;
                bus->PublishLayout(&s);
                g_hud2.simSeen = bus->Frame();
            }
        }
        const u32 f = bus->Frame();
        if (f != g_hud2.busFrame)
            g_hud2.busFrame = f, g_hud2.seenAt = g_hud2.frame;
        HudLayout l;
        if (g_hud2.frame - g_hud2.seenAt <= 2 && bus->ReadLayout(&l) && l.active)
            g_hud2.l = l, g_hud2.on = g_set.enabled;
        // the prompt lane (bus v2; an older bus has none): fresh = published within our last 2 frames
        g_hud2.laneOn = false;
        if (bus->version >= 2 && bus->LaneFrame && bus->ReadLane)
        {
            const u32 lf = bus->LaneFrame();
            if (lf != g_hud2.laneBusFrame)
                g_hud2.laneBusFrame = lf, g_hud2.laneSeenAt = g_hud2.frame;
            HudLane ln;
            if (g_hud2.frame - g_hud2.laneSeenAt <= 2 && bus->ReadLane(&ln) && ln.inUse && Shown(ln.rect))
                g_hud2.lane = ln, g_hud2.laneOn = true;
        }
    }
    else
        g_hud2.laneOn = false;
    if (g_hud2.on)
        g_hud2.offAt = -1;
    else if (was)
        g_hud2.offAt = Now(), g_topRoomAt = g_keepOutsAt = 0; // the game's HUD is back: read it afresh
}

// ---------------------------------------------------------------- FLUF's windows (HUD bus v4, core\fluf.h)
// FLUF's ImGui windows draw over FLvibed's overlay (the command palette at the top centre - where the flight top panel
// hangs -, its options / Player Status window and nav map, toasts top left): the dialogue block steps away from them.
// The bus reads FLUF once per its frame and filters by the controller's `fluf_windows` (2 until the controller says
// otherwise; this module alone keeps that default). No FLUF / an older bus: no keep-outs, today's placement exactly.
static HudFluf g_fluf{};
static std::string g_flufStatus;

static void FlufRead()
{
    g_fluf.present = 0, g_fluf.count = 0;
    const HudBus* bus = Bus();
    if (bus && bus->version >= 4 && bus->ReadFluf)
        bus->ReadFluf(&g_fluf);
    if (g_fluf.present && g_flufStatus != g_fluf.status)
    {
        g_flufStatus = g_fluf.status;
        Log("display: %s - the dialogue keeps clear of FLUF's open windows", g_fluf.status);
    }
}

// The block's anchor moved clear of FLUF's windows that cover its column: a block hanging from the top goes below them
// (the top panel with it: its frame and the comm portrait follow panelTop), a block standing on its bottom edge goes
// above a window over that edge while that leaves it room (else it stays: readable over a window beats hidden under it).
static Placement Place(int sw, int sh, int maxW, int avgChar, float padX)
{
    Placement p = PlaceBase(sw, sh, maxW, avgChar, padX);
    FlufRead();
    if (!g_fluf.count || p.inBar)
        return p;
    const float W = static_cast<float>(sw), H = static_cast<float>(sh), gap = H * kPanelGap;
    const float x0 = p.centerX - p.maxW * 0.5f, x1 = p.centerX + p.maxW * 0.5f;
    auto inColumn = [&](const HudRect& r) { return r.x0 * W < x1 && r.x1 * W > x0; };
    if (p.fromTop)
    {
        const float top = p.topPanel ? p.panelTop : p.bottom;
        float clear = top;
        for (int pass = 0; pass < 2; ++pass) // (a window right under another)
            for (int i = 0; i < g_fluf.count && i < kHudFlufMax; ++i)
            {
                const HudRect& r = g_fluf.win[i].rect;
                if (inColumn(r) && r.y0 * H <= clear + H * 0.06f && r.y1 * H > clear)
                    clear = r.y1 * H + gap;
            }
        if (clear > top && clear < H * 0.6f)
        {
            const float d = clear - top;
            p.bottom += d;
            if (p.topPanel)
                p.panelTop += d;
            if (p.bodyTop > 0)
                p.bodyTop += d;
            if (p.limit > 0)
                p.limit = std::max(p.limit, p.bottom + H * 0.15f);
        }
    }
    else
    {
        float bottom = p.bottom;
        for (int i = 0; i < g_fluf.count && i < kHudFlufMax; ++i)
        {
            const HudRect& r = g_fluf.win[i].rect;
            if (inColumn(r) && r.y0 * H < bottom && r.y1 * H > bottom - H * 0.12f && r.y0 * H - gap > H * 0.35f)
                bottom = std::min(bottom, r.y0 * H - gap);
        }
        p.bottom = bottom;
    }
    return p;
}

// The chat window's row width (chat.cpp): the lower-left panels' width (0.585 H), with HUD 2 inside its chat band (the
// game's log starts ~0.01 H from the left edge).
float ChatRowMax(int sw, int sh)
{
    float m = sh * 0.585f;
    if (const HudLayout* l = Hud2Layout(); l && Shown(l->chatFree))
        m = std::min(m, std::max(sh * 0.25f, l->chatFree.x1 * sw - sh * 0.02f));
    return m;
}

// The room HUD 2 leaves, per space_position (fractions of the screen from its layout, px here):
// - 2 (the top panel): the band topFree (as HUD 2 publishes it: it may overlap the nav's rectangle where the nav's text
//   does not reach); the panel centres on the screen and moves aside as far as its width needs; rows as wide as the band less the panel's
//   sides; the stack ends above the band's bottom or the ring's top, whichever is lower. Every space_narrow value maps to
//   this panel (the bar they arrange is hidden; the band is the room), 2 keeping its smaller font on narrow screens.
// - 1 (the top third, a backing per speaker): the same band, hanging from the HUD's top line (the nav's top), below the
//   objective banner while it shows, as today.
// - 0 (between the bottom panels): between the ship panel and the loadout, above the context line (the line's usual
//   place when none shows); bottom_space below 0.3, or a gap under 28 characters, keep today's fallback.
// The Neural Net column (all three): under the target card, left-aligned in its width (or in its place, top right, while
// no card shows).
static Placement PlaceHud2(const HudLayout& l, int sw, int sh, int maxW, int avgChar, float padX)
{
    const float W = static_cast<float>(sw), H = static_cast<float>(sh), gap = H * kPanelGap;
    // topFree is HUD 2's word on where the panel may go (it knows how far its nav text and target card reach); without it
    // the room between the nav and the target card
    HudRect band = l.topFree;
    if (!Shown(band))
        band = {Shown(l.nav) ? l.nav.x1 + kPanelGap * H / W : 0.30f, 0.0f,
                Shown(l.target) ? l.target.x0 - kPanelGap * H / W : 0.70f, 0.30f};
    const float L = band.x0 * W;
    float R = band.x1 * W;
    const float bandBottom = band.y1 * H;
    // No target card: the Neural Net column takes the top right corner (18 characters wide, at HUD 2's right margin); a
    // band too narrow for 20 characters a row then reaches to that column (the card's place is free).
    const float nnW = avgChar * 18 + 2 * padX;
    if (!Shown(l.target) && R - L - 2 * std::max(padX, H * (kChamW + kPanelGap)) < avgChar * 20)
        R = std::max(R, W * 0.95f - nnW - gap);
    Placement p{0, W * 0.5f, maxW, true, true};
    p.hud2 = true;
    p.limit = std::max(bandBottom, Shown(l.ring) ? l.ring.y0 * H - gap : 0.0f);
    if (p.limit <= 0)
        p.limit = H * kStackLimit;
    if (Shown(l.target))
        p.nnLeft = l.target.x0 * W, p.nnTop = l.target.y1 * H + gap, p.nnRight = l.target.x1 * W;
    else
        p.nnRight = W * 0.95f, p.nnLeft = std::max(R + gap, p.nnRight - nnW),
        p.nnTop = (Shown(l.nav) ? l.nav.y0 : 0.05f) * H;
    // HUD 2's alert has its place (at 4:3 it stacks under the target card and its list, where this column starts): the
    // column starts below it while it reaches into the column's width near its top (the alert never moves for the column)
    if (Shown(l.alert) && l.alert.x0 * W < p.nnRight && l.alert.x1 * W > p.nnLeft && l.alert.y1 * H > p.nnTop &&
        l.alert.y0 * H < p.nnTop + H * 0.15f)
        p.nnTop = std::max(p.nnTop, l.alert.y1 * H + gap);
    if (g_set.spacePosition == 2)
    {
        const float side = std::max(padX, H * (kChamW + kPanelGap)); // text clear of the chamfers
        // never under 20 characters a row: a band narrower than that (HUD 2's layout at 4:3, 25 Oct 2026) is spilled over
        // evenly on both sides (DrawImpl centres a panel wider than the band on it)
        const int w = std::max(avgChar * 20, static_cast<int>(R - L - 2 * side));
        p.topPanel = true;
        p.panelTop = band.y0 * H > 1.0f ? band.y0 * H : 0.0f;
        p.bottom = p.panelTop + H * kPanelTop;
        p.maxW = std::min(maxW, w);
        p.freeL = L, p.freeR = R;
        p.centerX = std::clamp(W * 0.5f, L, R);
        return p;
    }
    if (g_set.spacePosition == 1)
    {
        p.maxW = std::min(maxW, std::max(avgChar * 12, static_cast<int>(R - L - 2 * padX)));
        p.bottom = BannerShown() ? H * kBelowBanner : std::max(band.y0, Shown(l.nav) ? l.nav.y0 : 0.04f) * H;
        const float lo = L + p.maxW * 0.5f + padX, hi = R - p.maxW * 0.5f - padX;
        p.centerX = lo <= hi ? std::clamp(W * 0.5f, lo, hi) : (L + R) * 0.5f;
        return p;
    }
    const float l0 = Shown(l.ship) ? l.ship.x1 * W + gap : H * kLowPanelsLeft;
    const float r0 = Shown(l.loadout) ? l.loadout.x0 * W - gap : W - H * kLowPanelsRight;
    const float half = std::min(W * 0.5f - l0, r0 - W * 0.5f);
    const int gapW = static_cast<int>(2 * half - 2 * padX);
    p.fromTop = false;
    if (g_set.bottomSpace < 0.3f || gapW < avgChar * 28)
    {
        p.bottom = H * (1.0f - g_set.bottomSpace);
        return p;
    }
    if (Shown(l.context))
        p.bottom = l.context.y0 * H - gap;
    else
    {
        const float row = std::max(Shown(l.ship) ? l.ship.y1 : 0.0f, Shown(l.loadout) ? l.loadout.y1 : 0.0f);
        p.bottom = row > 0 ? row * H - H * 0.05f - gap : H * (1.0f - kLowBottom);
    }
    p.maxW = std::min(maxW, gapW);
    return p;
}

// Every flight frame: where the dialogue panel (with the comm portrait) and the chat column landed, onto the bus (HUD 2
// puts its alert below the panel). The dialogue as displayed (the panel with its slide; without the panel the blocks'
// extent); the chat column wherever chatter goes to the game's chat window (its log grows up from 0.70 H on the left).
static void Hud2Publish()
{
    const HudBus* bus = Bus();
    if (!bus)
        return;
    const u32 cam = Mem<u32>(exe::CurrentCamera);
    if (!cam || exe::CamCutscene.is(cam) || !hk::Call<void*(__cdecl*)()>(exe::GetPlayerIObj))
        return;
    const float sw = static_cast<float>(Mem<int>(exe::ScreenW)), sh = static_cast<float>(Mem<int>(exe::ScreenH));
    if (sw <= 0 || sh <= 0)
        return;
    HudSubtitles s{};
    s.frame = g_hud2.frame;
    const bool drawn = g_set.enabled && g_layout.flight && GetTickCount64() - g_layout.at < 100;
    if (drawn && g_layout.topPanel && g_panel.alpha > 0.01f && g_panel.w > 0)
    {
        const float top = g_panel.top > 0 ? g_panel.top + g_panel.yoff : 0.0f;
        s.dialogue = {(g_panel.x - g_panel.w * 0.5f) / sw, std::max(0.0f, top) / sh,
                      (g_panel.x + g_panel.w * 0.5f) / sw, std::max(0.0f, g_panel.h + g_panel.yoff) / sh};
    }
    else if (drawn)
    {
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        const float pad = sh * 0.01f;
        for (const LayoutBlock& b : g_layout.blocks)
            if (!b.corner)
                x0 = std::min(x0, b.x - pad), y0 = std::min(y0, b.y - pad), x1 = std::max(x1, b.x + b.w + pad),
                y1 = std::max(y1, b.y + b.h + pad);
        if (x1 > x0)
            s.dialogue = {x0 / sw, y0 / sh, x1 / sw, y1 / sh};
    }
    if (g_set.enabled && (g_set.chatter == 0 || g_set.chatAll))
    {
        const HudLayout* l = Hud2Layout();
        if (l && Shown(l->chatFree)) // HUD 2's comm feed in its band (DrawHud2Feed)
            s.chat = {l->chatFree.x0, l->chatFree.y0, std::min(l->chatFree.x1, l->chatFree.x0 + 0.30f), l->chatFree.y1};
        else
        {
            const float left = sh * 0.01f, top = 0.30f;
            s.chat = {left / sw, top, (left + ChatRowMax(static_cast<int>(sw), static_cast<int>(sh))) / sw, 0.70f};
        }
    }
    bus->PublishSubtitles(&s);
    g_hud2.last = s, ++g_hud2.published;
}

// ---- HUD 2's comm feed (the round 3 critique of the built HUD 2, 25 Oct 2026): the ambient radio chatter that went to
// the game's chat window (chatter 0) is drawn here while HUD 2 shows - in HUD 2's chat band (chatFree: its left edge at
// the 0.05 W margin, not the chat window's ~10 px), Agency FB for the name and the line, each line on a navy scrim with
// an accent bar in the speaker's colour (the mock-up's comm feed), newest at the band's bottom, at most three. One comm
// surface at a time: while the top panel shows a line (a comm to the player, with its portrait) the feed is hidden and
// its lines wait (their time starts when they show; a line not shown within 6 s is dropped - stale chatter).
struct FeedLine
{
    std::wstring name, faction, text;
    u32 rgb = 0xFFFFFF;
    float sec = 0;
    double arrived = 0, shown = -1, end = 0;
};
static std::deque<FeedLine> g_feed;
static u32 g_feedShown = 0, g_feedDropped = 0;

bool Hud2Feed(const std::wstring& speaker, u32 rgb, const std::wstring& text, float seconds)
{
    if (!g_set.enabled || !g_hud2.on || !g_hud2.l.active || !Shown(g_hud2.l.chatFree) || text.empty())
        return false;
    FeedLine f;
    const size_t dot = speaker.find(L" \x00B7 ");
    f.name = dot == std::wstring::npos ? speaker : speaker.substr(0, dot);
    f.faction = dot == std::wstring::npos ? L"" : speaker.substr(dot + 3);
    f.text = text, f.rgb = rgb & 0xFFFFFF, f.sec = seconds, f.arrived = Now();
    SeedGlyphs(speaker + text);
    g_feed.push_back(std::move(f));
    while (g_feed.size() > 6)
        g_feed.pop_front(), ++g_feedDropped;
    return true;
}

static void DrawHud2Feed(int sw, int sh, bool ownScene)
{
    if (g_feed.empty())
        return;
    const double now = Now();
    if (!g_hud2.on || !g_set.enabled)
    {
        g_feed.clear(); // HUD 2 went: the chat window takes the next lines (impl.cpp)
        return;
    }
    while (!g_feed.empty() && ((g_feed.front().shown < 0 && now - g_feed.front().arrived > 6.0) ||
                               (g_feed.front().shown >= 0 && now > g_feed.front().end + 0.3)))
    {
        if (g_feed.front().shown < 0)
            ++g_feedDropped;
        g_feed.pop_front();
    }
    // one comm surface at a time: nothing while the top panel shows (or slides), nor outside flight / in a cutscene
    const u32 cam = Mem<u32>(exe::CurrentCamera);
    // (nor while the game's letterbox is up - a jump's tunnel: HUD 2 collapses to its nav chip and ring then, and the
    // band would straddle the bar; the lines wait)
    if (g_feed.empty() || g_panel.alpha > 0.01f || g_panel.s > 0 || !cam || exe::CamCutscene.is(cam) ||
        !hk::Call<void*(__cdecl*)()>(exe::GetPlayerIObj) || LetterboxBar(sw, sh) > 0)
        return;
    const HudLayout& l = g_hud2.l;
    const int px = std::max(8, static_cast<int>(sh * kSizes[g_set.size] + 0.5f));
    if (g_font.px < 0 || g_fontsPx != px || !g_fontsAgency || !g_font.texture)
    {
        std::wstring chars = g_chars;
        for (wchar_t c = 32; c < 127; ++c)
            if (chars.find(c) == std::wstring::npos)
                chars += c;
        g_chars = chars;
        if (!BuildFonts(px, chars, true))
            return;
        for (Block& b : g_blocks)
            b.layoutPx = 0;
    }
    const render::Font& nameFont = g_nameFont.texture ? g_nameFont : g_font;
    // the newest three; their time starts now if it has not
    const size_t first = g_feed.size() > 3 ? g_feed.size() - 3 : 0;
    for (size_t i = first; i < g_feed.size(); ++i)
        if (g_feed[i].shown < 0)
        {
            FeedLine& f = g_feed[i];
            f.shown = now, ++g_feedShown;
            f.end = now + Quarter(std::max({1.25, static_cast<double>(f.sec) + g_set.linger, 0.3 * Words(f.text)}));
        }
    const float x0 = l.chatFree.x0 * sw, bandW = (l.chatFree.x1 - l.chatFree.x0) * sw;
    const float accent = std::max(2.0f, sh * 0.003f), pad = sh * 0.012f, fe = sh * 0.02f, gap = sh * 0.012f;
    float maxW = std::max(sh * 0.25f, std::min(bandW, sw * 0.30f) - accent - 2 * pad);
    // HUD 2's alert keeps clear (FIX-TEXT 2, 25 Oct 2026: at 4:3 with large text the alert, centred under the ring, met the
    // feed's lowest card): the alert is published on the bus and has the place; where it reaches into the band the feed
    // narrows to end left of it, or - too narrow then - ends above it (older lines that would rise over the band's top
    // wait). The alert never moves for the feed (one owner: no back-and-forth).
    float bottom = l.chatFree.y1 * sh;
    if (Shown(l.alert))
    {
        const float ax0 = l.alert.x0 * sw, ay0 = l.alert.y0 * sh, ay1 = l.alert.y1 * sh;
        if (ay1 > l.chatFree.y0 * sh && ay0 < bottom && ax0 < x0 + accent + 2 * pad + maxW + fe + gap)
        {
            const float room = ax0 - gap - fe - x0 - accent - 2 * pad;
            if (room >= sh * 0.25f)
                maxW = std::min(maxW, room);
            else
                bottom = std::min(bottom, ay0 - gap);
        }
    }
    // HUD 2's sizes, not the dialogue's (the feed is secondary to the top panel; the mock-up's feed: the line ~0.034 H,
    // the name ~0.030 H in Agency FB): the fonts drawn scaled to them, whatever subtitle size is set
    const float ts = std::min(1.0f, sh * 0.034f / std::max(1, g_font.px)),
                ns = std::min(1.0f, sh * 0.030f / std::max(1, nameFont.px));
    const float nameH = nameFont.lineHeight * 1.02f * ns, lineH = g_font.lineHeight * 1.04f * ts;
    struct Laid
    {
        const FeedLine* f;
        std::vector<std::wstring> rows;
        float w, h;
    };
    std::vector<Laid> laid;
    for (size_t i = first; i < g_feed.size(); ++i)
    {
        const FeedLine& f = g_feed[i];
        Laid L{&f, {}, 0, 0};
        std::wstring row;
        for (const std::wstring& w : SplitWords(f.text))
        {
            const std::wstring next = row.empty() ? w : row + L" " + w;
            if (!row.empty() && render::TextWidth(g_font, next) * ts > maxW)
                L.rows.push_back(row), row = w;
            else
                row = next;
        }
        if (!row.empty())
            L.rows.push_back(row);
        const std::wstring head = f.faction.empty() ? f.name : f.name + L"  " + f.faction;
        L.w = render::TextWidth(nameFont, head) * ns;
        for (const std::wstring& r : L.rows)
            L.w = std::max(L.w, render::TextWidth(g_font, r) * ts);
        L.h = nameH + lineH * L.rows.size();
        laid.push_back(std::move(L));
    }
    if (!render::Begin(g_font))
        return;
    // bottom-anchored in the band (newest lowest, nearest the ship panel), as the mock-up's feed
    float y = bottom;
    for (size_t i = laid.size(); i-- > 0;)
    {
        const Laid& L = laid[i];
        const FeedLine& f = *L.f;
        y -= L.h + 2 * pad * 0.5f;
        if (i + 1 < laid.size() && y - pad * 0.5f < l.chatFree.y0 * sh)
            break; // (the newest always shows; an older one over the band's top waits)
        const float a = static_cast<float>(std::clamp((now - f.shown) / kFadeIn, 0.0, 1.0) *
                                           std::clamp((f.end + 0.3 - now) / 0.3, 0.0, 1.0)) *
                        std::clamp(l.alpha, 0.0f, 1.0f);
        if (a > 0.01f)
        {
            const float bx = x0, by = y - pad * 0.5f, bw = accent + 2 * pad + L.w, bh = L.h + pad;
            // the navy scrim, feathered on the right (no hard box over a planet), the accent bar in the name's colour
            const u32 navy = 0x00081422;
            const u32 base = static_cast<u32>(std::clamp(g_set.bgOpacity + 15, 40, 90) * 255 / 100 * a);
            render::Box(bx, by, bw, bh, (base << 24) | navy);
            for (int s = 1; s <= 4; ++s)
                render::Box(bx + bw + fe * (s - 1) / 4, by, fe / 4, bh, ((base * (5 - s) / 5) << 24) | navy);
            render::Box(bx, by, accent, bh, Fade(0xFF000000 | f.rgb, a));
            const float tx = bx + accent + pad;
            const float shadow = std::max(1.0f, px / 16.0f);
            render::SetFont(nameFont);
            render::TextScaled(tx + shadow, y + shadow, f.name, Fade(0xC0000000, a), ns);
            render::TextScaled(tx, y, f.name, Fade(0xFF000000 | f.rgb, a), ns);
            if (!f.faction.empty())
            {
                const u32 dim = ((((f.rgb >> 16) & 0xFF) * 7 / 10) << 16) | ((((f.rgb >> 8) & 0xFF) * 7 / 10) << 8) |
                                ((f.rgb & 0xFF) * 7 / 10);
                const float fx = tx + render::TextWidth(nameFont, f.name + L"  ") * ns;
                render::TextScaled(fx, y, f.faction, Fade(0xFF000000 | dim, a), ns);
            }
            render::SetFont(g_font);
            float ry = y + nameH;
            for (const std::wstring& r : L.rows)
            {
                render::TextScaled(tx + shadow, ry + shadow, r, Fade(0xC0000000, a), ts);
                render::TextScaled(tx, ry, r, Fade(0xFFFFFFFF, a), ts);
                ry += lineH;
            }
        }
        y -= gap;
    }
    render::End(ownScene);
}

// debug `hud2sim on|off`
std::string Hud2Sim(const std::string& arg)
{
    if (arg == "on" || arg == "1")
        g_hud2.sim = true, g_hud2.simArmed = false, g_hud2.simYield = 0;
    else if (arg == "off" || arg == "0")
        g_hud2.sim = false;
    else if (!arg.empty())
        return "usage: hud2sim on|off";
    if (!Bus())
        return "no HUD bus (host older than v8): HUD 2 stays absent";
    return std::string("hud2sim ") + (g_hud2.sim ? "on" : "off");
}

static std::string Hud2Report()
{
    std::ostringstream o;
    auto r = [&](const char* n, const HudRect& x)
    {
        if (Shown(x))
            o << " " << n << "=" << x.x0 << "," << x.y0 << ".." << x.x1 << "," << x.y1;
    };
    o << "hud2 bus=" << (Bus() ? 1 : 0) << " on=" << (g_hud2.on ? 1 : 0) << " sim=" << (g_hud2.sim ? 1 : 0)
      << " sim-yield=" << g_hud2.simYield << " age=" << (g_hud2.frame - g_hud2.seenAt) << " published=" << g_hud2.published
      << " lane=" << (g_hud2.laneOn ? 1 : 0);
    if (g_hud2.laneOn)
        o << " lane_rect=" << g_hud2.lane.rect.x0 << "," << g_hud2.lane.rect.y0 << "-" << g_hud2.lane.rect.x1 << ","
          << g_hud2.lane.rect.y1;
    if (g_hud2.on)
    {
        o << " alpha=" << g_hud2.l.alpha;
        r("nav", g_hud2.l.nav), r("target", g_hud2.l.target), r("ship", g_hud2.l.ship), r("loadout", g_hud2.l.loadout);
        r("ring", g_hud2.l.ring), r("context", g_hud2.l.context), r("alert", g_hud2.l.alert);
        r("topFree", g_hud2.l.topFree), r("chatFree", g_hud2.l.chatFree);
    }
    o << "\nhud2-subs";
    r("dialogue", g_hud2.last.dialogue), r("chat", g_hud2.last.chat);
    o << "\n";
    return o.str();
}

// hud2sim: the mock-up's parts as thin outlines (HUD 2 itself draws nothing yet; the screenshots show its zones), the
// two free bands dashed in green. Only while the sim is the publisher, in flight.
static void DrawHud2Sim(bool ownScene, int sw, int sh)
{
    if (!g_hud2.sim || !g_hud2.on || g_hud2.simYield > 0 || !g_layout.flight)
        return;
    if (!render::BeginPlain())
        return;
    const float t = std::max(1.0f, sh / 1080.0f);
    auto outline = [&](const HudRect& r, u32 c, bool dashed)
    {
        if (!Shown(r))
            return;
        const float x0 = r.x0 * sw, y0 = r.y0 * sh, x1 = r.x1 * sw, y1 = r.y1 * sh, d = sh * 0.01f;
        if (!dashed)
        {
            render::Box(x0, y0, x1 - x0, t, c), render::Box(x0, y1 - t, x1 - x0, t, c);
            render::Box(x0, y0, t, y1 - y0, c), render::Box(x1 - t, y0, t, y1 - y0, c);
            return;
        }
        for (float x = x0; x < x1; x += 2 * d)
            render::Box(x, y0, std::min(d, x1 - x), t, c), render::Box(x, y1 - t, std::min(d, x1 - x), t, c);
        for (float y = y0; y < y1; y += 2 * d)
            render::Box(x0, y, t, std::min(d, y1 - y), c), render::Box(x1 - t, y, t, std::min(d, y1 - y), c);
    };
    const HudLayout& l = g_hud2.l;
    for (const HudRect* r : {&l.nav, &l.target, &l.ship, &l.loadout, &l.ring, &l.context, &l.alert})
        outline(*r, 0xC0FFC24A, false);
    outline(l.topFree, 0xA06CFF8A, true), outline(l.chatFree, 0xA06CFF8A, true);
    render::End(ownScene);
}

// ------------------------------------------------------------------ off-screen arrows clear of the HUD (hud_arrows)
// The target indicators (exe::VtHudIndicators) pin the off-screen arrows to the screen edge, under the HUD's panels
// and the subtitles. Each arrow part (its record compared with the indicators' fields; brackets and on-screen hats are
// left alone) is moved along its ray from the screen centre to just inside the nearest keep-out rectangle it lies in:
// pointing the same way, at the edge of the free space between the HUD elements. Keep-out rectangles, all live:
// - the bottom panel pairs from their buttons (an open pair shows its close button) and the gauge bar from HealthBar,
//   sized as measured at 3840x2160 relative to those anchors (HDE's Advanced Widescreen HUD: the left pair reaches
//   0.613 H from the left edge, the right pair starts 0.564 H from the right, both with their tabs from 0.694 H);
// - the maneuver bar (its buttons, the frame 0.040 / 0.042 H beyond them, down to 0.083 H; the cruise box under it)
//   and the NeuroNet box (its buttons, 0.020 H beyond, down to 0.032 H);
// - the subtitles drawn in the last 100 ms (the top panel as displayed, every block).
// Each grown by the arrow's half length (~0.045 H: the arrow is ~0.097 H long at 4K).
static const u32 kArrowFields[] = {0x7A4, 0x7A8, 0x7AC, 0x7B8, 0x7BC, 0x7C0, 0x7C8, 0x7CC, 0x7D0, 0x7D4, 0x7D8};

struct KeepOut
{
    float x0, y0, x1, y1;
};

// The off-screen arrows' keep-outs with HUD 2: its parts (the ring is not one: arrows never reach it), each reaching to
// the screen edge it lies near (an arrow pinned to the edge must not stand in the margin beside a corner block).
static void Hud2KeepOuts(const HudLayout& l, float sw, float sh, std::vector<KeepOut>& out)
{
    out.clear();
    for (const HudRect* r : {&l.nav, &l.target, &l.ship, &l.loadout, &l.context, &l.alert})
        if (Shown(*r))
            out.push_back({r->x0 < 0.07f ? -1.0f : r->x0 * sw, r->y0 < 0.07f ? -1.0f : r->y0 * sh,
                           r->x1 > 0.93f ? sw + 1 : r->x1 * sw, r->y1 > 0.93f ? sh + 1 : r->y1 * sh});
}


static void HudKeepOuts(float sw, float sh, std::vector<KeepOut>& out)
{
    static std::vector<KeepOut> hud;
    const ULONGLONG now = GetTickCount64();
    const HudLayout* h2 = Hud2Layout();
    if (h2) // HUD 2: its parts' rectangles (from the bus: no control walks)
    {
        Hud2KeepOuts(*h2, sw, sh, out);
        g_keepOutsAt = 0; // the game's HUD is read afresh when it returns
    }
    else if (now - g_keepOutsAt >= 100) // the HUD controls: a walk of the control list each, at most 10 times a second
    {
        g_keepOutsAt = now;
        hud.clear();
        // anchored on their buttons, which project like the other HUD controls (the panel backgrounds are images whose
        // depth does not nest the same way: projected, they landed near the centre, 21 Oct 2026); an open pair shows
        // its close button. Offsets measured at 3840x2160: the left pair reaches 601 px right of TargetCloseButton,
        // the right pair starts 489 px left of StatusCloseButton, their tabs 59 px above the button row (1559 px).
        float cx, cy, hw, hh, rowY = sh * 0.722f;
        if (ControlRect("NextEnemyButton", cx, cy, hw, hh) || ControlRect("HudRepairButton", cx, cy, hw, hh))
            rowY = cy;
        if (ControlRect("TargetCloseButton", cx, cy, hw, hh))
            hud.push_back({-1.0f, rowY - sh * 0.0273f, cx + sh * 0.2782f, sh + 1});
        if (ControlRect("StatusCloseButton", cx, cy, hw, hh))
            hud.push_back({cx - sh * 0.2264f, rowY - sh * 0.0273f, sw + 1, sh + 1});
        if (ControlRect("HealthBar", cx, cy, hw, hh)) // the gauge bar between them
            hud.push_back({cx - sh * kGaugeHalfW, cy - sh * kGaugeAbove, cx + sh * kGaugeHalfW, sh + 1});
        float bl = 1e9f, br = -1e9f, bb = 0;
        for (const char* n : {"ManeuverButton0", "ManeuverButton1", "ManeuverButton2", "ManeuverButton3"})
            if (ControlRect(n, cx, cy, hw, hh))
                bl = std::min(bl, cx - hw), br = std::max(br, cx + hw), bb = std::max(bb, cy + hh);
        if (br > bl) // the frame around the buttons (scaled with the bar: space_narrow 4)
            hud.push_back({bl - sh * kMbFrameIn * g_mbScale, -1.0f, std::max(br + sh * kMbFrameOut * g_mbScale, BarRight(sh)),
                           std::max(bb, sh * kBarFrameBottom * g_mbScale)});
        if (ControlRect("ManeuverCruiseButton", cx, cy, hw, hh))
            hud.push_back({cx - hw - sh * kCruiseOutX, -1.0f, cx + hw + sh * kCruiseOutX, cy + hh + sh * kCruiseBelow});
        float nl = 1e9f;
        for (const char* n : {"NNMissionLogButtonRoot", "NNNavmapButtonRoot", "NNInventoryButtonRoot",
                              "NNPlayerInfoButtonRoot", "NNHelpButtonRoot"})
            if (ControlRect(n, cx, cy, hw, hh))
                nl = std::min(nl, cx - hw);
        if (nl < sw)
            hud.push_back({nl - sh * 0.020f, -1.0f, sw + 1, sh * 0.032f});
    }
    if (!h2)
        out = hud;
    if (GetTickCount64() - g_layout.at < 100) // the subtitles of the last frame drawn
    {
        if (g_layout.topPanel && g_panel.alpha > 0.05f)
            out.push_back(
                {g_panel.x - g_panel.w * 0.5f, -1.0f, g_panel.x + g_panel.w * 0.5f, g_panel.h + g_panel.yoff});
        const float pad = sh * 0.01f;
        for (const LayoutBlock& b : g_layout.blocks)
            if (!(g_layout.topPanel && !b.corner)) // inside the panel already
                out.push_back({b.x - pad, b.y - pad, b.x + b.w + pad, b.y + b.h + pad});
    }
    const float grow = sh * 0.045f;
    for (KeepOut& k : out)
        k.x0 -= grow, k.y0 -= grow, k.x1 += grow, k.y1 += grow;
}

// Moves (px, py) along its ray from (cx, cy) to where the ray enters the first keep-out rectangle containing it, until
// it lies in none (overlapping rectangles: a few rounds).
static void PushClear(float cx, float cy, float& px, float& py, const std::vector<KeepOut>& ks)
{
    for (int round = 0; round < 8; ++round)
    {
        bool moved = false;
        for (const KeepOut& k : ks)
        {
            if (!(px > k.x0 && px < k.x1 && py > k.y0 && py < k.y1))
                continue;
            const float dx = px - cx, dy = py - cy;
            const float tx = dx > 0 ? (k.x0 - cx) / dx : dx < 0 ? (k.x1 - cx) / dx : -1e9f;
            const float ty = dy > 0 ? (k.y0 - cy) / dy : dy < 0 ? (k.y1 - cy) / dy : -1e9f;
            const float t = std::max(tx, ty); // the entry of the ray from the centre into the rectangle
            if (!(t > 0.0f && t < 1.0f))
                continue; // the centre itself is inside (should not happen): leave it
            px = cx + dx * (t - 0.002f), py = cy + dy * (t - 0.002f);
            moved = true;
        }
        if (!moved)
            break;
    }
}

static int g_arrowsMoved = 0;                                      // parts moved since the last `layout` report
static int g_partCalls = 0, g_partIndicator = 0, g_partArrows = 0; // the hook's stages (same window; RE / tests)
static float g_lastArrow[5] = {};                                  // the last arrow part: x, y, z, screen x, y
static std::vector<KeepOut> g_lastKeepOuts;
// `arrowtrace start|dump`: the current-target arrow's screen point per frame, as the game placed it and as drawn
struct ArrowSample
{
    double t;
    float gx, gy, x, y;
};
static std::vector<ArrowSample> g_arrowTrace;
static bool g_arrowTracing = false;

// `ebp` is the target indicators object and `part` one of its arrow parts (any caller's ebp arrives here: guarded).
static bool IsArrowPart(u32 ebp, const void* part)
{
    __try
    {
        if (*reinterpret_cast<const u32*>(ebp) != exe::VtHudIndicators.get())
            return false;
        ++g_partIndicator;
        for (u32 f : kArrowFields)
            if (*reinterpret_cast<const void* const*>(ebp + f) == part)
                return ++g_partArrows, true;
        return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The current-target arrow itself (+0x7A4): one sample per frame for `arrowtrace`.
static bool IsCurrentTargetArrow(u32 ebp, const void* part)
{
    __try
    {
        return *reinterpret_cast<const void* const*>(ebp + 0x7A4) == part;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

std::string ArrowTrace(const std::string& arg, const std::string& file)
{
    if (arg == "start")
    {
        g_arrowTrace.clear(), g_arrowTracing = true;
        return "tracing";
    }
    g_arrowTracing = false;
    FILE* f = nullptr;
    if (fopen_s(&f, file.c_str(), "w") || !f)
        return "cannot write " + file;
    fprintf(f, "t\tgame_x\tgame_y\tx\ty\n");
    for (const ArrowSample& s : g_arrowTrace)
        fprintf(f, "%.4f\t%.1f\t%.1f\t%.1f\t%.1f\n", s.t - (g_arrowTrace.empty() ? 0 : g_arrowTrace[0].t), s.gx, s.gy,
                s.x, s.y);
    fclose(f);
    return std::to_string(g_arrowTrace.size()) + " samples -> " + file;
}

bool ConfineHudArrow(u32 ebp, const void* part, const float* pos, float* out)
{
    PerfScope perf(g_perf.arrows);
    ++g_perf.arrowCalls;
    ++g_partCalls;
    if (!g_set.enabled || !g_set.hudArrows || !part || !ebp || !exe::VtHudIndicators.ok() || !exe::UiCamera.ok() ||
        !IsArrowPart(ebp, part))
        return false;
    const float z = pos[2];
    if (z > -0.01f)
        return false;
    const float ccx = Mem<float>(exe::UiCamera, 0x68), ccy = Mem<float>(exe::UiCamera, 0x6C);
    const float kx = Mem<float>(exe::UiCamera, 0x78), ky = Mem<float>(exe::UiCamera, 0x7C);
    const float sw = static_cast<float>(Mem<int>(exe::ScreenW)), sh = static_cast<float>(Mem<int>(exe::ScreenH));
    if (std::fabs(kx) < 1.0f || std::fabs(ky) < 1.0f || sw <= 0 || sh <= 0)
        return false;
    float px = ccx + kx * (pos[0] / -z), py = ccy + ky * (pos[1] / -z);
    std::vector<KeepOut> ks;
    HudKeepOuts(sw, sh, ks);
    g_lastArrow[0] = pos[0], g_lastArrow[1] = pos[1], g_lastArrow[2] = z, g_lastArrow[3] = px, g_lastArrow[4] = py;
    g_lastKeepOuts = ks;
    const float ox = px, oy = py;
    PushClear(ccx, ccy, px, py, ks);
    if (g_arrowTracing && g_arrowTrace.size() < 20000 && IsCurrentTargetArrow(ebp, part))
        g_arrowTrace.push_back({Now(), ox, oy, px, py});
    if (px == ox && py == oy)
        return false;
    out[0] = (px - ccx) / kx * -z, out[1] = (py - ccy) / ky * -z, out[2] = z;
    ++g_arrowsMoved;
    return true;
}

std::string LayoutReport()
{
    std::ostringstream o;
    o << "screen " << g_layout.sw << "x" << g_layout.sh << " px " << g_layout.px << " lineH " << g_layout.lineH
      << " nameH " << g_layout.nameH << " gap " << g_layout.gap << " maxW " << g_layout.maxW << " context "
      << (g_layout.movie      ? "movie"
          : g_layout.cutscene ? "cutscene"
          : g_layout.flight   ? "flight"
                              : "base")
      << " letterbox " << (g_layout.letterbox ? 1 : 0) << " anchor " << g_layout.bottom
      << (g_layout.fromTop ? " top" : " bottom") << " centerX " << g_layout.centerX << " bar " << g_layout.bar
      << (g_layout.inBar ? " in-bar" : " ") << " viewport " << g_layout.vpX << "," << g_layout.vpY << " "
      << g_layout.vpW << "x" << g_layout.vpH << " age_ms " << (GetTickCount64() - g_layout.at) << "\n";
    o << Hud2Report();
    if (g_layout.hud2)
        o << "hud2-place band=" << g_layout.freeL << ".." << g_layout.freeR << " limit=" << g_layout.limit
          << " nn=" << g_layout.nnLeft << "," << g_layout.nnTop << "\n";
    if (g_layout.topPanel)
        o << "panel x=" << g_layout.panelX << " w=" << g_layout.panelW << " h=" << g_layout.panelH
          << " yoff=" << g_layout.panelYoff << " s=" << g_layout.panelS << " bar-moved=" << (g_mbShifted ? 1 : 0)
          << " bar-right=" << g_barRight << " nn-left=" << g_nnLeft << " top=" << g_panel.top
          << " bar-scale=" << g_mbScale << " (" << g_mbScaleWhy
          << ") tab=" << (g_panel.bodyTop > 0 ? g_panel.tabW : 0.0f) << " body=" << g_panel.bodyTop
          << " tab-noroom=" << g_panel.tabNoRoom
          << " cruise=" << g_cruiseRight << "," << g_cruiseBottom << "\n";
    // the comm portrait: the slot drawn last frame and the game's window as it stands (open, centre, size in px)
    {
        float nativePx = 0;
        const bool open = CommWindowOpen(&nativePx);
        o << "portrait slot=" << g_layout.portraitX << "," << g_layout.portraitY << " s=" << g_layout.portraitS
          << " window-open=" << (open ? 1 : 0) << " moved=" << (g_comm.moved ? 1 : 0) << " native=" << nativePx
          << " held=" << (g_held.holding ? 1 : 0)
          << " capture-age-ms=" << (g_held.at ? GetTickCount64() - g_held.at : 0) << "\n";
    }
    o << "bar-scale " << g_mbScale << " (" << g_mbScaleWhy << ") shifted " << (g_mbShifted ? 1 : 0) << "\n";
    o << "arrows moved " << g_arrowsMoved << " (hud parts " << g_partCalls << ", indicator " << g_partIndicator
      << ", arrow parts " << g_partArrows << ")\n";
    g_arrowsMoved = g_partCalls = g_partIndicator = g_partArrows = 0;
    o << "last arrow pos (" << g_lastArrow[0] << ", " << g_lastArrow[1] << ", " << g_lastArrow[2] << ") screen ("
      << g_lastArrow[3] << ", " << g_lastArrow[4] << ")\n";
    for (const KeepOut& k : g_lastKeepOuts)
        o << "keepout " << k.x0 << "," << k.y0 << " .. " << k.x1 << "," << k.y1 << "\n";
    float bx, by, bw, bh;
    if (g_layout.flight && ControlRect("MissionObjectiveText", bx, by, bw, bh))
        o << "banner x=" << bx - bw << " y=" << by - bh << " w=" << 2 * bw << " h=" << 2 * bh << "\n";
    for (const LayoutBlock& b : g_layout.blocks)
    {
        char name[8];
        sprintf_s(name, "%06X", b.name & 0xFFFFFF);
        o << "block y=" << b.y << " h=" << b.h << " w=" << b.w << (b.corner ? " corner" : "") << " name=" << name
          << (b.hostile ? " hostile" : "") << " speaker=" << b.speaker << "\n";
    }
    return o.str();
}

// The hostile marker (hostile_marker, off by default): a red triangle pointing down (user, 22 Oct 2026; was the HUD
// contact list's upward one), drawn from one box per pixel row (no font glyph needed; the same in Direct3D and the movie
// renderer), centred on a row of height rowH.
static float MarkerWidth(float rowH) { return rowH * 0.62f; }

static void HostileMarker(float x, float y, float rowH, float a)
{
    const float s = rowH * 0.36f, w = s * 1.15f, top = y + (rowH - s) * 0.5f;
    const int n = std::max(3, static_cast<int>(s));
    const u32 red = Fade(NameColor(Relation::Hostile), a);
    for (int k = 0; k < n; ++k)
    {
        const float rw = w * static_cast<float>(n - k) / n; // widest row on top: pointing down
        render::Box(x + (w - rw) * 0.5f, top + s * k / n, rw, s / n + 0.5f, red);
    }
}

static MovieFrame g_movie; // what the last DrawMovie drew (movie.cpp)
// The movie's picture (client pixels; empty = the whole frame), whether the target can blend a box over it (false: a
// box is a hard cut, on or off) and each line's box target by its start time (movie.cpp's samples of the video)
static RECT g_moviePicture = {};
static bool g_movieTranslucent = true;
static std::vector<std::pair<double, float>> g_movieBoxTarget;
static float g_movieBoxDefault = 0; // a line inside the picture without a target: no box (the owner, 26 Oct 2026)

static void DrawImpl(int sw, int sh, bool ownScene, u32* soft)
{
    // (an empty stack still draws while the top panel slides out)
    if (!g_set.enabled || sw <= 0 || sh <= 0 || (g_blocks.empty() && (g_panel.s <= 0 || soft)))
        return;
    // space_narrow 2: one font size smaller for the top panel's short room on narrow screens (flight only)
    float fontScale = 1.0f;
    if (!soft && g_set.spacePosition == 2 && g_set.spaceNarrow == 2 && NarrowScreen(sw, sh) &&
        (Hud2Layout() || TopRoom(sw, sh) > 0) &&
        Mem<u32>(exe::CurrentCamera) && hk::Call<void*(__cdecl*)()>(exe::GetPlayerIObj))
        fontScale = g_set.size > 0 ? kSizes[g_set.size - 1] / kSizes[g_set.size] : 0.85f;
    const int px = std::max(8, static_cast<int>(sh * kSizes[g_set.size] * fontScale + 0.5f));
    // (Agency FB text while HUD 2 shows, in flight - not over a movie)
    const bool agency = !soft && Hud2Layout() != nullptr;
    if (g_font.px < 0 || g_fontsPx != px || g_fontsAgency != agency || !g_font.texture)
    {
        std::wstring chars = g_chars;
        for (wchar_t c = 32; c < 127; ++c)
            if (chars.find(c) == std::wstring::npos)
                chars += c;
        if (chars.find(textfit::kEllipsis) == std::wstring::npos) // (the fit rule's cut names)
            chars += textfit::kEllipsis;
        g_chars = chars;
        if (!BuildFonts(px, chars, agency))
            return;
        for (Block& b : g_blocks)
            b.layoutPx = 0;
    }
    const int avg = render::TextWidth(g_font, L"abcdefghijklmnopqrstuvwxyz ") / 27;
    // Arial Unicode MS has generous built-in leading (its rows read loose at 1.1x): tighter than Segoe UI
    const float lineH = g_font.lineHeight * (g_font.face == L"Arial Unicode MS" ? 0.98f : 1.10f);
    const float padX = px * 0.35f, padY = (lineH - g_font.lineHeight) * 0.5f;
    const int widthCap = std::min(static_cast<int>(sw * 0.8f), avg * kMaxChars);
    // movies: bottom centre over the picture (no game state while a movie plays)
    const Placement place =
        soft ? Placement{sh * (1.0f - g_set.bottomMovie), sw * 0.5f, widthCap} : Place(sw, sh, widthCap, avg, padX);
    const int maxW = place.maxW;
    {
        const u32 cam = soft ? 0 : Mem<u32>(exe::CurrentCamera);
        g_layout.sw = sw, g_layout.sh = sh, g_layout.px = px, g_layout.maxW = maxW, g_layout.lineH = lineH;
        g_layout.bottom = place.bottom, g_layout.centerX = place.centerX, g_layout.fromTop = place.fromTop;
        g_layout.flight = place.flight, g_layout.movie = soft != nullptr,
        g_layout.cutscene = !soft && exe::CamCutscene.is(cam);
        g_layout.bar = place.bar, g_layout.inBar = place.inBar;
        g_layout.hud2 = place.hud2, g_layout.freeL = place.freeL, g_layout.freeR = place.freeR;
        g_layout.limit = place.limit, g_layout.nnLeft = place.nnLeft, g_layout.nnTop = place.nnTop;
        g_layout.letterbox =
            !soft && exe::IsPlayerInCutscene.ok() && hk::Call<int(__cdecl*)()>(exe::IsPlayerInCutscene) != 0;
        if (soft || !render::Viewport(g_layout.vpX, g_layout.vpY, g_layout.vpW, g_layout.vpH))
            g_layout.vpX = g_layout.vpY = g_layout.vpW = g_layout.vpH = 0;
        g_layout.blocks.clear();
        g_layout.at = GetTickCount64();
    }
    const u32 bg = static_cast<u32>(g_set.bgOpacity * 255 / 100) << 24;
    const u32 textArgb = g_set.textColor == 1 ? 0xFFFFF04A : 0xFFFFFFFF;
    const float shadowOff = std::max(1.0f, px / 16.0f);
    const double now = Now();

    // the top panel's solid quads need a texture on this device (managed pool: survives resets)
    if (place.topPanel && g_host && (!g_white.texture || g_white.device != g_host->GetD3DDevice()))
    {
        render::FreeImage(g_white);
        render::BuildImage(g_white, std::vector<u32>{0xFFFFFFFFu}, 1, 1);
    }
    if (soft ? !render::BeginSoftware(g_font, soft, sw, sh) : !render::Begin(g_font))
        return;
    // Rows of each block (current page), oldest block first; the speaker's name has its own row (name font).
    struct Row
    {
        std::wstring text;
        u32 argb;
        bool name;
        float h, w;
        const std::vector<int>* words = nullptr; // a text row: its words of the line's text (the spoken-word pop)
        float scale = 1.0f;                      // a name row: its size by the fit rule (x the name font's)
    };
    const render::Font& nameFont = g_nameFont.texture || !g_nameFont.coverage.empty() ? g_nameFont : g_font;
    const float nameH = nameFont.lineHeight * 1.02f, gap = lineH * 0.35f;
    // A name row through the one fit rule (core\game\textfit.h; fix-text 4, 25 Oct 2026: a long comm name such as
    // "Manhattan Docking Ring · Police" ran past the top panel's edges): measured, shrunk down to the legibility floor,
    // then cut with an ellipsis, inside `room` (less the hostile marker); cached per string / size / room
    // (uidraw::FitText). The movie overlay (software target) draws at the font's size: cut only there.
    const float nameFloor = soft ? static_cast<float>(nameFont.px) : textfit::MinSize(static_cast<float>(sh));
    auto nameRow = [&](const std::wstring& s, u32 argb, float room, bool marker)
    {
        const float mw = marker ? MarkerWidth(nameH) : 0.0f;
        const textfit::Result f = uidraw::FitText(nameFont, s, std::max(0.0f, room - mw), nameFloor);
        Row row{f.text, argb, true, nameH, f.width + mw};
        row.scale = nameFont.px > 0 ? std::min(1.0f, f.size / static_cast<float>(nameFont.px)) : 1.0f;
        return row;
    };
    g_layout.nameH = nameH, g_layout.gap = gap;
    // Neural Net lines in flight: their own stack in the top-right corner (user, 19 Oct 2026: the dialogue got busy when
    // the Neural Net chimed in), under the HUD's top-right button strip (ends ~0.05 H), right-aligned, in the Neural
    // Net green without a name row (the corner and colour say who speaks), narrower rows.
    const bool cornerOn = place.flight && !soft;
    // Where the eyes are (user, 19 Oct 2026: minimise eye travel but keep the HUD spaced and logical; the top-right
    // corner was too far and not legible): a sibling column right of the dialogue band - top-aligned with it, starting
    // just past the widest dialogue row plus a gap, left-aligned, clear of the centred objective banner. Its place
    // follows the dialogue's width, so it adapts to the aspect ratio; with little room the column narrows.
    // With the top panel: right of the panel's widest extent, top-aligned under the NeuroNet box (top right).
    const float panelSide = std::max(padX, sh * (kChamW + kPanelGap));
    // (the top panel moved right for its tab - space_narrow 5 - takes the column with it: last frame's offset)
    const float panelShift = place.topPanel && g_panel.sized ? std::max(0.0f, g_panel.x - place.centerX) : 0.0f;
    // With HUD 2: under its target card (PlaceHud2), whatever the dialogue's place.
    const float cornerLeft = place.hud2 ? place.nnLeft
                             : place.topPanel
                                 ? sw * 0.5f + place.maxW * 0.5f + panelSide + sh * kPanelGap * 2 + panelShift
                                 : sw * 0.5f + widthCap * 0.5f + sh * 0.03f;
    const float cornerTop = place.hud2       ? place.nnTop
                            : place.topPanel ? sh * (kNnBottom + kPanelGap * 1.5f)
                                             : sh * kBelowBar;
    const int cornerMaxW =
        std::max(avg * 12, std::min(avg * 30, static_cast<int>(place.hud2 ? place.nnRight - place.nnLeft - 2 * padX
                                                                          : sw - sh * 0.045f - cornerLeft)));
    auto inCorner = [&](const Block& b) { return cornerOn && b.nn; };
    // The comm portrait's slot (comm_portrait; user, 21 Oct 2026): while the game's comm window is open, the window sits
    // in a square at the top panel's left, the rows to its right (at most its native size, 0.11 H); in the tab layout
    // (space_narrow 5) beside the name in the tab, which widens and deepens for it (sized below, from the name's width).
    float nativePortrait = 0, slotS = 0, slotW = 0;
    const bool tabLayout = place.bodyTop > 0;
    const bool panelPortrait = place.topPanel && !soft && g_set.commPortrait;
    const bool live = panelPortrait && CommWindowOpen(&nativePortrait) && nativePortrait > 1.0f;
    // fade_with_channel (user, 21 Oct 2026: "dialogue can actually fade out right after the portrait fades out ... make
    // it work like that for all space dialogue"): when the game closes its window, the line it opened it for fades out
    // right after the channel-closed animation - never before its reading minimum
    if (!live && g_liveBlock >= 0 && panelPortrait && g_set.fadeWithChannel)
        for (Block& b : g_blocks)
            if (b.start == g_liveBlock && b.leaving < 0)
            {
                const double at = std::max(now + kChannelClose, b.start + b.readMin);
                if (b.retireAt < 0 || at < b.retireAt)
                    b.retireAt = at;
            }
    // the held picture (g_held): starts when the window closes under a line still showing, ends with that line
    if (live)
        g_held.holding = false;
    else if (panelPortrait && !g_held.holding && g_liveBlock >= 0 && !g_held.px.empty() &&
             GetTickCount64() - g_held.at < 400)
    {
        g_held.holding = render::BuildImage(g_held.img, g_held.px, g_held.w, g_held.w);
        g_held.block = g_liveBlock, g_held.since = now;
    }
    g_liveBlock = -1;
    int heldBlock = -1;
    if (g_held.holding)
        for (size_t i = 0; i < g_blocks.size(); ++i)
            if (g_blocks[i].start == g_held.block && !inCorner(g_blocks[i]))
                heldBlock = static_cast<int>(i);
    // the hold lasts its closing animation; then the slot goes (the rows keep their breaks: keepW) - unless its line is
    // about to go anyway (fade_with_channel): then the empty slot stays until it has, so nothing shifts while it fades
    if (g_held.holding && heldBlock >= 0 && panelPortrait && now - g_held.since >= kChannelClose)
    {
        const Block& hb = g_blocks[heldBlock];
        if (!(hb.leaving >= 0 || (hb.retireAt >= 0 && hb.retireAt - now < 0.4)))
            heldBlock = -1;
    }
    if (g_held.holding && (heldBlock < 0 || !panelPortrait))
        g_held.holding = false, heldBlock = -1;
    if (!g_held.holding && g_held.img.texture)
        render::FreeImage(g_held.img);
    if (!live && heldBlock >= 0)
        nativePortrait = static_cast<float>(g_held.w);
    const bool portraitOn = live || heldBlock >= 0;
    // the line the portrait belongs to: the newest one the game opened its window for, else the first in the panel;
    // in the tab layout it goes into the tab only when that line's name is the tab's (else beside its line in the body)
    int portraitBlock = -1, firstMain = -1;
    if (portraitOn)
        for (size_t i = 0; i < g_blocks.size(); ++i)
        {
            if (inCorner(g_blocks[i]))
                continue;
            if (firstMain < 0)
                firstMain = static_cast<int>(i);
            if (g_blocks[i].portrait && g_blocks[i].leaving < 0)
                portraitBlock = static_cast<int>(i);
        }
    if (heldBlock >= 0 && !live)
        portraitBlock = heldBlock;
    if (portraitBlock < 0)
        portraitBlock = firstMain;
    const bool portraitInTab = tabLayout && portraitBlock >= 0 && portraitBlock == firstMain &&
                               !g_blocks[firstMain].speaker.empty() && g_set.nameMode == 0;
    // Rows too narrow beside it (under 20 characters: HUD 2's panel at 4:3, a narrow top panel): the portrait sits above its
    // line in the panel, centred, the rows full width below (fix-text 5, 25 Oct 2026, the owner on vanilla's Donau fight
    // at 1280x960: "the comm portrait was stuck at the left" - it went under HUD 2's nav, away from its dialogue)
    bool portraitTop = false;
    if (portraitOn && portraitBlock >= 0 && !portraitInTab)
    {
        slotS = std::min(nativePortrait, sh * 0.11f), slotW = slotS + sh * kPanelGap;
        if (maxW - slotW < avg * 20)
            slotW = 0, portraitTop = true;
    }
    const int mainW = maxW - static_cast<int>(slotW);
    std::vector<std::vector<Row>> blockRows;
    std::vector<float> heights;
    const bool inlineName = place.inBar && g_set.nameMode == 0; // inside the bar the name joins the first row
    for (Block& b : g_blocks)
    {
        // a line laid out beside the portrait keeps that width after the slot goes (it never re-wraps while read)
        int bw = inCorner(b) ? cornerMaxW : mainW;
        if (!inCorner(b))
        {
            if (slotW > 0)
                b.keepW = bw;
            else if (b.keepW > 0 && b.keepW < bw)
                bw = b.keepW;
        }
        const int key = px * 65536 + bw + (inlineName ? (1 << 30) : 0);
        if (b.layoutPx != key) // font size, row width, inline name
        {
            Layout(b, bw, inlineName);
            b.layoutPx = key;
        }
        const float t = static_cast<float>(now - b.start);
        const Page* page = &b.pages.back();
        for (const Page& p : b.pages)
            if (t >= p.t0 && t < p.t1)
            {
                page = &p;
                break;
            }
        std::vector<Row> rows;
        float h = 0;
        const bool corner = inCorner(b);
        if (g_set.nameMode == 0 && !inlineName && !b.speaker.empty() && !corner)
        {
            rows.push_back(nameRow(b.speaker, b.nameArgb, static_cast<float>(bw), b.hostile));
            h += nameH;
        }
        for (size_t ri = 0; ri < page->rows.size(); ++ri)
        {
            const std::wstring& r = page->rows[ri];
            // corner lines in the HUD's alert look: Agency FB Bold (the name font), a bright Neural Net green; by severity
            // (neural_net_severity): warning amber, danger red-orange, each with brightness pulses (never colour alone)
            if (corner)
            {
                static const u32 kSeverity[] = {0xFF6CFF8A, 0xFFFFC24A, 0xFFFF6A4A};
                const int sev = g_set.nnSeverity ? b.severity : 0;
                u32 argb = kSeverity[sev];
                // pulses toward white: one (warning) or two (danger) at 2 Hz from the line's start (< 3 Hz)
                const float age = static_cast<float>(now - b.start);
                if (sev > 0 && age < 0.5f * sev)
                {
                    const float w = 0.65f * std::sin(3.14159265f * std::fmod(age, 0.5f) / 0.5f);
                    u32 c = 0xFF000000;
                    for (int s = 0; s < 24; s += 8)
                    {
                        const float ch = static_cast<float>((argb >> s) & 0xFF);
                        c |= static_cast<u32>(ch + (255.0f - ch) * w + 0.5f) << s;
                    }
                    argb = c;
                }
                rows.push_back({r, argb, true, nameH, static_cast<float>(render::TextWidth(nameFont, r))});
                h += nameH;
                continue;
            }
            rows.push_back({r, textArgb, false, lineH, static_cast<float>(render::TextWidth(g_font, r)),
                            ri < page->rowWords.size() ? &page->rowWords[ri] : nullptr});
            h += lineH;
        }
        heights.push_back(h);
        blockRows.push_back(std::move(rows));
    }
    // The tab layout (space_narrow 5): the first speaker's name stays in the tab at the top, its rows start in the body
    // below the top HUD row - a jump after the name row, counted in the block's height.
    // With the portrait the tab holds it left of the name (the user's call for 4:3, 21 Oct 2026: "just about enough room
    // ... if you expand the name part of the rectangle a bit"): as wide as the room between the bar and the NeuroNet box
    // allows, the portrait shrunk to fit beside the name (not below 0.05 H: then it stays where the game puts it), and as
    // deep as the portrait; the body starts below it.
    // A "name · faction" speaker beside the portrait takes two rows, the name over the faction, as the game's own comm
    // caption does (pilot name, then group): the tab stays narrow enough for the portrait at 4:3.
    int tabBlock = -1, tabNames = 1;
    float tabJump = 0, bodyTop = place.bodyTop, tabNameW = 0, tabNameH = 0;
    if (tabLayout)
        for (size_t i = 0; i < g_blocks.size(); ++i)
        {
            if (inCorner(g_blocks[i]))
                continue;
            if (!blockRows[i].empty() && blockRows[i][0].name)
            {
                tabBlock = static_cast<int>(i);
                std::vector<Row>& rows = blockRows[i];
                const Block& tb = g_blocks[i];
                // the names fit the tab's room, less the smallest portrait beside them (0.05 H; below it stays out)
                const bool withPortrait = portraitOn && portraitInTab;
                const float room =
                    TabRoom(place) - 2 * panelSide - (withPortrait ? sh * (0.05f + kPanelGap) + 1.0f : 0.0f);
                const size_t dot = tb.speaker.find(L" \x00B7 ");
                if (withPortrait && dot != std::wstring::npos)
                {
                    Row second = nameRow(tb.speaker.substr(dot + 3), rows[0].argb, room, false);
                    rows[0] = nameRow(tb.speaker.substr(0, dot), rows[0].argb, room, tb.hostile);
                    // one size for both rows: the smaller (the other then fits too)
                    const float mk = tb.hostile ? MarkerWidth(nameH) : 0.0f;
                    if (second.scale > rows[0].scale)
                        second.w *= rows[0].scale / second.scale, second.scale = rows[0].scale;
                    else if (rows[0].scale > second.scale)
                        rows[0].w = (rows[0].w - mk) * second.scale / rows[0].scale + mk, rows[0].scale = second.scale;
                    rows.insert(rows.begin() + 1, second);
                    heights[i] += second.h;
                    tabNames = 2;
                }
                else
                    rows[0] = nameRow(tb.speaker, rows[0].argb, room, tb.hostile);
                for (int r = 0; r < tabNames; ++r)
                    tabNameW = std::max(tabNameW, rows[r].w), tabNameH += rows[r].h;
                if (portraitOn && portraitInTab)
                {
                    const float fit = TabRoom(place) - 2 * panelSide - tabNameW - sh * kPanelGap;
                    slotS = std::min({nativePortrait, sh * 0.11f, fit});
                    if (slotS < sh * 0.05f)
                        slotS = 0;
                    else
                        slotW = slotS + sh * kPanelGap,
                        bodyTop = std::max(bodyTop, place.bottom + slotS + sh * kPanelGap);
                }
                tabJump = std::max(0.0f, bodyTop + sh * kPanelTop - (place.bottom + tabNameH));
                heights[i] += tabJump;
            }
            break;
        }
    if (portraitBlock < 0 || (portraitInTab && portraitBlock != tabBlock))
        slotS = slotW = 0, portraitTop = false;
    const float portraitDrop = portraitTop && slotS > 0 ? slotS + sh * kPanelGap : 0.0f; // its line's rows start below it
    if (portraitDrop > 0)
        heights[portraitBlock] += portraitDrop;
    // A top-anchored stack hangs from place.bottom and must end above 0.44 H (clear of the crosshair and its reticle
    // at 0.5 H; two speakers with name rows need ~0.25 H): the oldest speaker still on retires (fades out) when it
    // would not fit.
    // Not while the stack waits for the top panel (user, 22 Oct 2026, the Donau's fall: out of the cutscene, before the
    // HUD was up, Newark's "Red Alert" stood lower for a moment, crossed the limit and was dropped for good - in the panel
    // a moment later both lines fit); at most 2 s, so a panel that never comes cannot leave lines over the crosshair.
    static double handOverSince = -1;
    if (!place.handOver)
        handOverSince = -1;
    else if (handOverSince < 0)
        handOverSince = now;
    const bool waitForPanel = place.handOver && now - handOverSince < 2.0;
    std::vector<char> released(g_blocks.size(), 0); // fading lines whose slot was given up this frame (below)
    // Which lines give way (the owner, 26 Oct 2026, M01 after the Donau's fall: King's long line fading out still counted
    // here and the limit dropped Newark's and the LPU Beta leader's fresh story lines within half a second): a line that is
    // fading already counts no more (it goes within its fade); first the lines already pushed out (retireAt) and chatter,
    // then the oldest; a comm whose voice is still heard holds its place until its audio ends (Insert's rule).
    if (place.fromTop && !waitForPanel)
    {
        float total = 0;
        size_t mainCount = 0;
        for (size_t i = 0; i < heights.size(); ++i)
            if (!inCorner(g_blocks[i]) && g_blocks[i].leaving < 0)
                total += heights[i] + (mainCount++ ? gap : 0);
        const float limit = place.limit > 0 ? place.limit : sh * kStackLimit;
        for (int pass = 0; pass < 2; ++pass)
            for (size_t i = 0; i + 1 < g_blocks.size() && place.bottom + total > limit; ++i)
            {
                const Block& x = g_blocks[i];
                if (x.leaving >= 0 || inCorner(x) || (pass == 0 && x.retireAt < 0 && !x.chatter) ||
                    (x.comm && now < x.audioEnd - 0.15))
                    continue;
                Retire(g_blocks[i], now), total -= heights[i] + gap;
            }
        // A fading line still holds its slot until it is gone, so the stack's real height counts it: when that would
        // carry the stack past the limit (34ee2e's display matrix, 26 Oct 2026: at 4:3 under HUD 2 / below FLUF's Debug
        // Window a two-speaker panel reached the crosshair ring for the half second the older line faded), the fading
        // lines' space is released first, oldest first - such a line goes at once and the stack closes up.
        float held = 0;
        size_t heldCount = 0;
        for (size_t i = 0; i < heights.size(); ++i)
            if (!inCorner(g_blocks[i]))
                held += heights[i] + (heldCount++ ? gap : 0);
        for (size_t i = 0; i < g_blocks.size() && place.bottom + held > limit; ++i)
        {
            Block& x = g_blocks[i];
            if (x.leaving < 0 || inCorner(x))
                continue;
            x.leaving = now - x.fadeOut; // gone: not drawn, erased at the next update
            released[i] = 1;
            held -= heights[i] + gap;
        }
    }
    // Inside the letterbox bar a pushed-out line keeps its grace (retire_delay) only while both fit in the bar (the
    // display-subtitles section measured 5-7 lines over the picture with the grace unbounded, 21 Oct 2026) - but never
    // goes before it was readable: a line pushed out before its reading minimum stays, stacked over the picture above
    // the bar, until it has had it (user, 21 Oct 2026, the M01a Donau cutscene's quick radio lines: "allow a new line to
    // push an older line ... a balance between keeping stuff to the black bars vs. allowing faster dialogue to push off
    // outside just to keep it readable"). Lines in sync with their audio either way; without bars nothing changes.
    else if (place.inBar && place.bar > 0)
    {
        float total = 0;
        size_t mainCount = 0;
        for (size_t i = 0; i < heights.size(); ++i)
            if (!inCorner(g_blocks[i]) && g_blocks[i].leaving < 0)
                total += heights[i] + (mainCount++ ? gap : 0);
        const float barTop = static_cast<float>(sh - place.bar);
        for (size_t i = 0; i < g_blocks.size() && place.bottom - total < barTop; ++i)
        {
            const Block& x = g_blocks[i];
            // (a comm whose voice is still heard stays over the picture: two comms at once - see Insert)
            if (x.leaving < 0 && x.retireAt >= 0 && !inCorner(x) && now >= x.start + x.readMin &&
                !(x.comm && now < x.audioEnd - 0.15))
                Retire(g_blocks[i], now), total -= heights[i] + gap;
        }
    }
    // The anchor and every block glide to their places (exponential ease; instant without motion or when the stack
    // starts afresh, in a new context or after a long frame gap).
    static double lastDraw = 0;
    static bool lastTop = false;
    static float anchor = 0;
    const float dt = static_cast<float>(std::clamp(now - lastDraw, 0.0, 0.25));
    const bool fresh = now - lastDraw > 0.5 || lastTop != place.fromTop || !g_set.motion;
    lastDraw = now, lastTop = place.fromTop;
    const float k = fresh ? 1.0f : 1.0f - std::exp(-dt / kGlideTau);
    anchor += (place.bottom - anchor) * k;
    // slots: top-anchored oldest first downwards; bottom-anchored newest at the bottom, older above; the corner hangs
    // from its own top
    std::vector<float> slot(g_blocks.size());
    if (place.fromTop)
    {
        float y = anchor;
        for (size_t i = 0; i < g_blocks.size(); ++i)
            if (!inCorner(g_blocks[i]))
            {
                slot[i] = y;
                if (!released[i])
                    y += heights[i] + gap;
            }
    }
    else
    {
        float y = anchor;
        for (size_t i = g_blocks.size(); i-- > 0;)
            if (!inCorner(g_blocks[i]))
                y -= heights[i], slot[i] = y, y -= gap;
    }
    {
        float y = cornerTop;
        for (size_t i = 0; i < g_blocks.size(); ++i)
            if (inCorner(g_blocks[i]))
                slot[i] = y, y += heights[i] + gap;
    }
    // The top panel (space_position 2): one HUD panel around the whole dialogue stack. Its target size follows the
    // stack (widest row, every block still holding a slot); it slides in from the top edge while a line shows and back
    // out after the last one; its width and height glide as the stack changes. Leaving flight hides it at once (the
    // HUD goes with it).
    float panelPartition[4] = {};
    int partitions = 0;
    // the rows' centre; the tab names' room right of the portrait (left edge, width; -1 without the portrait); the
    // panel's centre (off the screen's in the tab layout when the tab needs the room on the right)
    float textCx = place.centerX, tabNameX = -1, tabNameRoom = 0, panelCx = place.centerX;
    {
        const float pdt = static_cast<float>(std::clamp(now - g_panel.last, 0.0, 0.25));
        g_panel.last = now;
        if (!place.topPanel)
            g_panel.s = 0, g_panel.sized = false, g_panel.alpha = 0, g_panel.tabNoRoom = false;
        else
        {
            bool want = false;
            float rowsW = 0, stackH = 0;
            int mains = 0;
            for (size_t i = 0; i < g_blocks.size(); ++i)
            {
                if (inCorner(g_blocks[i]) || released[i])
                    continue;
                want = want || g_blocks[i].leaving < 0;
                for (const Row& r : blockRows[i])
                    rowsW = std::max(rowsW, r.w);
                if (mains++ && partitions < 4)
                    panelPartition[partitions++] = slot[i] - gap * 0.5f - anchor;
                stackH += heights[i] + (mains > 1 ? gap : 0);
            }
            const float line = std::max(1.5f, sh * kFrameLine);
            if (mains > 0)
            {
                const float maxPanel = place.maxW + 2 * panelSide;   // the placement's room (top or below the top row)
                const float sideSlot = portraitInTab ? 0.0f : slotW; // the portrait's column beside the rows
                const float tw = std::min(std::max(rowsW + sideSlot + 2 * panelSide, sh * kPanelMinW), maxPanel);
                g_panel.top = place.panelTop;
                // the tab around the first name (space_narrow 5), within the room between the bar and the NeuroNet box;
                // with the portrait it holds both. That room is wider on the right (the NeuroNet box is narrower than the
                // bar): the tab may take all of it, and the panel then moves right of centre so the tab fits (user, 22 Oct
                // 2026: "unused space to the right of the center-top ... push the NPC name to the right to give space for
                // the portrait")
                const float tabRoom = TabRoom(place);
                const float tabSlot = portraitInTab ? slotW : 0.0f;
                float tabTarget =
                    tabBlock >= 0 ? std::min(std::max(tabNameW + tabSlot + 2 * panelSide, sh * 0.16f), tabRoom) : 0.0f;
                // a body hardly wider than the tab (no room for the step's chamfers): one rectangle when it fits the room
                // between the bar and the NeuroNet box (it must not reach under the bar at the top), else a narrower tab
                const float bodyW = tw;
                if (tabBlock >= 0 && tw < tabTarget + 2 * sh * kChamW + 1)
                    tabTarget = tw <= tabRoom ? tw : tw - 2 * sh * kChamW - 1;
                // the panel's centre: the screen's, or as far right as the tab needs (the body kept on screen and clear of
                // the cruise box); the tab stays centred on the body
                panelCx = place.centerX;
                if (tabBlock >= 0 && place.tabRight > place.tabLeft)
                {
                    panelCx = std::clamp(panelCx, place.tabLeft + tabTarget * 0.5f,
                                         std::max(place.tabLeft + tabTarget * 0.5f, place.tabRight - tabTarget * 0.5f));
                    if (place.bodyMaxX - place.bodyMinX > bodyW)
                        panelCx = std::clamp(panelCx, place.bodyMinX + bodyW * 0.5f, place.bodyMaxX - bodyW * 0.5f);
                    // the keep-out (fix-text 5): the panel + tab (the portrait inside one of them) as one shape against
                    // the bar's live rectangle - the tab spans the top row, the body starts below the bar's frame; a
                    // tab the body's clamp pushed under the bar = no room: the panel goes below the top row (Place)
                    const float tabL = panelCx - tabTarget * 0.5f;
                    const float bodyL = panelCx - bodyW * 0.5f;
                    const float barR = g_barRight + sh * kPanelGap - 0.5f;
                    if (g_barRight >= 0 && (tabL < barR || (bodyTop < sh * kBarFrameBottom * g_mbScale && bodyL < barR)))
                        g_panel.tabNoRoom = true;
                }
                // HUD 2: centred on the screen, moved aside as far as its width needs to stay inside the free band
                if (place.freeR > place.freeL)
                    panelCx = bodyW > place.freeR - place.freeL
                                  ? (place.freeL + place.freeR) * 0.5f // wider than the band: spilled evenly
                                  : std::clamp(panelCx, place.freeL + bodyW * 0.5f, place.freeR - bodyW * 0.5f);
                textCx = sideSlot > 0 // the rows centred in the room right of the portrait
                             ? panelCx - tw * 0.5f + panelSide + sideSlot + (tw - 2 * panelSide - sideSlot) * 0.5f
                             : panelCx;
                if (tabBlock >= 0 && tabSlot > 0) // the names' room right of the portrait (each row centred in it)
                    tabNameX = panelCx - tabTarget * 0.5f + panelSide + tabSlot,
                    tabNameRoom = tabTarget - 2 * panelSide - tabSlot;
                g_panel.bodyTop = tabBlock >= 0 ? bodyTop : 0.0f;
                float th = anchor + stackH + sh * kPanelBottom + line;
                if (slotS > 0 && !portraitInTab) // deep enough for the portrait beside its line
                    th = std::max(th, slot[portraitBlock] + slotS + sh * kPanelBottom + line);
                if (!g_panel.sized || g_panel.s <= 0 || !g_set.motion)
                    g_panel.x = panelCx, g_panel.w = bodyW, g_panel.h = th, g_panel.tabW = tabTarget,
                    g_panel.sized = true;
                else
                {
                    const float pk = 1.0f - std::exp(-pdt / kGlideTau);
                    g_panel.x += (panelCx - g_panel.x) * pk;
                    g_panel.w += (bodyW - g_panel.w) * pk;
                    g_panel.h += (th - g_panel.h) * pk;
                    g_panel.tabW += (tabTarget - g_panel.tabW) * pk;
                }
            }
            if (!g_set.motion)
                g_panel.s = want ? 1.0f : 0.0f;
            else
                g_panel.s = std::clamp(g_panel.s + (want ? pdt / kSlideIn : -pdt / kSlideOut), 0.0f, 1.0f);
            const float e =
                1.0f - (1.0f - g_panel.s) * (1.0f - g_panel.s) * (1.0f - g_panel.s); // ease out in, ease in out
            // flush with the top edge: from beyond it; below the top row (space_narrow 3): a short drop with the fade
            g_panel.yoff = g_panel.top > 0 ? -(1.0f - e) * sh * 0.04f : -(1.0f - e) * (g_panel.h + sh * 0.02f);
            g_panel.alpha = e * e * (3 - 2 * e);
            if (g_panel.s <= 0)
                g_panel.sized = false, g_panel.tabNoRoom = false;
            // the hole where the game draws the comm window this frame: the slot PlaceCommWindow wrote (asked for by the
            // last frame)
            // (the game's outline as written - it grows with the opening line -, or the closing animation's own, from
            // the last frame)
            if (g_comm.moved && g_comm.s > 0)
            {
                // while it opens only the picture's part (the panel stays behind the growing outline), then the outline
                const bool whole = g_comm.w >= g_comm.s - 0.5f && g_comm.h >= g_comm.s - 0.5f;
                const float c = g_comm.s * 0.5f, fx = (whole ? g_comm.f : g_comm.w) * 0.5f,
                            fy = (whole ? g_comm.f : g_comm.h) * 0.5f;
                g_hole = {true, g_comm.x + c - fx, g_comm.y + c - fy, g_comm.x + c + fx, g_comm.y + c + fy};
            }
            else
                g_hole = g_closingHole;
            DrawTopPanel(static_cast<float>(sh), g_set.bgOpacity / 100.0f, now);
            g_hole.on = false;
            // partitions between speakers: the HUD's thin divider, inset from the chamfers
            const float divSlot = portraitInTab ? 0.0f : slotW; // the dividers start right of the portrait
            const float px0 = g_panel.x - g_panel.w * 0.5f + sh * kChamW + divSlot,
                        pw = g_panel.w - 2 * sh * kChamW - divSlot;
            for (int i = 0; i < partitions; ++i)
                render::Box(px0, anchor + panelPartition[i] + g_panel.yoff, pw, std::max(1.0f, sh / 2160.0f),
                            Fade(0x597697B4, g_panel.alpha));
        }
        g_layout.panelX = g_panel.x, g_layout.panelW = g_panel.w, g_layout.panelH = g_panel.h;
        g_layout.panelYoff = g_panel.yoff, g_layout.panelS = g_panel.s, g_layout.topPanel = place.topPanel;
    }
    // the rows' centre glides as the portrait's column comes and goes (it moved them in one frame)
    {
        static float shownCx = 0;
        shownCx = fresh || std::fabs(shownCx - textCx) > sw * 0.5f ? textCx : shownCx + (textCx - shownCx) * k;
        textCx = shownCx;
    }
    float portraitAlpha = 0;
    float commBoxL = -1, commBoxR = 0, commBoxT = 0; // the newest comm-with-portrait block's box outside the top panel
    for (size_t bi = 0; bi < g_blocks.size(); ++bi)
    {
        Block& b = g_blocks[bi];
        b.y = b.placed && !fresh && !b.snap ? b.y + (slot[bi] - b.y) * k : slot[bi];
        b.snap = false;
        b.placed = true;
        // fade in, fade out (smoothstep)
        auto smooth = [](double x)
        {
            x = std::clamp(x, 0.0, 1.0);
            return static_cast<float>(x * x * (3 - 2 * x));
        };
        const bool corner = inCorner(b);
        // inside the top panel: the lines ride its slide and share its fade
        const bool inPanel = place.topPanel && !corner;
        const float a =
            (!g_set.motion ? (b.leaving < 0 ? 1.0f : 0.0f)
                           : smooth((now - b.start) / kFadeIn) *
                                 (b.leaving < 0 ? 1.0f
                                  // a line kicked up fades visibly from the first frame (linear); others ease
                                  : b.fadeOut > kFadeOut
                                      ? 1.0f - static_cast<float>(std::clamp((now - b.leaving) / b.fadeOut, 0.0, 1.0))
                                      : 1.0f - smooth((now - b.leaving) / b.fadeOut))) *
            (inPanel ? g_panel.alpha : 1.0f);
        if (static_cast<int>(bi) == portraitBlock)
            portraitAlpha = a; // the held portrait fades with its line
        if (soft) // a movie: a transition runs while a block fades or glides (movie.cpp redraws at the display's rate)
            g_movie.animating = g_movie.animating || b.leaving >= 0 || (a > 0.01f && a < 0.995f) ||
                                std::fabs(slot[bi] - b.y) > 0.5f;
        if (a <= 0.01f)
            continue;
        const auto& rows = blockRows[bi];
        const float by = b.y + (inPanel ? g_panel.yoff : 0.0f);
        float movieBox = 1; // a movie line's backing share (soft): none on the bars, by the picture under it inside
        if (soft) // the block's rectangle (backing + feather + shadow + outline), its box, its share of the picture's key
        {
            float w = 0;
            for (const Row& r : rows)
                w = std::max(w, r.w);
            const float bx0 = (corner ? cornerLeft + padX : textCx - w * 0.5f) - padX, by0 = by - padY;
            const RECT boxRc = {static_cast<LONG>(std::floor(bx0)), static_cast<LONG>(std::floor(by0)),
                                static_cast<LONG>(std::ceil(bx0 + w + 2 * padX)),
                                static_cast<LONG>(std::ceil(by0 + heights[bi] + 2 * padY))};
            const float e = std::max(2.0f, px * 0.12f) + shadowOff + 3.0f;
            RECT rc = {static_cast<LONG>(std::floor(bx0 - e)), static_cast<LONG>(std::floor(by0 - e)),
                       static_cast<LONG>(std::ceil(bx0 + w + 2 * padX + e)),
                       static_cast<LONG>(std::ceil(by0 + heights[bi] + 2 * padY + e))};
            rc.left = std::max<LONG>(rc.left, 0), rc.top = std::max<LONG>(rc.top, 0);
            rc.right = std::min<LONG>(rc.right, sw), rc.bottom = std::min<LONG>(rc.bottom, sh);
            // the box: none wholly outside the picture (the bars are its backing), else movie.cpp's target; eased over
            // 0.3 s where the target blends (snapped while the line fades in), a hard cut where it cannot
            const RECT& pic = g_moviePicture;
            const bool inPicture = pic.right <= pic.left || (boxRc.right > pic.left && boxRc.left < pic.right &&
                                                             boxRc.bottom > pic.top && boxRc.top < pic.bottom);
            float target = 0;
            if (inPicture)
            {
                target = g_movieBoxDefault;
                for (const auto& t : g_movieBoxTarget)
                    if (t.first == b.start)
                        target = t.second;
            }
            if (!g_movieTranslucent || now - b.start < kFadeIn || fresh || !g_set.motion)
                b.movieBox = target;
            else
                b.movieBox += (target - b.movieBox) * (1.0f - std::exp(-dt / 0.1f));
            if (std::fabs(b.movieBox - target) < 0.01f)
                b.movieBox = target;
            g_movie.animating = g_movie.animating || b.movieBox != target;
            movieBox = b.movieBox;
            if (rc.right > rc.left && rc.bottom > rc.top)
            {
                g_movie.rects.push_back(rc), g_movie.boxes.push_back(boxRc), g_movie.ids.push_back(b.start);
                g_movie.box.push_back(movieBox);
            }
            auto mix = [](unsigned long long& k, unsigned long long v) { k = (k ^ v) * 1099511628211ull; };
            mix(g_movie.key, static_cast<unsigned long long>(rc.left) << 32 | static_cast<u32>(rc.top));
            mix(g_movie.key, static_cast<unsigned long long>(std::lround(by * 4)) << 16 | static_cast<u32>(a * 255.0f + 0.5f));
            mix(g_movie.key, static_cast<u32>(movieBox * 255.0f + 0.5f));
            for (const Row& r : rows)
                for (const wchar_t c : r.text)
                    mix(g_movie.key, c);
        }
        // without a box (a movie line on the bars or on flat picture) the text keeps an outline and its shadow: it reads
        // on any colour
        const bool outline = soft && movieBox < 0.5f;
        // (scale < 1: a name row shrunk by the fit rule, drawn scaled from its font)
        auto drawText = [&](float tx, float ty, const std::wstring& s, u32 c, float scale)
        {
            if (scale > 0.995f)
                render::Text(tx, ty, s, c);
            else
                render::TextScaled(tx, ty, s, c, scale);
        };
        auto under = [&](float tx, float ty, const std::wstring& s, float scale = 1.0f)
        {
            if (outline)
            {
                const float o = std::max(1.0f, px / 22.0f);
                const u32 oc = Fade(0x90000000, a * (1.0f - movieBox * 2.0f));
                drawText(tx - o, ty, s, oc, scale), drawText(tx + o, ty, s, oc, scale);
                drawText(tx, ty - o, s, oc, scale), drawText(tx, ty + o, s, oc, scale);
            }
            if (g_set.shadow || outline)
                drawText(tx + shadowOff, ty + shadowOff, s, Fade(0xC0000000, a), scale);
        };
        {
            float wmax = 0;
            for (const Row& r : rows)
                wmax = std::max(wmax, r.w);
            // the resting place (the top panel's slide offset is on the report's panel line)
            g_layout.blocks.push_back({data::Narrow(b.speaker), b.y, heights[bi], wmax, corner,
                                       corner ? cornerLeft + padX : textCx - wmax * 0.5f, b.nameArgb, b.hostile});
        }
        // a row's left edge: centred (right of the portrait's column when it is in the panel), or left-aligned in the
        // Neural Net column
        auto rowX = [&](float w) { return corner ? cornerLeft + padX : textCx - w * 0.5f; };
        // a comm's box outside the top panel (the portrait goes beside it: below)
        if (!corner && !inPanel && place.flight && b.portrait && b.leaving < 0)
        {
            float w = 0;
            for (const Row& r : rows)
                w = std::max(w, r.w);
            commBoxL = rowX(w) - padX, commBoxR = commBoxL + w + 2 * padX, commBoxT = by - padY;
        }
        // backing: one soft panel per speaker in the HUD's dark blue (its panels are translucent navy), feathered
        // over a few pixels instead of hard boxes per row
        // (none inside the letterbox bar - the black bar is the backing - nor inside the top panel)
        if (g_set.bgOpacity > 0 && !(place.inBar && !corner) && !inPanel)
        {
            float w = 0;
            for (const Row& r : rows)
                w = std::max(w, r.w);
            const float x0 = rowX(w) - padX, y0 = by - padY, bw = w + 2 * padX, bh = heights[bi] + 2 * padY;
            // the corner sits over bright space more often: a stronger panel there
            const int opacity = corner ? std::min(100, g_set.bgOpacity + 20) : g_set.bgOpacity;
            if (g_set.bgStyle == 1 && place.flight) // the HUD look in flight only (user: elsewhere the original)
                HudPanel(x0, y0, bw, bh, opacity / 100.0f * a, static_cast<float>(sh), now);
            else if (movieBox > 0.0f)
            {
                const u32 navy = 0x00081422, base = static_cast<u32>(opacity * 255 / 100 * a * movieBox);
                const float f = std::max(2.0f, px * 0.12f); // feather width
                for (int ring = 3; ring >= 1; --ring)
                {
                    const float e = f * ring / 3.0f;
                    render::Box(x0 - e, y0 - e, bw + 2 * e, bh + 2 * e, ((base * (4 - ring) / 8) << 24) | navy);
                }
                render::Box(x0, y0, bw, bh, (base << 24) | navy);
            }
        }
        float ry = by + (static_cast<int>(bi) == portraitBlock ? portraitDrop : 0.0f);
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const Row& r = rows[i];
            const bool tabName = static_cast<int>(bi) == tabBlock && static_cast<int>(i) < tabNames;
            const float x = tabName && tabNameX >= 0 ? tabNameX + (tabNameRoom - r.w) * 0.5f : rowX(r.w);
            if (r.name)
            {
                float nx = x;
                // the tab's names beside the portrait: centred together on its height
                const float ny = ry + (tabName && tabNameX >= 0 ? std::max(0.0f, (slotS - tabNameH) * 0.5f) : 0.0f);
                if (b.hostile && i == 0)
                {
                    HostileMarker(x, ny, r.h, a);
                    nx += MarkerWidth(r.h);
                }
                render::SetFont(nameFont);
                const float sy = ny + nameFont.lineHeight * (1.0f - r.scale) * 0.5f; // a shrunk name: the same centre line
                under(nx, sy, r.text, r.scale);
                drawText(nx, sy, r.text, Fade(r.argb, a), r.scale);
                render::SetFont(g_font);
                // the tab: after its last name the rows continue in the body
                ry += r.h + (static_cast<int>(bi) == tabBlock && static_cast<int>(i) == tabNames - 1 ? tabJump : 0.0f);
                continue;
            }
            // inline name: colour the "Name:" part of the first text row
            size_t split = 0;
            if ((g_set.nameMode == 1 || inlineName) && i == 0 && !b.speaker.empty() &&
                r.text.compare(0, b.speaker.size(), b.speaker) == 0)
                split = b.speaker.size() + 1;
            const std::wstring head = r.text.substr(0, split), tail = r.text.substr(split);
            const float tailX = x + render::TextWidth(g_font, head);
            if (split && b.hostile) // inline: the marker hangs left of the row (no backing inside the bar)
                HostileMarker(x - MarkerWidth(r.h), ry + padY, r.h, a);
            // the spoken-word pop (word_pop): each word's strength from its timing - up over kPopIn from its start, down
            // over kPopOut once the next word starts (the last: once the audio ends)
            std::vector<std::pair<size_t, size_t>> spans; // the row's words in r.text (begin, end)
            std::vector<float> pop;
            bool popping = false;
            if (g_set.wordPop && g_popFont.texture && !soft && r.words && !b.wordStarts.empty() && b.leaving < 0)
            {
                for (size_t c = 0; c < r.text.size();)
                {
                    while (c < r.text.size() && r.text[c] == L' ')
                        ++c;
                    size_t e = c;
                    while (e < r.text.size() && r.text[e] != L' ')
                        ++e;
                    if (e > c)
                        spans.push_back({c, e});
                    c = e;
                }
                if (spans.size() == r.words->size())
                {
                    const float age = static_cast<float>(now - b.start);
                    const float audio = static_cast<float>(b.audioEnd - b.start);
                    for (const int wi : *r.words)
                    {
                        float pk = 0;
                        if (wi >= 0 && static_cast<size_t>(wi) < b.wordStarts.size() && b.wordStarts[wi] >= 0)
                        {
                            // its time: until the next placed word starts (the last: until the audio ends)
                            const float s = b.wordStarts[wi];
                            float e = audio;
                            for (size_t n = static_cast<size_t>(wi) + 1; n < b.wordStarts.size(); ++n)
                                if (b.wordStarts[n] >= 0)
                                {
                                    e = b.wordStarts[n];
                                    break;
                                }
                            if (static_cast<size_t>(wi) + 1 < b.wordStarts.size() && b.wordStarts[wi + 1] < 0)
                                e = std::min(e, s + 0.5f); // unplaced words follow: not over all of them
                            if (e > s && age >= s)
                            {
                                pk = std::clamp((age - s) / kPopIn, 0.0f, 1.0f) *
                                     std::clamp(1.0f - (age - e) / kPopOut, 0.0f, 1.0f);
                                pk = pk * pk * (3 - 2 * pk); // smoothstep: eases in and out
                            }
                        }
                        pop.push_back(pk);
                        popping |= pk > 0;
                    }
                }
            }
            if (!popping)
            {
                under(x, ry + padY, head), under(tailX, ry + padY, tail);
                render::Text(x, ry + padY, head, Fade(b.nameArgb, a));
                render::Text(tailX, ry + padY, tail, Fade(r.argb, a));
                ry += r.h;
                continue;
            }
            // word by word: the inline name as before, each word in its own place; a popping word at the size of its
            // strength, centred on that place
            if (split)
            {
                if (g_set.shadow)
                    render::Text(x + shadowOff, ry + padY + shadowOff, head, Fade(0xC0000000, a));
                render::Text(x, ry + padY, head, Fade(b.nameArgb, a));
            }
            const int spaceW = render::TextWidth(g_font, L" ");
            for (size_t m = 0; m < spans.size(); ++m)
            {
                if ((*r.words)[m] < 0 && split)
                    continue; // drawn with the name
                const std::wstring word = r.text.substr(spans[m].first, spans[m].second - spans[m].first);
                const float wx = x + render::TextWidth(g_font, r.text.substr(0, spans[m].first));
                const float pk = pop[m];
                u32 argb = (*r.words)[m] < 0 ? b.nameArgb : r.argb;
                if (pk < 0.02f) // at rest: the text font itself
                {
                    if (g_set.shadow)
                        render::Text(wx + shadowOff, ry + padY + shadowOff, word, Fade(0xC0000000, a));
                    render::Text(wx, ry + padY, word, Fade(argb, a));
                    continue;
                }
                // toward white by a third at full pop (shows on the yellow text colour; white stays white)
                u32 lit = argb & 0xFF000000;
                for (int sh8 = 0; sh8 < 24; sh8 += 8)
                {
                    const float ch = static_cast<float>((argb >> sh8) & 0xFF);
                    lit |= static_cast<u32>(ch + (255.0f - ch) * 0.35f * pk + 0.5f) << sh8;
                }
                // the pop font scaled to 1 + (top - 1) pk of the text size, centred on the word's place and row; top =
                // kPopScale, or less where that would grow the word into its gaps (textfit::GrowCap: a fifth of a space
                // per side at most - Agency FB's space is narrow, a long word at 1.08 closed it)
                const float rest = static_cast<float>(render::TextWidth(g_font, word));
                const float toText = static_cast<float>(g_font.px) / static_cast<float>(g_popFont.px);
                const float popW = static_cast<float>(render::TextWidth(g_popFont, word)) * toText;
                const float top = textfit::GrowCap(rest, popW, static_cast<float>(spaceW), kPopScale);
                const float sc = (1 + (top - 1) * pk) * toText;
                const float px0 = std::floor(wx) + (rest - popW / toText * sc) * 0.5f;
                const float py0 = std::floor(ry + padY) + (g_font.lineHeight - g_popFont.lineHeight * sc) * 0.5f;
                render::SetFont(g_popFont);
                if (g_set.shadow)
                    render::TextScaled(px0 + shadowOff, py0 + shadowOff, word, Fade(0xC0000000, a), sc);
                render::TextScaled(px0, py0, word, Fade(lit, a), sc);
                render::SetFont(g_font);
            }
            ry += r.h;
        }
    }
    // the comm window into its slot (PlaceCommWindow writes it with the frame update): beside its line, riding the
    // panel's slide, never above the screen's top edge (the game's viewport must stay on screen)
    // It glides between places as the rows do (the line's slot in the body -> the tab when the older line leaves), and
    // appears in place.
    g_layout.portraitS = 0;
    static float gx = 0, gy = 0, gs = 0;
    static bool wasLive = false; // the window was in the slot last frame (else it opens with the animation)
    static double openSince = 0;
    if (slotS > 0 && g_panel.alpha > 0.01f)
    {
        const float x = portraitInTab ? g_panel.x - g_panel.tabW * 0.5f + panelSide
                        : portraitDrop > 0 ? textCx - slotS * 0.5f // above its line, centred on the rows
                                           : g_panel.x - g_panel.w * 0.5f + panelSide;
        const float y = portraitInTab ? anchor : slot[portraitBlock]; // the slide offset added after the glide
        if (gs <= 0 || fresh)
            gx = x, gy = y, gs = slotS;
        else
            gx += (x - gx) * k, gy += (y - gy) * k, gs += (slotS - gs) * k;
        const float shownY = std::max(1.0f, gy + g_panel.yoff);
        // The channel opening and closing (user, 21 Oct 2026: a frozen last picture "did not look good"; "an animation
        // ... that signifies 'channel closed' in a way that fits Freelancer's HUD language"; then "if you have a fade
        // out for it, you can also make a fade in", the outline included). Both in the HUD scanline's pale blue
        // (refreshtest2): closing, the picture - the game's thin frame with it - folds into a line that shrinks to the
        // centre and goes, then the slot is released; opening, a line grows from the centre and the live window unfolds
        // from it (its size written into the game's window: the head render squashes with its viewport).
        const float la = std::clamp(portraitAlpha, 0.0f, 1.0f);
        const float cx = gx + gs * 0.5f, cy = shownY + gs * 0.5f, thin = std::max(2.0f, sh * 0.0028f);
        auto ease = [](float x)
        {
            x = std::clamp(x, 0.0f, 1.0f);
            return x * x * (3 - 2 * x);
        };
        auto argb = [&](u32 rgb, float al)
        { return (static_cast<u32>(std::clamp(al, 0.0f, 1.0f) * 255.0f + 0.5f) << 24) | rgb; };
        // the line at width w (0..gs), fading by f: a bright core and a soft glow
        auto line = [&](float w, float f)
        {
            if (w <= 1.0f || f <= 0.0f)
                return;
            render::Box(cx - w * 0.5f, cy - thin * 2.0f, w, thin * 4.0f, argb(kScanBlue, 0.25f * f * la));
            render::Box(cx - w * 0.5f, cy - thin * 0.5f, w, thin, argb(kScanWhite, f * la));
        };
        if (live)
        {
            if (!wasLive)
                openSince = now;
            const float t = static_cast<float>(now - openSince);
            float sx = 1, sy = 1, fs = 1;
            if (t < kOpenLine) // the line grows from the centre; the window is that line, its outline grows with it
            {
                const float e = ease(t / static_cast<float>(kOpenLine));
                sx = std::max(0.01f, e), sy = thin / std::max(1.0f, gs), fs = std::max(0.02f, e);
                line(gs * e, 1.0f);
            }
            else if (t < kChannelOpen) // the window unfolds from it, the blue washing off
            {
                const float e = ease((t - static_cast<float>(kOpenLine)) / static_cast<float>(kOpenUnfold));
                sy = std::max(thin / std::max(1.0f, gs), e);
                const float h = gs * sy;
                render::Box(gx, cy - h * 0.5f, gs, h, argb(kScanBlue, 0.55f * (1.0f - e) * la));
                line(gs, 1.0f - e);
            }
            CommSlotRequest(gx, shownY, gs, sx, sy, fs);
            g_liveBlock = g_blocks[portraitBlock].start;
        }
        else if (g_held.img.texture)
        {
            const float t = static_cast<float>(now - g_held.since);
            // the game's outline, gone with its window: ours in its place (its colour, 130, 201, 243, measured at 4K),
            // full while the picture folds, then shrinking with the line; the panel leaves a hole inside it, as live
            const float shrink =
                t < kFoldTime ? 0.0f : ease((t - static_cast<float>(kFoldTime)) / static_cast<float>(kLineTime));
            const float F = gs * kCommFrameRatio * (1.0f - shrink), edge = std::max(1.0f, sh / 2160.0f);
            g_closingHole = {F > 2.0f, cx - F * 0.5f, cy - F * 0.5f, cx + F * 0.5f, cy + F * 0.5f};
            if (F > 2.0f)
            {
                const u32 oc = argb(0x82C9F3, 0.9f * (1.0f - shrink) * la);
                const float ox = cx - F * 0.5f, oy = cy - F * 0.5f;
                render::Box(ox, oy, F, edge, oc), render::Box(ox, oy + F - edge, F, edge, oc);
                render::Box(ox, oy, edge, F, oc), render::Box(ox + F - edge, oy, edge, F, oc);
            }
            if (t < kFoldTime) // the picture folds towards its centre line, washing out to the scanline blue
            {
                const float e = ease(t / static_cast<float>(kFoldTime)), h = std::max(thin, gs * (1.0f - e));
                const u32 tint = (static_cast<u32>(255 - 95 * e) << 16) | (static_cast<u32>(255 - 46 * e) << 8) | 255u;
                render::Sprite(g_held.img, cx, cy, gs, h, 0.0f, (static_cast<u32>(la * 255.0f) << 24) | tint, false);
                render::Box(gx, cy - h * 0.5f, gs, h, argb(kScanBlue, 0.55f * e * la));
            }
            else // the line shrinks to the centre and goes
            {
                const float e = ease((t - static_cast<float>(kFoldTime)) / static_cast<float>(kLineTime));
                line(gs * (1.0f - e), 1.0f - e);
            }
        }
        if (live || !g_held.img.texture)
            g_closingHole.on = false;
        wasLive = live;
        g_layout.portraitX = gx, g_layout.portraitY = shownY, g_layout.portraitS = gs;
    }
    else if (!soft && g_set.enabled && g_set.commPortrait && !place.topPanel && commBoxL >= 0 &&
             CommWindowOpen(&nativePortrait) && nativePortrait > 1.0f)
    {
        // No top panel (the classic HUD with the bar centred: vanilla has no HudShift, so the bar never moves; the
        // lines below it): the window goes beside its line's box, at its top - left of it, or right where the left has no
        // room (fix-text 5: it stayed at the game's place, the screen's top left, away from its line)
        const float s = std::min(nativePortrait, sh * 0.11f), pg = sh * kPanelGap;
        const float x = commBoxL - pg - s >= pg ? commBoxL - pg - s : commBoxR + pg;
        const float y = std::max(1.0f, commBoxT);
        CommSlotRequest(x, y, s);
        g_layout.portraitX = x, g_layout.portraitY = y, g_layout.portraitS = s;
        gs = 0, wasLive = false, g_closingHole.on = false;
    }
    else if (const HudLayout* h2 = Hud2Layout(); !soft && g_set.enabled && g_set.commPortrait && h2 && Shown(h2->nav) &&
                                                 CommWindowOpen(&nativePortrait) && nativePortrait > 1.0f)
    {
        // HUD 2 shows and the panel holds no slot for the portrait (rows too narrow beside it: 4:3 with a wide face,
        // comm_portrait in a layout without the panel): the window goes live under HUD 2's nav block, read from the bus
        // every frame - it follows the nav as it grows or shrinks (was: the native place shifted once when the window
        // opened, ShiftCommWindow; the nav's later rows ran under it - vanilla 1.1 at 1280x960, 25 Oct 2026). The same
        // on every build: no HudShift, the game's own comm window placed by its live fields (PlaceCommWindow).
        const float s = std::min(nativePortrait, sh * 0.11f);
        const float x = std::max(1.0f, h2->nav.x0 * sw), y = h2->nav.y1 * sh + sh * kPanelGap;
        if (y + s < sh * 0.6f)
        {
            CommSlotRequest(x, y, s);
            g_layout.portraitX = x, g_layout.portraitY = y, g_layout.portraitS = s;
        }
        gs = 0, wasLive = false, g_closingHole.on = false;
    }
    else
        gs = 0, wasLive = false, g_closingHole.on = false;
    (void)bg;
    render::End(ownScene);
}

// A game menu over the scene: the pause menu, the in-game load / save screens, Options, the yes / no dialog, the front
// end's load screen (by their verified control names, docs\FREELANCER_RE.md "Main menu, load screens, pause menu") or
// the main menu (a shown control whose click handler is the main menu's, exe::MainMenuButtonClick - its buttons are
// unnamed). The panel, the chatter feed and the HUD 2 preview step aside meanwhile (the comm panel drew over the pause
// plate's title and the main menu: Extended HUD 2 verify, 25 Oct 2026, both ui_style values) and come back with the
// menu gone; their timers run on. Looked up at 4 Hz (a name lookup each, one list walk for the main menu).
static bool MainMenuShown()
{
    if (!exe::UiControls.ok() || !exe::MainMenuButtonClick.ok())
        return false;
    __try
    {
        const u32 head = *reinterpret_cast<const u32*>(static_cast<u32>(exe::UiControls.get()));
        u32 node = head ? *reinterpret_cast<const u32*>(head + 4) : 0;
        for (int guard = 0; node && node != head && guard < 8000; ++guard, node = *reinterpret_cast<const u32*>(node + 4))
        {
            const u32 ctl = *reinterpret_cast<const u32*>(node + 8);
            if (!ctl || !(*reinterpret_cast<const u8*>(ctl + 0x6C) & 1))
                continue;
            const u32 vt = *reinterpret_cast<const u32*>(ctl);
            if (vt && exe::MainMenuButtonClick.is(*reinterpret_cast<const u32*>(vt + 0x4C)))
                return true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return false;
}
static bool g_menuOver = false;
static bool MenuOver()
{
    static ULONGLONG at = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - at < 250)
        return g_menuOver;
    at = now;
    bool over = false;
    for (const char* n : {"PauseButtonBox0", "LoadSave_LoadGame", "LoadSave_SaveGame", "PreferencesCloseButton",
                          "Confirm_delBtnYes", "LoadCharacter"})
        if (uiclick::FindVisible(n))
        {
            over = true;
            break;
        }
    if (!over)
        over = MainMenuShown();
    if (over != g_menuOver)
        Log("display: %s", over ? "a game menu is up - the panel steps aside" : "the game menu is gone - the panel is back");
    return g_menuOver = over;
}

void Draw(bool ownScene)
{
    PerfScope perf(g_perf.draw);
    int sw = 0, sh = 0;
    if (MenuOver())
        return;
    if (render::ScreenSize(sw, sh))
    {
        DrawHud2Sim(ownScene, sw, sh);
        DrawImpl(sw, sh, ownScene, nullptr);
        DrawHud2Feed(sw, sh, ownScene);
    }
}

void SetMoviePicture(const RECT& picture, bool translucent)
{
    g_moviePicture = picture, g_movieTranslucent = translucent;
}

void SetMovieBoxDefault(float target) { g_movieBoxDefault = target; }

void SetMovieBox(double id, float target)
{
    for (auto& t : g_movieBoxTarget)
        if (t.first == id)
        {
            t.second = target;
            return;
        }
    // (the targets of lines gone: dropped once the list grows past the few lines a movie shows at once)
    if (g_movieBoxTarget.size() >= 16)
        g_movieBoxTarget.erase(g_movieBoxTarget.begin());
    g_movieBoxTarget.push_back({id, target});
}

void DrawMovie(u32* pixels, int w, int h)
{
    g_movie.rects.clear(), g_movie.boxes.clear(), g_movie.ids.clear(), g_movie.box.clear();
    g_movie.key = 14695981039346656037ull;
    g_movie.animating = false;
    DrawImpl(w, h, false, pixels);
}

const MovieFrame& LastMovieFrame() { return g_movie; }

bool Active() { return g_set.enabled && !g_blocks.empty(); }
} // namespace display
