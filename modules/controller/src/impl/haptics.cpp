// Haptics' game side: turns hits, shield failure, hand-back and waiting boxes into effects mixed to the pad's rumble.
#include "haptics.h"

#include "device.h"
#include "gamepad.h"
#include "log.h"
#include "ui/uiopt.h"
#include "game/events.h"
#include "game/ai.h"
#include "game/room.h"
#include "game/space.h"

namespace haptics
{
static Mixer g_mix;
static Renderer g_out;
static HitDetector g_hits;
static Gates g_gates;
static bool g_apWas = false;
static bool g_popupWas = false;
static double g_popupCheck = -1e9;
static int g_hitEvents = 0, g_failEvents = 0, g_handBacks = 0, g_popups = 0;
static char g_last[96] = "none";

// The wall clock in seconds: the game's frame dt is 0 while it is paused (gameclock.h), and a box that pauses the game
// still asks with a pulse.
static double Now()
{
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) / static_cast<double>(freq.QuadPart);
}

static const char* ResultName(Mixer::Result r)
{
    switch (r)
    {
    case Mixer::kStarted: return "started";
    case Mixer::kMerged: return "merged";
    case Mixer::kSilent: return "silent";
    case Mixer::kFatigued: return "fatigued";
    case Mixer::kFull: return "full";
    case Mixer::kIgnored: return "ignored";
    }
    return "?";
}

static Mixer::Result Fire(int effect, float level, double now, const char* src)
{
    const Mixer::Result r = g_mix.Trigger(effect, level, now, g_gates);
    if (effect >= 0 && effect < kEffectCount)
    {
        snprintf(g_last, sizeof(g_last), "%s@%.2f %s src=%s", kEffects[effect].name, level, ResultName(r), src);
        // the harness reads the starts (rate-limited by each effect's gap; merges are counted only) and the injected
        // events' silences (the game's own while silent - a fight with the keyboard - only counted: no event flood)
        if (r == Mixer::kStarted || (r == Mixer::kSilent && strcmp(src, "debug") == 0))
            events::Push("haptics %s %s %.2f src=%s%s%s", ResultName(r), kEffects[effect].name, level, src,
                         r == Mixer::kSilent ? " why=" : "", r == Mixer::kSilent ? Silence(g_gates, kEffects[effect].cat) : "");
    }
    return r;
}

// The player ship's shield and hull now (shieldcue.cpp's reads); invalid out of space.
static Vitals ReadVitals(Where where)
{
    Vitals v;
    ai::Self s;
    if ((where != Where::Flight && where != Where::Cutscene) || !ai::ReadSelf(s) || s.hullMax <= 0)
        return v;
    v.valid = true;
    v.ship = space::PlayerShipId();
    float cur = 0, max = 0;
    bool up = false;
    if (s.shieldMax > 0 && !space::ShieldOf(v.ship, cur, max, up))
        cur = s.shield * s.shieldMax, max = s.shieldMax, up = s.shield > 0;
    v.shield = cur, v.shieldMax = max, v.shieldUp = up && s.shieldMax > 0;
    v.hull = s.hull * s.hullMax, v.hullMax = s.hullMax;
    return v;
}

// A box of the game's waits for the player's answer: pub::Player::PopUpDialog's box (the story's tutorial boxes, OK;
// YES / NO / LATER from scripts and servers), found by its buttons (docs\FREELANCER_RE.md "MMOkButton"). Looked up 4
// times a second, only while a prompt may play (the lookup walks the control list).
static bool PopupOpen()
{
    for (const char* n : {"MMOkButton", "MMYesButton", "MMNoButton", "MMLaterButton"})
        if (uiclick::FindVisible(n))
            return true;
    return false;
}

void Update(const Frame& f, Gamepad& pad)
{
    const double now = Now();
    g_gates.rumble = uiopt::Get().rumble;
    g_gates.enabled = f.enabled;
    g_gates.focus = !pad.ignoreReal;
    g_gates.pad = device::Pad();
    g_gates.paused = f.paused;
    g_gates.where = f.where;

    // the hits and the shield's failure (every frame in space, also while silent: the baseline stays current)
    const Hits h = g_hits.Step(ReadVitals(f.where));
    if (h.shieldFailed)
        ++g_failEvents, Fire(kShieldFail, 1.0f, now, "game");
    if (h.shieldShare > 0)
        ++g_hitEvents, Fire(kHitShield, HitStrength(h.shieldShare), now, "game");
    if (h.hullShare > 0)
        ++g_hitEvents, Fire(kHitHull, HitStrength(h.hullShare), now, "game");
    // the hand-back: the autopilot stopped (the player's stick, its key, its goal done, a stop of its own); one that ends
    // by a dock or in a cutscene is silent (Silence: docked / cutscene)
    if (g_apWas && !f.autopilot)
        ++g_handBacks, Fire(kHandBack, 1.0f, now, "game");
    g_apWas = f.autopilot;
    // a box that waits for the player: when it opens (also while silent - counted, never replayed later: a box already
    // up when the gate opens does not ask again)
    if (f.where == Where::Flight || f.where == Where::Docked)
    {
        if (now - g_popupCheck >= 0.25)
        {
            g_popupCheck = now;
            const bool open = PopupOpen();
            if (open && !g_popupWas)
                ++g_popups, Fire(kConfirm, 1.0f, now, "game");
            g_popupWas = open;
        }
    }
    else
        g_popupWas = false;

    Out send;
    if (g_out.Due(g_mix.Update(now, g_gates), now, send))
        pad.Rumble(send.low, send.high, send.low > 0 || send.high > 0 ? Renderer::kHold : 0.0f);
}

void Stop(Gamepad& pad)
{
    g_mix.Stop();
    g_out.Forget();
    pad.Rumble(0, 0, 0);
}

std::string Status()
{
    std::ostringstream o;
    const Out m = g_mix.Last(), s = g_out.LastSent();
    o.setf(std::ios::fixed);
    o.precision(3);
    o << "haptics rumble=" << g_gates.rumble << " world=" << (*Silence(g_gates, kWorld) ? Silence(g_gates, kWorld) : "open")
      << " assist=" << (*Silence(g_gates, kAssist) ? Silence(g_gates, kAssist) : "open")
      << " prompt=" << (*Silence(g_gates, kPrompt) ? Silence(g_gates, kPrompt) : "open") << " mix=(" << m.low << ","
      << m.high << ") sent=(" << s.low << "," << s.high << ") on=" << g_out.On() << " sends=" << g_out.Sent()
      << " playing=" << g_mix.Playing() << " duty=" << g_mix.Duty();
    for (int r = Mixer::kStarted; r <= Mixer::kIgnored; ++r)
        o << " " << ResultName(static_cast<Mixer::Result>(r)) << "=" << g_mix.Counted(static_cast<Mixer::Result>(r));
    o << " game_hits=" << g_hitEvents << " game_fails=" << g_failEvents << " handbacks=" << g_handBacks
      << " popups=" << g_popups << " last=" << g_last;
    return o.str();
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what.empty() || what == "status")
        return Status();
    if (what == "table")
    {
        std::ostringstream o;
        for (const EffectDef& d : kEffects)
            o << d.name << " cat=" << (d.cat == kWorld ? "world" : d.cat == kAssist ? "assist" : "prompt") << " low=" << d.low
              << " high=" << d.high << " a/h/r=" << d.attack << "/" << d.hold << "/" << d.release << " pulses=" << d.pulses
              << " period=" << d.period << " prio=" << d.prio << " gap=" << d.minGap << " len=" << Duration(d)
              << " cue: " << d.cue << "\n";
        return o.str();
    }
    if (what == "play") // TEST: an event into the mixer through the live gates (src=debug)
    {
        std::string name;
        float level = 1;
        in >> name >> level;
        const int e = EffectByName(name.c_str());
        if (e < 0)
            return "haptics: no effect '" + name + "' (haptics table)";
        const Mixer::Result r = Fire(e, level, Now(), "debug");
        return std::string("haptics play ") + name + ": " + ResultName(r) +
               (r == Mixer::kSilent ? std::string(" (") + Silence(g_gates, kEffects[e].cat) + ")" : std::string());
    }
    if (what == "stop")
    {
        g_mix.Stop();
        return Status();
    }
    return "usage: haptics [status] | table | play <effect> [level 0..1] | stop";
}
} // namespace haptics
