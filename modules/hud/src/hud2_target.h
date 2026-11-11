// HUD 2's targeting display (modules\hud\docs\UI_HUD2_SKIN.md 6.10): what stands at the target in place of the game's bracket, label and bars. A new target opens
// as a mini card beside the ship, settles after 2.0 s into the form the ship wears, and takes the shoulder seat inside the ring. Pure: no game call, so
// tests\hud2_target_test.cpp runs the same code; hud2_target.cpp draws it. Fractions of H x k unless named. The side-change and edge rules wait for a clip on real
// motion: their numbers are starting values, kept here together.
#pragma once
#include <cmath>

namespace hud2target
{
constexpr float kCardSec = 2.0f, kMorphSec = 0.35f;     // the card stands, then the change
constexpr float kMarkFloor = 0.030f, kMarkCap = 0.10f;  // marks follow the object's on-screen size, so the block stays on screen on a capital
constexpr float kSeatEnter = 0.03f, kSeatLeave = 0.09f; // the ring's radius plus: inside, the shoulder seat (the research's start; probe T8 sets them)
constexpr float kRingTop = 0.055f;                      // the ring's cue row ends this far above the ring; the seat stands on it, centred (the owner, 6 Nov 2026: the shoulder read as diagonal)
constexpr float kSideDead = 0.06f, kSideHoldSec = 0.8f; // the seat changes side only past the middle by this, and not again this soon (motion p2l: two 688 px flips in 0.48 s at 0.02)
constexpr float kSeatHoldSec = 0.60f;                   // the seat stands this long between changes: at 0.30 a target crossing the ring fast held the block at the floor (the second clip: 8 of 39)
constexpr float kAboveHoldSec = 0.50f;                  // the block above the ship or under it, held as long
constexpr float kJump = 0.03f;                          // the wanted place moving this much in a frame, against what it hangs on, is a jump
constexpr float kMoveSec = 0.20f;                       // a jump of the block (seat, side, out of a card's way) travels, never cuts (the owner: steps read choppy)
constexpr float kYieldSec = 0.15f;                      // the words fading where nothing leaves them room
constexpr float kAfterHitSec = 3.0f, kAfterHitFade = 0.4f;
constexpr float kTrailHoldSec = 0.35f, kTrailTau = 0.25f, kTrailFloor = 0.3f; // the white trail: held after the last hit, then drained (a 20 % hit is gone in about 0.6 s)
constexpr float kEdgeWaitSec = 0.5f;                    // leaving the screen, the words wait by the arrow, then fade
constexpr float kLockHz = 1.0f, kWaypointHz = 0.5f;     // the lock's breath on the cue, the waypoint diamond's
constexpr float kDiamondHalf = 0.027f, kDiamondLine = 0.0026f, kDiamondClear = 0.008f; // the waypoint diamond: half across, its line, its point above the object
constexpr float kGap = 0.012f, kBarW = 0.12f, kBarH = 0.007f, kBarGap = 0.005f, kCardW = 0.20f, kCardH = 0.098f;
// The target's carets: four, one on each diagonal of its mark, each pointing at the centre (the owner, 7 Nov 2026: "very hard to have eyes on your target when
// they're inside the reticle. The single marker on top of them is not good enough" - like the game's brackets, but triangles pointing in). A caret's point stands
// kQuadGap off the mark's corner along the diagonal, its body kQuadLen x its size long, its base 2 x its size across.
constexpr float kQuadGap = 0.004f, kQuadLen = 1.6f;
// A caret's size (its base's half): with the object's size, a mark from 0.008 to 0.012 H (a ship's radius reaches the cap at 300 m)
inline float CaretSize(float mark, float H, float k)
{
    const float lo = 0.008f * H * k, hi = 0.012f * H * k, v = mark * 0.14f;
    return v < lo ? lo : v > hi ? hi : v;
}
// How far the carets reach past the mark's half, on each axis: the base's outer corner on the diagonal
inline float QuadReach(float mark, float H, float k) { return 0.70711f * (kQuadGap * H * k + (kQuadLen + 1.0f) * CaretSize(mark, H, k)); }

struct Box
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
inline bool Touch(const Box& a, const Box& b) { return a.x0 < b.x1 && a.x1 > b.x0 && a.y0 < b.y1 && a.y1 > b.y0; }

enum Attitude : int
{
    Hostile,
    Neutral,
    Friendly
};
// skin_motion: 0 full, 1 fades, 2 none
enum Motion : int
{
    MotionFull,
    MotionFades,
    MotionNone
};

// The marks' size from the object's on-screen radius (pixels), floored and capped. Pixels.
inline float MarkSize(float radiusPx, float H, float k)
{
    const float d = 2 * radiusPx, lo = kMarkFloor * H * k, hi = kMarkCap * H * k;
    return d < lo ? lo : d > hi ? hi : d;
}

struct In
{
    float dt = 0;
    unsigned id = 0;       // the target, 0 none
    bool onScreen = false; // it projects in front of the camera, inside the screen
    float x = 0, y = 0;    // its centre, pixels
    float r = 0;           // its on-screen radius, pixels
    float W = 0, H = 0, k = 1, R = 0; // the screen, the HUD's scale, the ring's radius (pixels)
    float shield = -1, hull = -1;     // 0..1, under 0: nothing to damage
    int motion = MotionFull;
    Box ring;      // the ring and its cue row, while the form stands off the seat
    Box panels[16]; // the HUD's reserved areas (hud2_layout.h Reserved: panels, prompts, alert cards, subtitles): a mini card that would touch one does not stand, the block keeps off them
    int nPanels = 0;
    bool hasRing = false;
    bool ringGone = false; // the ring has faded: no seat, the words stay with the target (the owner, 6 Nov 2026)
    bool cueRow = true;    // the weapon group's row shows above the ring: the seat stands over it, else just over the ring
    float cardW = 0; // the mini card's width for its words (pixels), 0 the spec's
    float wordsW = 0; // the settled words' width (pixels)
};

struct Out
{
    bool show = false;    // anything stands at the target
    float settle = 0;     // 0 the mini card .. 1 the settled form
    float classA = 0;     // the card's class line, which fades first
    float plateA = 0;     // the card's plate
    float wordsA = 0;     // bars and words; the words wait by the arrow off screen
    bool seat = false;    // the shoulder seat
    int side = 1;         // the card and the seat stand on this side (-1 left, +1 right)
    float mark = 0;       // the marks' size, pixels
    float caretX = 0, caretY = 0;  // the attitude caret
    float blockX = 0, blockY = 0;  // the block's centre top: bars, then words under them
    float cardX0 = 0, cardY0 = 0;  // the mini card's top left
    float hitShieldA = 0, hitHullA = 0; // the numbers beside each bar after a hit
    float lostShield = 0, lostHull = 0; // the white pips: what was just lost, 0..1 of the bar, starting at the value
};

class State
{
public:
    Out Step(const In& in)
    {
        Out o;
        const float H = in.H, k = in.k, cx = in.W / 2, cy = in.H / 2;
        if (in.id != id_)
        {
            id_ = in.id, age_ = 0, seat_ = false, seatAge_ = kSeatHoldSec, offAge_ = 0, hadBlock_ = false, moveT_ = 1, yield_ = 0, sideAge_ = kSideHoldSec;
            opened_ = false, above_ = false, aboveAge_ = kAboveHoldSec, hadCard_ = false, cardT_ = 1;
            hitS_ = hitH_ = 1e9f, wasS_ = in.shield, wasH_ = in.hull, lostS_ = lostH_ = 0;
        }
        if (!in.id)
            return o;
        // after a hit: the numbers for 3 s, then a fade; another hit starts it again
        Hit(in.shield, wasS_, hitS_, lostS_, in.dt, in.motion == MotionNone);
        Hit(in.hull, wasH_, hitH_, lostH_, in.dt, in.motion == MotionNone);
        o.hitShieldA = HitAlpha(hitS_), o.hitHullA = HitAlpha(hitH_);
        o.lostShield = lostS_, o.lostHull = lostH_; // (its own clock: the trail, not the numbers' 3 s)
        const float gap = kGap * H * k, bars = kBarW * H * k;
        const float halfW = (in.wordsW > bars ? in.wordsW : bars) / 2 + gap, blockH = BlockH(H, k); // (the words are wider than the bars on a long name)
        auto hits = [&](float x, float y, const Box* list, int n)
        {
            const Box b = {x - halfW, y, x + halfW, y + blockH};
            for (int i = 0; i < n; ++i)
                if (Touch(b, list[i]))
                    return true;
            return false;
        };
        // the words fade where they meet a panel, and come back where they leave it (kYieldSec); `hit` = where they stand now
        auto yieldStep = [&](bool hit)
        {
            yield_ = !hadBlock_ ? (hit ? 1.0f : 0.0f) : Unit(yield_ + (hit ? 1 : -1) * in.dt / kYieldSec); // (a block opening on a panel opens faded: it stood there whole for 0.09 s)
            return in.motion == MotionNone ? (hit ? 0.0f : 1.0f) : 1 - yield_;
        };
        if (!in.onScreen)
        {
            offAge_ += in.dt;
            if (!hadBlock_) // (never on screen: the game's arrow and the top card say it)
                return o;
            o.show = offAge_ < kEdgeWaitSec + 0.25f;
            o.wordsA = offAge_ < kEdgeWaitSec ? 1.0f : 1 - (offAge_ - kEdgeWaitSec) / 0.25f;
            o.wordsA = o.wordsA < 0 ? 0 : o.wordsA;
            // (waiting where it stood: on a panel it stays faded - GitHub issue 77, a block cut whole over the weapons)
            const bool on = hits(shownX_, shownY_, in.panels, in.nPanels);
            const float kept = yieldStep(on);
            o.wordsA = on ? 0.0f : o.wordsA < kept ? o.wordsA : kept;
            o.settle = 1, o.mark = lastMark_, o.seat = seat_, o.side = side_;
            o.caretX = o.caretY = -1; // (no caret: the game's arrow says where)
            o.blockX = shownX_, o.blockY = shownY_; // (where it stood, fading: a dying ship's block cut 454 px to a clamped place)
            return o;
        }
        const bool back = offAge_ > 0 && hadBlock_;
        offAge_ = 0;
        age_ += in.dt; // (the card's 2.0 s are seen ones)
        seatAge_ += in.dt, sideAge_ += in.dt, aboveAge_ += in.dt;
        o.show = true;
        o.mark = MarkSize(in.r, H, k);
        lastMark_ = o.mark, lastX_ = in.x, lastY_ = in.y;
        const float hm = o.mark / 2 + QuadReach(o.mark, H, k); // (the mark with its carets: what stands round it keeps off them)
        const float d = sqrtf((in.x - cx) * (in.x - cx) + (in.y - cy) * (in.y - cy));
        const float dead = kSideDead * H * k;
        // The seat and side stand as they opened until the change starts: the mini card is beside the ship, and a card that would touch the ring or a panel goes (below).
        // A target inside the ring opens at the seat. Then the seat, by distance with hysteresis and a hold, and the side, past the middle
        if (!opened_)
        {
            opened_ = true;
            seat_ = !in.ringGone && d <= in.R + kSeatEnter * H * k;
            side_ = cardSide_ = in.x >= cx ? 1 : -1;
        }
        else if (age_ >= kCardSec)
        {
            const bool seatNow = !in.ringGone && (seat_ ? d <= in.R + kSeatLeave * H * k : d <= in.R + kSeatEnter * H * k);
            if (seatNow != seat_ && seatAge_ >= kSeatHoldSec)
                seat_ = seatNow, seatAge_ = 0;
            if (sideAge_ >= kSideHoldSec && ((side_ > 0 && in.x < cx - dead) || (side_ < 0 && in.x > cx + dead)))
                side_ = -side_, sideAge_ = 0;
        }
        o.seat = seat_, o.side = side_;
        o.caretX = Clamp(in.x, gap, in.W - gap), o.caretY = in.y - hm - gap < gap ? gap : in.y - hm - gap; // (whole at the screen's edge)
        // settled: under the ship; the seat: on top of the reticle, over the cue's row
        float bx = in.x, by = in.y + hm + gap;
        if (seat_)
            bx = cx, by = cy - in.R - (in.cueRow ? kRingTop : kGap) * H * k - blockH;
        // the block stays on screen
        o.blockX = Clamp(bx, halfW, in.W - halfW), o.blockY = Clamp(by, gap, in.H - blockH - gap);
        // in the seat, what stands there (an alert over the ring) moves the block up over it: it takes the next free place, it does not fade out
        for (int i = 0; seat_ && i < in.nPanels; ++i)
            if (hits(o.blockX, o.blockY, &in.panels[i], 1))
                o.blockY = Clamp(in.panels[i].y0 - blockH - gap, gap, in.H - blockH - gap);
        // off the seat, the block keeps off the ring: beside it the block stands outside it, which is continuous; over it, above the ship, held so it does not hop. The same
        // for the alert cards; where one leaves no room the words go, and the top card still names the target
        if (!seat_ && in.hasRing && in.y > in.ring.y0 && in.y < in.ring.y1)
            o.blockX = in.x < cx ? (o.blockX + halfW > in.ring.x0 ? Clamp(in.ring.x0 - halfW, halfW, in.W - halfW) : o.blockX)
                                 : (o.blockX - halfW < in.ring.x1 ? Clamp(in.ring.x1 + halfW, halfW, in.W - halfW) : o.blockX);
        const float aboveY = Clamp(in.y - hm - 2 * gap - blockH - kMarkFloor * H * k * 0.5f, gap, in.H - blockH - gap);
        if (!seat_)
        {
            const bool want = hits(o.blockX, o.blockY, in.panels, in.nPanels) || (in.hasRing && hits(o.blockX, o.blockY, &in.ring, 1) && !hits(o.blockX, aboveY, &in.ring, 1));
            if (want != above_ && aboveAge_ >= kAboveHoldSec)
                above_ = want, aboveAge_ = 0;
            if (above_)
                o.blockY = aboveY;
        }
        else
            above_ = false;
        const bool yieldWanted = hits(o.blockX, o.blockY, in.panels, in.nPanels);
        // any jump travels, whatever made it: the wanted place against what it hangs on, the ship off the seat, the screen on it. A list of causes missed two (the fourth clip)
        const float relX = o.blockX - (seat_ ? 0 : in.x), relY = o.blockY - (seat_ ? 0 : in.y);
        const bool jump = fabsf(relX - relX_) + fabsf(relY - relY_) > kJump * H * k;
        if (hadBlock_ && in.motion == MotionFull && (back || jump || seat_ != wasSeat_))
            offX_ = shownX_ - o.blockX, offY_ = shownY_ - o.blockY, moveT_ = 0;
        relX_ = relX, relY_ = relY, wasSeat_ = seat_;
        moveT_ = Unit(moveT_ + in.dt / kMoveSec);
        const float rest = in.motion == MotionFull ? 1 - Smooth(moveT_) : 0;
        o.blockX += offX_ * rest, o.blockY += offY_ * rest;
        // the keep-off where the block is drawn as well as where it goes (GitHub issue 77): travelling off a panel it stays faded until it has left it - the
        // words came back at once while the block still crossed the weapons. And ahead of it: the drawn block's motion carried kYieldSec on, so the words have
        // faded by the time they reach an area; touching one they are gone - a target's words never stand visibly over a panel (the owner's rule; the lint's
        // part.over counts a word from 10 %: the fade had run over the cross's corner for its 0.15 s)
        const bool touch = hits(o.blockX, o.blockY, in.panels, in.nPanels);
        bool ahead = false;
        if (hadBlock_ && in.dt > 0)
        {
            const float frames = kYieldSec / in.dt + 1; // (the fade's frames and the one it starts in)
            const float ax = o.blockX + (o.blockX - shownX_) * frames, ay = o.blockY + (o.blockY - shownY_) * frames;
            const Box sweep = {(ax < o.blockX ? ax : o.blockX) - halfW, ay < o.blockY ? ay : o.blockY, (ax > o.blockX ? ax : o.blockX) + halfW,
                               (ay > o.blockY ? ay : o.blockY) + blockH};
            for (int i = 0; i < in.nPanels && !ahead; ++i)
                ahead = Touch(sweep, in.panels[i]);
        }
        o.wordsA = yieldStep(yieldWanted || touch || ahead);
        if (touch)
            yield_ = 1, o.wordsA = 0;
        shownX_ = o.blockX, shownY_ = o.blockY, hadBlock_ = true;
        // the card: beside the ship, on the side away from the ring; the other side only where the screen ends on it, and it travels there. Never over the ship
        const float cw = in.cardW > 0 ? in.cardW : kCardW * H * k, ch = kCardH * H * k;
        auto fits = [&](int side) { return side > 0 ? in.x + hm + gap + cw <= in.W - gap : in.x - hm - gap - cw >= gap; };
        const int was = cardSide_;
        if (!fits(cardSide_) && fits(-cardSide_))
            cardSide_ = -cardSide_;
        const float cardX = seat_ ? o.blockX - cw / 2 : cardSide_ > 0 ? in.x + hm + gap : in.x - hm - gap - cw;
        o.cardX0 = Clamp(cardX, gap, in.W - cw - gap);
        o.cardY0 = Clamp(seat_ ? o.blockY : in.y - ch / 2, gap, in.H - ch - gap);
        if (cardSide_ != was && hadCard_ && in.motion == MotionFull)
            cardOff_ = shownCardX_ - o.cardX0, cardT_ = 0;
        cardT_ = Unit(cardT_ + in.dt / kMoveSec);
        o.cardX0 += cardOff_ * (in.motion == MotionFull ? 1 - Smooth(cardT_) : 0);
        shownCardX_ = o.cardX0, hadCard_ = true;
        // no room for the card: the settled form at once at its opening, the change from there on (6.10 "Close ships"). A card worn beside a moving ship crosses what it meets
        // otherwise (three clips: the ring, GOALS, the weapons panel)
        if (age_ < kCardSec)
        {
            const Box card = {o.cardX0, o.cardY0, o.cardX0 + cw, o.cardY0 + ch};
            bool touch = in.hasRing && Touch(card, in.ring); // (in the seat too: a card there covered the ring's pips at 1280x800)
            for (int i = 0; i < in.nPanels && !touch; ++i)
                touch = Touch(card, in.panels[i]);
            if (touch) // (at once, also after its opening: the change from the card's place ran its 0.35 s over the panel - the lint's part.over, the cross)
                age_ = kCardSec + kMorphSec;
        }
        // the card, then the change
        const bool morph = in.motion == MotionFull;
        const float t = age_ - kCardSec;
        o.settle = t <= 0 ? 0 : !morph ? (in.motion == MotionNone || t >= kMorphSec ? 1.0f : t / kMorphSec) : t >= kMorphSec ? 1.0f : Smooth(t / kMorphSec);
        o.classA = 1 - Unit(o.settle * 4); // (gone before the bars reach its line: nothing crosses)
        o.plateA = 1 - o.settle;
        if (in.motion != MotionNone) // (in, not popped)
            o.plateA *= Unit(age_ / 0.12f);
        return o;
    }

    static float BlockH(float H, float k) { return (2 * (kBarH + kBarGap) + 0.030f) * H * k; }
    static float Clamp(float v, float lo, float hi) { return lo > hi ? (lo + hi) / 2 : v < lo ? lo : v > hi ? hi : v; }

private:
    static float Unit(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
    static float Smooth(float t) { return t * t * (3 - 2 * t); }
    static float HitAlpha(float since)
    {
        if (since >= kAfterHitSec + kAfterHitFade)
            return 0;
        return since <= kAfterHitSec ? 1.0f : 1 - (since - kAfterHitSec) / kAfterHitFade;
    }
    // The white "just lost" part of a bar: a hit adds to it, it holds kTrailHoldSec after the last hit, then drains to the left - towards the value - as a damage
    // trail does (Shockwave via the owner, 7 Nov 2026: "should smoothly drain to the left, rather than lingering for far longer than it truly should"; it stood the
    // numbers' 3 s). Exponential with a floor speed, so a sliver goes too; under skin_motion NONE it goes at the hold's end.
    static void Hit(float v, float& was, float& since, float& lost, float dt, bool still = false)
    {
        since += dt;
        if (was >= 0 && v >= 0 && v < was - 1e-4f)
            lost += was - v, since = 0;
        else if (since > kTrailHoldSec && lost > 0)
            lost = still ? 0.0f : std::max(0.0f, lost - dt * std::max(kTrailFloor, lost / kTrailTau));
        was = v;
    }
    unsigned id_ = 0;
    float age_ = 0, offAge_ = 0, hitS_ = 1e9f, hitH_ = 1e9f, wasS_ = -1, wasH_ = -1, lostS_ = 0, lostH_ = 0;
    float lastMark_ = 0, lastX_ = 0, lastY_ = 0;
    bool seat_ = false, wasSeat_ = false, hadBlock_ = false, opened_ = false, above_ = false, hadCard_ = false;
    int side_ = 1, cardSide_ = 1;
    float relX_ = 0, relY_ = 0;
    float sideAge_ = kSideHoldSec, seatAge_ = kSeatHoldSec, aboveAge_ = kAboveHoldSec, moveT_ = 1, offX_ = 0, offY_ = 0, shownX_ = 0, shownY_ = 0, yield_ = 0;
    float cardT_ = 1, cardOff_ = 0, shownCardX_ = 0;
};

// A breath at this rate, 0..1, phase integrated by the caller's clock in seconds; held at 1 under FADES and NONE.
inline float Breathe(float seconds, float hz, int motion)
{
    return motion != MotionFull ? 1.0f : 0.5f - 0.5f * cosf(2 * 3.14159265f * hz * seconds);
}

// ---- the other ships (6.10 "Close ships"): each takes the richest form that touches nothing, a step down when it would.
enum Form : int
{
    CaretName,
    CaretDistance,
    CaretPips,
    CaretBare
};
// `forms`: the candidate's box in each form, richest first; `taken`: what already stands. The form to draw: a bare caret when nothing else fits.
inline int FitForm(const Box forms[4], const Box* taken, int n)
{
    for (int f = CaretName; f < CaretBare; ++f)
    {
        bool hit = false;
        for (int i = 0; i < n && !hit; ++i)
            hit = Touch(forms[f], taken[i]);
        if (!hit)
            return f;
    }
    return CaretBare;
}
} // namespace hud2target
