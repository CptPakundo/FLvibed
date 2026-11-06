// The settings bus's sections (core\settings_bus.cpp): a section may have several owners, a key has one, and one module's writer
// never runs for another's section. And `enabled`'s meaning per module (core\enabled_gate.h).
#include "enabled_gate.h"
#include "settings_bus.h"

#include "test.h"

extern "C" const SettingsBus* flv_settings_bus();

static bool g_hudRow = true, g_padRow = false, g_otherRow = false;
static int g_hudSaves = 0, g_padSaves = 0, g_applied = 0;

static const settings::Def kHud[] = {{"hud_row", settings::Bool, &g_hudRow, 0, 1, "a row"}};
static const settings::Def kPad[] = {{"pad_row", settings::Bool, &g_padRow, 0, 1, "a row"}};
static const settings::Def kClash[] = {{"HUD_ROW", settings::Bool, &g_otherRow, 0, 1, "a row"}};

static SettingsBusSection Section(const char* module, const settings::Def* defs, void (*save)(void*))
{
    SettingsBusSection s = {};
    s.module = module;
    s.section = "TestShared";
    s.defs = defs;
    s.count = 1;
    s.applied = [](void*, const settings::Def*) { ++g_applied; };
    s.save = save;
    return s;
}

void Test_settings_bus()
{
    const SettingsBus* bus = flv_settings_bus();
    const SettingsBusSection hud = Section("hud", kHud, [](void*) { ++g_hudSaves; });
    const SettingsBusSection pad = Section("controller", kPad, [](void*) { ++g_padSaves; });
    const SettingsBusSection clash = Section("speedrun", kClash, nullptr);
    const int a = bus->Register(&hud), b = bus->Register(&pad);
    CHECK(a > 0 && b > 0 && a != b);
    CHECK(bus->Register(&hud) == 0);   // the same owner twice: a logic that did not unregister, the old one stays
    CHECK(bus->Register(&clash) == 0); // a key has one owner, whatever its case

    // each row is found in its owner, one section to the reader
    CHECK(bus->Find("TestShared", "hud_row") == &kHud[0]);
    CHECK(bus->Find("TestShared", "pad_row") == &kPad[0]);
    CHECK(bus->Find("TestShared", "no_row") == nullptr);
    CHECK(bus->Set("TestShared", "pad_row", "1") && g_padRow && g_applied == 1);
    char v[96];
    CHECK(bus->Get("TestShared", "hud_row", v, sizeof(v)) && v[0] == '1');
    CHECK(bus->Default("TestShared", "pad_row", v, sizeof(v)) && v[0] == '0');
    CHECK(bus->Reset("TestShared", "pad_row") && !g_padRow);

    // the save reaches every owner of the section
    CHECK(bus->Save("TestShared") && g_hudSaves == 1 && g_padSaves == 1);

    // an owner going leaves the other's rows
    bus->Unregister(a);
    CHECK(bus->Find("TestShared", "hud_row") == nullptr);
    CHECK(bus->Find("TestShared", "pad_row") == &kPad[0]);
    CHECK(bus->Save("TestShared") && g_hudSaves == 1 && g_padSaves == 2);
    bus->Unregister(b);
    CHECK(bus->Find("TestShared", "pad_row") == nullptr);
    CHECK(!bus->Save("TestShared"));

    // a long table fits up to the cap and is refused above it (163 rows against a cap of 160 greyed the whole Autopilot section)
    {
        static std::vector<settings::Def> rows;
        static std::vector<std::string> names;
        static bool row = false;
        names.resize(kSettingsBusMaxRows + 1);
        rows.clear();
        for (size_t i = 0; i <= kSettingsBusMaxRows; ++i)
        {
            names[i] = "row" + std::to_string(i);
            rows.push_back({names[i].c_str(), settings::Bool, &row, 0, 1, "a row"});
        }
        SettingsBusSection big = Section("autopilot", rows.data(), nullptr);
        big.count = rows.size();
        CHECK(bus->Register(&big) == 0);
        big.count = kSettingsBusMaxRows;
        const int id = bus->Register(&big);
        CHECK(id > 0);
        bus->Unregister(id);
    }

    // `enabled`: every module follows the switch, as the one frame always did
    const enabled_gate::Modules on = enabled_gate::Read(true), off = enabled_gate::Read(false);
    CHECK(on.controller && on.hud && on.autopilot && on.speedrun && enabled_gate::AnyRuns(on));
    CHECK(!off.controller && !off.hud && !off.autopilot && !off.speedrun && !enabled_gate::AnyRuns(off));
    CHECK(enabled_gate::AnyRuns(enabled_gate::Modules{false, true, false, false}));
}
