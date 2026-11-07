// FLvibed - the game's windows as FLvibed's screens meet them (core\ui\winplates.cpp). The open windows are found by their signed vtables when the
// host's UI generation counter moves, on an older host by a cheap check at most every 500 ms; each family of windows has its plates. From
// them come the prompt lane's keep-outs: pixels, this frame's, for both ui_style values, every frame the pad is the device. An unknown window
// has none, so it fails closed: the lane treats it as nothing. Nothing here writes to the game. The writes and the glass are Extended HUD
// 2's (modules\hud uiskin.*), through `Ops`; without it the classic pass below is all that runs.
#pragma once
#include "common.h"
#include "hook.h"
#include "ui/hud2_kit.h"

#include <string>
#include <vector>

namespace winplates
{
// guarded memory: false on a control that went away
bool R8(u32 a, u8& v);
bool R32(u32 a, u32& v);
bool RBytes(u32 a, void* out, size_t n);
bool W8(u32 a, u8 v);
bool W32(u32 a, u32 v);
// A text control's string (VtTitleText only; its text object +0x334 holds the wide pointer at +0, reallocated at every set, so it is copied, never kept).
// False on any other class, on an empty text or a read that faults. docs\FREELANCER_RE.md "The game's text size is its font's".
bool TextOf(u32 ctl, std::wstring& out);
// The room the pad's bumpers picked, not yet entered ("" = none): the docked bar's underline stands under it (padbases, the owner's 7 Nov 2026 settle rule)
void PickRoom(const std::string& label);

constexpr u32 kParent = 0x04, kSibling = 0x08, kChild = 0x0C, kName = 0x18, kState = 0x6C, kOwnMesh = 0x84,
              kColour = 0x348, kLit = 0x333;
constexpr u32 kNoMesh = 0xFFFFFFFFu;

bool NameIs(u32 ctl, const char* want);
bool Shown(u32 ctl); // bit 0: the game's own "visible" (bit 1 may be the skin's)
bool FullyShown(u32 ctl);
bool Enabled(u32 ctl);
// the first descendant of `root` named `name` (any state), depth-first; 0 none
u32 Child(u32 root, const char* name, int depth = 0);
bool IsAncestor(u32 root, u32 ctl);

// The control's projected rectangle (pixels: x0 y0 x1 y1) and centre; false when not projectable.
struct Box
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0, cx = 0, cy = 0;
    bool box = false; // it has extents (else only the anchor point)
};
bool RectOf(u32 ctl, Box& b);

constexpr int kPlates = 7; // the most plates a window family has (the dealer)
// How far the panel under a fitted window's row stands off the table, pixels: its text boxes go with it (0 none). A family index.
float FitBoxShift(int fam);
bool Blocked(float x0, float y0, float x1, float y1); // a keep-out stands on this rectangle, pixels
bool WindowOpen(); // a window stands over the room: the pad's focus is its
bool BarBusy(); // the room bar moved within the last 0.3 s: no focus on it, its targets are not where they will stand
std::string BarReport(); // the room bar's changes since the last read, frame by frame
float TextHeightPx(u32 ctl); // a text control's drawn height, blank lines counted; 0 no text
bool FitOpen(); // a window with fitted panels is open: what stands in it is read every frame
// What the skin draws in a window itself, pixels: the fit counts it as content. Each frame's list, cleared first
void ClearSkinItems();
void AddSkinItem(float x0, float y0, float x1, float y1);
void SetMapOver(bool on); // the full-screen map stands: no docked frame (its plates stood over the map, the room bar's icons hidden under it: uiskin)
void SetRootGrow(float grow); // the log and the info window stand this much taller than their root, fractions of H (windowtext's boxes fill it)
bool IsTitle(const char* control); // a window's heading, by the table
// Each fitted panel of the open windows: its rectangle and the air round what it holds, fractions of H: what the walk reads
std::string FitReport();

enum Cond
{
    Always,
    AnchorShown, // drawn while the anchor control is shown (ErrorHelp, the save description field)
    Xfer,        // dealer: a row is selected: the buy button, or the item alone where the game refuses it (no free hardpoint
                 // ...: no button, the item, its compare lines and its infocard all the same; GitHub issue 10)
    NoXfer,
    ShipWords, // dealer: the ship dealer's final stage with no row selected: its words stand in the infocard's place
    SellFull, // dealer: the player's list has rows (NN_Inventory ItemView01 shown)
    SellEmpty,
};
// One plate. anchor: "" = the window root's own rect (edges + the offsets), "#Name" = that control's rect edges +
// offsets, "Name" = that control's centre + offsets. Offsets and the rules in fractions of H.
// What the dealer's quantity block rises by while the game hides the slider above it (a single item), fractions of H: the slider's room, less a readable gap. windowtext moves the block.
constexpr float kStackLift = 0.045f;
struct PlateDef
{
    const char* anchor;
    float x0, y0, x1, y1;
    int corners;
    float rule;       // a header rule this far under the plate's top (0 none)
    float div1, div2; // full-width dividers under the top (0 none)
    Cond cond;
    float fill;
    // the bottom lane (modules\hud\docs\UI_HUD2_EXTENDED.md 13, rule 3): while the prompt lane is in use the plate ends here instead
    // of y1 (same anchor rule) - its content's bottom + 0.006 H, measured in the lane probe (0 = never trimmed)
    float trim = 0;
    float liftFoot = 0; // the plate ends this much higher while the dealer's quantity slider is hidden: its room is empty then
    bool holdsWords = false; // the plate grows by what SetWordsRoom says: the skin's lines under its rows stand inside it
};
constexpr int kTLBR = hud2kit::kTL | hud2kit::kBR;

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
    PlateDef plates[kPlates];
    const char* titles[3];
    FocusKind focus;
    const char* fa; // focus prefix a (FocusShift / FocusLeft: the prefix it applies to)
    const char* fb;
    float fd;
};

extern const FamilyDef kFamilies[];
extern const int kFamilyCount;
int FamilyIndex(const char* label);
extern const int kFamDealer, kFamInventory, kFamNavBar, kFamNNBox, kFamOptions, kFamOffer, kFamConfirm, kFamLog, kFamNavMap, kFamShipDealer,
    kFamPause, kFamJobBoard, kFamMainMenu, kFamPopup, kFamExit, kFamError, kFamPrompt, kFamNetOp, kFamHelp;

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
    u32 anchors[kPlates] = {};        // per plate
    u32 condCtl = 0, condCtl2 = 0, condCtl3 = 0; // dealer: BuySellTradeButton, the partner's ItemView01, its Full_Cargobar
    u32 condCtl4 = 0;                            // dealer: TransferItemView
    u32 condCtl5 = 0;                            // dealer: BackToShipDealer (the final stage alone)
    u32 storyText = 0;                           // job board: the chosen job's rich text (the plate grows to it)
    bool alive = true;
};
extern std::vector<Win> g_wins;

// What the skin does to the windows and over the plates. Null without it: nothing is written or painted.
struct Ops
{
    void (*Write)(Win& w);   // from a rescan, while g_on: the art, own draws and titles of the window
    void (*Restore)(Win& w); // a window that left the list but is still valid
    void (*Plate)(float x0, float y0, float x1, float y1, const PlateDef& p);
    void (*Tab)(const Box& b, bool current);
    void (*Glass)(float x0, float y0, float x1, float y1, int corners, float fill);
    void (*Underline)(float x, float y); // the current room, at the bar's bottom edge
    void (*Bumpers)(float x0, float x1, float cy); // LB / RB at the bar's ends
};
extern const Ops* g_ops;

extern bool g_on;        // the skin is wanted this frame
extern bool g_padDevice; // the pad is the device: the lane's geometry runs, the bar's bumpers count
extern u32 g_gen, g_genSeen;
extern bool g_genHost;
extern u32 g_rescans;
extern float g_lowestModalY1; // pixels, for the prompt lane
extern bool g_modalOpen, g_dockedFrame, g_dockedLane;
extern float g_nnRight, g_nnBottom;
// What the prompt pills must never cover (pixels, this frame's glass, drawn or not): the room bar and its sub-row, the NeuroNet box, every
// window's plates and tabs (a plate-less window: its own rect).
struct KeepOut
{
    float x0, y0, x1, y1;
};
extern std::vector<KeepOut> g_keep;
extern bool g_paint; // the paint pass: the plates draw through g_ops, the keep-outs are not collected twice
extern int g_sw, g_sh;
extern float g_H, g_W;
// The game's letterbox is up (room entry / story scenes, launch, dock): the non-modal windows go native, the game hides the room bar and
// the NeuroNet box itself then; modal menus keep their skin.
extern bool g_scene;
extern int g_jobHeightTest; // debug `uiskin jobheight <px>`: the text's height as read (0: the game's)

float LetterboxNow();
bool CaptionLit(u32 colour, u32 selected);
u32 CurrentTab(const Win& w, int page);
bool Valid(u32 ctl, u32 vt);
void Refresh();  // the registry current: a compare when the generation did not move
void Geometry(void* device);

// One window's plates and tabs (keep-outs, or paint under g_paint). What the paint pass needs next comes back in `out`.
struct WindowPass
{
    int page = 0;        // the Options window's page state
    u32 currentTab = 0;
    bool haveTabFocus = false;
    Box tabFocus;
};
void WindowPlates(const Win& w, WindowPass& out);
// The docked frame: the room bar, its sub-row, the NeuroNet box. draw = false: the geometry only (the game does not draw them in a conversation).
void DockedPlates(const Win& bar, const Win* nn, bool draw);
// The docked frame this frame, pixels, as DockedPlates measured it from the game's buttons: the room bar's icons, the row of the room's actions under them, the NeuroNet box.
// `live` only while the game draws the bar. HUD 2's words stand on this (nnstrip::DrawRoom) and write nothing to the game.
struct DockedGeo
{
    bool live = false, sub = false, nn = false, glyphs = false; // glyphs: LB / RB step the rooms now
    float bx0 = 0, bx1 = 0, by1 = 0, sx0 = 0, sx1 = 0, sy1 = 0, nx0 = 0, nx1 = 0, ny1 = 0;
};
extern DockedGeo g_docked;
// Takes the skin under a scene's letterbox: the modal windows, and the offer a room's scene brings up (the game's ornate frame stood there, the owner's order 7 Nov 2026).
bool SceneSkinned(int fam);
// The room the bar's underline stands under: the pad's pick while the bumpers step, else the room itself (a hotspot's label).
std::string CurrentRoomKey();

// The under-HUD pass without the skin: `classic` = ui_style 0, where the game's name card is placed too. Also how the skin runs while it is off.
void ClassicPass(void* device, bool enabled, bool pad, bool classic);

// The NeuroNet box's bottom edge on a docked non-modal screen (0 = none): the last resort's top lane hangs under it.
float NeuroNetBottom();
// A docked screen with the room bar up and no modal menu (non-modal windows included).
bool DockedScreen();
// A modal menu (pause, save / load, Options) is shown.
bool ModalShown();
// The lowest bottom edge of a keep-out overlapping x0..x1 and reaching below `below` (0 = none).
float LowestKeepOut(float x0, float x1, float below);
// The left limit (pixels) of a pill row spanning y0..y1 that ends at `right`: right of everything the pills must not cover there this frame,
// plus a gap; `right` when the band is blocked; 0.05 W at least.
// The keep-out reaching furthest right across the band y0..y1: the panel a column of prompts stands beside. False when none does.
bool PanelBeside(float y0, float y1, float& right, float& top, float& bottom);
// The room the lines under the ship dealer's list take this frame, fractions of H (uistrip).
void SetWordsRoom(float room);
float LaneLeft(float W, float H, float y0, float y1, float right);
// A family's plate anchored at control `anchor` (e.g. "options", "OptionsBox"): its offsets from the anchor's centre in fractions of H
// (x0 y0 x1 y1). False when the family has no such plate.
bool PlateOffsets(const char* family, const char* anchor, float out[4]);
} // namespace winplates
