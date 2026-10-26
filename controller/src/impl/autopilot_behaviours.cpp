// Advanced Autopilot - behaviours: tactics, the game's behaviours on the player ship. Part of autopilot.cpp until 21 Oct 2026; shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"

namespace autopilot
{
// ------------------------------------------------------------------ behaviours
void SetTactic(Tactic t)
{
    if (g.tactic == t)
        return;
    // Loop guard: more than 8 changes within a second is a flip-flop (a bug): report it once, mute the per-change
    // events until it calms down.
    if (g.time - g.tacticWindow > 1.0f)
    {
        g.tacticWindow = g.time;
        g.tacticChanges = 0;
        g.tacticLoop = false;
    }
    if (++g.tacticChanges > 8 && !g.tacticLoop)
    {
        g.tacticLoop = true;
        events::Push("autopilot tactic-loop %s->%s (more than 8 changes in 1 s)", TacticName(g.tactic), TacticName(t));
        Log("autopilot: tactic loop %s -> %s", TacticName(g.tactic), TacticName(t));
    }
    g.tactic = t;
    g.tacticTime = 0;
    g.issueTimer = 0;
    if (t == Tactic::Flee)
    {
        g.combat.retreat = RetreatPhase::Break; // every flee starts by breaking contact
        g.combat.retreatPhaseTime = 0;
        g.combat.lastNearest = -1;
        g.combat.nearestRate = 0;
    }
    if (!g.tacticLoop)
        events::Push("autopilot tactic=%s", TacticName(t));
}

int Op()
{
    ai::State s;
    return ai::ReadState(s) ? s.op : -1;
}

void Trail(u32 target)
{
    ai::Directive d;
    d.op = ai::OpTrail;
    d.target = target;
    d.range = Pf(g_p.Trail.trail_distance, 30, 2000, 150);
    d.client = true;
    SubmitNow(d);
}

void Follow(u32 target)
{
    ai::Directive d;
    d.op = ai::OpFollow;
    d.target = target;
    d.range = g_set.followDistance;
    memcpy(d.offset, g_set.followOffset, sizeof(d.offset));
    SubmitNow(d); // the server's follow (verified on the player ship)
}

// The game's Evade behaviour away from `threat` (needs the host's guard, ai::SetEvadeGuard). False when refused.
bool GameEvade(u32 threat)
{
    ai::Directive d;
    d.op = ai::OpEvade;
    d.target = threat;
    d.client = true;
    return SubmitNow(d) == 1;
}

// Dodge direction by the personality's evade_dodge_direction_weight (left, right, up, down); all zero = any.
int DodgeDirection()
{
    const float* w = g_p.EvadeDodge.evade_dodge_direction_weight;
    float sum = 0;
    for (int i = 0; i < 4; ++i)
        sum += std::max(0.0f, w[i]);
    float r = (rand() / static_cast<float>(RAND_MAX)) * (sum > 0 ? sum : 4.0f);
    for (int i = 0; i < 4; ++i)
    {
        r -= sum > 0 ? std::max(0.0f, w[i]) : 1.0f;
        if (r <= 0)
            return i + 1;
    }
    return 1;
}

// Break off: the game's goto to a point beside and behind the attack axis, afterburner on (the NPC's break style
// "sideways" / "outrun").
void BreakAway(const aim::Target& t)
{
    fl::Kinematics k = {};
    if (!fl::PlayerKinematics(k))
        return;
    float away[3] = {k.pos[0] - t.world[0], k.pos[1] - t.world[1], k.pos[2] - t.world[2]};
    float len = geom::Len(away);
    if (len < 1)
    {
        away[0] = 1;
        len = 1;
    }
    const float side = (rand() & 1) ? 1.0f : -1.0f;
    ai::Directive d;
    d.op = ai::OpGoto;
    d.hasPos = true;
    // 1.5 km out along the attack axis, 800 m to one side (rotated in the horizontal plane).
    d.pos[0] = k.pos[0] + away[0] / len * 1500 + side * away[2] / len * 800;
    d.pos[1] = k.pos[1] + away[1] / len * 1500;
    d.pos[2] = k.pos[2] + away[2] / len * 1500 - side * away[0] / len * 800;
    d.range = 100;
    d.cruise = false;
    SubmitNow(d);
}

} // namespace autopilot
