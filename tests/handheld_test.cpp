// core\game\hudsize.h and core\game\handheld.h (pure): the one size row, its migration and floors, and the chain that answers Automatic.
#include "test.h"

#include "game/handheld.h"
#include "game/promptfit.h"

#include <cstring>
#include <vector>

namespace
{
// A minimal RSMB buffer: the 8-byte header, a type 1 and a type 2 structure with their strings, the end marker.
std::vector<unsigned char> Smbios(const char* sysMaker, const char* sysProduct, const char* boardMaker, const char* boardProduct)
{
    std::vector<unsigned char> b(8, 0);
    auto add = [&b](int type, const char* maker, const char* product)
    {
        const size_t at = b.size();
        b.resize(at + 8, 0);
        b[at] = static_cast<unsigned char>(type), b[at + 1] = 8, b[at + 4] = 1, b[at + 5] = 2;
        for (const char* s : {maker, product})
        {
            b.insert(b.end(), s, s + strlen(s));
            b.push_back(0);
        }
        b.push_back(0);
    };
    add(1, sysMaker, sysProduct);
    add(2, boardMaker, boardProduct);
    const unsigned char end[] = {127, 4, 0, 0, 0, 0};
    b.insert(b.end(), end, end + sizeof(end));
    return b;
}

handheld::Facts Parsed(const char* sysMaker, const char* sysProduct, const char* boardMaker, const char* boardProduct, float W, float H)
{
    const std::vector<unsigned char> b = Smbios(sysMaker, sysProduct, boardMaker, boardProduct);
    handheld::Facts f;
    handheld::ParseSmbios(b.data(), b.size(), f);
    f.W = W, f.H = H;
    return f;
}

int Answer(const handheld::Facts& f) { return handheld::Decide(f).preset; }
} // namespace

void Test_handheld()
{
    using namespace hudsize;
    // the setting wins, Automatic takes the machine's answer, a broken answer is Normal
    CHECK(Resolve(Automatic, Large) == Large);
    CHECK(Resolve(Automatic, Normal) == Normal);
    CHECK(Resolve(Automatic, Automatic) == Normal);
    CHECK(Resolve(Small, Large) == Small);
    CHECK(Resolve(Normal, Large) == Normal);
    CHECK(Resolve(Large, Normal) == Large);
    // deck_mode's values, moved once (core\settings_reset.h writes them)
    CHECK(FromDeckMode(0) == Automatic);
    CHECK(FromDeckMode(1) == Large);
    CHECK(FromDeckMode(2) == Normal);
    // Small: 0.875 and the 0.021 floor, but Normal's text at the handheld's height
    CHECK_NEAR(BaseK(Small, 2160), 0.875, 1e-6);
    CHECK_NEAR(FloorH(Small, 2160), 0.021, 1e-6);
    CHECK_NEAR(BaseK(Small, 800), 1.0, 1e-6);
    CHECK_NEAR(FloorH(Small, 800), 0.024, 1e-6);
    CHECK_NEAR(BaseK(Normal, 2160), 1.0, 1e-6);
    CHECK_NEAR(FloorH(Large, 800), 0.024, 1e-6);
    // the frame's one write, and what the layouts read from it
    CHECK(Apply(Large, 800, 100) == deckmode::kTextSize);
    CHECK(deckmode::g_on);
    CHECK(Apply(Large, 800, 130) == 130); // a size the player chose stays theirs
    CHECK(Apply(Normal, 2160, 100) == 100);
    CHECK(!deckmode::g_on);
    CHECK_NEAR(promptfit::Hud2Scale(3840, 2160, 100, 100, false), 1.0, 1e-6);
    CHECK_NEAR(textfit::MinSize(2160), 0.024 * 2160, 1e-3);
    Apply(Small, 2160, 100);
    CHECK_NEAR(promptfit::Hud2Scale(3840, 2160, 100, 100, false), 0.875, 1e-6);
    CHECK_NEAR(textfit::MinSize(2160), 0.021 * 2160, 1e-3);
    CHECK_NEAR(promptfit::Hud2Scale(3840, 2160, 120, 100, false), 1.05, 1e-5); // a trim rides on the preset
    Apply(Small, 800, 100);
    CHECK_NEAR(textfit::MinSize(800), 0.024 * 800, 1e-3);
    Apply(Normal, 2160, 100); // (what every other test expects)
    // the game's text follows the size only under Automatic
    CHECK(GameTextPercent(0, Large) == 125);
    CHECK(GameTextPercent(0, Normal) == 100);
    CHECK(GameTextPercent(0, Small) == 100);
    CHECK(GameTextPercent(150, Normal) == 150);

    // the SMBIOS reader
    {
        const handheld::Facts f = Parsed("Valve", "Jupiter", "Valve", "Jupiter", 1280, 800);
        CHECK(f.systemMaker == "Valve");
        CHECK(f.systemProduct == "Jupiter");
        CHECK(f.boardProduct == "Jupiter");
        handheld::Facts none;
        handheld::ParseSmbios(nullptr, 0, none);
        CHECK(none.systemProduct.empty());
        const unsigned char junk[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 200, 0, 0}; // a length past the end: stops, reads nothing
        handheld::ParseSmbios(junk, sizeof(junk), none);
        CHECK(none.systemProduct.empty());
    }
    // the model table
    CHECK(Answer(Parsed("Valve", "Jupiter", "Valve", "Jupiter", 1280, 800)) == Large);
    CHECK(Answer(Parsed("Valve", "Galileo", "Valve", "Galileo", 1280, 800)) == Large);
    CHECK(Answer(Parsed("Valve", "Jupiter", "Valve", "Jupiter", 1920, 1080)) == Normal);    // docked
    CHECK(Answer(Parsed("Valve", "Fremont", "Valve", "Fremont", 1280, 800)) == Normal);     // no handheld, whatever the screen
    CHECK(Answer(Parsed("ASUSTeK COMPUTER INC.", "ROG Ally", "ASUSTeK COMPUTER INC.", "RC71L", 1920, 1080)) == Large);
    CHECK(Answer(Parsed("LENOVO", "83E1", "LENOVO", "LNVNB161216", 1920, 1200)) == Large);
    CHECK(Answer(Parsed("LENOVO", "20XY", "LENOVO", "LNVNB161216", 1920, 1200)) == Normal); // a laptop of the same maker
    CHECK(Answer(Parsed("GPD", "G1618-04", "GPD", "Default string", 1920, 1080)) == Large);
    CHECK(Answer(Parsed("GPD", "G1628-04", "GPD", "Default string", 1920, 1080)) == Normal); // the Pocket 4
    CHECK(Answer(Parsed("AYANEO", "AIR Pro", "AYANEO", "AIR Pro", 1920, 1080)) == Large);
    CHECK(Answer(Parsed("Acme", "AIR", "Acme", "AIR", 1920, 1080)) == Normal);               // a bare name needs the maker
    CHECK(Answer(Parsed("ONE-NETBOOK", "ONEXPLAYER Mini Pro", "ONE-NETBOOK", "x", 1920, 1080)) == Large);
    // a handheld on an external monitor
    {
        handheld::Facts f = Parsed("ASUSTeK COMPUTER INC.", "ROG Ally", "ASUSTeK COMPUTER INC.", "RC71L", 1920, 1080);
        f.externalMonitor = true;
        CHECK(Answer(f) == Normal);
    }
    // no model: the device form guarded by the panel, a small panel alone, Steam's variable, exactly 1280x800, else Normal
    {
        handheld::Facts f;
        f.W = 1920, f.H = 1080;
        CHECK(Answer(f) == Normal);
        f.familyHandheld = true;
        CHECK(Answer(f) == Normal); // (desktop owners set it by hand: no panel, no answer)
        f.panelInches = 11;
        CHECK(Answer(f) == Large);
        f.familyHandheld = false;
        CHECK(Answer(f) == Normal);
        f.panelInches = 8;
        CHECK(Answer(f) == Large);
        f.externalMonitor = true;
        CHECK(Answer(f) == Normal);
    }
    {
        handheld::Facts f;
        f.W = 1920, f.H = 1080, f.wine = true, f.steamDeckEnv = true;
        CHECK(Answer(f) == Large);
        f.wine = false;
        CHECK(Answer(f) == Normal); // (set on living-room PCs: only under Wine, and only with no readable SMBIOS)
        f.W = 1280, f.H = 800;
        CHECK(Answer(f) == Large);  // the owner: 1280x800 is almost only the Deck
        f.W = 1280, f.H = 720;
        CHECK(Answer(f) == Normal);
    }
}
