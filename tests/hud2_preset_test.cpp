// HUD 2's layout preset plumbing (controller\src\impl\hud2_preset.h, UI_HUD2_PRESETS.md 6 step 4): hud_preset clamps to
// a valid preset (out of range = Standard), Standard's and Focus's layouts are built (every other preset draws as
// Standard and its label says so), each preset's data is its own (the lint's zones / caps), the value ids' homes per preset with the
// off-screen hand-over, and the controller setup's read (PRESETS 4: column, needs, the compatibility cell, the
// suggested pairing) from core\ui_meta.h's generated table.
#include "../controller/src/impl/hud2_preset.h"

#include "test.h"

void Test_hud2_preset()
{
    using namespace hud2preset;
    // ---- the index and the layout that draws
    CHECK(Clamp(0) == Standard && Clamp(3) == Guided);
    CHECK(Clamp(-1) == Standard && Clamp(4) == Standard && Clamp(99) == Standard); // fail closed
    CHECK(Built(Standard) && Built(Focus) && !Built(Speedrun) && !Built(Guided)); // Focus since 31 Oct 2026 (6 step 8)
    CHECK(Drawn(Standard) == Standard && Drawn(Focus) == Focus);
    CHECK(Drawn(Speedrun) == Standard && Drawn(Guided) == Standard); // not built: Standard draws
    CHECK(Label(Standard) == "Standard");
    CHECK(Label(Focus) == "Focus");
    CHECK(Label(Guided) == "Guided (layout not built: Standard drawn)");
    CHECK(Label(7) == "Standard");
    // ---- each preset's own data (what the lint and step 5 read)
    CHECK(&Data(Focus) == &uimeta::hud2p::kPresets[1]);
    CHECK(std::string(Data(Speedrun).id) == "speedrun");
    CHECK(Data(Standard).zoneCount == 3 && Data(Guided).zoneCount == 4); // S1 / S2 / core; core / Z1 / Z2L / Z2R
    CHECK(std::fabs(Data(Focus).pickR - 0.12f) < 1e-6f);                 // PRESETS 2.2 / 3
    CHECK(&Data(-5) == &Data(Standard));
    // ---- the value ids' homes (PRESETS 1.7)
    {
        const uimeta::hud2p::ValueId* tn = uimeta::hud2p::FindValueId("target.name");
        CHECK(tn != nullptr);
        if (tn)
        {
            CHECK(std::string(Home(*tn, Standard, false)) == "native.bracket"); // pair 1 A: on screen the bracket
            CHECK(std::string(Home(*tn, Standard, true)) == "card");            // off screen the card
        }
        const uimeta::hud2p::ValueId* eta = uimeta::hud2p::FindValueId("eta");
        CHECK(eta && std::string(Home(*eta, Speedrun, false)) == "flank.right" &&
              std::string(Home(*eta, Speedrun, true)) == "flank.right"); // no off-screen home: the same
        const uimeta::hud2p::ValueId* info = uimeta::hud2p::FindValueId("info");
        CHECK(info && Home(*info, Focus, false) == nullptr); // Focus does not show it
        const uimeta::hud2p::ValueId* dist = uimeta::hud2p::FindValueId("target.distance");
        CHECK(dist && std::string(Home(*dist, Speedrun, true)) == "native.bracket"); // Speedrun's off-screen home open
    }
    // ---- the controller setup (PRESETS 4)
    {
        Setup s; // FREELANCER on the pad, weapon_groups 1, no toggles
        Adapt a = AdaptTo(Standard, s);
        CHECK(a.column == uimeta::hud2p::kColFreelancer);
        CHECK(a.needs.throttleWord && a.needs.killWord && !a.needs.throttleReadout && !a.needs.apLine);
        CHECK(!a.needs.groupRows && !a.needs.latchedWords && !a.needs.keycaps);
        CHECK(a.compat.fit == uimeta::hud2p::Fit::Fine && a.compat.suggested);
        CHECK(a.suggested == Standard);
        CHECK(Describe(a) == "setup=FREELANCER / CUSTOM groups=1 toggles=0 fit=fine(suggested) suggested=Standard "
                             "needs=throttle_word,kill_word");

        s.padPreset = 3; // CUSTOM = FREELANCER until the Bindings editor
        CHECK(ColumnOf(s) == uimeta::hud2p::kColFreelancer);

        s.padPreset = 1; // ACE
        a = AdaptTo(Focus, s);
        CHECK(a.column == uimeta::hud2p::kColAce && a.needs.throttleReadout && !a.needs.throttleWord);
        CHECK(a.compat.fit == uimeta::hud2p::Fit::Adapts && !a.compat.suggested);
        CHECK(std::string(a.compat.note).find("under-arc band") != std::string::npos);

        s.padPreset = 2; // ASSISTED
        a = AdaptTo(Focus, s);
        CHECK(a.needs.apLine && a.needs.takeControlPinned && a.needs.reticleAlways);
        CHECK(a.compat.fit == uimeta::hud2p::Fit::Discouraged); // 7 answer 5
        CHECK(a.suggested == Guided);                            // ASSISTED -> Guided
        CHECK(AdaptTo(Guided, s).compat.suggested && AdaptTo(Guided, s).compat.fit == uimeta::hud2p::Fit::Fine);

        s.padPreset = 0, s.weaponGroups = 2, s.holdsAsToggles = true;
        a = AdaptTo(Speedrun, s);
        CHECK(a.needs.groupRows && a.needs.latchedWords && a.needs.throttleWord);

        s.kbm = true; // K&M: its own column; the pad rows do not apply
        a = AdaptTo(Speedrun, s);
        CHECK(a.column == uimeta::hud2p::kColKbm && a.needs.keycaps && a.needs.throttleReadout);
        CHECK(!a.needs.groupRows && !a.needs.latchedWords && !a.needs.throttleWord);
        CHECK(a.compat.fit == uimeta::hud2p::Fit::Adapts && a.suggested == Standard);

        s.kbm = false, s.padPreset = 9; // out of range: FREELANCER's column and pairing
        CHECK(ColumnOf(s) == uimeta::hud2p::kColFreelancer && SuggestedFor(s) == Standard);
    }
    // ---- the generated table is complete (every cell has a fit; the lever's range)
    for (int p = 0; p < kCount; ++p)
        for (int c = 0; c < uimeta::hud2p::kColumnCount; ++c)
            CHECK(uimeta::hud2p::kCompat[p][c].note != nullptr);
    CHECK(uimeta::hud2p::kLeverPartialLo < uimeta::hud2p::kLeverPartialHi);
}
