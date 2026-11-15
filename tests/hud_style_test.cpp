// core\ui\hud_rows.h StyleOf / Settle: hud_style has two states (the owner's decision 77, 13 Nov 2026), 0 the classic HUD and 2 HUD 2.
// A file of an older release still loads: its 1 (HUD 2 on the pad only, retired) reads as 2 and the next save writes 2; 0 and 2 stay; what the
// row's clamp (0..2) makes of anything else stays as it was (below 0 classic, above 2 HUD 2); a value that does not parse keeps the default.
// The load is the one both loaders make (settings::Load of the rows, then Settle: the controller's LoadConfig, core\logic\alone.cpp); a live
// set through the settings bus ends in its owner's `applied` hook, which calls Settle (the controller's and alone.cpp's), so 1 never stays.
#include <windows.h>

#include "settings_bus.h"
#include "ui/hud_rows.h"

#include "test.h"

extern "C" const SettingsBus* flv_settings_bus();

// hud_style after loading an ini whose [Controller] holds `line` (nothing: no key), from the default 2.
static int Loaded(const std::string& ini, const char* line)
{
    WriteTextFile(ini, std::string("[Controller]\r\n") + line + "\r\n");
    hudrows::g.hudStyle = 2;
    settings::Load("Controller", ini, hudrows::kRows, hudrows::kRowCount);
    hudrows::Settle();
    return hudrows::g.hudStyle;
}

void Test_hud_style()
{
    // the pure rule
    CHECK(hudrows::StyleOf(0) == 0);
    CHECK(hudrows::StyleOf(1) == 2);
    CHECK(hudrows::StyleOf(2) == 2);
    CHECK(hudrows::StyleOf(-5) == 0);
    CHECK(hudrows::StyleOf(7) == 2);

    const std::string ini = TempPath("FLController.ini", "hud_style");
    CHECK(Loaded(ini, "hud_style = 0") == 0); // the classic HUD stays
    CHECK(Loaded(ini, "hud_style = 1") == 2); // an older release's HUD 2 on the pad: HUD 2
    CHECK(Loaded(ini, "hud_style = 2") == 2); // HUD 2 stays
    CHECK(Loaded(ini, "hud_style = 1 ; a note") == 2);
    CHECK(Loaded(ini, "hud_style = 1.9") == 2);  // (the Int truncates to 1)
    CHECK(Loaded(ini, "hud_style = -3") == 0);   // the clamp, as before
    CHECK(Loaded(ini, "hud_style = 9") == 2);    // the clamp, as before
    CHECK(Loaded(ini, "hud_style = 0.5") == 0);  // (truncates to 0)
    CHECK(Loaded(ini, "hud_style = abc") == 2);  // does not parse: the default stands
    CHECK(Loaded(ini, "rumble = 60") == 2);      // no key: the default stands

    // the next save writes the state it reads as, and the row's value is never 1 after a load
    CHECK(Loaded(ini, "hud_style = 1") == 2);
    settings::Save("Controller", ini, hudrows::kRows, hudrows::kRowCount);
    CHECK(GetPrivateProfileIntA("Controller", "hud_style", -1, ini.c_str()) == 2);
    DeleteFileA(ini.c_str());

    // a live set through the settings bus, the rows' owner wired as the controller and alone.cpp wire it (`applied` = Settle): 1 is 2 at once
    {
        const SettingsBus* bus = flv_settings_bus();
        SettingsBusSection s = {};
        s.module = "hud";
        s.section = "Controller";
        s.defs = hudrows::kRows;
        s.count = hudrows::kRowCount;
        s.applied = [](void*, const settings::Def*) { hudrows::Settle(); };
        const int id = bus->Register(&s);
        CHECK(id > 0);
        char v[16] = {};
        CHECK(bus->Set("Controller", "hud_style", "1") && hudrows::g.hudStyle == 2);
        CHECK(bus->Get("Controller", "hud_style", v, sizeof(v)) && !strcmp(v, "2"));
        CHECK(bus->Set("Controller", "hud_style", "0") && hudrows::g.hudStyle == 0);
        CHECK(bus->Set("Controller", "hud_style", "2") && hudrows::g.hudStyle == 2);
        if (id > 0)
            bus->Unregister(id);
    }
    hudrows::g.hudStyle = 2; // (the shared value back to its default for the tests after this one)
}
