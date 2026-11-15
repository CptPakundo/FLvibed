// The design bench's replay (tools\hudbench\replay.*, audit 71b B5a) over the recorded frame tests\bench holds: Step and
// Draw run with no game from the record's inputs and states, every host call answered from the record's tape in call
// order; a tape one answer short, or one answer long, fails the replay loudly, whichever half and kind. Synthetic source
// (block glyphs, disc icons): the list's look is level 1's business (`flbench replay` against the game's lists, with the
// game's font: tests\bench_level1.ps1); this test holds the mechanics.
#include "test.h"

#include "../tools/hudbench/fldl_write.h"
#include "../tools/hudbench/replay.h"

#include <algorithm>
#include <cstring>

namespace
{
bool Contains(const std::string& s, const char* what) { return s.find(what) != std::string::npos; }
} // namespace

void Test_hudbench_replay()
{
    std::string why, text;
    CHECK(kitlist::Open(kitlist::Source::Synthetic, L"", why) == 0);
    const std::string path = g_test.fixtures + "\\..\\bench\\kbm_cruise_1280x800_f0.flstate.json";
    if (FILE* f = fopen(path.c_str(), "rb"))
    {
        char buf[65536];
        for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;)
            text.append(buf, n);
        fclose(f);
    }
    auto doc = std::make_unique<hud2state::StateDoc>();
    CHECK(hud2state::Read(text, *doc, &why));
    if (!why.empty())
        fprintf(stderr, "hudbench_replay: %s\n", why.c_str());

    // the record as recorded: replayed, every answer used; Draw drew the K&M form (hotspots on) and its parts' mark
    auto run = std::make_unique<benchreplay::Run>();
    benchreplay::Replay(*doc, *run);
    CHECK(run->ok);
    if (!run->ok)
        fprintf(stderr, "hudbench_replay: %s\n", run->error.c_str());
    CHECK(run->hotOn == doc->render.hotOn && run->hot.size() == doc->render.hot.size());
    CHECK(!run->over.marks.empty() && std::string(run->over.marks[0].name) == "parts.end");
    CHECK(!run->over.v[hud2kit::LShape].empty());
    // Step left the state the game's Draw took (the layout's size; the transition)
    CHECK(run->stepped.L.W == doc->render.st.L.W && run->stepped.t == doc->render.st.t);

    // each kind one answer short, then one answer long, in each half: the replay fails and says which
    struct Plant
    {
        bool draw;
        int kind; // 0 tickMs, 1 tickMs64, 3 typing, 4 hovered
    };
    const Plant plants[] = {{false, 1}, {false, 3}, {true, 0}, {true, 3}, {true, 4}};
    for (const Plant& p : plants)
        for (int longer = 0; longer < 2; ++longer)
        {
            auto bad = std::make_unique<hud2state::StateDoc>(*doc);
            hud2state::Tape& t = p.draw ? bad->render.tape : bad->update.tape;
            bool had = false;
            auto edit = [&](auto& v)
            {
                had = !v.empty();
                if (!had)
                    return;
                if (longer)
                    v.push_back(v.back());
                else
                    v.pop_back();
            };
            if (p.kind == 0)
                edit(t.tickMs);
            else if (p.kind == 1)
                edit(t.tickMs64);
            else if (p.kind == 3)
                edit(t.typing);
            else
                edit(t.hovered);
            CHECK(had); // (the fixture's halves read each of these: hud2_state_test.cpp)
            benchreplay::Replay(*bad, *run);
            const char* half = p.draw ? "Draw's tape" : "Step's tape";
            CHECK(!run->ok && Contains(run->error, half) && Contains(run->error, longer ? "left over" : "ran out"));
            if (run->ok)
                fprintf(stderr, "hudbench_replay: a planted tape (%s, kind %d, %s) replayed without a failure\n", half, p.kind,
                        longer ? "long" : "short");
        }
    // a record whose Render drew nothing: refused, not replayed in part
    auto none = std::make_unique<hud2state::StateDoc>(*doc);
    none->render.drawn = false;
    benchreplay::Replay(*none, *run);
    CHECK(!run->ok && Contains(run->error, "no pass"));
    // the original again: the failures left nothing behind (the kit's marquee table and clock unbound each time)
    benchreplay::Replay(*doc, *run);
    CHECK(run->ok);
    benchreplay::Unbind();

    // ---- a variant (B5a's rest)
    auto same = [](const kitlist::Pass& a, const kitlist::Pass& b)
    {
        for (int li = 0; li < hud2kit::LCount; ++li)
        {
            if (a.v[li].size() != b.v[li].size())
                return false;
            for (size_t i = 0; i < a.v[li].size(); ++i)
                if (memcmp(&a.v[li][i], &b.v[li][i], sizeof(hud2kit::Vertex)) != 0)
                    return false;
        }
        return a.sw == b.sw && a.sh == b.sh;
    };
    // (the variant draws from its own Step's state, which measured with this test's block glyphs; the replay draws from the
    // state the game's Draw took: with the game's font they are the same picture - `flbench replay --set` reports the
    // unchanged frame's level 1 and the difference -, here the variants are compared with each other)
    auto var = std::make_unique<benchreplay::Run>(), plain = std::make_unique<benchreplay::Run>();
    benchreplay::VariantOut vo;
    benchreplay::ReplayVariant(*doc, benchreplay::Overrides(), *plain, vo);
    CHECK(plain->ok && !vo.relayout && vo.lines.empty());
    // a setting set to the value it has: the same passes, bit for bit
    benchreplay::Overrides noop;
    CHECK(benchreplay::ParseSet("hud_scale=100", noop, why));
    benchreplay::ReplayVariant(*doc, noop, *var, vo);
    CHECK(var->ok && same(plain->over, var->over) && !vo.relayout);
    CHECK(vo.lines.size() == 2 && Contains(vo.lines[0], "opt.hud_scale 100 -> 100") && Contains(vo.lines[1], "rows.hud_scale"));
    // a real setting changes the picture
    benchreplay::Overrides big;
    CHECK(benchreplay::ParseSet("hud_scale=130", big, why));
    benchreplay::ReplayVariant(*doc, big, *var, vo);
    CHECK(var->ok && !same(plain->over, var->over));
    // unknown names and bad values fail before anything is drawn, saying why
    benchreplay::Overrides bad;
    CHECK(benchreplay::ParseSet("no_such_setting=1", bad, why));
    benchreplay::ReplayVariant(*doc, bad, *var, vo);
    CHECK(!var->ok && Contains(var->error, "no setting of that name"));
    benchreplay::Overrides badValue;
    CHECK(benchreplay::ParseSet("hud_scale=big", badValue, why) && !benchreplay::ParseSet("hud_scale", badValue, why));
    benchreplay::ReplayVariant(*doc, badValue, *var, vo);
    CHECK(!var->ok && Contains(var->error, "a whole number"));
    hud2::PartsIn parts;
    std::vector<std::string> lines;
    CHECK(benchreplay::ApplySet({{"hud_preset", "2"}}, 800, parts, lines, why) && parts.opt.hudPreset == 2 && parts.rows.hudPreset == 2);
    CHECK(!lines.empty() && Contains(lines[0], "OUTSIDE")); // (Speedrun cannot be chosen yet: said)
    CHECK(benchreplay::ApplySet({{"family", "playstation"}, {"rows.shield.on", "0"}}, 800, parts, lines, why) &&
          parts.family == glyphs::Family::PlayStation && !parts.rows.shield.on);
    CHECK(benchreplay::ApplySet({{"hud_size", "large"}}, 800, parts, lines, why) && parts.size.deck && parts.opt.textSize == 120);
    CHECK(!benchreplay::ApplySet({{"hud_size", "automatic"}}, 800, parts, lines, why));

    // a layout file: the preset's numbers by name; a name the tables lack, a repeat, a non-number fail
    std::vector<benchreplay::LayoutNum> nums;
    CHECK(benchreplay::ReadLayout("{\"_about\": \"a note\", \"standard.calm.context.y\": 0.85}", nums, why) && nums.size() == 1 &&
          nums[0].was == 0.915f && nums[0].value == 0.85f);
    CHECK(!benchreplay::ReadLayout("{\"standard.calm.nope.y\": 1}", nums, why) && Contains(why, "no number of that name"));
    CHECK(!benchreplay::ReadLayout("{\"standard.calm.context.y\": 1, \"standard.calm.context.y\": 2}", nums, why));
    CHECK(!benchreplay::ReadLayout("{\"standard.calm.context.y\": \"1\"}", nums, why));
    CHECK(!benchreplay::ReadLayout("{} x", nums, why));
    CHECK(Contains(benchreplay::LayoutNames(), "standard.calm.context.y = 0.915"));
    benchreplay::Overrides lay;
    CHECK(benchreplay::ReadLayout("{\"standard.calm.context.y\": 0.85}", lay.layout, why));
    benchreplay::ReplayVariant(*doc, lay, *var, vo);
    CHECK(var->ok && !same(plain->over, var->over));
    // the run's numbers end with the run: the replays after it draw the spec's again
    benchreplay::Replay(*doc, *run);
    CHECK(run->ok && run->stepped.L.W == doc->render.st.L.W);
    auto again = std::make_unique<benchreplay::Run>();
    benchreplay::ReplayVariant(*doc, noop, *again, vo);
    CHECK(again->ok && same(plain->over, again->over));

    // another size: the pass at that size, said so
    benchreplay::Overrides sized;
    sized.w = 2560, sized.h = 1600;
    benchreplay::ReplayVariant(*doc, sized, *var, vo);
    CHECK(var->ok && var->over.sw == 2560 && var->over.sh == 1600 && vo.relayout && var->stepped.L.W == 2560);
    CHECK(std::any_of(vo.notes.begin(), vo.notes.end(), [](const std::string& s) { return Contains(s, "another size"); }));

    // two lists compared as sets of vertices: the same list is the same picture; the variant's differs where it moved
    fldl::Meta meta;
    std::string la, lb, err;
    benchreplay::ReplayVariant(*doc, noop, *again, vo);
    CHECK(fldl::ListJson(meta, nullptr, again->over, {}, la, err));
    benchreplay::ReplayVariant(*doc, big, *var, vo);
    CHECK(fldl::ListJson(meta, nullptr, var->over, {}, lb, err));
    benchreplay::ListRead ra, rb;
    CHECK(benchreplay::ReadList(la, ra, err) && benchreplay::ReadList(lb, rb, err));
    std::vector<std::string> diff;
    size_t changed = 99;
    benchreplay::DiffLists(ra, ra, diff, changed);
    CHECK(changed == 0 && diff.empty());
    benchreplay::DiffLists(ra, rb, diff, changed);
    CHECK(changed > 0 && !diff.empty() && Contains(diff[0], "only before"));
    benchreplay::Unbind();
}
