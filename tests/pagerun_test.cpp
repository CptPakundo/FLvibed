// The page runtime's pure rules (audit 70b F2a), each against the code it replaced (nnpage.cpp's INFO, replayed here as it
// was): the scroll's lines (pageinput.h ScrollLines with INFO's repeat) at several frame rates, and INFO's read moments
// (core\game\screens\nnhelp_model.h Composes / Takes driven by the runtime's stamp) over sequences of targets.
#include "test.h"

#include "../core/game/screens/nnhelp_model.h"
#include "../modules/hud/src/pages/pageinput.h"

#include <vector>

namespace
{
// nnpage.cpp's INFO pad before F2a: a press a line, then g_infoStep at 15 a second past 1 + 0.3 * 15.
struct OldScroll
{
    float step = 0;
    int Lines(bool pd, bool pu, bool hd, bool hu, float dt)
    {
        int first = (pd ? 1 : pu ? -1 : 0);
        const int dir = hd ? 1 : hu ? -1 : 0;
        step = dir ? step + dt * 15 : 0;
        for (; step >= 1 + 0.3f * 15; step -= 1)
            first += dir;
        return first;
    }
};

// One input frame: the D-pad pressed / held, down and up.
struct In
{
    bool pd, pu, hd, hu;
};

// INFO's moments before F2a: on the open; then for a new non-zero target; a composed TargetGone ignored unless opened.
struct OldInfo
{
    int taken = -1; // which compose's text the page shows
    unsigned seen = 0;
    void Frame(bool opened, unsigned target, bool gone, int compose)
    {
        if (opened || (target && target != seen))
            if (opened || !gone)
                taken = compose;
        seen = target;
    }
};
// The runtime's: the model made on the open (fresh), read on the open and when the stamp (the target) changes.
struct NewInfo
{
    infobody::Model m;
    int taken = -1;
    unsigned stamp = 0;
    bool open = false;
    void Read(unsigned target, bool gone, int compose)
    {
        if (!infobody::Composes(m, target))
            return;
        if (!infobody::Takes(m, gone ? infobody::Case::TargetGone : infobody::Case::Target))
            return;
        m.fresh = false, taken = compose;
    }
    void Frame(bool opened, unsigned target, bool gone, int compose)
    {
        if (opened || !open)
        {
            m = infobody::Model(), open = true;
            Read(target, gone, compose);
        }
        else if (target != stamp)
            Read(target, gone, compose);
        stamp = target;
    }
};
} // namespace

void Test_pagerun()
{
    // The scroll: the same lines every frame as INFO's own code, at 30 / 60 / 144 / 240 frames a second, over presses, holds
    // past the delay, a direction turned mid-hold, a release, and the right stick's hold (the same bits).
    for (const float fps : {30.0f, 60.0f, 144.0f, 240.0f})
    {
        const float dt = 1 / fps;
        std::vector<In> seq;
        auto hold = [&](bool down, float secs)
        {
            seq.push_back({down, !down, down, !down});
            for (int i = 1; i < static_cast<int>(secs * fps); ++i)
                seq.push_back({false, false, down, !down});
        };
        auto rest = [&](float secs)
        {
            for (int i = 0; i < static_cast<int>(secs * fps); ++i)
                seq.push_back({false, false, false, false});
        };
        hold(true, 1.5f), rest(0.2f), hold(false, 0.25f), rest(0.1f), hold(false, 2.0f);
        for (int i = 0; i < static_cast<int>(0.7f * fps); ++i) // a turn while held: no release between
            seq.push_back({false, false, true, false});
        rest(0.05f), seq.push_back({true, false, false, false}), seq.push_back({false, true, false, false});
        OldScroll was;
        pageinput::RepeatState s;
        int diff = 0, total = 0;
        for (const In& f : seq)
        {
            const int a = was.Lines(f.pd, f.pu, f.hd, f.hu, dt);
            const int b = pageinput::ScrollLines(pageinput::kInfoTextRepeat, s, f.pd, f.pu, f.hd, f.hu, dt);
            diff += a != b, total += b;
        }
        CHECK(diff == 0);
        CHECK(total != 0); // (the sequence moved the text at all)
    }
    // One long frame past the delay gives several lines, as before (15 a second).
    {
        pageinput::RepeatState s;
        CHECK(pageinput::ScrollLines(pageinput::kInfoTextRepeat, s, false, false, true, false, 0.3f) == 0);
        CHECK(pageinput::ScrollLines(pageinput::kInfoTextRepeat, s, false, false, true, false, 0.2f) == 3);
        CHECK(pageinput::ScrollLines(pageinput::kInfoTextRepeat, s, false, false, false, false, 0.1f) == 0 && s.acc == 0);
    }
    // The read moments: the open always takes (a gone card too); later only a new target the card takes; a cleared target
    // keeps the text; the same target again reads nothing.
    {
        infobody::Model m;
        CHECK(infobody::Composes(m, 0) && infobody::Takes(m, infobody::Case::TargetGone));
        m.fresh = false;
        CHECK(!infobody::Composes(m, 0) && infobody::Composes(m, 7));
        CHECK(!infobody::Takes(m, infobody::Case::TargetGone) && infobody::Takes(m, infobody::Case::Target) &&
              infobody::Takes(m, infobody::Case::Space) && infobody::Takes(m, infobody::Case::Docked));
    }
    // Over sequences of frames (open, targets, a cleared target, a gone card, the same target, a re-open over another
    // window): the runtime takes the same text as the old code on every frame.
    {
        struct F
        {
            bool opened;
            unsigned target;
            bool gone;
        };
        const std::vector<std::vector<F>> runs = {
            {{true, 0, false}, {false, 0, false}, {false, 5, false}, {false, 5, false}, {false, 0, false}, {false, 6, true},
             {false, 6, true}, {false, 7, false}, {false, 5, false}},
            {{true, 9, true}, {false, 9, true}, {false, 0, false}, {false, 9, false}, {true, 9, false}, {false, 3, false}},
            {{true, 4, false}, {false, 4, false}, {true, 0, false}, {false, 4, true}, {false, 8, false}, {false, 8, false}},
        };
        for (const auto& run : runs)
        {
            OldInfo was;
            NewInfo now;
            int frame = 0, diff = 0;
            for (const F& f : run)
            {
                was.Frame(f.opened, f.target, f.gone, frame);
                now.Frame(f.opened, f.target, f.gone, frame);
                diff += was.taken != now.taken, ++frame;
            }
            CHECK(diff == 0);
        }
    }
}
