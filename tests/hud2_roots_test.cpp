// modules\hud\src\hud2_roots.h (pure): every native flight root has its row, wanted things are kept, an alert goes only with a home, and a root with no
// row is left alone (the seam audit logs it, nothing hides it).
#include "test.h"

#include "../modules/hud/src/hud2_roots.h"

void Test_hud2_roots()
{
    using namespace hudroots;
    for (const char* wanted : {"Targetable_Objects", "HUD_Waypoint", "NN_Scanner", "NN_GroupControl", "BriefingParentWindow", "MissionObjectiveWindow", "CommText"})
    {
        const Row* r = Find(wanted);
        CHECK(r && r->action == Keep);
    }
    for (const char* gone : {"HUD_Maneuvers", "HUD_Target", "HUD_ContactList", "HUD_Status", "HUD_Core", "HUD_CruiseProgress", "HUD_DamageIndicator", "MsgWin"})
    {
        const Row* r = Find(gone);
        CHECK(r && r->action == Hide);
    }
    const Row* nn = Find("NN_MainButton");
    CHECK(nn && nn->action == KeepNoArt);
    for (const char* alert : {"MissileAlert", "RadiationAlert", "TrailAlert", "LagAlert", "NN_ArrivalText"})
    {
        const Row* r = Find(alert);
        CHECK(r && r->action == Home && r->home[0]);
    }
    CHECK(Find("SomeModsWindow") == nullptr);
    CHECK(Find(nullptr) == nullptr);
    CHECK(strcmp(ActionName(nullptr), "unknown") == 0);
    // no name twice
    for (const Row& a : kRows)
    {
        int n = 0;
        for (const Row& b : kRows)
            n += strcmp(a.name, b.name) == 0;
        CHECK(n == 1);
    }
}
