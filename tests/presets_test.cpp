// The pad's binding tables (modules\controller\src\impl\presets.cpp) come straight from the spec's generated tables
// (core\ui_meta.h kBind_*, since 29 Oct 2026; a hand transcription before): every button and action name of the
// flight presets and the autopilot's flight_ap set resolves (an unknown one would leave a slot unbound), and the
// bindings the player's muscle memory hangs on stay where the spec puts them.
#include "ui/presets.h"

#include "test.h"
#include "ui_meta.h"

// glyphs::FromName / Name without the glyph atlas (glyphs.cpp draws: the game's renderer); the positional names are
// the enum's (modules\controller\src\impl\glyphs.h)
namespace glyphs
{
static const char* kTestNames[COUNT] = {"A",  "B",  "X",  "Y",  "LB", "RB", "LT", "RT", "LS",
                                        "RS", "L3", "R3", "DU", "DD", "DL", "DR", "VIEW", "MENU"};
Id FromName(const char* name)
{
    for (int i = 0; name && i < COUNT; ++i)
        if (_stricmp(kTestNames[i], name) == 0)
            return static_cast<Id>(i);
    return COUNT;
}
const char* Name(Id id) { return id >= 0 && id < COUNT ? kTestNames[id] : "?"; }
} // namespace glyphs

namespace
{
using presets::Action;

// every name in a spec table resolves to a button and to an action ("none" = nothing bound)
void CheckResolves(const uimeta::Bind* binds, int count)
{
    for (int i = 0; i < count; ++i)
    {
        CHECK(glyphs::FromName(binds[i].button) != glyphs::COUNT);
        for (const char* a : {binds[i].tap, binds[i].hold, binds[i].dbl})
            CHECK(_stricmp(a, "none") == 0 || presets::ActionFromName(a) != Action::None);
    }
}
} // namespace

void Test_presets()
{
    for (const uimeta::BindSet& s : uimeta::kBindingPresets)
        CheckResolves(s.binds, s.count);
    CheckResolves(uimeta::kBind_common_flight_ap,
                  static_cast<int>(sizeof(uimeta::kBind_common_flight_ap) / sizeof(uimeta::Bind)));

    const presets::Table& fl = presets::Flight(presets::FREELANCER);
    CHECK(fl.b[glyphs::A].tap == Action::Context && fl.b[glyphs::A].hold == Action::Formation);
    CHECK(fl.b[glyphs::RT].tap == Action::Fire && fl.b[glyphs::LT].tap == Action::Afterburner);
    CHECK(fl.b[glyphs::RB].tap == Action::Missile && fl.b[glyphs::RB].hold == Action::Torpedo);
    // every flight table reaches the torpedo: the hold of its missile button (GitHub issue 7)
    for (const presets::Table* t : {&presets::Flight(presets::FREELANCER), &presets::Flight(presets::ACE),
                                    &presets::Flight(presets::ASSISTED), &presets::FlightAp()})
    {
        glyphs::Id btn = glyphs::COUNT, missile = glyphs::COUNT;
        int slot = -1, missileSlot = -1;
        CHECK(presets::Find(*t, Action::Torpedo, btn, slot) && slot == 1);
        CHECK(presets::Find(*t, Action::Missile, missile, missileSlot) && missile == btn);
    }
    CHECK(fl.b[glyphs::R3].tap == Action::TargetSmart && fl.b[glyphs::R3].hold == Action::Rear &&
          fl.b[glyphs::R3].dbl == Action::ClearTarget);
    const presets::Table& ace = presets::Flight(presets::ACE);
    CHECK(ace.b[glyphs::RT].tap == Action::Throttle && ace.b[glyphs::RT].hold == Action::Afterburner);
    CHECK(ace.b[glyphs::LS].tap == Action::Steer);
    const presets::Table& as = presets::Flight(presets::ASSISTED);
    CHECK(as.b[glyphs::RS].tap == Action::Aim && as.b[glyphs::B].tap == Action::Handback);
    // View hold = the autopilot in every preset (UI_PLAN 6.4), and the autopilot's own set hands back on the sticks
    for (int p : {presets::FREELANCER, presets::ACE, presets::ASSISTED})
        CHECK(presets::Flight(p).b[glyphs::VIEW].hold == Action::Autopilot);
    const presets::Table& ap = presets::FlightAp();
    CHECK(ap.b[glyphs::A].tap == Action::Accept && ap.b[glyphs::X].hold == Action::Turret);
    // weapon_groups 2 under the autopilot: the triggers as in free flight (GitHub issue 31); no other setting moves anything
    for (int p : {presets::FREELANCER, presets::ASSISTED})
    {
        const presets::Table g = presets::EffectiveAp(p, 2), free = presets::Effective(p, 2);
        for (glyphs::Id b : {glyphs::RT, glyphs::LT, glyphs::L3})
            CHECK(g.b[b].tap == free.b[b].tap);
        CHECK(g.b[glyphs::RT].tap == Action::FireGroup1 && g.b[glyphs::LT].tap == Action::FireGroup2 && g.b[glyphs::L3].tap == Action::Afterburner);
        CHECK(g.b[glyphs::B].tap == Action::Handback && g.b[glyphs::A].tap == Action::Accept);
        for (int w : {0, 1})
            CHECK(presets::EffectiveAp(p, w).b[glyphs::LT].tap == ap.b[glyphs::LT].tap);
    }
    CHECK(presets::EffectiveAp(presets::ACE, 2).b[glyphs::B].tap == Action::Handback); // ACE's group buttons stay the autopilot's
    CHECK(ap.b[glyphs::LS].tap == Action::Handback && ap.b[glyphs::RS].tap == Action::Handback);
    // the layout option on top: simple drops Y hold's weapon group
    CHECK(presets::Effective(presets::FREELANCER, 0).b[glyphs::Y].hold == Action::None);
    // Y (GitHub issue 15): next group on the tap, cruise held; y_swap the old order; no cycling = cruise on the tap
    {
        const presets::Binding y = presets::Effective(presets::FREELANCER, 1).b[glyphs::Y];
        CHECK(y.tap == Action::WeaponGroup && y.hold == Action::Cruise);
        const presets::Binding old = presets::Effective(presets::FREELANCER, 1, true).b[glyphs::Y];
        CHECK(old.tap == Action::Cruise && old.hold == Action::WeaponGroup);
        for (int groups : {0, 2})
            for (bool swap : {false, true})
            {
                const presets::Binding b = presets::Effective(presets::FREELANCER, groups, swap).b[glyphs::Y];
                CHECK(b.tap == Action::Cruise && b.hold == Action::None);
            }
        CHECK(presets::Effective(presets::ACE, 1, true).b[glyphs::Y].tap == presets::Flight(presets::ACE).b[glyphs::Y].tap);
        CHECK(presets::Effective(presets::ASSISTED, 1, true).b[glyphs::Y].hold == Action::ActionWheel);
    }

    // a latch follows the game: it waits for the start, stands while the game runs it, ends with it, and a refused press ends too
    {
        presets::LatchWatch w;
        CHECK(!presets::LatchEnded(w, false, false, 1.0f)); // nothing latched: never
        CHECK(!presets::LatchEnded(w, true, false, 0.4f));  // the press, the game not there yet
        CHECK(!presets::LatchEnded(w, true, true, 0.1f));
        CHECK(!presets::LatchEnded(w, true, true, 100.0f)); // a long burn
        CHECK(!presets::LatchEnded(w, true, false, 0.05f)); // a short break
        CHECK(!presets::LatchEnded(w, true, true, 0.05f));
        CHECK(!presets::LatchEnded(w, true, false, 0.05f));
        CHECK(presets::LatchEnded(w, true, false, 0.05f));  // the game ended it
        CHECK(!presets::LatchEnded(w, true, false, 0.4f));  // a new latch starts clean
        CHECK(presets::LatchEnded(w, true, false, 0.1f));   // refused: never ran
        presets::Dispatcher d;
        d.toggles = true;
        presets::Table t = {};
        t.b[glyphs::L3].tap = Action::Afterburner;
        auto press = [&] {
            bool down[glyphs::COUNT] = {};
            std::vector<presets::Event> ev;
            down[glyphs::L3] = true;
            d.Update(1.0f / 60, t, down, ev);
            down[glyphs::L3] = false;
            d.Update(1.0f / 60, t, down, ev);
        };
        press();
        CHECK(d.Latched(Action::Afterburner) && d.Held(Action::Afterburner));
        d.Unlatch(Action::Afterburner);
        CHECK(!d.Held(Action::Afterburner));
        press(); // one press starts it again: issue 29
        CHECK(d.Latched(Action::Afterburner));
    }

    // the dispatcher on FREELANCER's RB (missile, hold torpedo) and RT (fire, nothing else), 60 frames a second
    auto run = [&](glyphs::Id btn, int downFrames, int totalFrames) {
        presets::Dispatcher d;
        std::vector<presets::Event> all;
        for (int f = 0; f < totalFrames; ++f)
        {
            bool down[glyphs::COUNT] = {};
            down[btn] = f < downFrames;
            std::vector<presets::Event> ev;
            d.Update(1.0f / 60, fl, down, ev);
            for (presets::Event& e : ev)
                all.push_back(e);
        }
        return all;
    };
    auto count = [](const std::vector<presets::Event>& ev, Action a, presets::Phase p) {
        int n = 0;
        for (const presets::Event& e : ev)
            n += e.action == a && e.phase == p;
        return n;
    };
    // a short press (0.1 s): one missile, as a short hold that ends on its own; no torpedo
    auto tap = run(glyphs::RB, 6, 60);
    CHECK(count(tap, Action::Missile, presets::Phase::HoldStart) == 1 && count(tap, Action::Missile, presets::Phase::HoldEnd) == 1);
    CHECK(count(tap, Action::Torpedo, presets::Phase::HoldStart) == 0);
    // a hold (0.8 s): the torpedo alone - no missile on the press (until 29 Oct 2026 every torpedo cost a missile)
    auto hold = run(glyphs::RB, 48, 80);
    CHECK(count(hold, Action::Torpedo, presets::Phase::HoldStart) == 1 && count(hold, Action::Torpedo, presets::Phase::HoldEnd) == 1);
    CHECK(count(hold, Action::Missile, presets::Phase::HoldStart) == 0);
    // a button with no hold action still acts on the press (RT fire starts at once, held to the release)
    auto fire = run(glyphs::RT, 30, 40);
    CHECK(!fire.empty() && fire[0].action == Action::Fire && fire[0].phase == presets::Phase::HoldStart);
    CHECK(count(fire, Action::Fire, presets::Phase::HoldEnd) == 1);
}
