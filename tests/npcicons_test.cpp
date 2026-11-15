// The NPC icons' game-free half (modules\hud\src\npcicons_rules.h; GitHub 84 + 85, audit 147_): the model CRC, a synthetic
// .cmp's parts placed by their Cons joint and read top-down, the silhouette's fill, close, floor and fit, the bake, the rank words.
#include "../modules/hud/src/npcicons_rules.h"

#include "test.h"

namespace
{
struct L
{
    std::string path;
    std::vector<unsigned char> data;
};
template <class T> void Put(std::vector<unsigned char>& d, size_t o, T v)
{
    if (d.size() < o + sizeof v)
        d.resize(o + sizeof v);
    memcpy(d.data() + o, &v, sizeof v);
}
// One VMeshData: a mesh of `quad` (two triangles) with the given corner positions.
std::vector<unsigned char> MeshData(const float corners[4][3])
{
    std::vector<unsigned char> d;
    Put<uint32_t>(d, 0, 0x1), Put<uint32_t>(d, 4, 0x4);
    Put<uint16_t>(d, 8, 1), Put<uint16_t>(d, 10, 6), Put<uint16_t>(d, 12, 0x112), Put<uint16_t>(d, 14, 4);
    Put<uint32_t>(d, 16, 0), Put<uint16_t>(d, 20, 0), Put<uint16_t>(d, 22, 3), Put<uint16_t>(d, 24, 6), Put<uint16_t>(d, 26, 0);
    const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; ++i)
        Put<uint16_t>(d, 28 + 2 * i, idx[i]);
    for (int v = 0; v < 4; ++v) // stride 24: position + normal
        for (int k = 0; k < 6; ++k)
            Put<float>(d, 40 + 24 * v + 4 * k, k < 3 ? corners[v][k] : 0.0f);
    return d;
}
std::vector<unsigned char> MeshRef(uint32_t lib)
{
    std::vector<unsigned char> d;
    Put<uint32_t>(d, 0, 60), Put<uint32_t>(d, 4, lib);
    Put<uint16_t>(d, 8, 0), Put<uint16_t>(d, 10, 4), Put<uint16_t>(d, 12, 0), Put<uint16_t>(d, 14, 6);
    Put<uint16_t>(d, 16, 0), Put<uint16_t>(d, 18, 1);
    d.resize(60);
    return d;
}
std::vector<unsigned char> Str(const char* s) { return std::vector<unsigned char>(s, s + strlen(s) + 1); }
} // namespace

void Test_npcicons()
{
    using namespace npcicons;
    CHECK(ModelCrc("li_elite_lod1.vms") == 0x0FE69642u); // tools\mk3db.py fl_crc
    CHECK(ModelCrc("LI_ELITE_LOD1.VMS") == 0x0FE69642u); // lower-cased first
    CHECK(ModelCrc("a") == 0xFCB7BE43u);

    // a .cmp: a hull 10 x 40 at the root, a wing 30 x 4 on part "wing", joined by a Fix 20 m forward (-z)
    const float hull[4][3] = {{-5, 0, -20}, {5, 0, -20}, {5, 0, 20}, {-5, 0, 20}};
    const float wing[4][3] = {{-15, 0, -2}, {15, 0, -2}, {15, 0, 2}, {-15, 0, 2}};
    std::vector<L> cmp;
    cmp.push_back({"\\VMeshLibrary\\s_lod0.vms\\VMeshData", MeshData(hull)});
    cmp.push_back({"\\VMeshLibrary\\w_lod0.vms\\VMeshData", MeshData(wing)});
    cmp.push_back({"\\Cmpnd\\Root\\File name", Str("s.3db")});
    cmp.push_back({"\\Cmpnd\\Root\\Object name", Str("Root")});
    cmp.push_back({"\\Cmpnd\\Part_wing\\File name", Str("w.3db")});
    cmp.push_back({"\\Cmpnd\\Part_wing\\Object name", Str("wing")});
    std::vector<unsigned char> fix(176, 0);
    memcpy(fix.data(), "Root", 4), memcpy(fix.data() + 64, "wing", 4);
    Put<float>(fix, 128, 0), Put<float>(fix, 132, 0), Put<float>(fix, 136, -20);
    const float ident[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    for (int i = 0; i < 9; ++i)
        Put<float>(fix, 140 + 4 * i, ident[i]);
    cmp.push_back({"\\Cmpnd\\Cons\\Fix", fix});
    cmp.push_back({"\\s.3db\\MultiLevel\\Level0\\VMeshPart\\VMeshRef", MeshRef(ModelCrc("s_lod0.vms"))});
    cmp.push_back({"\\s.3db\\MultiLevel\\Level1\\VMeshPart\\VMeshRef", MeshRef(0xDEAD)}); // a lower level: not read
    cmp.push_back({"\\w.3db\\VMeshPart\\VMeshRef", MeshRef(ModelCrc("w_lod0.vms"))});
    const std::vector<Tri2> tris = TopDown(cmp);
    CHECK(tris.size() == 4);
    float minY = 1e9f, maxY = -1e9f, minX = 1e9f, maxX = -1e9f;
    for (const Tri2& t : tris)
        for (int k = 0; k < 3; ++k)
            minY = std::min(minY, t.y[k]), maxY = std::max(maxY, t.y[k]), minX = std::min(minX, t.x[k]), maxX = std::max(maxX, t.x[k]);
    CHECK(minX == -15 && maxX == 15); // the wing, placed
    CHECK(minY == -22 && maxY == 20); // the wing 20 m forward of the root (-z = nose)

    Mask m;
    CHECK(Silhouette(tris, 128, 0.0f, m));
    CHECK(m.n == 128 && m.lengthM == 42 && m.widthM == 30);
    // the nose up: the wing's row (near the top) is wider than the hull's row near the bottom
    auto rowWidth = [&](int y) {
        int w = 0;
        for (int x = 0; x < m.n; ++x)
            w += m.px[size_t(y) * m.n + x] != 0;
        return w;
    };
    CHECK(rowWidth(6) > rowWidth(120) * 2);
    CHECK(rowWidth(64) > 0);
    // the width floor: a 4 x 40 stick with floor 0.5 comes out about half as wide as long
    const float stick[4][3] = {{-2, 0, -20}, {2, 0, -20}, {2, 0, 20}, {-2, 0, 20}};
    std::vector<L> one = {{"\\VMeshLibrary\\a.vms\\VMeshData", MeshData(stick)},
                          {"\\MultiLevel\\Level0\\VMeshPart\\VMeshRef", MeshRef(ModelCrc("a.vms"))}};
    const std::vector<Tri2> st = TopDown(one);
    CHECK(st.size() == 2);
    Mask f0, f5;
    CHECK(Silhouette(st, 128, 0.0f, f0) && Silhouette(st, 128, 0.5f, f5));
    auto widthOf = [](const Mask& k) {
        int w = 0;
        for (int x = 0; x < k.n; ++x)
            w += k.px[size_t(k.n / 2) * k.n + x] != 0;
        return w;
    };
    CHECK(widthOf(f0) < 20);
    CHECK(widthOf(f5) > 56 && widthOf(f5) < 72);
    CHECK(WidthFloor(1u << 16) == 0.0f && WidthFloor(1u << 21) == 0.5f && WidthFloor(1u << 17) == 0.5f);

    // the bake: the cell's size, opaque in the middle, empty in a corner; nothing from an empty mask
    const std::vector<unsigned char> cell = Bake(m, 22);
    CHECK(cell.size() == 22u * 22u);
    CHECK(cell[11 * 22 + 11] == 255 && cell[21 * 22 + 0] == 0);
    CHECK(Bake(Mask(), 22).empty());
    // nothing reads: no triangle, no mask
    CHECK(TopDown(std::vector<L>()).empty());
    CHECK(!Silhouette(std::vector<Tri2>(), 128, 0, m));
    std::vector<L> bad = {{"\\MultiLevel\\Level0\\VMeshPart\\VMeshRef", MeshRef(0x1234)}};
    CHECK(TopDown(bad).empty()); // a reference to a library the file lacks

    // rank words: the pirates' and Liberty Navy's sets, a word two factions disagree on
    RankWords rw;
    const std::wstring bd[3] = {L"Rookie", L"Veteran", L"Ace"}, ln[3] = {L"Lt.", L"Capt.", L"Col."},
                       x1[3] = {L"Cpl.", L"Sgt.", L"Maj."}, x2[3] = {L"Ens.", L"Cpl.", L"Cmdr."};
    rw.AddFaction(bd), rw.AddFaction(ln), rw.AddFaction(x1), rw.AddFaction(x2);
    size_t len = 0;
    CHECK(rw.TierOf(L"Rookie Juni Ohara", &len) == 1 && len == 7);
    CHECK(rw.TierOf(L"Ace Bart Kessler") == 3);
    CHECK(rw.TierOf(L"Capt. Joan Hart") == 2);
    CHECK(rw.TierOf(L"Cpl. Ed Moss", &len) == 0 && len == 0); // tier 1 in one faction, 2 in another: no mark
    CHECK(rw.TierOf(L"Rookies Den") == 0);                     // a word that only starts with one
    CHECK(rw.TierOf(L"Ace") == 0);                             // the word alone is not a name
    CHECK(rw.TierOf(L"Planet Manhattan") == 0);
    CHECK(rw.Ambiguous() == 1);
}
