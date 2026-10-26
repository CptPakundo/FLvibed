// Extended HUD 2's skin over the game's non-flight windows - see uiskin.h. The design and the build rules:
// docs\UI_HUD2_EXTENDED.md 9-10 (10.1 pinned); the geometry, colours and order: tools\uimock\hud2x.py (its plates are
// re-expressed here relative to each window's own controls, so the plates follow the window's open slide and every
// aspect); the technique, offsets and the per-screen art lists: research\evidence\controller_ui\hud2x_feasibility.md
// and research\experience\runs\hud2x_layers_2026-10-25\ (the probe that measured them was folded in here, 25 Oct 2026).
//
// Writes (each remembered, each restored only while it still holds FLvibed's value):
//   art      state byte +0x6C bit 1 cleared on a static-image art child (bit 0 kept: the harness's FindVisible still
//            sees it); re-cleared every frame (the NavBar re-shows its frame / tab each frame; the others cost a compare).
//            Restore: bit 1 set again only while bit 0 is set (a control the game hid since keeps the game's state).
//   own      +0x84 (the window's / button's own mesh handle) = -1 after the draw slot was checked to reach the signed
//            WindowBaseDraw; restored while still -1.
//   colour   +0x348 of a title text (VtTitleText only) = CHROME; restored while still CHROME.
//   tooltip  the top-level `ToolTip`'s bit 1 cleared while uistrip's character card replaces it (uistrip::CardTooltip),
//            re-cleared each frame; given back (bit 1 while bit 0) the frame the card goes.
// Under the game's letterbox (exe::LetterboxAmount > 0: room scenes, launch, dock) the non-modal windows are restored
// and drawn native; the rescan after the letterbox writes them again.
#include "uiskin.h"

#include "../fl.h"
#include "debug.h"
#include "flexe.h"
#include "game/room.h"
#include "glyphs.h"
#include "host.h"
#include "hud2_kit.h"
#include "log.h"
#include "promptbar.h"
#include "ui.h"
#include "uiopt.h"
#include "uistrip.h"
#include "game/render.h"

#include <sstream>
#include <vector>

namespace uiskin
{
namespace
{
using namespace hud2kit;

// ---------------------------------------------------------------- guarded memory
bool R8(u32 a, u8& v)
{
    __try
    {
        v = *reinterpret_cast<const volatile u8*>(a);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
bool R32(u32 a, u32& v)
{
    __try
    {
        v = *reinterpret_cast<const volatile u32*>(a);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
bool RBytes(u32 a, void* out, size_t n)
{
    __try
    {
        memcpy(out, reinterpret_cast<const void*>(a), n);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
bool W8(u32 a, u8 v)
{
    __try
    {
        *reinterpret_cast<volatile u8*>(a) = v;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
bool W32(u32 a, u32 v)
{
    __try
    {
        *reinterpret_cast<volatile u32*>(a) = v;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

constexpr u32 kParent = 0x04, kSibling = 0x08, kChild = 0x0C, kName = 0x18, kState = 0x6C, kOwnMesh = 0x84,
              kColour = 0x348, kLit = 0x333;
constexpr u32 kNoMesh = 0xFFFFFFFFu;

bool NameIs(u32 ctl, const char* want)
{
    char n[33] = {};
    return RBytes(ctl + kName, n, 32) && strncmp(n, want, 32) == 0;
}
bool Shown(u32 ctl) // bit 0: the game's own "visible" (bit 1 may be ours)
{
    u8 st = 0;
    return ctl && R8(ctl + kState, st) && (st & 1);
}
bool FullyShown(u32 ctl)
{
    u8 st = 0;
    return ctl && R8(ctl + kState, st) && (st & 3) == 3;
}
bool Enabled(u32 ctl)
{
    u8 st = 0;
    return ctl && R8(ctl + kState, st) && (st & 8);
}

// the first descendant of `root` named `name` (any state), depth-first; 0 none
u32 Child(u32 root, const char* name, int depth = 0)
{
    if (!root || depth > 12)
        return 0;
    u32 c = 0;
    R32(root + kChild, c);
    for (int guard = 0; c && guard < 600; ++guard)
    {
        if (NameIs(c, name))
            return c;
        if (const u32 d = Child(c, name, depth + 1))
            return d;
        if (!R32(c + kSibling, c))
            break;
    }
    return 0;
}

bool IsAncestor(u32 root, u32 ctl)
{
    for (int i = 0; ctl && i < 32; ++i)
    {
        if (ctl == root)
            return true;
        if (!R32(ctl + kParent, ctl))
            return false;
    }
    return false;
}

// The control's projected rectangle (pixels: x0 y0 x1 y1) and centre; false when not projectable.
struct Box
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0, cx = 0, cy = 0;
    bool box = false; // it has extents (else only the anchor point)
};
bool RectOf(u32 ctl, Box& b)
{
    ui::Item it;
    if (!ctl || !ui::ControlRect(ctl, it))
        return false;
    b.cx = it.x, b.cy = it.y;
    b.box = it.hw > 0.5f && it.hh > 0.5f;
    b.x0 = it.x - it.hw, b.x1 = it.x + it.hw, b.y0 = it.y - it.hh, b.y1 = it.y + it.hh;
    return true;
}

// ---------------------------------------------------------------- the families (docs\UI_HUD2_EXTENDED.md 10; the mock)
enum Cond
{
    Always,
    AnchorShown, // drawn while the anchor control is shown (ErrorHelp, the save description field)
    Xfer,        // dealer: the transfer panel is up (BuySellTradeButton shown)
    NoXfer,
    SellFull, // dealer: the player's list has rows (NN_Inventory ItemView01 shown)
    SellEmpty,
};
// One plate. anchor: "" = the window root's own rect (edges + the offsets), "#Name" = that control's rect edges +
// offsets, "Name" = that control's centre + offsets. Offsets and the rules in fractions of H.
struct PlateDef
{
    const char* anchor;
    float x0, y0, x1, y1;
    int corners;
    float rule;       // a header rule this far under the plate's top (0 none)
    float div1, div2; // full-width dividers under the top (0 none)
    Cond cond;
    float fill;
    // the bottom lane (docs\UI_HUD2_EXTENDED.md 13, rule 3): while the prompt lane is in use the plate ends here instead
    // of y1 (same anchor rule) - its content's bottom + 0.006 H, measured in the lane probe (0 = never trimmed)
    float trim = 0;
};
constexpr int kTLBR = kTL | kBR;

enum FocusKind
{
    FocusButton, // the hovered button's own rect
    FocusShift,  // ... moved down by `dy` (the pause boxes' drawn frame sits under their hit box)
    FocusLeft,   // ... widened to the left by `dx` (the job board's icon cell)
    FocusPair,   // ... joined with its sibling of the same number (prefix a <-> prefix b: a dealer row = icon + view)
};
struct FamilyDef
{
    const char* label;
    hk::Sym* vt;
    bool modal;     // the 70 % scrim; the pills' bottom lane
    bool ownRoot;   // the root draws its own art (+0x84)
    const char* art[6];
    const char* own[8];  // children drawing their own art (+0x84): hidden, and given a tab plate when `tabs`
    bool tabs;
    PlateDef plates[6];
    const char* titles[3];
    FocusKind focus;
    const char* fa; // focus prefix a (FocusShift / FocusLeft: the prefix it applies to)
    const char* fb;
    float fd;
};

// The plates: the mock's rectangles (hud2x.py) minus the anchor's position in the layered captures' rects.json.
const FamilyDef kFamilies[] = {
    {"pause", &exe::VtPauseMenu, true, false, {"Pause_BlackPlane", "PauseFrame"}, {}, false,
     {{"PauseFrame", -0.269f, -0.318f, 0.269f, 0.312f, kTLBR, 0.066f, 0, 0, Always, 0.85f}},
     // (the boxes' drawn frames coincide with their hit boxes in game, 25 Oct 2026 at 4K - not 0.027 H lower as the
     // layered captures' pixel regions suggested: no shift)
     {"PauseTitle"}, FocusButton, nullptr, nullptr, 0},
    {"loadsave", &exe::VtGameLoadScreen, true, false, {"LoadSave_BlackPlane", "LoadSave_Frame", "LoadSave_InfoFrame"},
     {"LoadSave_SaveGame", "LoadSave_LoadGame", "LoadSave_Delete", "LoadSave_MainMenu", "LoadSave_ResumeGame",
      "LoadSave_DescripButton"},
     true,
     {{"LoadSave_Frame", -0.372f, -0.350f, 0.368f, 0.360f, kTLBR, 0.072f, 0, 0, Always, 0.80f},
      {"LoadSave_InfoFrame", -0.160f, -0.127f, 0.179f, 0.113f, kTLBR, 0, 0, 0, Always, 0.80f}},
     {"LoadSave_Title"}, FocusButton, nullptr, nullptr, 0},
    // the front-end load screen (main menu -> LOAD GAME; root vt 0x5E2A84 = VtLoadScreen): the in-game load / save
    // family's look - its art is two named static images (`bigBox` the list frame, `smallBox` the info box: hidden by
    // bit 1), both plates from the list's anchor point (ui::ControlRect of a front list / button has no extents; the
    // list's point = its first row's top-left, 1560, 510 at 4K = 0.722, 0.236 H; the front buttons' points are not
    // their centres) - measured at 4K from the verify frames: the frame 0.689..1.531 x 0.172..0.880 H with the title
    // above it, the info box 0.300..0.634 x 0.640..0.871 H; the bottom lane; the front buttons draw their own frames
    // (front-button class, no +0x84 checked) and the title is a front label (no +0x348): both native.
    // research\evidence\controller_ui\hud2x_inventory.md 1.3, hud2x_trees.md "load_front"
    {"loadfront", &exe::VtLoadScreen, true, false, {"bigBox", "smallBox"}, {}, false,
     {{"CharSelDesc", -0.033f, -0.121f, 0.809f, 0.644f, kTLBR, 0.072f, 0, 0, Always, 0.80f},
      {"CharSelDesc", -0.422f, 0.404f, -0.088f, 0.635f, kTLBR, 0, 0, 0, Always, 0.80f}},
     {}, FocusButton, nullptr, nullptr, 0},
    // (in game: Options_BlackPlane + OptionsBox; from the main menu: `big box` and the CREDITS page button - measured
    // in game 25 Oct 2026, big box's anchor (0.986, 0.533) H, its frame 0.674..1.497 x 0.168..0.964 with the title above)
    {"options", &exe::VtPrefs, true, false, {"Options_BlackPlane", "OptionsBox", "big box"},
     {"interfaceButton", "keymapButton", "VideoButton", "AudioButton", "CreditsButton", "FLvibedOptionsButton",
      "MainMenuButton"},
     true,
     // (trimmed for the lane: in game the DEFAULT / CANCEL row ends 0.902 H -> 0.908; the front end's MAIN MENU button
     // ends 0.923 H -> 0.929 - research\evidence\controller_ui\hud2x_lane_probe.md 1)
     {{"OptionsBox", -0.179f, -0.399f, 0.591f, 0.371f, kTLBR, 0.078f, 0, 0, Always, 0.85f, 0.359f},
      {"big box", -0.312f, -0.408f, 0.511f, 0.431f, kTLBR, 0.080f, 0, 0, Always, 0.85f, 0.396f}},
     {}, FocusButton, nullptr, nullptr, 0},
    {"jobboard", &exe::VtMissionVendor, false, false,
     {"MissionVendorBackground", "MissionVendorHeadlinesFrame", "MissionVendorStoryFrame", "MissionVendorVideoFrame"}, {},
     false, {{"MissionVendorBackground", -0.504f, -0.437f, 0.536f, 0.213f, kTLBR, 0.066f, 0.406f, 0, Always, 0.85f}},
     {"MissionVendorTitle"}, FocusLeft, "MissionVendorSummaryBox", nullptr, 0.082f},
    {"news", &exe::VtNews, false, false, {"NewsBackground", "NewsHeadlinesFrame", "NewsStoryFrame", "NewsVideoFrame"}, {},
     // (trimmed for the lane: the ticker ends 0.903 H, 0.905 at 21:9 -> 0.909)
     false, {{"NewsBackground", -0.504f, -0.437f, 0.536f, 0.343f, kTLBR, 0.066f, 0.410f, 0.700f, Always, 0.85f, 0.3224f}},
     {"NewsTitle"}, FocusButton, nullptr, nullptr, 0},
    {"offer", &exe::VtOfferDialog, false, false, {"OfferDialogBackground"}, {}, false,
     {{"OfferDialogBackground", -0.439f, -0.285f, 0.431f, 0.265f, kTLBR, 0.085f, 0, 0, Always, 0.85f}},
     {"OfferDialogTitle"}, FocusButton, nullptr, nullptr, 0},
    // the commodity / equipment dealer: NN_Dealer + its partner NN_Inventory (whose own mesh is all four panels)
    {"dealer", &exe::VtDealer, false, false, {}, {}, false,
     {{"Inventory_Close", -1.250f, -0.038f, -0.828f, 0.457f, kTLBR, 0.092f, 0, 0, SellFull, 0.85f},
      {"Inventory_Close", -1.250f, -0.038f, -0.828f, 0.127f, kTLBR, 0.092f, 0, 0, SellEmpty, 0.85f},
      // (the transfer plate reaches under its QUANTITY n / m label, 0.668 H at 4K = 0.459 under the X; the infocard
      // plate starts 0.012 H below it - critique 3, 5)
      {"Inventory_Close", -0.810f, -0.038f, -0.418f, 0.478f, kTLBR, 0, 0, 0, Xfer, 0.85f},
      {"Inventory_Close", -0.810f, -0.038f, -0.418f, 0.292f, kTLBR, 0, 0, 0, NoXfer, 0.85f},
      {"Inventory_Close", -0.404f, -0.038f, 0.022f, 0.422f, kTLBR, 0.092f, 0, 0, Always, 0.85f},
      {"Inventory_Close", -1.200f, 0.490f, -0.060f, 0.767f, kTLBR, 0.052f, 0, 0, Xfer, 0.85f}},
     {"PlayerTitle"}, FocusPair, "ItemButton", "ItemView", 0},
    {"shipdealer", &exe::VtShipTrader, false, true, {}, {}, false,
     {{"ShipTraderCancelButton", -1.2365f, 0.007f, -0.8265f, 0.222f, kTLBR, 0.075f, 0, 0, Always, 0.85f},
      // the centre panel keeps its full height (critique 3, 5: collapsed to its title, the live 3D preview and the
      // refusal floated over the room's display ship); the native frame 0.680..1.098 x 0.160..0.702 H at 4K, the X at
      // 1.500, 0.208 H; it holds the refusal (ErrorHelp) too, so no plate of its own
      {"ShipTraderCancelButton", -0.8125f, -0.033f, -0.4025f, 0.470f, kTLBR, 0, 0, 0, Always, 0.85f},
      // (the dealer list's plate reaches up over the close X - 0.04 H higher than the mock, whose X hung outside it)
      {"ShipTraderCancelButton", -0.3885f, -0.033f, 0.0235f, 0.352f, kTLBR, 0.115f, 0, 0, Always, 0.85f},
      {"ShipTraderCancelButton", -1.1885f, 0.482f, -0.0985f, 0.767f, kTLBR, 0.052f, 0, 0, Always, 0.85f}},
     {"ShipTraderTitle"}, FocusPair, "ShipTraderShipNameIconFrame", "ShipTraderShipNameFrame", 0},
    // the repair dialog (its own art, like the NeuroNet windows; verify 25 Oct 2026: it had no family and stayed native
    // among skinned screens): one plate from the close X (its frame measured at 4K: 0.630..1.178 x 0.195..0.804 H, the
    // X at 1.148, 0.228 H), the rule under the title and its credits line; the title `RepairList` (a title text at the
    // window's top, 0.233 beside the close X at 0.231 - research\evidence\controller_ui\hud2x_trees.md "repair")
    // recoloured as the others
    {"repair", &exe::VtRepair, false, true, {}, {}, false,
     {{"RepairCancelButton", -0.528f, -0.043f, 0.040f, 0.586f, kTLBR, 0.104f, 0, 0, Always, 0.85f}},
     {"RepairList"}, FocusButton, nullptr, nullptr, 0},
    // the NeuroNet windows: one plate from the root's own rect
    {"inventory", &exe::VtInventory, false, true, {}, {}, false,
     {{"", -0.007f, -0.012f, 0.019f, 0.015f, kTLBR, 0.057f, 0, 0, Always, 0.85f}}, {"PlayerTitle"}, FocusPair, "ItemButton",
     "ItemView", 0},
    {"log", &exe::VtStoryStar, false, true, {}, {}, false,
     {{"", -0.007f, -0.012f, 0.019f, 0.015f, kTLBR, 0.057f, 0, 0, Always, 0.85f}}, {"StoryStarTitle"}, FocusButton, nullptr,
     nullptr, 0},
    {"playerinfo", &exe::VtPlayerInfo, false, true, {}, {}, false,
     {{"", -0.007f, -0.012f, 0.019f, 0.015f, kTLBR, 0.057f, 0, 0, Always, 0.85f}}, {"PlayerInfoMainTitle"}, FocusButton,
     nullptr, nullptr, 0},
    {"help", &exe::VtHelp, false, true, {}, {}, false,
     {{"", -0.007f, -0.012f, 0.019f, 0.015f, kTLBR, 0.057f, 0, 0, Always, 0.85f}}, {"PlayerInfoTitle"}, FocusButton, nullptr,
     nullptr, 0},
    // the nav map stays native (fails closed): its draw 0x4926D0 neither is nor calls the base draw within reach, so
    // its own art's +0x84 is not trusted (the probe wrote it unchecked); its side windows hold the mission / info
    // content and have no plate measured. Only the focus. (Asked of HOST-X: the nav map's draw path.)
    {"navmap", &exe::VtNavMap, false, false, {}, {}, false, {}, {}, FocusButton, nullptr, nullptr, 0},
    // the yes / no dialog: its own dimming box stays (decided); only the focus
    {"confirm", &exe::VtConfirm, false, false, {}, {}, false, {}, {}, FocusButton, nullptr, nullptr, 0},
    // the docked frame: handled by DrawDocked (the art here; the plates from the buttons)
    {"navbar", &exe::VtNavBar, false, false, {"ManeuverFrame", "ManeuverTab"}, {}, false, {}, {}, FocusButton, nullptr,
     nullptr, 0},
    {"nnbox", &exe::VtNNMainButton, false, false, {"NNBackground"}, {}, false, {}, {}, FocusButton, nullptr, nullptr, 0},
};
constexpr int kFamilyCount = sizeof(kFamilies) / sizeof(kFamilies[0]);
int FamilyIndex(const char* label)
{
    for (int i = 0; i < kFamilyCount; ++i)
        if (!strcmp(kFamilies[i].label, label))
            return i;
    return -1;
}
const int kFamDealer = FamilyIndex("dealer"), kFamInventory = FamilyIndex("inventory"), kFamNavBar = FamilyIndex("navbar"),
          kFamNNBox = FamilyIndex("nnbox"), kFamOptions = FamilyIndex("options"),
          kFamOffer = FamilyIndex("offer"), kFamConfirm = FamilyIndex("confirm"), kFamLog = FamilyIndex("log"),
          kFamNavMap = FamilyIndex("navmap"), kFamShipDealer = FamilyIndex("shipdealer"), kFamPause = FamilyIndex("pause"),
          kFamJobBoard = FamilyIndex("jobboard");

// ---------------------------------------------------------------- the registry
struct Art
{
    u32 ctl;
    char name[12]; // its name's start, revalidated before each write
};
struct Own
{
    u32 ctl;
    u32 before;
};
struct Colour
{
    u32 ctl;
    u32 before;
    u32 mine; // the value written (restored only while it still holds it)
};
struct Win
{
    u32 root = 0, vt = 0;
    int fam = -1;
    u32 partner = 0, partnerVt = 0; // the dealer's NN_Inventory
    std::vector<Art> art;
    std::vector<Own> own;
    std::vector<Colour> colour;
    std::vector<u32> tabs;            // controls given a tab plate
    std::vector<u32> tabCaps;         // their caption text controls (Options: the current page's is the game's yellow)
    u32 field = 0;                    // the save description's field (LoadSave_DescripButton)
    u32 anchors[6] = {};              // per plate
    u32 condCtl = 0, condCtl2 = 0, condCtl3 = 0; // dealer: BuySellTradeButton, the partner's ItemView01, its Full_Cargobar
    bool alive = true;
};
std::vector<Win> g_wins;

Context g_c;
bool g_on = false;      // the skin is wanted this frame
bool g_applied = false; // something is written
u32 g_gen = 0, g_genSeen = 0xFFFFFFFFu;
bool g_genHost = false;
ULONGLONG g_lastPoll = 0;
u32 g_rescans = 0, g_reclears = 0, g_ownRefused = 0;
float g_lowestModalY1 = 0; // pixels, for the prompt lane
bool g_modalOpen = false, g_dockedFrame = false, g_dockedLane = false;
float g_nnRight = 0, g_nnBottom = 0;
// What the prompt pills must never cover (pixels, this frame's glass, drawn or not): the room bar and its sub-row, the
// NeuroNet box, every window's plates and tabs (a plate-less window: its own rect). Collected by Geometry, read by
// LaneLeft / LowestKeepOut.
struct KeepOut
{
    float x0, y0, x1, y1;
};
std::vector<KeepOut> g_keep;
// The keep-outs are collected by the geometry pass (both ui_style values, every frame the pad is the device: Geometry);
// the paint pass (ui_style 1: Draw) runs the same code with g_paint set, which draws and collects nothing twice.
bool g_paint = false;
bool g_laneTrim = false; // the prompt lane is in use this frame: plates with a `trim` end there
void Keep(float x0, float y0, float x1, float y1)
{
    if (!g_paint && x1 > x0 && y1 > y0)
        g_keep.push_back({x0, y0, x1, y1});
}
int g_sw = 0, g_sh = 0;
float g_lastMs = 0;
// The game's letterbox is up (room entry / story scenes, launch, dock): the non-modal windows go native (their writes
// restored, no plates, no docked frame - the game hides the room bar and the NeuroNet box itself then); modal menus
// keep their skin. Re-applied by a rescan the frame the letterbox is gone.
bool g_scene = false;
u32 g_scenes = 0;
// The native ToolTip hidden (bit 1) while uistrip's character card replaces it - only then.
u32 g_tip = 0, g_tipHides = 0;

float LetterboxNow()
{
    if (!exe::LetterboxAmount.ok())
        return 0;
    __try
    {
        return Mem<float>(exe::LetterboxAmount);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

// HostApi v23 UiGeneration, when this build's api.h has it (SFINAE: the logic compiles against an older api.h too)
template <class H> auto HostGen(const H* h, u32& g, int) -> decltype(h->UiGeneration(), bool())
{
    if (!h || h->version < 23 || !h->UiGeneration)
        return false;
    g = static_cast<u32>(h->UiGeneration());
    return true;
}
template <class H> bool HostGen(const H*, u32&, long) { return false; }

// the fallback's cheap check: the flat list's length and a checksum of its controls (at most every 500 ms)
u32 ListSignature()
{
    u32 head = 0, node = 0;
    if (!exe::UiControls.ok() || !R32(static_cast<u32>(exe::UiControls.get()), head) || !head)
        return 0;
    R32(head + 4, node);
    u32 sig = 2166136261u;
    for (int n = 0; node && node != head && n < 8000; ++n)
    {
        u32 ctl = 0;
        if (!R32(node + 8, ctl) || !R32(node + 4, node))
            break;
        sig = (sig ^ ctl) * 16777619u;
    }
    return sig;
}

// The own-art write is allowed only where the draw slot is the signed base draw or a draw that calls / jumps to it
// within its first bytes (NN_Inventory / NN_Dealer 0x479700, the icon button's draw): read once per window.
bool DrawReachesBase(u32 ctl)
{
    if (!exe::WindowBaseDraw.ok())
        return false;
    const u32 base = static_cast<u32>(exe::WindowBaseDraw.get());
    u32 vt = 0, draw = 0;
    if (!R32(ctl, vt) || !R32(vt + 0x34, draw) || !draw)
        return false;
    if (draw == base)
        return true;
    // the icon button class (signed vtable): its own draw 0x55AF90 draws its mesh only while +0x84 != -1 (0x55B360,
    // read 25 Oct 2026; the probe hid the save screen's buttons this way) - not a call to the base draw. (A signed
    // IconButtonDraw symbol would let the slot itself be checked: asked of HOST-X.)
    if (exe::VtIconButton.is(vt))
        return true;
    u8 code[0x300] = {}; // (NN_Inventory / NN_Dealer 0x479700 calls it at +0x126)
    if (!RBytes(draw, code, sizeof(code)))
        return false;
    for (int i = 0; i + 5 <= static_cast<int>(sizeof(code)); ++i)
        if (code[i] == 0xE8 || code[i] == 0xE9)
        {
            i32 rel = 0;
            memcpy(&rel, code + i + 1, 4);
            if (draw + i + 5 + rel == base)
                return true;
        }
    return false;
}

bool StartsWithName(u32 ctl, const char* name12)
{
    char n[12] = {};
    return RBytes(ctl + kName, n, 11) && strncmp(n, name12, 11) == 0;
}

void HideArt(Win& w, u32 ctl)
{
    if (!ctl)
        return;
    for (const Art& a : w.art)
        if (a.ctl == ctl)
            return;
    Art a = {ctl, {}};
    RBytes(ctl + kName, a.name, sizeof(a.name) - 1);
    u8 st = 0;
    if (R8(ctl + kState, st) && (st & 2))
        W8(ctl + kState, static_cast<u8>(st & ~2));
    w.art.push_back(a);
}

void HideOwn(Win& w, u32 ctl)
{
    u32 h = 0;
    for (const Own& o : w.own)
        if (o.ctl == ctl)
            return;
    if (!ctl || !R32(ctl + kOwnMesh, h) || h == kNoMesh)
        return;
    if (!DrawReachesBase(ctl))
    {
        ++g_ownRefused;
        return;
    }
    if (W32(ctl + kOwnMesh, kNoMesh))
        w.own.push_back({ctl, h});
}

u32 Rgba(u32 rgb, u8 a) // hud2kit's 0xRRGGBB -> the text control's bytes R G B A
{
    return (static_cast<u32>(a) << 24) | ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb >> 16) & 0xFF);
}
const u32 kTitleColour = Rgba(kChrome, 0xFF);

void Recolour(Win& w, u32 ctl, u32 want = kTitleColour)
{
    u32 vt = 0, c = 0;
    if (!ctl || !R32(ctl, vt) || !exe::VtTitleText.is(vt) || !R32(ctl + kColour, c) || c == want)
        return;
    for (const Colour& k : w.colour)
        if (k.ctl == ctl)
            return;
    if (W32(ctl + kColour, want))
        w.colour.push_back({ctl, c, want});
}

// A page caption's colour (bytes R G B A) reads lit: the game's selected yellow (red well above blue; the normal caption
// is light blue 87 C3 E0)
bool CaptionLit(u32 colour, u32 selected)
{
    return (selected && colour == selected) || (colour & 0xFF) > ((colour >> 16) & 0xFF) + 0x40;
}
// FIX2 (final capture 25 Oct 2026, problem 5): in the front-end Options the GENERAL page was current (its button disabled,
// its content shown) while PERFORMANCE's caption kept the game's yellow from its click - two tabs read current. Under
// ui_style 1 a lit caption of a tab that is not the current page gets the normal caption colour, written with alpha 0xFE
// so the restore can tell our value from the game's own (restored only while it still holds ours); the game re-lighting
// it later is answered again the next frame.
constexpr u32 kUnlitCaption = 0xFEE0C387u; // 87 C3 E0, alpha FE
void Unlight(Win& w, u32 cap)
{
    u32 vt = 0, c = 0;
    if (!cap || !R32(cap, vt) || !exe::VtTitleText.is(vt) || !R32(cap + kColour, c) || c == kUnlitCaption)
        return;
    bool known = false;
    for (const Colour& k : w.colour)
        known |= k.ctl == cap;
    if (W32(cap + kColour, kUnlitCaption) && !known)
        w.colour.push_back({cap, c, kUnlitCaption});
}

// The current Options page's tab (0: none of the game's - FLvibed's own page, or nothing known). FIX2, measured 25 Oct
// 2026 on instance 3 (front end and in game): the game colours the current page's caption its selected yellow (in the
// front end ~1.5 s after the screen opens), while a button's disabled state is no page mark - in the front end the
// VideoButton stayed disabled after its page was left and the screen reopened on GENERAL (and AudioButton is disabled in a
// muted game). So: the lit caption; of several lit ones a disabled one; with none lit, a disabled tab (the old rule).
u32 CurrentTab(const Win& w, int page)
{
    if (page == 2 || w.tabs.size() != w.tabCaps.size())
        return 0;
    const u32 selected = exe::UiColourSelected.ok() ? Mem<u32>(exe::UiColourSelected) : 0;
    u32 lit = 0, litDisabled = 0, disabled = 0;
    for (size_t i = 0; i < w.tabs.size(); ++i)
    {
        const u32 t = w.tabs[i];
        if (!FullyShown(t) || NameIs(t, "FLvibedOptionsButton"))
            continue;
        u32 c = 0;
        const bool on = w.tabCaps[i] && R32(w.tabCaps[i] + kColour, c) && CaptionLit(c, selected);
        if (on && !lit)
            lit = t;
        if (on && !Enabled(t) && !litDisabled)
            litDisabled = t;
        if (!Enabled(t) && !disabled)
            disabled = t;
    }
    return litDisabled ? litDisabled : lit ? lit : disabled;
}

// The Options page buttons' captions (title texts the game colours UiColourSelected on the current page).
const char* CaptionOf(const char* button)
{
    static const char* const map[][2] = {{"interfaceButton", "InterfaceText"}, {"keymapButton", "keymapText"},
                                         {"VideoButton", "VideoText"},         {"AudioButton", "AudioText"},
                                         {"MainMenuButton", "mainMenuText"},   {"CreditsButton", "CreditsText"}};
    for (const auto& m : map)
        if (!strcmp(m[0], button))
            return m[1];
    return "";
}

// At open and at every later rescan of a kept window (its children may have been rebuilt: the NavBar's per room): the
// art list is resolved again (entries no longer found are dropped unwritten), new own-art / titles are added.
// The own-art hides (+0x84) of a window (ui_style 1 only).
void HideOwnAll(Win& w)
{
    const FamilyDef& f = kFamilies[w.fam];
    if (!g_on || (g_scene && !f.modal))
        return;
    if (f.ownRoot)
        HideOwn(w, w.root);
    if (w.partner)
        HideOwn(w, w.partner);
    for (const char* n : f.own)
        if (n)
            if (const u32 c = Child(w.root, n))
                HideOwn(w, c);
    g_applied = true;
}

// (under ui_style 0 - the geometry pass for the prompt lane - only the anchors, tabs and conditions are resolved: no
// write at all)
void Apply(Win& w)
{
    const FamilyDef& f = kFamilies[w.fam];
    if (g_scene && !f.modal && g_on)
        return; // native under the letterbox (written again by the rescan after it)
    const u32 roots[2] = {w.root, w.partner};
    w.art.clear();
    w.tabs.clear();
    w.tabCaps.clear();
    w.field = 0;
    if (g_on)
        for (const char* n : f.art)
            if (n)
                for (u32 r : roots)
                    if (r)
                        HideArt(w, Child(r, n));
    HideOwnAll(w);
    for (const char* n : f.own)
        if (n)
            if (const u32 c = Child(w.root, n))
            {
                if (f.tabs && !strstr(n, "Descrip"))
                {
                    w.tabs.push_back(c);
                    const char* cap = CaptionOf(n);
                    w.tabCaps.push_back(*cap ? Child(w.root, cap) : 0);
                }
                else if (strstr(n, "Descrip"))
                    w.field = c;
            }
    if (g_on)
        for (const char* n : f.titles)
            if (n)
                for (u32 r : roots)
                    if (r)
                        Recolour(w, Child(r, n));
    for (int i = 0; i < 6; ++i)
    {
        const PlateDef& p = f.plates[i];
        if (!p.anchor || !*p.anchor)
            continue;
        const char* n = p.anchor[0] == '#' ? p.anchor + 1 : p.anchor;
        w.anchors[i] = Child(w.root, n);
    }
    if (w.fam == kFamDealer)
    {
        w.condCtl = Child(w.root, "BuySellTradeButton");
        w.condCtl2 = w.partner ? Child(w.partner, "ItemView01") : 0;
        w.condCtl3 = w.partner ? Child(w.partner, "Full_Cargobar") : 0;
    }
    if (g_on)
        g_applied = true;
}

// After +0x84 is given back: the engine's mesh instance gets the control's current pose again. The window's setters
// (the FlControl vtable slots +0x14 position, +0x1C orientation - the base class calls them on every child with the
// parent's values: 0x5A0880 / 0x5A0950) store the pose at +0x60 / +0x3C and pass it to the engine only while +0x84 is a
// handle, so a window hidden through its open slide kept the mesh at its first pose (blank) after the restore. Calling
// them with the stored values repeats the game's last call (children included); nothing else changes. Each slot is
// called only while it holds a signed setter - the base ones (exe::ControlSetPosition / ControlSetOrientation) or the two
// position overrides that call the base first (exe::InventorySetPosition: NN_Inventory / NN_Dealer, the partner follows;
// exe::NavMapSetPosition) - any other override (the icon button's 0x55B4A0 / 0x55B5C0) or an unresolved symbol skips that
// control's repose (fail closed, logged once; `uiskin windows` counts it as reposeRefused).
constexpr u32 kPos = 0x60, kOrient = 0x3C;
u32 g_reposed = 0, g_reposeFaults = 0, g_reposeRefused = 0;
void Repose(u32 ctl)
{
    float pos[3] = {}, orient[9] = {};
    u32 vt = 0, setPos = 0, setOrient = 0;
    u8 st = 0;
    if (!R8(ctl + kState, st) || (st & 4) || !R32(ctl, vt) || !R32(vt + 0x14, setPos) || !R32(vt + 0x1C, setOrient) || !setPos || !setOrient ||
        !debug::SafeRead(ctl + kPos, pos, sizeof(pos)) || !debug::SafeRead(ctl + kOrient, orient, sizeof(orient)))
        return;
    const bool posOk = exe::ControlSetPosition.is(setPos) || exe::InventorySetPosition.is(setPos) || exe::NavMapSetPosition.is(setPos);
    if (!posOk || !exe::ControlSetOrientation.is(setOrient))
    {
        if (!g_reposeRefused++)
            Log("uiskin: repose refused - control vt 0x%08X slots +0x14 0x%08X / +0x1C 0x%08X are not signed setters", vt,
                setPos, setOrient);
        return;
    }
    __try
    {
        reinterpret_cast<void(__thiscall*)(u32, const float*)>(setOrient)(ctl, orient);
        reinterpret_cast<void(__thiscall*)(u32, const float*)>(setPos)(ctl, pos);
        ++g_reposed;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ++g_reposeFaults;
    }
}

void Restore(Win& w)
{
    for (const Art& a : w.art)
    {
        u8 st = 0;
        if (StartsWithName(a.ctl, a.name) && R8(a.ctl + kState, st) && (st & 1) && !(st & 2))
            W8(a.ctl + kState, static_cast<u8>(st | 2));
    }
    for (const Own& o : w.own)
    {
        u32 h = 0;
        if (R32(o.ctl + kOwnMesh, h) && h == kNoMesh && W32(o.ctl + kOwnMesh, o.before))
            Repose(o.ctl);
    }
    for (const Colour& c : w.colour)
    {
        u32 v = 0;
        if (R32(c.ctl + kColour, v) && v == c.mine)
            W32(c.ctl + kColour, c.before);
    }
    w.art.clear(), w.own.clear(), w.colour.clear(), w.tabs.clear();
}

// a cached window still exists: its vtable, not deleted (+0x6C bit 2)
bool Valid(u32 ctl, u32 vt)
{
    u32 v = 0;
    u8 st = 0;
    return ctl && R32(ctl, v) && v == vt && R8(ctl + kState, st) && !(st & 4);
}

void RestoreTip()
{
    u8 st = 0;
    if (g_tip && NameIs(g_tip, "ToolTip") && R8(g_tip + kState, st) && (st & 1) && !(st & 2))
        W8(g_tip + kState, static_cast<u8>(st | 2));
    g_tip = 0;
}

// every frame: the ToolTip under the card hidden (the game re-shows it while the hover lasts), else given back
void ApplyTip()
{
    u32 want = uistrip::CardTooltip();
    // on the pad, under a modal menu, the native button help goes too (critique 3, 1/3: a stale "Go to the options
    // screen." sat over QUIT TO MAIN MENU - the cursor it follows is not the pad's focus there); the mouse keeps it
    if (!want && g_c.pad)
    {
        bool modal = false;
        for (const Win& w : g_wins)
            modal |= kFamilies[w.fam].modal && Shown(w.root);
        if (modal)
        {
            static u32 tip = 0, tipGen = 0xFFFFFFFFu;
            if (tipGen != g_gen || (tip && !NameIs(tip, "ToolTip")))
                tip = static_cast<u32>(ui::FindControl("ToolTip")), tipGen = g_gen;
            want = tip;
        }
    }
    if (want && !NameIs(want, "ToolTip"))
        want = 0;
    if (g_tip && g_tip != want)
        RestoreTip();
    u8 st = 0;
    if (!want || !R8(want + kState, st))
        return;
    if (g_tip != want)
        ++g_tipHides;
    g_tip = want;
    if (st & 2)
        W8(want + kState, static_cast<u8>(st & ~2));
}

void RestoreAll()
{
    RestoreTip();
    for (Win& w : g_wins)
        if (Valid(w.root, w.vt))
            Restore(w);
    g_wins.clear();
    g_applied = false;
}

// The windows open now (one walk of the flat control list): kept, added (applied) or dropped (gone: nothing written).
void Rescan()
{
    ++g_rescans;
    u32 head = 0, node = 0;
    if (!exe::UiControls.ok() || !R32(static_cast<u32>(exe::UiControls.get()), head) || !head)
        return;
    R32(head + 4, node);
    struct Found
    {
        u32 ctl, vt;
        int fam;
    };
    Found found[24];
    int nf = 0;
    u32 inventory = 0, inventoryVt = 0;
    bool dealer = false, navbar = false;
    for (int n = 0; node && node != head && n < 8000 && nf < 24; ++n)
    {
        u32 ctl = 0, vt = 0;
        u8 st = 0;
        if (!R32(node + 8, ctl) || !R32(node + 4, node))
            break;
        if (!ctl || !R32(ctl, vt) || !R8(ctl + kState, st) || (st & 4))
            continue;
        for (int f = 0; f < kFamilyCount; ++f)
            if (kFamilies[f].vt->is(vt))
            {
                if (f == kFamInventory)
                    inventory = ctl, inventoryVt = vt;
                if (f == kFamDealer)
                    dealer = true;
                if (f == kFamNavBar)
                    navbar = true;
                found[nf++] = {ctl, vt, f};
                break;
            }
    }
    for (Win& w : g_wins)
        w.alive = false;
    for (int i = 0; i < nf; ++i)
    {
        Found& fd = found[i];
        if (fd.fam == kFamInventory && dealer)
            continue; // the dealer's partner (its panels' art): owned by the dealer's entry
        if (fd.fam == kFamNNBox && !navbar)
            continue; // the same NN_MainButton is the flight HUD's NeuroNet box in space: skinned only docked
        Win* have = nullptr;
        for (Win& w : g_wins)
            if (w.root == fd.ctl && w.vt == fd.vt)
                have = &w;
        if (have)
        {
            have->alive = true;
            if (have->fam == kFamDealer && have->partner != inventory)
                have->partner = inventory, have->partnerVt = inventoryVt;
            Apply(*have);
            continue;
        }
        Win w;
        w.root = fd.ctl, w.vt = fd.vt, w.fam = fd.fam;
        if (fd.fam == kFamDealer)
            w.partner = inventory, w.partnerVt = inventoryVt;
        Apply(w);
        g_wins.push_back(std::move(w));
    }
    for (size_t i = g_wins.size(); i-- > 0;)
        if (!g_wins[i].alive)
        {
            // gone from the list: destroyed (or about to be) - nothing is written to it; a window that is still valid
            // (e.g. the dealer's partner went) gets its bytes back first
            if (Valid(g_wins[i].root, g_wins[i].vt))
                Restore(g_wins[i]);
            g_wins.erase(g_wins.begin() + i);
        }
}

void Refresh()
{
    u32 g = 0;
    g_genHost = HostGen(g_host, g, 0);
    if (!g_genHost)
    {
        const ULONGLONG now = GetTickCount64();
        if (now - g_lastPoll < 500 && g_genSeen != 0xFFFFFFFFu)
        {
            // between checks: drop what no longer validates (no writes to it)
            for (size_t i = g_wins.size(); i-- > 0;)
                if (!Valid(g_wins[i].root, g_wins[i].vt) ||
                    (g_wins[i].partner && !Valid(g_wins[i].partner, g_wins[i].partnerVt)))
                    g_wins.erase(g_wins.begin() + i);
            return;
        }
        g_lastPoll = now;
        g = ListSignature();
    }
    g_gen = g;
    if (g != g_genSeen)
    {
        g_genSeen = g;
        Rescan();
    }
}

// every frame: the art bits the game re-showed cleared again (the NavBar's frame / tab do it each frame)
void Reclear()
{
    for (Win& w : g_wins)
        for (const Art& a : w.art)
        {
            u8 st = 0;
            if (R8(a.ctl + kState, st) && (st & 2) && StartsWithName(a.ctl, a.name))
                W8(a.ctl + kState, static_cast<u8>(st & ~2)), ++g_reclears;
        }
}

// ---------------------------------------------------------------- drawing (fractions of H)
float g_H = 0, g_W = 0;
inline float Hf(float f) { return f * g_H; }

void Rim(float x0, float y0, float x1, float y1, float c, int corners, float a)
{
    const float tl = (corners & kTL) ? c : 0, tr = (corners & kTR) ? c : 0, br = (corners & kBR) ? c : 0,
                bl = (corners & kBL) ? c : 0;
    const float pts[16] = {x0 + tl, y0, x1 - tr, y0, x1, y0 + tr, x1, y1 - br, x1 - br, y1, x0 + bl, y1, x0, y1 - bl, x0, y0 + tl};
    Outline(pts, 8, std::max(2.0f, Hf(0.0013f)), Argb(kChrome, a));
}

// one HUD 2 plate: glass 85 %, the firmer rim (CHROME 70 %), chamfers 0.012 H
void Glass(float x0, float y0, float x1, float y1, int corners, float fill = 0.85f, float rimA = 0.69f)
{
    if (x1 - x0 < 4 || y1 - y0 < 4)
        return;
    const float c = Hf(0.012f);
    Plate(x0, y0, x1, y1, c, corners, fill, kChrome, 0);
    Rim(x0, y0, x1, y1, c, corners, rimA);
}

void RuleAt(float x0, float x1, float y, float a = 130 / 255.0f)
{
    Line(x0 + Hf(0.020f), y, x1 - Hf(0.020f), y, std::max(2.0f, Hf(0.0012f)), Argb(kChrome, a));
}

// The one focus style: the fill, ACTIVE edges top and bottom (critique 3, 7: the fill alone measured olive (86, 85, 49)
// over the navy glass and half-read "disabled") and the bar outside the left edge.
void FocusFill(float x0, float y0, float x1, float y1, float fill = 0.32f)
{
    const float g = Hf(0.008f), bw = Hf(0.005f), e = std::max(1.0f, Hf(0.002f));
    Rect(x0, y0, x1 - x0, y1 - y0, Argb(kActive, fill));
    Rect(x0, y0, x1 - x0, e, Argb(kActive, 0.85f));
    Rect(x0, y1 - e, x1 - x0, e, Argb(kActive, 0.85f));
    Rect(x0 - g - bw, y0, bw, y1 - y0, Argb(kActive, 1));
}
// The confirm dialog's focus: its own dimming box (kept, decided) is drawn over the UI pass's glass and dimmed the fill
// to (33, 27, 54) - so the confirm's focus box is remembered here and drawn in the overlay pass (DrawOver).
Box g_overFocus;
bool g_haveOverFocus = false;

void TabPlate(const Box& b, bool current)
{
    const float x0 = b.x0, y0 = b.y0 - Hf(0.004f), x1 = b.x1, y1 = b.y1 + Hf(0.004f), c = Hf(0.010f);
    Plate(x0, y0, x1, y1, c, kTLBR, 220 / 255.0f, kChrome, 0);
    Rim(x0, y0, x1, y1, c, kTLBR, current ? 200 / 255.0f : 120 / 255.0f);
    if (current)
        Line(x0 + Hf(0.012f), y1 - Hf(0.004f), x1 - Hf(0.012f), y1 - Hf(0.004f), Hf(0.004f), Argb(kActive, 1));
}

bool CondOk(const Win& w, const PlateDef& p, u32 anchor)
{
    switch (p.cond)
    {
    case Always: return true;
    case AnchorShown: return Shown(anchor);
    case Xfer: return FullyShown(w.condCtl);
    case NoXfer: return !FullyShown(w.condCtl);
    // (an empty player list collapses its plate - unless the cargo bar shows: a plate covers every visible native child,
    // critique 3, 5 - the "Cargo Space" bar floated on the room under the collapsed plate)
    case SellFull: return !w.condCtl2 || FullyShown(w.condCtl2) || Shown(w.condCtl3);
    case SellEmpty: return w.condCtl2 && !FullyShown(w.condCtl2) && !Shown(w.condCtl3);
    }
    return false;
}

// Focus = the game's selection where the window keeps one (critique 3, 1: the job board and the ship dealer marked the
// hovered row while the detail / NET described the selected one): a hovered list row is replaced by the list's selected
// row (the hover stays the mark for buttons, and for a list with nothing selected). The pad's row walk clicks the row it
// reaches (padbases), so on the pad both agree; with the mouse, or a native click from the harness, the selection wins.
// Returns the control to mark (hov when it is no row or nothing is selected).
// The text-button rows (vt 0x5E614C: job board MissionVendorSummaryBoxN, news NewsHeadlineBoxN, the dealers' ItemView0N)
// keep their own "selected" flag: +0x35C bit 0x10 (0x60 -> 0x70 on the row the game selected, 0x70 -> 0x60 on the one
// it left; +0x384 bit 2 moves with it) - measured 25 Oct 2026, instance 3, SaveC701, clicking rows natively and
// diffing the rows and windows. So: the hovered row's siblings of the same name (and for a pair, the partner name)
// are asked for the flag; a visible selected row is the mark, else the hover (nothing selected, or it scrolled away).
bool RowSelected(u32 ctl)
{
    u8 f = 0;
    return R8(ctl + 0x35C, f) && (f & 0x10);
}
size_t RowPrefix(const char* n) // the name without its trailing number ("ItemView03" -> 8), 0 = no number
{
    size_t len = strlen(n), p = len;
    while (p > 0 && isdigit(static_cast<u8>(n[p - 1])))
        --p;
    return p < len ? p : 0;
}
u32 SelectedSibling(u32 parent, const char* prefix, size_t plen)
{
    u32 c = 0;
    R32(parent + kChild, c);
    for (int guard = 0; c && guard < 200; ++guard)
    {
        char n[33] = {};
        RBytes(c + kName, n, 32);
        u32 vt = 0, setProp = 0;
        if (!strncmp(n, prefix, plen) && RowPrefix(n) == plen && FullyShown(c) && R32(c, vt) && R32(vt + 0xA8, setProp) &&
            exe::TextButtonSetProperty.is(setProp) && RowSelected(c))
            return c;
        if (!R32(c + kSibling, c))
            break;
    }
    return 0;
}
u32 SelectionFor(const Win& w, u32 hov)
{
    const FamilyDef& f = kFamilies[w.fam];
    // the job board: the selected row is the focus whatever is hovered (FIX2: at 21:9 the game's hover sat on ACCEPT
    // JOB after the entry, so the fill went there while the selected row kept only the game's outline - two marks, and
    // where the fill landed depended on the entry path). The pad's A (hold) accepts the selected job whatever the
    // focus, so the row is the honest mark; with no row selected the hover stays the mark.
    if (w.fam == kFamJobBoard && f.fa)
    {
        static u32 lastRoot = 0, lastGen = 0, rowParent = 0;
        if (w.root != lastRoot || g_gen != lastGen)
        {
            char first[40];
            sprintf_s(first, "%s0", f.fa);
            const u32 r0 = Child(w.root, first);
            rowParent = 0;
            if (r0)
                R32(r0 + kParent, rowParent);
            lastRoot = w.root, lastGen = g_gen;
        }
        if (rowParent)
            if (const u32 s = SelectedSibling(rowParent, f.fa, strlen(f.fa)))
                return s;
    }
    char name[33] = {};
    u32 parent = 0;
    if (!hov || !RBytes(hov + kName, name, 32) || !R32(hov + kParent, parent) || !parent)
        return hov;
    const size_t plen = RowPrefix(name);
    if (!plen)
        return hov; // no row
    if (const u32 s = SelectedSibling(parent, name, plen))
        return s;
    if (f.focus == FocusPair && f.fa && f.fb && !strncmp(name, f.fa, strlen(f.fa)) && plen == strlen(f.fa))
        if (const u32 s = SelectedSibling(parent, f.fb, strlen(f.fb)))
            return s;
    // the other list of a dealer (the player's / the dealer's): one selection on the screen
    if (w.fam == kFamDealer)
        for (u32 list : {w.root, w.partner})
        {
            u32 row = list && !IsAncestor(list, hov) ? Child(list, "ItemView01") : 0, p = 0;
            if (row && R32(row + kParent, p) && p)
                if (const u32 s = SelectedSibling(p, "ItemView", 8))
                    return s;
        }
    if (w.fam == kFamShipDealer && !strncmp(name, "ShipTraderShipName", 18))
    {
        u32 sel = 0xFFFFFFFFu; // NN_ShipTrader +0x3D0 the selected offer (0..2; baseinfo SelectedShip)
        if (R32(w.root + 0x3D0, sel) && sel < 3)
        {
            char want[40];
            sprintf_s(want, "ShipTraderShipNameIconFrame%u", sel);
            if (const u32 c = Child(w.root, want))
                return c;
        }
    }
    return hov;
}

// The drawn element of the hovered control (the one focus style: docs\UI_HUD2_EXTENDED.md 10 "Focus").
bool FocusBox(const Win& w, u32 hov, Box& out)
{
    hov = SelectionFor(w, hov);
    const FamilyDef& f = kFamilies[w.fam];
    if (!hov || !(IsAncestor(w.root, hov) || (w.partner && IsAncestor(w.partner, hov))) || !Enabled(hov))
        return false;
    for (u32 t : w.tabs) // never on the current page's tab (a disabled one)
        if (t == hov && !Enabled(t))
            return false;
    u32 vt = 0, setProp = 0;
    if (!R32(hov, vt) || !R32(vt + 0xA8, setProp))
        return false;
    const bool button = exe::TextButtonSetProperty.is(setProp) || exe::IconButtonSetProperty.is(setProp);
    char name[33] = {};
    RBytes(hov + kName, name, 32);
    if (!button || !RectOf(hov, out) || !out.box)
        return false;
    if (f.focus == FocusShift && f.fa && !strncmp(name, f.fa, strlen(f.fa)))
        out.y0 += Hf(f.fd), out.y1 += Hf(f.fd);
    else if (f.focus == FocusLeft && f.fa && !strncmp(name, f.fa, strlen(f.fa)))
        out.x0 -= Hf(f.fd);
    else if (f.focus == FocusPair && f.fa && f.fb)
    {
        const size_t la = strlen(f.fa), lb = strlen(f.fb);
        const char* other = nullptr;
        const char* num = nullptr;
        // (the longer prefix first: ShipTraderShipNameIconFrame vs ShipTraderShipNameFrame)
        if (!strncmp(name, f.fa, la) && isdigit(static_cast<u8>(name[la])))
            other = f.fb, num = name + la;
        else if (!strncmp(name, f.fb, lb) && isdigit(static_cast<u8>(name[lb])))
            other = f.fa, num = name + lb;
        u32 parent = 0;
        if (other && R32(hov + kParent, parent))
        {
            // the numbering differs per name (ItemButton2 / ItemView02): both forms
            // (looked up once per focus change, not per frame)
            static u32 lastHov = 0, lastSib = 0, lastGen = 0;
            u32 sib = lastSib;
            if (hov != lastHov || g_gen != lastGen)
            {
                char want[40];
                sprintf_s(want, "%s%d", other, atoi(num));
                sib = Child(parent, want);
                if (!sib)
                    sprintf_s(want, "%s%02d", other, atoi(num)), sib = Child(parent, want);
                lastHov = hov, lastSib = sib, lastGen = g_gen;
            }
            Box o;
            if (RectOf(sib, o) && o.box)
                out.x0 = std::min(out.x0, o.x0), out.y0 = std::min(out.y0, o.y0), out.x1 = std::max(out.x1, o.x1),
                out.y1 = std::max(out.y1, o.y1);
        }
    }
    return true;
}

bool DrawWindow(const Win& w, u32 hov) // true: it drew the focus
{
    const FamilyDef& f = kFamilies[w.fam];
    Box root;
    const bool haveRoot = RectOf(w.root, root) && root.box;
    for (int i = 0; i < 6; ++i)
    {
        const PlateDef& p = f.plates[i];
        if (!p.anchor)
            continue;
        Box a;
        float x0, y0, x1, y1;
        if (!*p.anchor)
        {
            if (!haveRoot)
                continue;
            x0 = root.x0 + Hf(p.x0), y0 = root.y0 + Hf(p.y0), x1 = root.x1 + Hf(p.x1), y1 = root.y1 + Hf(p.y1);
        }
        else
        {
            if (!w.anchors[i] || !RectOf(w.anchors[i], a) || !CondOk(w, p, w.anchors[i]))
                continue;
            if (p.anchor[0] == '#')
                x0 = a.x0 + Hf(p.x0), y0 = a.y0 + Hf(p.y0), x1 = a.x1 + Hf(p.x1), y1 = a.y1 + Hf(p.y1);
            else
                x0 = a.cx + Hf(p.x0), y0 = a.cy + Hf(p.y0), x1 = a.cx + Hf(p.x1), y1 = a.cy + Hf(p.y1);
        }
        if (p.trim > 0 && g_laneTrim) // (the lane in use: the plate ends at its content - rule 3)
            y1 += Hf(p.trim - p.y1);
        if (g_paint)
        {
            Glass(x0, y0, x1, y1, p.corners, p.fill);
            if (p.rule > 0)
                RuleAt(x0, x1, y0 + Hf(p.rule));
            if (p.div1 > 0)
                RuleAt(x0, x1, y0 + Hf(p.div1), 110 / 255.0f);
            if (p.div2 > 0)
                RuleAt(x0, x1, y0 + Hf(p.div2), 110 / 255.0f);
        }
        if (f.modal)
            g_lowestModalY1 = std::max(g_lowestModalY1, y1);
        Keep(x0, y0, x1, y1); // (modal plates too: the lane keeps clear of them in 2D)
    }
    // a non-modal window without plates (the nav map, the confirm box): its own rect keeps the pills off it (a rect
    // covering most of the screen is no window frame: ignored)
    bool anyPlate = false;
    for (const PlateDef& p : f.plates)
        anyPlate |= p.anchor != nullptr;
    if (!f.modal && !anyPlate && haveRoot && root.x1 - root.x0 < g_W * 0.9f)
        Keep(root.x0, root.y0, root.x1, root.y1);
    // the tab plates (Options' page buttons, the save / load side buttons); the current page = a disabled tab, or
    // FLvibed's own page (the host's page state)
    const int page = (w.fam == kFamOptions && HostAtLeast(18) && g_host->OptionsPageState) ? g_host->OptionsPageState() : 0;
    // one current tab (critique 3, 1; FIX2): CurrentTab - the game's lit caption first, a disabled tab only without one
    const u32 optCurrent = w.fam == kFamOptions && page == 1 ? CurrentTab(w, page) : 0;
    bool haveTabFocus = false;
    u32 currentTab = 0;
    Box tabFocus;
    for (size_t i = 0; i < w.tabs.size(); ++i)
    {
        const u32 t = w.tabs[i];
        Box b;
        if (!FullyShown(t) || !RectOf(t, b) || !b.box)
            continue;
        const bool flv = page && !w.tabCaps[i] && NameIs(t, "FLvibedOptionsButton");
        bool current = false;
        if (w.fam == kFamOptions && page == 2)
            current = flv;
        else if (w.fam == kFamOptions && page == 1)
            current = t == optCurrent; // (FIX2: CurrentTab - the lit caption first)
        if (g_paint)
            TabPlate(b, current);
        if (f.modal)
            g_lowestModalY1 = std::max(g_lowestModalY1, b.y1);
        Keep(b.x0, b.y0, b.x1, b.y1);
        if (current)
            currentTab = t;
        // (FIX2) the tab under the cursor: the front end's page buttons are front buttons - the game's hit test does
        // not report them and they are no text / icon buttons - so the pad's focus on them is found by rectangle
        const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
        if (!current && Enabled(t) && mx >= b.x0 && mx <= b.x1 && my >= b.y0 && my <= b.y1)
            tabFocus = b, haveTabFocus = true;
    }
    if (!g_paint)
        return false; // (the geometry pass: no field, no focus)
    // the save description's field (its own art hidden): a visible field
    if (const u32 d = w.field)
    {
        Box b;
        if (FullyShown(d) && RectOf(d, b) && b.box)
        {
            Plate(b.x0, b.y0, b.x1, b.y1, 0, 0, 150 / 255.0f, kChrome, 0);
            Rim(b.x0, b.y0, b.x1, b.y1, 0, 0, 140 / 255.0f);
        }
    }
    Box fb;
    // (FLvibed's own page draws its own focus and owns the pad: no second mark on the game's tabs - FIX2)
    if (w.fam == kFamOptions && page == 2)
        return false;
    // (never on the current page's tab - in game it is not disabled, its yellow caption marks it - FIX2)
    if ((currentTab && hov == currentTab) || !FocusBox(w, hov, fb))
    {
        if (!haveTabFocus)
            return false;
        fb = tabFocus;
    }
    if (w.fam == kFamConfirm)
        g_overFocus = fb, g_haveOverFocus = true;
    else
        FocusFill(fb.x0, fb.y0, fb.x1, fb.y1);
    return true;
}

// The docked frame: the room bar's slim glass under the room icons, the current room's yellow underline, the
// current room's sub-buttons' plate (the game's tab art hidden), the NeuroNet box's plate; LB / RB at the bar's ends
// in pad mode.
// draw = false: the geometry only (the prompt lane) - the game does not draw the bar / box during a conversation
void DrawDocked(const Win& bar, const Win* nn, bool draw)
{
    float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f, sx0 = 1e9f, sy0 = 1e9f, sx1 = -1e9f, sy1 = -1e9f;
    float litX = -1, anyRoom = 0;
    u32 c = 0;
    R32(bar.root + kChild, c);
    for (int guard = 0; c && guard < 64; ++guard)
    {
        char n[16] = {};
        RBytes(c + kName, n, 15);
        Box b;
        const bool button = !strncmp(n, "ButtonControl", 13), sub = !strncmp(n, "RoomControl", 11);
        if ((button || sub) && FullyShown(c) && RectOf(c, b) && b.box)
        {
            if (button)
            {
                bx0 = std::min(bx0, b.x0), by0 = std::min(by0, b.y0), bx1 = std::max(bx1, b.x1), by1 = std::max(by1, b.y1);
                u8 lit = 0;
                if (R8(c + kLit, lit) && lit == 1)
                    litX = b.cx;
            }
            else
                sx0 = std::min(sx0, b.x0), sy0 = std::min(sy0, b.y0), sx1 = std::max(sx1, b.x1), sy1 = std::max(sy1, b.y1),
                ++anyRoom;
        }
        if (!R32(c + kSibling, c))
            break;
    }
    if (bx1 > bx0)
    {
        const float x0 = bx0 - Hf(0.029f), x1 = bx1 + Hf(0.029f), y0 = -Hf(0.02f), y1 = by1 + Hf(0.020f);
        if (draw && g_paint)
            Glass(x0, y0, x1, y1, kBR | kBL, 0.82f);
        if (draw)
            Keep(x0, y0, x1, y1);
        if (litX < 0 && anyRoom) // no lit flag read: the room whose sub-row hangs under it
            litX = (sx0 + sx1) / 2;
        if (litX >= 0 && draw && g_paint)
        {
            const float u = Hf(0.0045f);
            Line(litX - Hf(0.030f), y1 - u * 1.6f, litX + Hf(0.030f), y1 - u * 1.6f, u, Argb(kActive, 1));
        }
        if (anyRoom && draw)
        {
            if (g_paint)
                Glass(sx0 - Hf(0.039f), sy0 - Hf(0.010f), sx1 + Hf(0.039f), sy1 + Hf(0.012f), kBR | kBL, 0.82f);
            Keep(sx0 - Hf(0.039f), sy0 - Hf(0.010f), sx1 + Hf(0.039f), sy1 + Hf(0.012f));
        }
        if (g_c.pad && draw) // the LB / RB glyphs at the bar's ends
            Keep(x0 - Hf(0.050f), y0, x1 + Hf(0.050f), y1);
        // one glyph, one meaning (critique 3, 4): not while an open window binds the bumpers itself - the dealers
        // (SIDE), the NeuroNet log (MISSION / LOG), the nav map (SYSTEM / UNIVERSE)
        bool bumpers = false;
        for (const Win& w : g_wins)
            if ((w.fam == kFamDealer || w.fam == kFamLog || w.fam == kFamNavMap) && Shown(w.root))
                bumpers = true;
        if (g_c.pad && draw && !bumpers && g_paint)
        {
            const float cy = (y0 + y1) / 2;
            Glyph(glyphs::LB, x0 - Hf(0.030f), cy, Hf(0.032f), Argb(kChrome, 1));
            Glyph(glyphs::RB, x1 + Hf(0.030f), cy, Hf(0.032f), Argb(kChrome, 1));
        }
    }
    if (!nn)
        return;
    float nx0 = 1e9f, ny1 = -1e9f, nx1 = -1e9f;
    c = 0;
    R32(nn->root + kChild, c);
    for (int guard = 0; c && guard < 64; ++guard)
    {
        char n[40] = {};
        RBytes(c + kName, n, 32);
        Box b;
        const size_t len = strlen(n);
        if (len > 4 && !strcmp(n + len - 4, "Root") && FullyShown(c) && RectOf(c, b) && b.box)
            nx0 = std::min(nx0, b.x0), nx1 = std::max(nx1, b.x1), ny1 = std::max(ny1, b.y1);
        if (!R32(c + kSibling, c))
            break;
    }
    if (nx1 > nx0)
    {
        // The box's plate only while the room bar's buttons are up: in a room's camera move (room -> dealer, 26 Oct
        // 2026, with and without FLUF) the game hides the bar's buttons and draws no NeuroNet icons while their
        // controls still read shown - the plate stood empty at the top right for ~1.5 s
        if (draw && g_paint && bx1 > bx0)
            Glass(nx0 - Hf(0.026f), -Hf(0.02f), nx1 + Hf(0.020f), ny1 + Hf(0.023f), kBL, 0.82f);
        if (draw)
            Keep(nx0 - Hf(0.026f), -Hf(0.02f), nx1 + Hf(0.020f), ny1 + Hf(0.023f));
        g_nnRight = nx1 + Hf(0.020f), g_nnBottom = ny1 + Hf(0.023f);
    }
}

// The control the focus is on: the pad's focus IS the game's cursor on a control (padbases / padmenus / the generic
// D-pad focus place it), and the game recomputes its hovered control only while it has the window's focus - so the
// game's own hit test at the cursor, asked again only when the cursor or the window set changed (cached).
// (OneCurrentTab, FIX2: exactly one current Options tab - see Unlight)
void OneCurrentTab(Win& w)
{
    if (w.fam != kFamOptions || w.tabs.size() != w.tabCaps.size())
        return;
    const int page = HostAtLeast(18) && g_host->OptionsPageState ? g_host->OptionsPageState() : 0;
    const u32 current = CurrentTab(w, page); // (0 on FLvibed's page: none of the game's)
    if (!current && page != 2)
        return;
    const u32 selected = exe::UiColourSelected.ok() ? Mem<u32>(exe::UiColourSelected) : 0;
    for (size_t i = 0; i < w.tabs.size(); ++i)
    {
        u32 c = 0;
        if (w.tabs[i] != current && w.tabCaps[i] && R32(w.tabCaps[i] + kColour, c) && CaptionLit(c, selected))
            Unlight(w, w.tabCaps[i]);
    }
}

u32 FocusedControl()
{
    static int lx = -1, ly = -1;
    static u32 lgen = 0, lhit = 0;
    static ULONGLONG lat = 0;
    const int mx = fl::MouseX(), my = fl::MouseY();
    const ULONGLONG now = GetTickCount64();
    if (mx != lx || my != ly || g_gen != lgen || now - lat > 250) // (and 4 Hz: a window sliding under a still cursor)
    {
        lx = mx, ly = my, lgen = g_gen, lat = now;
        lhit = static_cast<u32>(ui::HitTest(static_cast<float>(mx), static_cast<float>(my)));
    }
    return lhit ? lhit : static_cast<u32>(ui::Hovered());
}

// The geometry pass (both ui_style values, every frame the pad is the device): the docked / modal flags, the NeuroNet
// box's edge and every keep-out of the prompt lane (the plates' rectangles - under ui_style 0 those of the glass that
// would replace the game's art, which they were measured from - the room bar, its sub-row, the tabs), with no drawing
// and no write. The paint pass (Draw, ui_style 1) then draws the same rectangles.
void Geometry(void* device)
{
    g_lowestModalY1 = 0, g_modalOpen = false, g_dockedFrame = false, g_dockedLane = false, g_nnRight = 0, g_nnBottom = 0;
    g_keep.clear();
    g_paint = false;
    if (g_wins.empty() || !device)
        return;
    render::SetDevice(device);
    if (!render::ScreenSize(g_sw, g_sh) || g_sh < 100)
        return;
    g_H = static_cast<float>(g_sh), g_W = static_cast<float>(g_sw);
    g_laneTrim = promptbar::LaneWanted();
    const Win* bar = nullptr;
    const Win* nn = nullptr;
    bool conversation = false;
    for (const Win& w : g_wins)
    {
        if (w.fam == kFamNavBar)
            bar = &w;
        else if (w.fam == kFamNNBox)
            nn = &w;
        else if (kFamilies[w.fam].modal && Shown(w.root))
            g_modalOpen = true;
        else if (w.fam == kFamOffer && Shown(w.root))
            conversation = true;
    }
    // (not in a NeuroNet window's close: the game hides the bar's and the box's art that frame while their controls
    // still read shown - the empty plates stood one frame, 26 Oct 2026, research\experience\runs\dealer_close_2026-10-26)
    if (bar && !g_modalOpen && !g_scene && FullyShown(bar->root) && !room::NNWindowClosing())
    {
        g_dockedFrame = !conversation;
        g_dockedLane = true;
        DrawDocked(*bar, nn && Shown(nn->root) ? nn : nullptr, !conversation);
    }
    for (const Win& w : g_wins)
        if (w.fam != kFamNavBar && w.fam != kFamNNBox && Shown(w.root) && (!g_scene || kFamilies[w.fam].modal))
            DrawWindow(w, 0);
}

void Draw(void* device)
{
    g_haveOverFocus = false;
    if (g_wins.empty() || !device || g_sh < 100)
        return;
    struct PaintScope
    {
        PaintScope() { g_paint = true; }
        ~PaintScope() { g_paint = false; }
    } paint;
    // (the same text size as HUD 2's pass: hud2kit rebuilds its fonts when it changes)
    const uiopt::UiOptions& o = uiopt::Get();
    const float k = std::min(1.3f, std::max(0.8f, o.hudScale / 100.0f)) * std::min(1.5f, std::max(0.8f, o.textSize / 100.0f));
    if (!Begin(device, g_sw, g_sh, std::max(8, static_cast<int>(0.026f * g_H * k + 0.5f))))
        return;
    SetAlpha(1), SetOffset(0, 0);
    const Win* bar = nullptr;
    const Win* nn = nullptr;
    bool conversation = false; // a mission offer: the game hides the room bar and the NeuroNet box meanwhile
    for (const Win& w : g_wins)
    {
        if (w.fam == kFamNavBar)
            bar = &w;
        else if (w.fam == kFamNNBox)
            nn = &w;
        else if (kFamilies[w.fam].modal && Shown(w.root))
            g_modalOpen = true;
        else if (w.fam == kFamOffer && Shown(w.root))
            conversation = true;
    }
    if (g_modalOpen) // the full-screen scrim (the black planes are hidden)
        Rect(0, 0, g_W, g_H, Argb(0x060716, 0.70f));
    // the docked frame while the game draws it (not under a modal menu: the NavBar is not drawn while paused)
    // (not in a NeuroNet window's close: the game hides the bar's and the box's art that frame while their controls
    // still read shown - the empty plates stood one frame, 26 Oct 2026, research\experience\runs\dealer_close_2026-10-26)
    if (bar && !g_modalOpen && !g_scene && FullyShown(bar->root) && !room::NNWindowClosing())
    {
        g_dockedFrame = !conversation; // (the focus on the bar's buttons: not while hidden)
        g_dockedLane = true;
        DrawDocked(*bar, nn && Shown(nn->root) ? nn : nullptr, !conversation);
    }
    for (Win& w : g_wins)
        if (w.fam == kFamOptions && Shown(w.root))
            OneCurrentTab(w);
    const u32 hov = FocusedControl();
    bool focused = false, window = false;
    for (const Win& w : g_wins)
        if (w.fam != kFamNavBar && w.fam != kFamNNBox && Shown(w.root) && (!g_scene || kFamilies[w.fam].modal))
            focused |= DrawWindow(w, hov), window = true;
    // one focus mark per screen (critique 3, 1): while a window owns the input, no mark on the room bar / its sub-row
    // (Player Status / Inventory left a second bar on the room's sub-tab under the resting cursor)
    if (window)
        focused = true;
    // (not while a room character is hovered - the pad's focus is on the character strip then, and the game's resting
    // cursor over the room bar is no focus: verify 25 Oct 2026, a fill beside the Equipment icon under the bar's card)
    if (g_dockedFrame && !focused && !uistrip::CardTooltip()) // the room / NeuroNet buttons take the one focus style too (the NeuroNet windows
        for (const Win& w : g_wins) // hang under the NN box: theirs drew above)
            if ((w.fam == kFamNavBar || w.fam == kFamNNBox) && Shown(w.root))
            {
                Box fb;
                if (FocusBox(w, hov, fb))
                    FocusFill(fb.x0, fb.y0, fb.x1, fb.y1);
            }
    SetKeepDestAlpha();
    End();
}
} // namespace

void Update(float, const Context& c) { g_c = c; }

bool On() { return g_c.enabled && g_c.style == 1; }

void RenderUnder(void* device)
{
    perf::Scope ps("uiskin");
    const double t0 = perf::Now();
    static bool wasOn = false;
    g_on = On();
    if (!g_on)
    {
        if (g_applied || g_tip || wasOn)
            RestoreAll(), g_genSeen = 0xFFFFFFFFu;
        wasOn = false;
        // ui_style 0: the registry without writes and the geometry pass, for the prompt lane's keep-outs (the pad only)
        if (!g_c.enabled || !g_c.pad)
        {
            g_wins.clear();
            g_keep.clear();
            g_scene = false, g_modalOpen = g_dockedLane = g_dockedFrame = false, g_nnBottom = g_lowestModalY1 = 0;
            if (g_c.enabled && g_c.style == 0)
                uistrip::NativeCard(LetterboxNow() > 0.001f); // the game's name card off the character (mouse)
            return;
        }
        g_scene = LetterboxNow() > 0.001f;
        Refresh();
        Geometry(device);
        if (g_c.style == 0)
            uistrip::NativeCard(g_scene); // the game's name card off the character, clear of the keep-outs just collected
        return;
    }
    if (!wasOn)
    {
        RestoreAll(); // (the geometry pass's windows carry no writes: the rescan finds them again and writes)
        g_genSeen = 0xFFFFFFFFu;
        wasOn = true;
    }
    const bool scene = LetterboxNow() > 0.001f;
    if (scene != g_scene)
    {
        g_scene = scene;
        if (scene)
        {
            ++g_scenes;
            for (Win& w : g_wins)
                if (!kFamilies[w.fam].modal && Valid(w.root, w.vt))
                    Restore(w);
        }
        g_genSeen = 0xFFFFFFFFu; // (out of the scene: the rescan writes the windows again)
    }
    Refresh();
    Reclear();
    ApplyTip();
    Geometry(device);
    Draw(device);
    g_lastMs = static_cast<float>((perf::Now() - t0) * 1000.0);
}

bool ModalOpen()
{
    // (only while the skin already runs: its first frame on, or one going off, answers false - RenderUnder owns the
    // switch and the restore)
    if (!g_on || !On())
        return false;
    Refresh();
    for (const Win& w : g_wins)
        if ((kFamilies[w.fam].modal || w.fam == kFamConfirm) && Valid(w.root, w.vt) && Shown(w.root))
            return true;
    return false;
}

float NeuroNetBottom() { return g_dockedLane && g_nnBottom > 0 && g_sh > 0 ? g_nnBottom : 0; }

bool DockedScreen() { return g_dockedLane; }

bool ModalShown() { return g_modalOpen; }

float LowestKeepOut(float x0, float x1, float below)
{
    float y = 0;
    for (const KeepOut& k : g_keep)
        if (k.x1 > x0 && k.x0 < x1 && k.y1 > below)
            y = std::max(y, k.y1);
    return y;
}

void DrawOver(void* device)
{
    if (!g_on || !g_haveOverFocus || !device)
        return;
    int sw = 0, sh = 0;
    render::SetDevice(device);
    if (!render::ScreenSize(sw, sh) || sh < 100 || sw != g_sw || sh != g_sh) // (the box is this mode's pixels)
        return;
    if (!Begin(device, sw, sh, std::max(8, static_cast<int>(0.026f * sh + 0.5f))))
        return;
    SetAlpha(1), SetOffset(0, 0);
    // (over the dialog's text: a lighter fill than under the glass, the edges and bar carry the mark)
    FocusFill(g_overFocus.x0, g_overFocus.y0, g_overFocus.x1, g_overFocus.y1, 0.22f);
    End();
}

float LaneLeft(float W, float H, float y0, float y1, float right)
{
    const float gap = H * 0.012f;
    float left = W * 0.05f;
    auto block = [&](float x0, float by0, float x1, float by1)
    {
        if (by0 >= y1 || by1 <= y0 || x0 >= right)
            return;
        left = std::max(left, x1 >= right ? right : x1 + gap); // one over the lane's right end blocks the band
    };
    for (const KeepOut& k : g_keep) // (both ui_style values: the geometry pass)
        block(k.x0, k.y0, k.x1, k.y1);
    // the strip (drawn in the overlay before the pills: this frame's)
    const float sr = uistrip::StripRight(), sb = uistrip::StripBottom();
    if (sr > 0 && sb > 0)
        block(0, 0, sr, sb * H);
    return std::min(left, right);
}

void Shutdown()
{
    RestoreAll();
    g_genSeen = 0xFFFFFFFFu;
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    std::ostringstream o;
    if (what == "restore")
    {
        RestoreAll();
        g_genSeen = 0xFFFFFFFFu;
        return "uiskin: restored (it re-applies next frame while ui_style is 1)";
    }
    if (what == "scan") // every control of a known window class in the flat list now (research)
    {
        u32 head = 0, node = 0;
        if (!exe::UiControls.ok() || !R32(static_cast<u32>(exe::UiControls.get()), head) || !head)
            return "no control list";
        R32(head + 4, node);
        int n = 0;
        for (; node && node != head && n < 8000; ++n)
        {
            u32 ctl = 0, vt = 0;
            u8 st = 0;
            if (!R32(node + 8, ctl) || !R32(node + 4, node))
                break;
            if (!ctl || !R32(ctl, vt))
                continue;
            R8(ctl + kState, st);
            for (int f = 0; f < kFamilyCount; ++f)
                if (kFamilies[f].vt->is(vt))
                {
                    char tmp[96];
                    sprintf_s(tmp, "  %-10s %08X st=%02X\n", kFamilies[f].label, ctl, st);
                    o << tmp;
                }
        }
        return "list " + std::to_string(n) + " controls\n" + o.str();
    }
    if (what == "rescan")
    {
        g_genSeen = 0xFFFFFFFFu;
        return "uiskin: rescan next frame";
    }
    if (what == "windows" || what == "state" || what.empty())
    {
        o << "uiskin on=" << g_on << " style=" << g_c.style << " pad=" << g_c.pad << " gen=" << g_gen
          << (g_genHost ? " (host v23)" : " (poll 500 ms)") << " rescans=" << g_rescans << " reclears=" << g_reclears
          << " ownRefused=" << g_ownRefused << " reposed=" << g_reposed << "/" << g_reposeFaults << " reposeRefused=" << g_reposeRefused << " modal=" << g_modalOpen << " docked=" << g_dockedFrame
          << " scene=" << g_scene << " scenes=" << g_scenes << " tip=" << (g_tip ? 1 : 0) << " tipHides=" << g_tipHides
          << " ms=" << g_lastMs << " windows=" << g_wins.size();
        const u32 hov = FocusedControl();
        char hn[33] = {};
        u8 hs = 0;
        if (hov)
            RBytes(hov + kName, hn, 32), R8(hov + kState, hs);
        char tmp0[96];
        sprintf_s(tmp0, " hovered=%08X %s st=%02X\n", hov, hn, hs);
        o << tmp0;
        for (const KeepOut& k : g_keep) // the pills' keep-outs this frame (pixels)
        {
            char tmp[96];
            sprintf_s(tmp, "  keep %.0f,%.0f - %.0f,%.0f\n", k.x0, k.y0, k.x1, k.y1);
            o << tmp;
        }
        for (const Win& w : g_wins)
        {
            char tmp[200];
            sprintf_s(tmp, "  %-10s root=%08X%s shown=%d art=%zu own=%zu colour=%zu tabs=%zu partner=%08X\n",
                      kFamilies[w.fam].label, w.root, Valid(w.root, w.vt) ? "" : " (INVALID)", Shown(w.root) ? 1 : 0,
                      w.art.size(), w.own.size(), w.colour.size(), w.tabs.size(), w.partner);
            o << tmp;
        }
        return o.str();
    }
    return "usage: uiskin state | windows | restore | rescan";
}

bool PlateOffsets(const char* family, const char* anchor, float out[4])
{
    for (const FamilyDef& f : kFamilies)
        if (strcmp(f.label, family) == 0)
            for (const PlateDef& p : f.plates)
                if (p.anchor && strcmp(p.anchor, anchor) == 0)
                {
                    out[0] = p.x0, out[1] = p.y0, out[2] = p.x1, out[3] = p.y1;
                    return true;
                }
    return false;
}
} // namespace uiskin
