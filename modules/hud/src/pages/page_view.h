// FLvibed - what a page's view is given and gives back (the HUD 2 page framework: audit 70b 2.4). A view draws its model
// in the box it is given and keeps nothing of the game: View(const ViewIn&, const Model&, PageState&, ViewOut&). Pure: no
// game include (a view includes only this, page_kit.h and its model: tools\module_lint.py).
#pragma once
#include "game/screens/screen_rules.h" // (pure: screens::List, a list's window on its rows, as a model holds it)

#include <vector>

namespace page
{
// Several rectangles in one page's ground (pixels): where the game's 3D icons show through it (page_kit.h GroundBut).
struct Holes
{
    float r[16][4];
    int n = 0;
    void Add(float x0, float y0, float x1, float y1)
    {
        if (n < 16)
            r[n][0] = x0, r[n][1] = y0, r[n][2] = x1, r[n][3] = y1, ++n;
    }
};

// The page's content box on the screen (pixels) and the screen's size (the sizes' unit is the height).
struct ViewIn
{
    float W, H;
    float x0, x1, top, bottom;
    int hover = -1; // the hit box (ViewOut::hits, by its index) the mouse is on, -1 none: its chip draws lit
    // What the docked pages' views also take of the screen (audit 70b F3a; their shell, basepage.cpp, fills them):
    float titleY = 0;   // the frame's title line (page_kit.h Frame)
    float pageTop = 0;  // where a docked page's ground starts, under the strip and the room's words
    bool pad = false;  // the pad's form: no mouse chips, the pad's words
    float laneY = 0, laneH = 0, laneMid = 0; // the prompts' lane: the mouse's chips stand in it, level with CLOSE; its middle line
    Holes holes;        // the page's holes as its last frame left them (its ground is drawn round them)
    // What the NeuroNet's pages also take of the screen (audit 70b F4a; their shell, nnpage.cpp, fills them): the game's text
    // the page shows through its hole, its box as the page cuts it (pixels), and whether that text stands laid out where it
    // stays (until then the hole stays shut: nnpage_kit.h OpenHole).
    float text[4] = {};
    bool textOk = false, textReady = false;
    // What the last under-pass gave each of the page's game lists (GameList, by its index): its window and first row then, and
    // each shown row's mesh (-1 none); n 0: none sent. An `atSend` list's view cuts its holes from it (audit 70b F4 unit b2).
    struct Sent
    {
        unsigned window = 0;
        int first = -1, n = 0;
        int meshOf[screens::kMeshes] = {};
    };
    Sent sent[2];
};

// What the runtime keeps of a page between frames for its view and its input. It outlives a close, as the pages' state
// always did: the next open's first prompts read it before that page's first draw.
struct PageState
{
    int first = 0;        // the first line shown (the wheel, the D-pad and the right stick move it; the view clamps it)
    int room = 1;         // lines that fit the box (the view's)
    bool scrolls = false; // the text is longer than the box: the pad's SCROLL shows only then (the owner, 12 Nov 2026)
    float wrapW = -1;     // the width the text was laid out for (-1: lay it out again)
};

// A box the mouse can click (pixels) and what a click there does: the page's action and its argument. A view only records
// it; whoever runs the page hands the boxes to the game's mouse, in this order (an index is the box's name for ViewIn::hover).
struct Hit
{
    float x0, y0, x1, y1;
    int action, arg;
};

// A row's icon: where the game's own mesh of that row stands (pixels, its centre), or off the page.
struct Icon
{
    float cx = 0, cy = 0;
    bool on = false;
};
// A list's icons: the game's controls `format` (by base .. base + count - 1; a format with no number names one control), the
// `nth` of each name on the screen, each placed at its icon. A view records it; the runtime moves the game's controls
// (nnpage_kit.h PlaceIcons).
struct IconSet
{
    const char* format = nullptr;
    int base = 0, count = 0, nth = 1;
    Icon icons[9];
};

// A list of the game's window drawn as the view's rows, its five item meshes in them (the dealers' two lists, audit 70b F3b).
// The view says which of the game's rows and items its rows are and where they stand; the runtime gives each shown row a
// mesh (nnpage_kit.h RowIcons), cuts a hole round each one that got one, moves the meshes there, and sends the rows' models
// between the game's row fill and its UI pass (IconsUnder) from the list as it stands then. The windows and the list are the
// model's (read where it reads them): the runtime reads them when it works, so they are pointers.
struct ListRow
{
    int gameRow = -1; // the game's row (-1: none, an EMPTY slot)
    unsigned arch = 0;
};
struct GameList
{
    const unsigned* window = nullptr;  // the game's list window
    const unsigned* partner = nullptr; // the other list's window beside it
    bool right = false;                // this list stands right of the other on the screen (when the meshes' parents do not tell)
    const screens::List* list = nullptr; // which rows show (its first and rows)
    float x0 = 0, cell = 0, top = 0, pitch = 0; // the rows' left edge, the icon's square, the first row's top, the rows' pitch
    std::vector<ListRow> rows;                  // every row of the list, in the page's order
    // The INVENTORY's form (audit 70b F4 unit b2): nothing is given or placed at draw time; the rows' meshes are given, placed
    // and sent under the next UI pass only, and the view cuts its holes itself from what that pass gave (ViewIn::sent).
    bool atSend = false;
    // With atSend, a fixed set of rows at their own centres instead of `list` (the jettison panel's one item); `rows` then are
    // the fills' models only.
    struct Fixed
    {
        ListRow row;
        float cx = 0, cy = 0;
    };
    Fixed fixedRows[screens::kMeshes];
    int fixed = 0;
};

// What a view gives back for what stands round it.
struct ViewOut
{
    float used = 0; // the content's height on the screen (INFO: SHIPS NEAR YOU stands under its text)
    static constexpr int kHits = 32;
    Hit hits[kHits];
    int nHits = 0; // a box past kHits is not recorded: its chip draws, but has no mouse route
    Holes holes;   // this frame's holes in the ground (the game's icons): the ground under the game's pass is drawn in them
    static constexpr int kIconSets = 2;
    IconSet iconSets[kIconSets];
    int nIconSets = 0;
    static constexpr int kLists = 2;
    GameList lists[kLists]; // worked after the view, in this order, before the icon sets
    int nLists = 0;
};
} // namespace page
