// Haptics' game side: reads the game into Signals by tiers, runs the engine, sends the frame to the pad. Every reader here is core's; HAPTICS.md 3 says how far each stands.
#include "haptics.h"

#include "device.h"
#include "gamepad.h"
#include "log.h"
#include "flexe.h"
#include "ui/uiopt.h"
#include "game/events.h"
#include "game/ai.h"
#include "game/fl.h"
#include "game/flhash.h"
#include "game/flight.h"
#include "game/hostcalls.h"
#include "game/hudinfo.h"
#include "game/ini.h"
#include "game/room.h"
#include "game/space.h"

namespace haptics
{
static Engine g_engine;
static Render g_render;
static Gates g_gates;
static Signals g_s;
static EngineData g_engineData;
static u32 g_engineArch = 0;
static StanceInput g_in;
static Caps g_caps;
static bool g_open = false; // the engine runs: a pad in use and something on
static bool g_apWas = false;
static bool g_popupWas = false;
static bool g_hasBurner = true;
static double g_popupCheck = -1e9, g_next30 = -1e9, g_next10 = -1e9, g_last = -1e9;
static int g_cmWas = -1, g_mineWas = -1;
static u32 g_padSerial = 0;
static bool g_towardHole = false;
static int g_hitEvents = 0, g_failEvents = 0, g_handBacks = 0, g_popups = 0;
static char g_lastFired[96] = "none";
static const char* g_waveWhy = "";
static RecordingSink* g_record = nullptr; // debug `haptics record`: allocated only then

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

// The harness reads the starts (rate-limited by each effect's gap; merges are counted only) and the injected events' silences (the game's own while silent - a fight with the
// keyboard - only counted: no event flood).
static void Tell(int effect, float level, Mixer::Result r, const char* src)
{
    if (effect < 0 || effect >= kEffectCount)
        return;
    snprintf(g_lastFired, sizeof(g_lastFired), "%s@%.2f %s src=%s", kEffects[effect].name, level, ResultName(r), src);
    const bool frequent = kEffects[effect].soften || kEffects[effect].click || effect == kMissileNear || effect == kHullBeat; // a gun's every shot is no event
    if ((r == Mixer::kStarted && !frequent) || (r == Mixer::kSilent && strcmp(src, "debug") == 0))
        events::Push("haptics %s %s %.2f src=%s%s%s", ResultName(r), kEffects[effect].name, level, src, r == Mixer::kSilent ? " why=" : "",
                     r == Mixer::kSilent ? Silence(g_gates, kEffects[effect].cat) : "");
}

static Mixer::Result Fire(int effect, float level, double now, const char* src)
{
    const Mixer::Result r = g_engine.Fire(effect, level, now, g_gates);
    Tell(effect, level, r, src);
    return r;
}

// The pad. Every new output stops at the switch for a real pad: `haptics_device` 0, or a background game (a test), leaves only the recording. A stop always passes: Off is safe.
class PadSink : public Sink
{
  public:
    PadSink(Gamepad& pad, bool always) : pad_(pad), always_(always) {}
    void Motors(float low, float high, float hold) override
    {
        if (g_record)
            g_record->Motors(low, high, hold);
        if (Open() || (low <= 0 && high <= 0))
            pad_.Rumble(low, high, hold);
    }
    void TriggerMotors(float left, float right, float hold) override
    {
        if (g_record)
            g_record->TriggerMotors(left, right, hold);
        if (Open() || (left <= 0 && right <= 0))
            pad_.RumbleTriggers(left, right, hold);
    }
    void Stance(const unsigned char left[kEffectBytes], const unsigned char right[kEffectBytes]) override
    {
        if (g_record)
            g_record->Stance(left, right);
        const bool off = left[0] == 0x05 && right[0] == 0x05;
        if (Open() || off)
            pad_.TriggerEffects(left, right);
    }
    void Wave(const wavesynth::WaveFrame& f) override
    {
        if (g_record)
            g_record->Wave(f);
        pad_.WaveFeed(f);
    }

  private:
    bool Open() const
    {
        static const bool background = EnvVar("FLVIBED_BACKGROUND") == "1";
        return always_ || !pad_.OutputIsReal() || (uiopt::Get().hapticsDevice && !background);
    }
    Gamepad& pad_;
    bool always_;
};

// ---------------------------------------------------------------- the readers
// The mounted engine's rows of DATA\EQUIPMENT\engine_equip.ini, parsed once. An engine not found keeps the defaults: its bed differs by nothing but the throttle.
static EngineData EngineRows(u32 arch)
{
    static std::map<u32, EngineData> rows;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        for (const ini::Section& s : ini::Read(DllDir() + "..\\DATA\\EQUIPMENT\\engine_equip.ini")) // text or BINI
        {
            if (s.type != "engine")
                continue;
            std::string nick;
            EngineData e;
            for (const auto& kv : s.kv)
            {
                float a = 0, b = 0;
                if (kv.first == "nickname")
                    nick = kv.second;
                else if (kv.first == "rumble_atten_range" && sscanf_s(kv.second.c_str(), "%f , %f", &a, &b) == 2)
                    e.attenLow = a, e.attenHigh = b;
                else if (kv.first == "rumble_pitch_range" && sscanf_s(kv.second.c_str(), "%f , %f", &a, &b) == 2)
                    e.pitchLow = a, e.pitchHigh = b;
            }
            if (!nick.empty())
                rows[flhash::CreateId(nick)] = e;
        }
    }
    const auto it = rows.find(arch);
    return it == rows.end() ? EngineData() : it->second;
}

static float PanFrom(const float point[3], const fl::Kinematics& k)
{
    const float rel[3] = {point[0] - k.pos[0], point[1] - k.pos[1], point[2] - k.pos[2]};
    return PanOf(rel, k.rot);
}

// The game's hit report: the HUD's damage indicator's source. A kind 1 point is a round's far back along its flight or a blast's centre; the side reads the same from both.
static void ReadHit(Signals& s)
{
    float point[3];
    u32 kind = 0;
    const u32 seq = hostcalls::g.PlayerHit ? hostcalls::g.PlayerHit(point, &kind, nullptr) : 0;
    if (seq == s.hit.seq)
        return;
    s.hit.seq = seq, s.hit.kind = kind, s.hit.pan = 0, s.hit.distance = -1;
    fl::Kinematics k = {};
    if (fl::PlayerKinematics(k))
    {
        const float d[3] = {point[0] - k.pos[0], point[1] - k.pos[1], point[2] - k.pos[2]};
        s.hit.pan = PanFrom(point, k);
        s.hit.distance = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    }
}

// The shots from each gun's refire clock, the fastest gun for the trigger's rate.
static void ReadGuns(Signals& s, StanceInput& in, int& guns)
{
    Gun list[ShotDetector::kMax];
    int n = 0;
    float fastest = 0;
    guns = 0;
    for (const space::Weapon& w : space::PlayerWeapons())
    {
        if (n >= ShotDetector::kMax)
            break;
        Gun& g = list[n++];
        g.id = w.subObj, g.refire = w.refire, g.elapsed = w.elapsed, g.damage = w.hullDamage, g.ammo = w.ammo, g.homing = w.homing;
        // HDE's torpedoes sit on their own hardpoints; a disruptor there is no torpedo (docs\FREELANCER_RE.md "Weapons")
        g.torpedo = w.homing && !w.cruiseDisruptor && (strstr(w.hardpoint, "orpedo") != nullptr || w.blastRadius >= 20);
        if (!w.homing)
        {
            ++guns;
            if (w.refire > 0 && (fastest <= 0 || w.refire < fastest))
                fastest = w.refire;
        }
    }
    s.shots = g_engine.ShotReader().Step(list, n);
    in.refire = fastest;
}

static void Read30Hz(Signals& s)
{
    flight::Model m;
    if (flight::ReadModel(m))
    {
        s.cruiseEngaged = m.cruiseEngaged, s.chargeTime = m.cruiseChargeTime, s.chargeElapsed = m.cruiseChargeElapsed;
        g_hasBurner = m.hasBurner;
    }
    s.disrupted = hudinfo::CruiseDisrupted(0.5f);
    s.gauge = hudinfo::ThrustGauge();
    const int cm = space::PlayerCountermeasures(), mines = space::PlayerMines();
    s.countermeasure = g_cmWas >= 0 && cm >= 0 && cm < g_cmWas;
    s.mine = g_mineWas >= 0 && mines >= 0 && mines < g_mineWas;
    g_cmWas = cm, g_mineWas = mines;
}

static void Read10Hz(Signals& s, StanceInput& in, int guns)
{
    // the set piece running
    s.piece = SetPiece::None, s.ringsLeft = -1, s.jumpLeft = -1;
    const int exclusion = space::PlayerControlExclusion();
    hudinfo::LaneRun lane;
    hudinfo::Jump jump;
    hudinfo::Dock dock;
    if (hudinfo::Lane(lane))
        s.piece = SetPiece::Lane, s.ringsLeft = lane.ringsLeft;
    else if (hudinfo::Jumping(jump))
        s.piece = g_towardHole ? SetPiece::Hole : SetPiece::Jump, s.jumpLeft = jump.secondsLeft;
    else
    {
        const std::wstring cls = hudinfo::ClassName(fl::TargetId()); // what the ship is about to enter: the jump's own reader does not say
        if (cls == L"JUMP HOLE" || cls == L"JUMP GATE")
            g_towardHole = cls == L"JUMP HOLE";
        if (hudinfo::Docking(dock) && dock.state == 4)
            s.piece = SetPiece::Dock;
        else if (exclusion == 6)
            s.piece = SetPiece::Launch;
    }
    in.excluded = exclusion > 0;
    // can the guns fire: the game's own verdict at a point far ahead
    fl::Kinematics k = {};
    const bool kin = fl::PlayerKinematics(k);
    int codes[ShotDetector::kMax];
    int n = 0;
    if (kin)
    {
        // the ship's nose is -z of its back axis, the third column
        const float point[3] = {k.pos[0] - k.rot[2] * 3000, k.pos[1] - k.rot[5] * 3000, k.pos[2] - k.rot[8] * 3000};
        space::GunFire fire[ShotDetector::kMax];
        n = space::GunFireAt(point, fire, ShotDetector::kMax);
        for (int i = 0; i < n; ++i)
            codes[i] = fire[i].fire;
    }
    s.fire = FireStateOf(codes, n, guns, in.excluded || s.cruiseEngaged, s.power, s.fire);
    // the nearest missile at the player
    s.missileDistance = -1;
    for (const space::Missile& m : space::Missiles())
        if (m.targetsPlayer)
        {
            s.missileDistance = m.distance, s.missileClosing = m.closing;
            s.missilePan = kin ? PanFrom(m.pos, k) : 0;
            break;
        }
    // the space around the ship
    s.radiation = exe::RadiationZone.ok() && Mem<u8>(exe::RadiationZone) != 0;
    const int field = kin ? space::FieldAt(k.pos) : 0;
    s.field = field == 1 ? Field::Asteroids : field == 2 ? Field::Nebula : Field::None;
    s.tractor = hudinfo::TractorActive();
}

// The player ship's shield and hull now (shieldcue.cpp's reads); invalid out of space.
static bool ReadVitals(const Frame& f, Vitals& v, float& power)
{
    v = Vitals();
    ai::Self s;
    if ((f.where != Where::Flight && f.where != Where::Cutscene) || !ai::ReadSelf(s) || s.hullMax <= 0)
        return false;
    v.valid = true;
    v.ship = space::PlayerShipId();
    float cur = 0, max = 0;
    bool up = false;
    if (s.shieldMax > 0 && !space::ShieldOf(v.ship, cur, max, up))
        cur = s.shield * s.shieldMax, max = s.shieldMax, up = s.shield > 0;
    v.shield = cur, v.shieldMax = max, v.shieldUp = up && s.shieldMax > 0;
    v.hull = s.hull * s.hullMax, v.hullMax = s.hullMax;
    power = s.power;
    return true;
}

static void ReadSignals(const Frame& f, double now)
{
    Signals& s = g_s;
    ReadHit(s);
    float power = -1;
    s.ship = ReadVitals(f, s.vitals, power);
    if (!s.ship)
    {
        const HitReport hit = s.hit;
        s = Signals();
        s.hit = hit;
        g_engine.ShotReader().Forget();
        g_cmWas = g_mineWas = -1;
        g_in.excluded = false;
        return;
    }
    s.power = power;
    s.throttle = fl::GetThrottle();
    s.burner = hudinfo::AfterburnerOn();
    s.engineKilled = hudinfo::EngineKilled();
    s.fireHeld = f.fireHeld, s.firePressed = f.firePressed, s.launcherPressed = f.launcherPressed;
    int mask = (f.role[0] == Role::Fire ? 1 : 0) | (f.role[1] == Role::Fire ? 2 : 0);
    s.fireMask = f.autopilot || !mask ? 4 : mask;
    int guns = 0;
    ReadGuns(s, g_in, guns);
    s.mine = s.countermeasure = false;
    if (now >= g_next30)
        g_next30 = now + 1.0 / 30, Read30Hz(s);
    if (now >= g_next10)
        g_next10 = now + 0.1, Read10Hz(s, g_in, guns);
    const u32 arch = hudinfo::EngineArch();
    if (arch != g_engineArch)
        g_engineArch = arch, g_engineData = EngineRows(arch);
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

static void ReadGates(const Frame& f, const Gamepad& pad)
{
    const uiopt::UiOptions& o = uiopt::Get();
    g_gates.rumble = o.rumble;
    g_gates.enabled = f.enabled;
    g_gates.focus = !pad.ignoreReal;
    g_gates.pad = device::Pad();
    g_gates.paused = f.paused;
    g_gates.where = f.where;
    for (int c = 0; c < kClassCount; ++c)
        g_gates.cls[c] = o.hapticsClass[c];
}

void Update(const Frame& f, Gamepad& pad)
{
    ReadGates(f, pad);
    const uiopt::UiOptions& o = uiopt::Get();
    pad.impulseWanted = o.triggerEffects > 0 && o.hapticsDevice != 0;
    // the switch on the record, so that a test can show it before any effect fires
    static int deviceWas = -1;
    const int device = o.hapticsDevice != 0 && EnvVar("FLVIBED_BACKGROUND") != "1" ? 1 : 0;
    if (device != deviceWas)
        Log("haptics: output to a real pad %s (haptics_device %d, background %d)", device ? "ON" : "OFF", o.hapticsDevice, EnvVar("FLVIBED_BACKGROUND") == "1" ? 1 : 0), deviceWas = device;
    // closed: nothing is read. A pad that left, or a hand on the keyboard, gives everything back once.
    const bool wanted = f.enabled && g_gates.pad && pad.Cur().connected && (o.rumble > 0 || o.triggerEffects > 0);
    if (!wanted)
    {
        if (g_open)
        {
            PadSink sink(pad, true);
            g_engine.Stop();
            g_render.Stop(sink);
            pad.Wave(false);
            g_s = Signals();
            g_open = false;
        }
        g_apWas = f.autopilot;
        return;
    }
    g_open = true;
    const double now = Now();
    const float dt = g_last > -1e8 && now > g_last ? static_cast<float>(now - g_last) : 0.0f;
    g_last = now;
    ReadSignals(f, now);

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

    const HapticFrame frame = g_engine.Step(g_s, g_gates, g_engineData, now);
    for (int i = 0; i < g_engine.FiredCount(); ++i)
    {
        const Engine::FiredOne& e = g_engine.Fired(i);
        g_hitEvents += e.effect == kHitShield || e.effect == kHitHull;
        g_failEvents += e.effect == kShieldFail;
        if (e.effect != kHandBack && e.effect != kConfirm)
            Tell(e.effect, e.level, e.result, "game");
    }

    StanceInput& in = g_in;
    in.open = f.where == Where::Flight && !f.window && !f.paused && !f.autopilot && g_gates.focus && g_s.ship;
    in.role[0] = f.role[0], in.role[1] = f.role[1];
    in.held[0] = f.held[0], in.held[1] = f.held[1];
    in.fire = g_s.fire, in.power = g_s.power, in.gauge = g_s.gauge;
    in.burner = g_s.burner;
    in.hasBurner = g_hasBurner && !in.excluded && !g_s.cruiseEngaged;
    in.effects = o.triggerEffects, in.strength = o.triggerStrength;
    // the grips' stream: asked of the host only for a real DualSense on USB in a foreground game, with the mode for it
    const haptics::PadFacts facts = pad.Facts();
    static const bool background = EnvVar("FLVIBED_BACKGROUND") == "1";
    pad.Wave(wavesynth::WantWave(o.hapticsMode) && facts.sdl && facts.type == padbackend::kTypePs5 && pad.OutputIsReal() &&
             !wavesynth::WaveOpenRefusal(o.hapticsDevice != 0, background, pad.Wired()));
    g_waveWhy = pad.WaveWhy();
    g_caps = CapsFor(pad.Facts());
    PadSink sink(pad, false);
    if (g_record)
        g_record->SetTime(now);
    if (pad.PadSerial() != g_padSerial) // another pad: it holds nothing of what the last one was sent
        g_padSerial = pad.PadSerial(), g_render.PadChanged();
    g_render.Send(frame, in, g_caps, now, dt, sink);
}

void Stop(Gamepad& pad)
{
    PadSink sink(pad, true);
    g_engine.Stop();
    g_render.Stop(sink);
    pad.Wave(false);
    delete g_record;
    g_record = nullptr;
    g_open = false;
}

std::string Status()
{
    std::ostringstream o;
    const Mixer& mix = g_engine.Mix();
    const Out m = mix.Last(), s = g_render.MotorState().LastSent();
    o.setf(std::ios::fixed);
    o.precision(3);
    o << "haptics rumble=" << g_gates.rumble << " world=" << (*Silence(g_gates, kWorld) ? Silence(g_gates, kWorld) : "open")
      << " assist=" << (*Silence(g_gates, kAssist) ? Silence(g_gates, kAssist) : "open")
      << " prompt=" << (*Silence(g_gates, kPrompt) ? Silence(g_gates, kPrompt) : "open") << " mix=(" << m.low << ","
      << m.high << ") sent=(" << s.low << "," << s.high << ") on=" << g_render.MotorState().On() << " sends=" << g_render.MotorState().Sent()
      << " playing=" << mix.Playing() << " duty=" << mix.Duty();
    for (int r = Mixer::kStarted; r <= Mixer::kIgnored; ++r)
        o << " " << ResultName(static_cast<Mixer::Result>(r)) << "=" << mix.Counted(static_cast<Mixer::Result>(r));
    o << " game_hits=" << g_hitEvents << " game_fails=" << g_failEvents << " handbacks=" << g_handBacks
      << " popups=" << g_popups << " last=" << g_lastFired;
    o << " open=" << g_open << " caps=" << (g_caps.motors ? "motors" : "none") << (g_caps.motorsAreLR ? "+lr" : "") << (g_caps.triggerMotors ? "+impulse" : "")
      << (g_caps.adaptive ? "+adaptive" : "") << (g_caps.waveform ? "+waveform" : "") << " stance=" << g_render.StanceOn() << " stance_sends=" << g_render.StanceSends();
    if (*g_waveWhy)
        o << " wave_why='" << g_waveWhy << "'";
    return o.str();
}

static std::string RecordDump()
{
    if (!g_record)
        return "haptics record: off";
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(3);
    for (int i = 0; i < g_record->Count(); ++i)
    {
        const RecordingSink::Entry& e = g_record->At(i);
        o << e.at;
        if (e.kind == RecordingSink::kMotors)
            o << " motors " << e.a << " " << e.b << "\n";
        else if (e.kind == RecordingSink::kTriggerMotors)
            o << " trigger_motors " << e.a << " " << e.b << "\n";
        else
        {
            char hex[80] = {};
            int at = 0;
            for (int b = 0; b < kEffectBytes; ++b)
                at += snprintf(hex + at, sizeof(hex) - at, "%02X", e.left[b]);
            at += snprintf(hex + at, sizeof(hex) - at, " ");
            for (int b = 0; b < kEffectBytes; ++b)
                at += snprintf(hex + at, sizeof(hex) - at, "%02X", e.right[b]);
            o << " stance " << hex << "\n";
        }
    }
    o << "wave frames " << g_record->Count(RecordingSink::kWave) << " peak " << g_record->WavePeak() << "\n";
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
            o << d.name << " cat=" << (d.cat == kWorld ? "world" : d.cat == kAssist ? "assist" : d.cat == kPrompt ? "prompt" : "setpiece") << " low=" << d.low
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
        g_engine.Mix().Stop();
        return Status();
    }
    if (what == "signals") // TEST: the snapshot the engine reads, every reader's raw value
    {
        const Signals& s = g_s;
        fl::Kinematics k = {};
        fl::PlayerKinematics(k);
        char line[520];
        snprintf(line, sizeof(line),
                 "signals ship=%d throttle=%.3f burner=%d killed=%d gauge=%.4f power=%.3f engaged=%d charge=%.2f/%.2f disrupted=%d piece=%d rings=%d jump=%.2f tractor=%d field=%d "
                 "radiation=%d missile=%.0f pan=%.2f fire=%d excluded=%d refire=%.3f hit_seq=%u hit_kind=%u hit_pan=%.2f hit_dist=%.0f hull=%.0f/%.0f held=%d/%d open=%d "
                 "pos=%.0f,%.0f,%.0f nose=%.3f,%.3f,%.3f",
                 s.ship, s.throttle, s.burner, s.engineKilled, s.gauge, s.power, s.cruiseEngaged, s.chargeElapsed, s.chargeTime, s.disrupted, static_cast<int>(s.piece), s.ringsLeft,
                 s.jumpLeft, s.tractor, static_cast<int>(s.field), s.radiation, s.missileDistance, s.missilePan, static_cast<int>(s.fire), g_in.excluded, g_in.refire, s.hit.seq,
                 s.hit.kind, s.hit.pan, s.hit.distance, s.vitals.hull, s.vitals.hullMax, g_in.held[0], g_in.held[1], g_in.open, k.pos[0], k.pos[1], k.pos[2], -k.rot[2], -k.rot[5], -k.rot[8]);
        return line;
    }
    if (what == "stall") // TEST: the game thread stops for that long: the SDL thread's lease (host\sdlpad.cpp) must put the pad to rest by itself
    {
        int ms = 1500;
        in >> ms;
        Sleep(static_cast<DWORD>(ms < 0 ? 0 : ms > 5000 ? 5000 : ms));
        return "haptics stall: done";
    }
    if (what == "record") // TEST: what would reach the pad, in order, with `haptics_device` 0 or not
    {
        std::string op;
        in >> op;
        if (op == "on")
        {
            if (!g_record)
                g_record = new RecordingSink();
            g_record->Clear();
            return "haptics record: on";
        }
        if (op == "off")
        {
            delete g_record;
            g_record = nullptr;
            return "haptics record: off";
        }
        return RecordDump();
    }
    return "usage: haptics [status] | table | play <effect> [level 0..1] | stop | signals | stall <ms> | record on|off|dump";
}
} // namespace haptics
