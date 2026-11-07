// FLvibed - the game's windows as FLvibed's screens meet them: the registry of the open windows by their signed vtables, their plates' rectangles
// and the prompt lane's keep-outs (winplates.h for the API). Core, because two modules need it alone: the pad's prompts keep clear of the
// windows on the classic HUD with the HUD module off, and Extended HUD 2 paints its glass on the same rectangles (modules\hud uiskin.*).
#include "ui/winplates.h"

#include "flexe.h"
#include "game/baseinfo.h"
#include "game/deckmode.h"
#include "game/fl.h"
#include "game/hostcalls.h"
#include "game/render.h"
#include "game/room.h"
#include "ui/gamecard.h"
#include "ui/ui.h"
#include "ui/views.h"
#include "ui/winfit.h"

namespace winplates
{
using namespace hud2kit;

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

bool TextOf(u32 ctl, std::wstring& out)
{
    u32 vt = 0, obj = 0, text = 0;
    out.clear();
    if (!ctl || !R32(ctl, vt) || !exe::VtTitleText.is(vt) || !R32(ctl + 0x334, obj) || !obj || !R32(obj, text) || !text)
        return false;
    for (int i = 0; i < 512; ++i)
    {
        wchar_t c = 0;
        if (!RBytes(text + static_cast<u32>(i) * 2, &c, 2))
            return false;
        if (!c)
            break;
        out.push_back(c);
    }
    return !out.empty();
}

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
u32 Child(u32 root, const char* name, int depth)
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

bool RectOf(u32 ctl, Box& b)
{
    ui::Item it;
    u32 vt = 0;
    const bool front = ctl && R32(ctl, vt) && exe::VtFrontButton.is(vt); // (the main menu's buttons: their own layout)
    bool found = false;
    if (ctl && exe::VtFrontTextButton.is(vt)) // (a .page button reports no extents of its own: the hit test's rectangle)
        for (const ui::Item& i : ui::CollectAllCached())
            if (i.ctl == ctl)
                it = i, found = true;
    if (!ctl || !(found || (front ? ui::FrontButtonRect(ctl, it) : ui::ControlRect(ctl, it))))
        return false;
    b.cx = it.x, b.cy = it.y;
    b.box = it.hw > 0.5f && it.hh > 0.5f;
    b.x0 = it.x - it.hw, b.x1 = it.x + it.hw, b.y0 = it.y - it.hh, b.y1 = it.y + it.hh;
    return true;
}

// The plates: the mock's rectangles (hud2x.py) minus the anchor's position in the layered captures' rects.json.
extern const FamilyDef kFamilies[] = {
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
    {"loadfront", &exe::VtLoadScreen, true, false, {"bigBox", "smallBox"}, {"LoadCharacter", "DeleteCharacter", "MainMenu"}, true,
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
     false, {{"MissionVendorBackground", -0.534f, -0.437f, 0.536f, 0.213f, kTLBR, 0.066f, 0.406f, 0, Always, 0.85f}}, // (x: centred on the screen, the air even: its left stood 0.03 H short)
     {"MissionVendorTitle"}, FocusLeft, "MissionVendorSummaryBox", nullptr, 0.082f},
    {"news", &exe::VtNews, false, false, {"NewsBackground", "NewsHeadlinesFrame", "NewsStoryFrame", "NewsVideoFrame"}, {},
     // (trimmed for the lane: the ticker ends 0.903 H, 0.905 at 21:9 -> 0.909)
     false, {{"NewsBackground", -0.534f, -0.437f, 0.536f, 0.343f, kTLBR, 0.066f, 0.410f, 0.700f, Always, 0.85f, 0.3224f}},
     {"NewsTitle"}, FocusButton, nullptr, nullptr, 0},
    {"offer", &exe::VtOfferDialog, false, false, {"OfferDialogBackground"}, {}, false,
     {{"OfferDialogBackground", -0.439f, -0.285f, 0.431f, 0.265f, kTLBR, 0.085f, 0, 0, Always, 0.85f}},
     {"OfferDialogTitle"}, FocusButton, nullptr, nullptr, 0},
    // the commodity / equipment dealer: NN_Dealer + its partner NN_Inventory (whose own mesh is all four panels)
    // (WelcomeLine: the game's line under its welcome title stands where a two-line title ends; a third line ran through it. The plate's rule takes its place, under the title as it stands)
    {"dealer", &exe::VtDealer, false, false, {"WelcomeLine"}, {}, false,
     {{"Inventory_Close", -1.250f, -0.038f, -0.828f, 0.457f, kTLBR, 0.098f, 0, 0, SellFull, 0.85f},
      {"Inventory_Close", -1.250f, -0.038f, -0.828f, 0.127f, kTLBR, 0.098f, 0, 0, SellEmpty, 0.85f},
      // (the transfer plate reaches under its QUANTITY n / m label; the infocard plate starts 0.010 H below it - critique 3,
      // 5. 5 Nov 2026: the infocard took 0.022 H more, for the larger text: windowtext's boxes follow these two numbers)
      {"Inventory_Close", -0.810f, -0.038f, -0.418f, 0.470f, kTLBR, 0, 0, 0, Xfer, 0.85f, 0, kStackLift},
      {"Inventory_Close", -0.810f, -0.038f, -0.418f, 0.355f, kTLBR, 0, 0, 0, NoXfer, 0.85f}, // (the welcome text at windowtext's size)
      {"Inventory_Close", -0.404f, -0.038f, 0.022f, 0.422f, kTLBR, 0.098f, 0, 0, Always, 0.85f},
      // (no rule: a long heading wraps and its second line stood on it)
      {"Inventory_Close", -1.200f, 0.480f, -0.060f, 0.779f, kTLBR, 0, 0, 0, Xfer, 0.85f}, // (not the panels' full width: the prompts' column stands beside it)
      // (a keep-out too: bare on the room the final stage's words took the lane's pills over them at 4K. No title, so no rule: it ran through the text)
      {"Inventory_Close", -1.200f, 0.480f, -0.060f, 0.779f, kTLBR, 0, 0, 0, ShipWords, 0.85f}},
     {"PlayerTitle"}, FocusPair, "ItemButton", "ItemView", 0},
    {"shipdealer", &exe::VtShipTrader, false, true, {}, {}, false,
     // (the X stands on the headings' line, windowtext's kShipCloseDrop under the game's place: the panels start at the headings, not 0.06 H over them, the owner, 6 Nov 2026)
     {{"ShipTraderCancelButton", -1.2365f, -0.039f, -0.8265f, 0.172f, kTLBR, 0.070f, 0, 0, Always, 0.85f}, // (its top and rule the dealer list's: the two side panels even)
      // the centre panel keeps its full height (collapsed to its title, the live 3D preview and the
      // refusal floated over the room's display ship); the native frame 0.680..1.098 x 0.160..0.702 H at 4K, the X at
      // 1.500, 0.208 H; it holds the refusal (ErrorHelp) too, so no plate of its own
      {"ShipTraderCancelButton", -0.8125f, -0.039f, -0.4025f, 0.420f, kTLBR, 0, 0, 0, Always, 0.85f},
      // (the dealer list's plate reaches up over the close X - 0.04 H higher than the mock, whose X hung outside it)
      {"ShipTraderCancelButton", -0.3885f, -0.039f, 0.0235f, 0.302f, kTLBR, 0.070f, 0, 0, Always, 0.85f, 0, 0, true},
      // (the air round its text even; not the panels' full width: the prompts' column stands beside it)
      {"ShipTraderCancelButton", -1.1885f, 0.432f, -0.0575f, 0.729f, kTLBR, 0, 0, 0, Always, 0.85f}},
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
     {{"", -0.007f, -0.012f, 0.019f, 0.015f, kTLBR, 0.056f, 0, 0, Always, 0.85f}}, {"StoryStarTitle"}, FocusButton, nullptr,
     nullptr, 0},
    {"playerinfo", &exe::VtPlayerInfo, false, true, {}, {}, false,
     {{"", -0.007f, -0.012f, 0.019f, 0.015f, kTLBR, 0.057f, 0, 0, Always, 0.85f}}, {"PlayerInfoMainTitle"}, FocusButton,
     nullptr, nullptr, 0},
    {"help", &exe::VtHelp, false, true, {}, {}, false,
     {{"", -0.007f, -0.012f, 0.019f, 0.015f, kTLBR, 0.056f, 0, 0, Always, 0.85f}}, {"PlayerInfoTitle"}, FocusButton, nullptr,
     nullptr, 0},
    // the nav map: only the focus here. Its frame's writes and its panels are the HUD's navskin, by the probes' reads of its draw (modules\hud\docs\UI_HUD2_SKIN.md 6.1):
    // this table's own check does not fit it, since its draw 0x4926D0 calls the art half 0x5A06A0 and never the base draw.
    {"navmap", &exe::VtNavMap, false, false, {}, {}, false, {}, {}, FocusButton, nullptr, nullptr, 0},
    // the dialogs (UI_HUD2_SKIN.md 6.2): their ornate art goes through skindialog, which alone decides whether a dialog takes the skin, and draws the plate
    // over the UI pass: under it the buttons of the screen behind would show through. Named art: 6.2's list; no plates here, no scrim of their own
    {"confirm", &exe::VtConfirm, false, false, {"Confirm_deleteBox"}, {}, false, {}, {}, FocusButton, nullptr, nullptr, 0},
    {"exit", &exe::VtExit, false, false, {"ExitFrame", "ExitTextFrame"}, {}, false, {}, {}, FocusButton, nullptr, nullptr, 0},
    {"error", &exe::VtError, false, false, {"Frame"}, {}, false, {}, {}, FocusButton, nullptr, nullptr, 0},
    {"prompt", &exe::VtPrompt, false, false, {"PromptFrame"}, {}, false, {}, {}, FocusButton, nullptr, nullptr, 0},
    {"netop", &exe::VtNetOperation, false, false, {"ExitFrame"}, {}, false, {}, {}, FocusButton, nullptr, nullptr, 0},
    // the docked frame: handled by DrawDocked (the art here; the plates from the buttons)
    {"navbar", &exe::VtNavBar, false, false, {"ManeuverFrame", "ManeuverTab"}, {}, false, {}, {}, FocusButton, nullptr,
     nullptr, 0},
    {"nnbox", &exe::VtNNMainButton, false, false, {"NNBackground"}, {}, false, {}, {}, FocusButton, nullptr, nullptr, 0},
    // the main menu (root vt 0x5E27CC = VtMainMenu, the static object 0x67B780): same layout, HUD 2 look - its five `bImg`
    // static images (FRONT_button.cmp, the button frames; re-created on each appearance, all of the same name) hidden,
    // a tab plate under each front button (VtFrontButton: rect from its +0x60 / +0xB4, ui::FrontButtonRect), the
    // captions and the clicks the game's own. Resolved by the registry and the skin's writes (the names are not unique). hud2x_feasibility.md 72
    {"mainmenu", &exe::VtMainMenu, false, false, {}, {}, true, {}, {}, FocusButton, nullptr, nullptr, 0},
    // the mission message box (pub::Player::PopUpDialog - M01's tutorial boxes, the story's YES / NO / LATER boxes; vt
    // 0x5D24F4 = VtMissionMessage): its root draws its own art like the NeuroNet windows - one plate from the root's own
    // rect, hidden only while that rect is known (HideOwnAll: else native)
    {"popup", &exe::VtMissionMessage, false, true, {}, {"MMOkButton"}, true,
     {{"", -0.007f, -0.012f, 0.019f, 0.015f, kTLBR, 0, 0, 0, Always, 0.85f}}, {"MissionMessageTitle"}, FocusButton, nullptr, nullptr, 0},
};
const int kFamilyCount = sizeof(kFamilies) / sizeof(kFamilies[0]);
int FamilyIndex(const char* label)
{
    for (int i = 0; i < kFamilyCount; ++i)
        if (!strcmp(kFamilies[i].label, label))
            return i;
    return -1;
}
extern const int kFamDealer = FamilyIndex("dealer");
extern const int kFamInventory = FamilyIndex("inventory");
extern const int kFamNavBar = FamilyIndex("navbar");
extern const int kFamNNBox = FamilyIndex("nnbox");
extern const int kFamOptions = FamilyIndex("options");
extern const int kFamOffer = FamilyIndex("offer");
extern const int kFamConfirm = FamilyIndex("confirm");
extern const int kFamLog = FamilyIndex("log");
extern const int kFamNavMap = FamilyIndex("navmap");
extern const int kFamShipDealer = FamilyIndex("shipdealer");
extern const int kFamPause = FamilyIndex("pause");
extern const int kFamJobBoard = FamilyIndex("jobboard");
extern const int kFamMainMenu = FamilyIndex("mainmenu");
extern const int kFamPopup = FamilyIndex("popup");
extern const int kFamExit = FamilyIndex("exit");
extern const int kFamError = FamilyIndex("error");
extern const int kFamPrompt = FamilyIndex("prompt");
extern const int kFamNetOp = FamilyIndex("netop");
extern const int kFamHelp = FamilyIndex("help");

std::vector<Win> g_wins;
bool g_on = false;
bool g_padDevice = false;
const Ops* g_ops = nullptr;
u32 g_gen = 0, g_genSeen = 0xFFFFFFFFu;
bool g_genHost = false;
static ULONGLONG g_lastPoll = 0;
u32 g_rescans = 0;
float g_lowestModalY1 = 0;
bool g_modalOpen = false, g_dockedFrame = false, g_dockedLane = false;
float g_nnRight = 0, g_nnBottom = 0;
std::vector<KeepOut> g_keep;
bool g_paint = false;
static bool g_laneTrim = false; // the prompt lane is in use this frame: plates with a `trim` end there
int g_sw = 0, g_sh = 0;
bool g_scene = false;
float g_H = 0, g_W = 0;
int g_jobHeightTest = 0;

static inline float Hf(float f) { return f * g_H; }

// The keep-outs are collected by the geometry pass; the paint pass runs the same code with g_paint set, which draws and collects nothing twice.
static void Keep(float x0, float y0, float x1, float y1)
{
    if (!g_paint && x1 > x0 && y1 > y0)
        g_keep.push_back({x0, y0, x1, y1});
}

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

bool CaptionLit(u32 colour, u32 selected)
{
    return (selected && colour == selected) || (colour & 0xFF) > ((colour >> 16) & 0xFF) + 0x40;
}

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

bool Valid(u32 ctl, u32 vt)
{
    u32 v = 0;
    u8 st = 0;
    return ctl && R32(ctl, v) && v == vt && R8(ctl + kState, st) && !(st & 4);
}


// At open and at every later rescan of a kept window (its children may have been rebuilt): its anchors, tabs and conditions, and the skin's
// writes when it is on. Without the skin, no write at all: the prompt lane's geometry only.
void Resolve(Win& w)
{
    const FamilyDef& f = kFamilies[w.fam];
    if (g_scene && !SceneSkinned(w.fam) && g_on)
        return; // native under the letterbox (written again by the rescan after it)
    w.art.clear();
    w.tabs.clear();
    w.tabCaps.clear();
    w.field = 0;
    if (g_on && g_ops)
        g_ops->Write(w);
    if (w.fam == kFamMainMenu) // its direct children: every front button (a tab plate each)
    {
        u32 c = 0;
        R32(w.root + kChild, c);
        for (int guard = 0; c && guard < 64; ++guard)
        {
            u32 vt = 0;
            if (R32(c, vt) && exe::VtFrontButton.is(vt))
                w.tabs.push_back(c), w.tabCaps.push_back(0);
            if (!R32(c + kSibling, c))
                break;
        }
    }
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
    for (int i = 0; i < kPlates; ++i)
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
        w.condCtl4 = Child(w.root, "TransferItemView");
        w.condCtl5 = Child(w.root, "BackToShipDealer");
        w.condCtl2 = w.partner ? Child(w.partner, "ItemView01") : 0;
        w.condCtl3 = w.partner ? Child(w.partner, "Full_Cargobar") : 0;
    }
    if (w.fam == kFamJobBoard)
        w.storyText = Child(w.root, "MissionVendorMissionFullText");
    if (w.fam == kFamHelp)
        w.storyText = Child(w.root, "PlayerInfoText");
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
            Resolve(*have);
            continue;
        }
        Win w;
        w.root = fd.ctl, w.vt = fd.vt, w.fam = fd.fam;
        if (fd.fam == kFamDealer)
            w.partner = inventory, w.partnerVt = inventoryVt;
        Resolve(w);
        g_wins.push_back(std::move(w));
    }
    for (size_t i = g_wins.size(); i-- > 0;)
        if (!g_wins[i].alive)
        {
            // gone from the list: destroyed (or about to be) - nothing is written to it; a window that is still valid
            // (e.g. the dealer's partner went) gets its bytes back first
            if (g_ops && Valid(g_wins[i].root, g_wins[i].vt))
                g_ops->Restore(g_wins[i]);
            g_wins.erase(g_wins.begin() + i);
        }
}

void Refresh()
{
    u32 g = 0;
    // (generation 0 = the host's UI-generation detours were refused, or nothing changed yet: the poll, else a single
    // rescan would be all the skin ever saw)
    if (hostcalls::g.UiGeneration)
        g = hostcalls::g.UiGeneration();
    g_genHost = g != 0;
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

bool CondOk(const Win& w, const PlateDef& p, u32 anchor)
{
    switch (p.cond)
    {
    case Always: return true;
    case AnchorShown: return Shown(anchor);
    case Xfer: return FullyShown(w.condCtl) || FullyShown(w.condCtl4);
    case NoXfer: return !FullyShown(w.condCtl) && !FullyShown(w.condCtl4);
    case ShipWords: return !FullyShown(w.condCtl) && !FullyShown(w.condCtl4) && Shown(w.condCtl5);
    // (an empty player list collapses its plate - unless the cargo bar shows: a plate covers every visible native child,
    // - the "Cargo Space" bar floated on the room under the collapsed plate)
    case SellFull: return !w.condCtl2 || FullyShown(w.condCtl2) || Shown(w.condCtl3);
    case SellEmpty: return w.condCtl2 && !FullyShown(w.condCtl2) && !Shown(w.condCtl3);
    }
    return false;
}

float RichTextBottom(u32 ctl)
{
    u32 vt = 0, view = 0, content = 0;
    Box b;
    if (!ctl || !R32(ctl, vt) || !exe::VtText.is(vt) || !R32(ctl + 0x35C, view) || !R32(ctl + 0x380, content) ||
        !RectOf(ctl, b))
        return 0;
    if (g_jobHeightTest > 0)
        content = static_cast<u32>(g_jobHeightTest);
    if (view == 0 || view > static_cast<u32>(g_sh) || content == 0 || content > 100000u)
        return 0;
    u32 top = 0;
    return (R32(ctl + 0x34C, top) && top && top < 16384 ? static_cast<float>(top) : b.cy) + static_cast<float>(std::min(view, content));
}

static float g_wordsRoom = 0;
void SetWordsRoom(float room) { g_wordsRoom = room; }
static float g_rootGrow = 0;
void SetRootGrow(float grow) { g_rootGrow = grow; }
static bool g_mapOver = false;
void SetMapOver(bool on) { g_mapOver = on; }

// The panels fitted to what they hold (winfit.h; the owner, 6 Nov 2026). Per family, per plate: how it takes part. Families not here stand as the table says: their content is
// not all controls to read (Options' pages, the menus, the repair list's rows, the news ticker that scrolls, the root-framed NeuroNet windows).
using FitKind = winfit::Kind;
struct FitRow
{
    const char* family;
    FitKind kinds[kPlates];
};
static const FitRow kFits[] = {
    // (the collapsed SellEmpty plate stays out of the row; the infocard and the final stage's words stand under it)
    {"dealer", {FitKind::Hug, FitKind::Keep, FitKind::Hug, FitKind::Hug, FitKind::Hug, FitKind::Under, FitKind::Under}},
    // (the centre holds the 3D view, which has no extents to read)
    {"shipdealer", {FitKind::Hug, FitKind::Match, FitKind::Hug, FitKind::Under}},
    {"jobboard", {FitKind::Hug}},
    {"offer", {FitKind::Hug}},
};
constexpr winfit::Rules kFitRules = {0.018f, 0.012f, 0.06f, 0.12f, 0.15f}; // fractions of H
constexpr int kFitEvery = 1; // frames between measures: every frame, or a panel lags what it backs (the owner, 6 Nov 2026)
constexpr float kFitSpeed = 3.0f; // an edge's travel, H per second
static std::vector<winfit::Rect> g_skinItems; // what the skin draws in a window itself: its panel holds that too
void ClearSkinItems() { g_skinItems.clear(); }
void AddSkinItem(float x0, float y0, float x1, float y1) { g_skinItems.push_back({x0, y0, x1, y1}); }

struct FitState
{
    u32 root = 0;
    float y0[kPlates] = {}, y1[kPlates] = {};         // what each edge is off the table's, pixels, now
    float wantY0[kPlates] = {}, wantY1[kPlates] = {}; // ... and where it is going
    bool measured = false;
    u32 mask = 0; // the plates standing: another set is another state of the window
    int age = 0, rest = 0;
    float cx = 0, cy = 0;
    ULONGLONG tick = 0;
    std::vector<winfit::Panel> panels; // the last solve, for the report
};
static FitState g_fit[32];

static const FitRow* FitOf(int fam)
{
    for (const FitRow& r : kFits)
        if (!strcmp(r.family, kFamilies[fam].label))
            return &r;
    return nullptr;
}

// A text's real height. The object's +0x24 leaves a blank line out (the welcome text: 156 for seven lines of 26), and the game draws line i at i x the line's height (0x4143C0), blank or not:
// so the panel ended a line short of the text it holds. A line's vt[0x18] gives its size, the height second.
static bool LinePitch(u32 line, int& pitch)
{
    int size[2] = {};
    __try
    {
        reinterpret_cast<void(__thiscall*)(u32, int*)>(Mem<u32>(Mem<u32>(line) + 0x18))(line, size);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    pitch = size[1];
    return true;
}
float TextHeightPx(u32 ctl)
{
    u32 obj = 0, a = 0, e = 0, h = 0, first = 0;
    if (!R32(ctl + 0x334, obj) || !obj || !R32(obj + 0x10, a) || !R32(obj + 0x14, e) || e <= a || !R32(obj + 0x24, h) || !h || h >= 4096)
        return 0;
    const int lines = static_cast<int>((e - a) >> 3);
    int pitch = 0;
    if (R32(a, first) && first && LinePitch(first, pitch) && pitch > 0 && pitch < 512)
        return std::max(static_cast<float>(h), static_cast<float>(pitch * lines));
    return static_cast<float>(h);
}

// Everything the eye sees in a window: text by its measured lines, rich text by what it really fills, buttons by their frames. No art (a static image's place is its camera's) and no
// scroll bar (no extents). The game's text stands at its top, line under line (0x4143C0); its left depends on its alignment, so a text is a place on the panel's axis, not a width.
static void FitItems(u32 parent, int depth, std::vector<winfit::Panel>& ps, float reach)
{
    if (!parent || depth > 8)
        return;
    u32 c = 0;
    R32(parent + kChild, c);
    for (int guard = 0; c && guard < 400; ++guard)
    {
        u32 vt = 0;
        if (FullyShown(c) && R32(c, vt))
        {
            Box b;
            if (exe::VtTitleText.is(vt) || exe::VtTextEdit.is(vt))
            {
                const float h = TextHeightPx(c);
                if (h > 0 && RectOf(c, b))
                    winfit::Add(ps, {b.cx, b.cy, b.cx, b.cy + h}, reach);
            }
            else if (exe::VtText.is(vt))
            {
                u32 left = 0, top = 0, w = 0, h = 0, content = 0;
                if (R32(c + 0x348, left) && R32(c + 0x34C, top) && R32(c + 0x358, w) && R32(c + 0x35C, h) && R32(c + 0x380, content) && w && h && w < 16384 && h < 16384)
                {
                    const float x = static_cast<float>(static_cast<int>(left)), y = static_cast<float>(static_cast<int>(top));
                    winfit::Add(ps, {x, y, x + static_cast<float>(w), y + static_cast<float>(content ? std::min(h, content) : h)}, reach);
                }
            }
            else if ((!exe::VtImage.is(vt) || NameIs(c, "Full_Cargobar")) && RectOf(c, b) && b.box && b.x1 - b.x0 < g_W * 0.9f)
                winfit::Add(ps, {b.x0, b.y0, b.x1, b.y1}, reach);
            FitItems(c, depth + 1, ps, reach);
        }
        if (!R32(c + kSibling, c))
            break;
    }
}

// The table's rectangles in, the fitted ones out. Measured in the geometry pass only, once the window stands still (its slide would be read as content); the paint pass reads the same state.
static void FitPlates(const Win& w, const FitRow& row, float x0[], float y0[], float x1[], float y1[], const bool on[], const Box& still, bool haveStill)
{
    FitState& s = g_fit[w.fam];
    if (s.root != w.root)
        s = FitState(), s.root = w.root;
    if (!g_paint)
    {
        // The window changed state (a row selected, a refusal, the list emptied): another plate stands, and it answers now, from nothing: an old offset left it short of its text
        u32 mask = 0;
        for (int i = 0; i < kPlates; ++i)
            mask |= on[i] ? 1u << i : 0;
        if (mask != s.mask)
        {
            s.mask = mask, s.measured = false, s.age = kFitEvery;
            for (int i = 0; i < kPlates; ++i)
                s.y0[i] = s.y1[i] = s.wantY0[i] = s.wantY1[i] = 0;
        }
        if (haveStill)
        {
            s.rest = fabsf(still.cx - s.cx) > 0.5f || fabsf(still.cy - s.cy) > 0.5f ? 0 : s.rest + 1;
            s.cx = still.cx, s.cy = still.cy;
        }
        if (s.rest >= 2 && ++s.age >= kFitEvery)
        {
            s.age = 0;
            std::vector<winfit::Panel> ps;
            int index[kPlates] = {};
            for (int i = 0; i < kPlates; ++i)
                if (on[i])
                {
                    winfit::Panel p;
                    p.plate = {x0[i], y0[i], x1[i], y1[i]};
                    p.kind = row.kinds[i];
                    if (kFamilies[w.fam].plates[i].rule > 0)
                        p.least = y0[i] + Hf(kFamilies[w.fam].plates[i].rule + kFitRules.air);
                    index[ps.size()] = i;
                    ps.push_back(p);
                }
            const float reach = Hf(kFitRules.reach);
            FitItems(w.root, 0, ps, reach);
            FitItems(w.partner, 0, ps, reach);
            for (const winfit::Rect& r : g_skinItems) // (the comparison, the trade-in, the refusal: no controls, but the eye reads them as the panel's)
                winfit::Add(ps, r, reach);
            for (size_t i = 0; i < ps.size(); ++i)
            {
                const PlateDef& def = kFamilies[w.fam].plates[index[i]];
                if (def.cond == Xfer && def.liftFoot > 0 && g_padDevice && ps[i].has && uiclick::FindVisible("QuantitySlider")) // (the pad's QUANTITY n / m line under the block)
                    ps[i].foot += Hf(0.030f);
            }
            const winfit::Rules r = {Hf(kFitRules.air), Hf(kFitRules.gap), Hf(kFitRules.snap), reach, Hf(kFitRules.least)};
            winfit::Solve(ps, r);
            for (size_t i = 0; i < ps.size(); ++i)
            {
                const int at = index[i];
                s.wantY0[at] = ps[i].plate.y0 - y0[at], s.wantY1[at] = ps[i].plate.y1 - y1[at];
                if (!s.measured) // (the first fit is where the window opens: nothing to travel from)
                    s.y0[at] = s.wantY0[at], s.y1[at] = s.wantY1[at];
            }
            s.measured = true;
            s.panels = std::move(ps);
        }
        const ULONGLONG now = GetTickCount64();
        const float step = Hf(kFitSpeed) * static_cast<float>(std::min<ULONGLONG>(now - s.tick, 100)) / 1000.0f;
        s.tick = now;
        for (int i = 0; i < kPlates; ++i)
            s.y0[i] = winfit::Toward(s.y0[i], s.wantY0[i], step), s.y1[i] = winfit::Toward(s.y1[i], s.wantY1[i], step);
    }
    for (int i = 0; i < kPlates; ++i)
        if (on[i])
            y0[i] += s.y0[i], y1[i] += s.y1[i];
}

bool FitOpen()
{
    for (const Win& w : g_wins)
        if (FitOf(w.fam) && Shown(w.root))
            return true;
    return false;
}

float FitBoxShift(int fam)
{
    if (fam < 0 || fam >= kFamilyCount || !g_on)
        return 0;
    const FitRow* row = FitOf(fam);
    if (!row)
        return 0;
    for (int i = 0; i < kPlates; ++i)
        if (row->kinds[i] == FitKind::Under && fabsf(g_fit[fam].wantY0[i]) > 0)
            return g_fit[fam].wantY0[i];
    return 0;
}

std::string FitReport()
{
    std::string o;
    char line[160];
    for (const Win& w : g_wins)
    {
        if (!FitOf(w.fam) || !Shown(w.root))
            continue;
        const FitState& s = g_fit[w.fam];
        for (const winfit::Panel& p : s.panels)
        {
            const float H = g_H > 0 ? g_H : 1;
            sprintf_s(line, "fit %s kind=%d plate %.3f %.3f %.3f %.3f air top=%.3f foot=%.3f left=%.3f right=%.3f\n", kFamilies[w.fam].label, static_cast<int>(p.kind), p.plate.x0 / H, p.plate.y0 / H, p.plate.x1 / H,
                      p.plate.y1 / H, p.has ? (p.top - p.plate.y0) / H : -1, p.has ? (p.plate.y1 - p.foot) / H : -1, p.has ? (p.left - p.plate.x0) / H : -1, p.has ? (p.plate.x1 - p.right) / H : -1);
            o += line;
        }
    }
    for (const Win& w : g_wins)
        if (FitOf(w.fam) && Shown(w.root))
            for (const winfit::Rect& r : g_skinItems)
            {
                bool inside = false;
                for (const winfit::Panel& p : g_fit[w.fam].panels)
                    inside = inside || (r.x1 > p.plate.x0 && r.x0 < p.plate.x1 && r.y0 >= p.plate.y0 && r.y1 <= p.plate.y1);
                if (!inside)
                {
                    sprintf_s(line, "over panel skin line %.0f %.0f %.0f %.0f\n", r.x0, r.y0, r.x1, r.y1);
                    o += line;
                }
            }
    return o.empty() ? "fit: no fitted window open\n" : o;
}

void WindowPlates(const Win& w, WindowPass& out)
{
    const FamilyDef& f = kFamilies[w.fam];
    Box root;
    const bool haveRoot = RectOf(w.root, root) && root.box;
    // The dealer's plates all hang on its right window's close X, and its two windows slide in from opposite sides: painted during the slide, they opened offset and then settled
    // (the owner, 6 Nov 2026). They wait for the X to stand still.
    // Standing still is not enough: the slide pauses on its way, and the plates painted there, offset, with the prompts pushed to the top right by them (the owner, 6 Nov 2026). They wait for the X
    // to reach its column, the same from the screen's middle at every size (0.611 H, read at 4K and at 800 lines), and until then the prompts keep clear of where the plates will stand.
    bool settled = true;
    float homeX = 0, homeY = 0;
    if (w.fam == kFamDealer || w.fam == kFamShipDealer)
    {
        static u32 rootWas = 0;
        static float xWas = 0, yWas = 0;
        static int rest = 0;
        static ULONGLONG restAt = 0;
        homeX = g_W / 2 + Hf(0.611f), homeY = Hf(w.fam == kFamDealer ? 0.209f : 0.259f);
        u32 ctl = 0;
        for (int i = 0; i < kPlates && !ctl; ++i)
            ctl = w.anchors[i];
        Box a;
        if (ctl && RectOf(ctl, a))
        {
            const ULONGLONG now = GetTickCount64();
            if (w.root != rootWas || fabsf(a.cx - xWas) > 0.5f || fabsf(a.cy - yWas) > 0.5f)
                rest = 0, restAt = now;
            else
                ++rest;
            rootWas = w.root, xWas = a.cx, yWas = a.cy;
            // (a mod's window that ends elsewhere: still for half a second is its place)
            settled = (fabsf(a.cx - homeX) <= Hf(0.006f) && rest >= 2) || now - restAt >= 500;
        }
    }
    float rx0[kPlates] = {}, ry0[kPlates] = {}, rx1[kPlates] = {}, ry1[kPlates] = {};
    bool on[kPlates] = {};
    Box still;
    bool haveStill = false;
    for (int i = 0; i < kPlates; ++i)
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
            x0 = root.x0 + Hf(p.x0), y0 = root.y0 + Hf(p.y0), x1 = root.x1 + Hf(p.x1), y1 = root.y1 + Hf(p.y1 + (w.fam == kFamLog || w.fam == kFamHelp || w.fam == kFamPopup ? g_rootGrow : 0));
        }
        else
        {
            if (!w.anchors[i] || !RectOf(w.anchors[i], a) || !CondOk(w, p, w.anchors[i]))
                continue;
            if (!settled)
                a.cx = homeX, a.cy = homeY;
            if (p.anchor[0] == '#')
                x0 = a.x0 + Hf(p.x0), y0 = a.y0 + Hf(p.y0), x1 = a.x1 + Hf(p.x1), y1 = a.y1 + Hf(p.y1);
            else
                x0 = a.cx + Hf(p.x0), y0 = a.cy + Hf(p.y0), x1 = a.cx + Hf(p.x1), y1 = a.cy + Hf(p.y1);
        }
        if (p.trim > 0 && g_laneTrim) // (the lane in use: the plate ends at its content - rule 3)
            y1 += Hf(p.trim - p.y1);
        if (p.cond == NoXfer && uiclick::FindVisible("BackToShipDealer")) // (the ship's final stage: the panel holds its BUY SHIP, which stood bare under it)
            y1 += Hf(0.115f);
        if (p.holdsWords)
            y1 += Hf(g_wordsRoom);
        if (p.liftFoot > 0 && !uiclick::FindVisible("QuantitySlider")) // (the ship's final stage keeps more of it: its refusal, two lines, rose with the block)
            y1 -= Hf(uiclick::FindVisible("BackToShipDealer") ? 0.02f : p.liftFoot);
        if (w.fam == kFamJobBoard) // (the plate holds its text: it grows to it, with the margin under the text it keeps today)
            if (const float bottom = RichTextBottom(w.storyText))
                y1 = std::max(y1, bottom + Hf(0.030f));
        if (w.fam == kFamHelp) // (INFO's plate ends at its content: one line in a 0.55 H plate breaks "never empty", R8)
            if (const float bottom = RichTextBottom(w.storyText))
                y1 = std::min(y1, bottom + Hf(0.030f));
        on[i] = true, rx0[i] = x0, ry0[i] = y0, rx1[i] = x1, ry1[i] = y1;
        if (!haveStill && *p.anchor)
            still = a, haveStill = true;
    }
    if (const FitRow* fit = g_on ? FitOf(w.fam) : nullptr) // (only over the skin's own plates: under the game's art its frames stand)
        FitPlates(w, *fit, rx0, ry0, rx1, ry1, on, still, haveStill && settled);
    for (int i = 0; i < kPlates; ++i)
    {
        if (!on[i])
            continue;
        const PlateDef& p = f.plates[i];
        const float x0 = rx0[i], y0 = ry0[i], x1 = rx1[i], y1 = ry1[i];
        if (g_paint && settled)
        {
            PlateDef q = p;
            if (p.cond == NoXfer)
            {
                const u32 title = Child(w.root, "WelcomeText");
                Box b;
                if (FullyShown(title) && RectOf(title, b) && TextHeightPx(title) > 0)
                    q.rule = (b.cy + TextHeightPx(title) + Hf(0.008f) - y0) / g_H;
            }
            // FLvibed's own page in the Options window has its own strip: the game's header rule under it ran through the page's first row at small
            // sizes (Shockwave via the owner, 7 Nov 2026, "HUD 2" crossed out at 80 %)
            if (w.fam == kFamOptions && hostcalls::g.OptionsPageState && hostcalls::g.OptionsPageState() == 2)
                q.rule = 0;
            g_ops->Plate(x0, y0, x1, y1, q);
        }
        else if (g_paint)
        {
            // (still reported, with nothing to see: the skin's watchdog takes a window with no plate for a fault after 30 frames, which a slide outlasts at a high frame rate)
            PlateDef held = p;
            held.rule = held.div1 = held.div2 = 0;
            g_ops->Plate(x0, y0, x0, y0, held);
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
    const int page = (w.fam == kFamOptions && hostcalls::g.OptionsPageState) ? hostcalls::g.OptionsPageState() : 0;
    // one current tab CurrentTab - the game's lit caption first, a disabled tab only without one
    const u32 optCurrent = w.fam == kFamOptions && page == 1 ? CurrentTab(w, page) : 0;
    out.page = page;
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
            current = t == optCurrent; // (CurrentTab - the lit caption first)
        if (g_paint)
            g_ops->Tab(b, current);
        if (f.modal)
            g_lowestModalY1 = std::max(g_lowestModalY1, b.y1);
        Keep(b.x0, b.y0, b.x1, b.y1);
        if (current)
            out.currentTab = t;
        // the tab under the cursor: the front end's page buttons are front buttons - the game's hit test does
        // not report them and they are no text / icon buttons - so the pad's focus on them is found by rectangle
        const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
        if (!current && Enabled(t) && mx >= b.x0 && mx <= b.x1 && my >= b.y0 && my <= b.y1)
            out.tabFocus = b, out.haveTabFocus = true;
    }
}

// What the room bar did, frame by frame, when it changed: `gametext bar` reads and clears it. A slide is over before a frame can be saved, so it is read as numbers
struct BarStep
{
    ULONGLONG tick;
    float bx0, bx1, by1, sx0, sx1, sy1, lit;
    int draw, rooms;
};
static std::vector<BarStep> g_barSteps;
static BarStep g_barWas = {};
static ULONGLONG g_barMoved = 0;
static void BarRecord(const BarStep& s)
{
    {
        const BarStep& l = g_barWas;
        const bool same = fabsf(l.bx0 - s.bx0) < 0.5f && fabsf(l.bx1 - s.bx1) < 0.5f && fabsf(l.by1 - s.by1) < 0.5f && fabsf(l.sx0 - s.sx0) < 0.5f && fabsf(l.sx1 - s.sx1) < 0.5f && fabsf(l.sy1 - s.sy1) < 0.5f &&
                          fabsf(l.lit - s.lit) < 0.5f && l.draw == s.draw && l.rooms == s.rooms;
        if (!same)
            g_barMoved = s.tick;
        g_barWas = s;
    }
    if (!g_barSteps.empty())
    {
        const BarStep& l = g_barSteps.back();
        if (fabsf(l.bx0 - s.bx0) < 0.5f && fabsf(l.bx1 - s.bx1) < 0.5f && fabsf(l.by1 - s.by1) < 0.5f && fabsf(l.sx0 - s.sx0) < 0.5f && fabsf(l.sx1 - s.sx1) < 0.5f && fabsf(l.sy1 - s.sy1) < 0.5f &&
            fabsf(l.lit - s.lit) < 0.5f && l.draw == s.draw && l.rooms == s.rooms)
            return;
    }
    if (g_barSteps.size() < 400)
        g_barSteps.push_back(s);
}
bool BarBusy() { return GetTickCount64() - g_barMoved < 300; }

bool WindowOpen()
{
    for (const Win& w : g_wins)
        if (w.fam != kFamNavBar && w.fam != kFamNNBox && w.fam != kFamMainMenu && Shown(w.root))
            return true;
    return false;
}

// The button of the room the player is in, as the game finds it: its builder compares each hotspot's room switch with the room's nickname (0x43F2E0). The bar's own marks say nothing: +0x333 reads
// no button lit, and the sub-row's middle, which stood in for it, slides, and for 0.6 s after a room change stands under another room's button (the owner's video, 6 Nov 2026)
static std::string g_pickedRoom; // the pad's ring pick while the bumpers step (PickRoom): the underline stands under it until it opens

void PickRoom(const std::string& label) { g_pickedRoom = label; }

DockedGeo g_docked;

bool SceneSkinned(int fam) { return kFamilies[fam].modal || fam == kFamOffer; }

// A station's trader and equipment dealer are virtual rooms of its Deck: the location stays the Deck, so DECK stood lit in them (Shockwave, 7 Nov 2026). The room is
// the virtual room of the actions on screen when a room button of the bar carries that name (the deck's own actions say Deck)
static std::string RoomNow()
{
    const std::string key = baseinfo::RoomKey();
    const std::vector<room::Hotspot> hs = room::Hotspots();
    for (const room::Hotspot& a : hs)
        if (!a.virt.empty() && _stricmp(a.virt.c_str(), key.c_str()) != 0 && !strncmp(a.control.c_str(), "RoomControl", 11) && a.ctl && FullyShown(a.ctl))
            for (const room::Hotspot& b : hs)
                if (!strncmp(b.control.c_str(), "ButtonControl", 13) && !_stricmp(b.label.c_str(), a.virt.c_str()))
                    return b.label;
    return key;
}

std::string CurrentRoomKey() { return g_pickedRoom.empty() ? RoomNow() : g_pickedRoom; }

static u32 CurrentRoomButton()
{
    static std::string key;
    static u32 gen = 0xFFFFFFFFu, ctl = 0;
    const std::string now = CurrentRoomKey();
    if (now != key || gen != g_gen)
    {
        key = now, gen = g_gen, ctl = 0;
        for (const room::Hotspot& h : room::Hotspots())
            if (!_stricmp(h.label.c_str(), now.c_str()) && !strncmp(h.control.c_str(), "ButtonControl", 13))
                ctl = h.ctl;
    }
    return ctl;
}

std::string BarReport()
{
    std::string o;
    char line[160];
    const ULONGLONG t0 = g_barSteps.empty() ? 0 : g_barSteps.front().tick;
    for (const BarStep& s : g_barSteps)
    {
        sprintf_s(line, "bar %5llu ms bar %.0f..%.0f foot %.0f sub %.0f..%.0f foot %.0f lit %.0f draw %d rooms %d\n", s.tick - t0, s.bx0, s.bx1, s.by1, s.sx0, s.sx1, s.sy1, s.lit, s.draw, s.rooms);
        o += line;
    }
    g_barSteps.clear();
    return o;
}

// The docked frame: the room bar's slim glass under the room icons, the current room's yellow underline, the
// current room's sub-buttons' plate (the game's tab art hidden), the NeuroNet box's plate; LB / RB at the bar's ends
// in pad mode.
// draw = false: the geometry only (the prompt lane) - the game does not draw the bar / box during a conversation
void DockedPlates(const Win& bar, const Win* nn, bool draw)
{
    float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f, sx0 = 1e9f, sy0 = 1e9f, sx1 = -1e9f, sy1 = -1e9f;
    float litX = -1, anyRoom = 0, allRoom = 0;
    float ax0 = 1e9f, ay0 = 1e9f, ax1 = -1e9f, ay1 = -1e9f; // every shown action, whatever button it stands under
    g_docked = DockedGeo();
    Box cur;
    const u32 curButton = CurrentRoomButton();
    const bool haveCur = curButton && FullyShown(curButton) && RectOf(curButton, cur) && cur.box;
    if (haveCur)
        litX = cur.cx;
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
            }
            else
            {
                ax0 = std::min(ax0, b.x0), ay0 = std::min(ay0, b.y0), ax1 = std::max(ax1, b.x1), ay1 = std::max(ay1, b.y1), ++allRoom;
                if (!haveCur || fabsf(b.cx - cur.cx) <= Hf(0.13f)) // (one left from the room before stands under another button)
                    sx0 = std::min(sx0, b.x0), sy0 = std::min(sy0, b.y0), sx1 = std::max(sx1, b.x1), sy1 = std::max(sy1, b.y1),
                    ++anyRoom;
            }
        }
        if (!R32(c + kSibling, c))
            break;
    }
    // none under the current room's button: the game centres them under the bar instead (a station's DECK at the bar's left
    // end, its three actions under the middle: they stood bare, without plate or words - Shockwave, 7 Nov 2026)
    if (!anyRoom && allRoom)
        sx0 = ax0, sy0 = ay0, sx1 = ax1, sy1 = ay1, anyRoom = allRoom;
    if (!g_paint)
        BarRecord({GetTickCount64(), bx1 > bx0 ? bx0 : -1, bx1 > bx0 ? bx1 : -1, bx1 > bx0 ? by1 : -1, anyRoom ? sx0 : -1, anyRoom ? sx1 : -1, anyRoom ? sy1 : -1, litX, draw ? 1 : 0, static_cast<int>(anyRoom)});
    if (bx1 > bx0)
    {
        const float x0 = bx0 - Hf(0.029f), x1 = bx1 + Hf(0.029f), y0 = -Hf(0.02f), y1 = by1 + Hf(0.020f);
        if (draw && g_paint)
            g_ops->Glass(x0, y0, x1, y1, kBR | kBL, 0.82f);
        if (draw)
            Keep(x0, y0, x1, y1);
        if (litX >= 0 && draw && g_paint)
            g_ops->Underline(litX, y1);
        if (anyRoom && draw)
        {
            if (g_paint)
                g_ops->Glass(sx0 - Hf(0.039f), sy0 - Hf(0.010f), sx1 + Hf(0.039f), sy1 + Hf(0.012f), kBR | kBL, 0.82f);
            Keep(sx0 - Hf(0.039f), sy0 - Hf(0.010f), sx1 + Hf(0.039f), sy1 + Hf(0.012f));
        }
        if (g_padDevice && draw) // the LB / RB glyphs at the bar's ends
            Keep(x0 - Hf(0.050f), y0, x1 + Hf(0.050f), y1);
        // one glyph, one meaning not while an open window binds the bumpers itself - the dealers
        // (SIDE), the NeuroNet log (MISSION / LOG), the nav map (SYSTEM / UNIVERSE)
        bool bumpers = views::HudNNStrip(); // (the strip's tabs)
        for (const Win& w : g_wins)
            if ((w.fam == kFamDealer || w.fam == kFamLog || w.fam == kFamNavMap) && Shown(w.root))
                bumpers = true;
        if (g_padDevice && draw && !bumpers && g_paint)
            g_ops->Bumpers(x0, x1, (y0 + y1) / 2);
        g_docked.live = draw, g_docked.glyphs = g_padDevice && !bumpers;
        g_docked.bx0 = bx0, g_docked.bx1 = bx1, g_docked.by1 = by1;
        g_docked.sub = anyRoom > 0, g_docked.sx0 = sx0, g_docked.sx1 = sx1, g_docked.sy1 = sy1;
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
            g_ops->Glass(nx0 - Hf(0.026f), -Hf(0.02f), nx1 + Hf(0.020f), ny1 + Hf(0.023f), kBL, 0.82f);
        if (draw)
            Keep(nx0 - Hf(0.026f), -Hf(0.02f), nx1 + Hf(0.020f), ny1 + Hf(0.023f));
        g_nnRight = nx1 + Hf(0.020f), g_nnBottom = ny1 + Hf(0.023f);
        g_docked.nn = bx1 > bx0, g_docked.nx0 = nx0, g_docked.nx1 = nx1, g_docked.ny1 = ny1;
    }
}

// The geometry pass (both ui_style values, every frame the pad is the device): the docked / modal flags, the NeuroNet
// box's edge and every keep-out of the prompt lane (the plates' rectangles - under ui_style 0 those of the glass that
// would replace the game's art, which they were measured from - the room bar, its sub-row, the tabs), with no drawing
// and no write. The paint pass (Draw, ui_style 1) then draws the same rectangles.
void Geometry(void* device)
{
    g_lowestModalY1 = 0, g_modalOpen = false, g_dockedFrame = false, g_dockedLane = false, g_nnRight = 0, g_nnBottom = 0;
    g_docked = DockedGeo();
    g_keep.clear();
    g_paint = false;
    if (g_wins.empty() || !device)
        return;
    render::SetDevice(device);
    if (!render::ScreenSize(g_sw, g_sh) || g_sh < 100)
        return;
    g_H = static_cast<float>(g_sh), g_W = static_cast<float>(g_sw);
    g_laneTrim = views::LaneWanted();
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
    if (bar && !g_modalOpen && !g_scene && !g_mapOver && FullyShown(bar->root) && !room::NNWindowClosing())
    {
        g_dockedFrame = !conversation;
        g_dockedLane = true;
        DockedPlates(*bar, nn && Shown(nn->root) ? nn : nullptr, !conversation);
    }
    for (const Win& w : g_wins)
        if (w.fam != kFamNavBar && w.fam != kFamNNBox && Shown(w.root) && (!g_scene || SceneSkinned(w.fam)) &&
            !(w.fam == kFamMainMenu && g_modalOpen)) // (the main menu's plates never over a modal screen's scrim)
        {
            WindowPass pass;
            WindowPlates(w, pass);
        }
}

void ClassicPass(void* device, bool enabled, bool pad, bool classic)
{
    g_padDevice = pad;
    // the registry without writes and the geometry pass, for the prompt lane's keep-outs (the pad only)
    if (!enabled || !pad)
    {
        g_wins.clear();
        g_keep.clear();
        g_scene = false, g_modalOpen = g_dockedLane = g_dockedFrame = false, g_nnBottom = g_lowestModalY1 = 0;
        if (enabled && classic)
            gamecard::NativeCard(LetterboxNow() > 0.001f); // the game's name card off the character (mouse)
        return;
    }
    g_scene = LetterboxNow() > 0.001f;
    Refresh();
    Geometry(device);
    if (classic)
        gamecard::NativeCard(g_scene); // the game's name card off the character, clear of the keep-outs just collected
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

bool Blocked(float x0, float y0, float x1, float y1)
{
    for (const KeepOut& k : g_keep)
        if (k.x0 < x1 && k.x1 > x0 && k.y0 < y1 && k.y1 > y0)
            return true;
    return false;
}

bool PanelBeside(float y0, float y1, float& right, float& top, float& bottom)
{
    bool found = false;
    for (const KeepOut& k : g_keep)
        if (k.y0 < y1 && k.y1 > y0 && (!found || k.x1 > right))
            found = true, right = k.x1, top = k.y0, bottom = k.y1;
    return found;
}

float LaneLeft(float W, float H, float y0, float y1, float right)
{
    const float gap = H * 0.012f;
    float left = W * deckmode::SideW();
    auto block = [&](float x0, float by0, float x1, float by1)
    {
        if (by0 >= y1 || by1 <= y0 || x0 >= right)
            return;
        left = std::max(left, x1 >= right ? right : x1 + gap); // one over the lane's right end blocks the band
    };
    for (const KeepOut& k : g_keep) // (both ui_style values: the geometry pass)
        block(k.x0, k.y0, k.x1, k.y1);
    // the strip (drawn in the overlay before the pills: this frame's)
    const float sr = views::HudStripRight(), sb = views::HudStripBottom();
    if (sr > 0 && sb > 0)
        block(0, 0, sr, sb * H);
    return std::min(left, right);
}

bool IsTitle(const char* control)
{
    for (const FamilyDef& f : kFamilies)
        for (const char* t : f.titles)
            if (t && strcmp(t, control) == 0)
                return true;
    return false;
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
} // namespace winplates
