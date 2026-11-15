// FLvibed - the model of the job board's and the news's page (the game's NN_MissionVendor and NN_News windows; the HUD 2 page
// framework, audit 70b F3d / F3e: one body of code in two modes, as it was): what the page draws, as a plain value. Each is a
// panel in the room, the room showing beside it: the game's window stays where it stands and does the work (its five rows are
// what its list shows, its text and picture show through the panel's holes, its buttons accept and close); board_screen.cpp
// fills the model from its controls, modules\hud\src\pages\neuronet\board_view.cpp draws it. Nothing here calls the game.
#pragma once
#include "common.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace boardscreen
{
constexpr int kRows = 5; // the window's rows (MissionVendorSummaryBox0..4, NewsHeadlineBox0..4): what its list shows

// Which window: the job board or the news.
enum Mode
{
    kJobs = 0,
    kNews = 1,
};

// The page's actions (board_screen.cpp's table: each the window's own control).
enum Action
{
    kActPick = 1, // a row clicked (arg: its row)
    kActStep,     // the focus a row up or down (arg: -1 / 1); at the job list's edge the game's list moves under the same row
    kActText,     // the text scrolled (arg: the pixels, a float's bits: FloatArg)
    kActAccept,   // ACCEPT (MissionVendorAcceptButton; the job board's)
    kActClose,    // CLOSE (MissionVendorCloseButton, NewsCloseButton)
};
// A float through an action's int argument (its bits, exactly): the text's scroll in pixels.
inline int FloatArg(float v)
{
    int a = 0;
    memcpy(&a, &v, sizeof(a));
    return a;
}
inline float ArgFloat(int a)
{
    float v = 0;
    memcpy(&v, &a, sizeof(v));
    return v;
}

// The panel on the screen (pixels): its box, its middle line, the game's text's box, the game's picture's box (it stays where
// the game draws it: it has no place of its own, only its window's). `pageTop`: where a docked page's ground starts.
struct Panel
{
    float x0, y0, x1, y1, mid, tx0, tx1, ty0, ty1, px0, py0, px1, py1;
};
inline Panel PanelOf(float W, float H, float pageTop, bool jobs)
{
    Panel p;
    p.x0 = 0.07f * W, p.x1 = 0.93f * W, p.y0 = std::max(0.175f * H, pageTop + 0.010f * H), p.y1 = 0.885f * H;
    p.mid = W / 2 - 0.01f * W;
    p.px0 = W / 2 + 0.1525f * H, p.px1 = W / 2 + 0.479f * H;
    p.py0 = (jobs ? 0.219f : 0.259f) * H, p.py1 = p.py0 + 0.24f * H;
    p.tx0 = p.mid + 0.03f * H, p.tx1 = p.x1 - 0.05f * H;
    p.ty0 = p.py1 + 0.075f * H, p.ty1 = p.y1 - (jobs ? 0.02f : 0.075f) * H;
    return p;
}

// One of the window's rows as its controls say it.
struct Row
{
    bool box = false;      // the row's control stands (a job or a story on it)
    std::wstring type;     // the job's kind in capitals: its icon's hover text (MissionVendorIconFrame<n>)
    std::wstring reward, dest, title;
    int difficulty = 0;    // the job's difficulty from the base's offers, lined up by the rewards (0: none lined up)
    std::wstring headline; // the story's headline (NewsHeadlineHeadline<n>)
};

struct Model
{
    int mode = kJobs; // the description's (board_page.cpp: one per window)
    // the page's own (reset on every open, as the old statics were on each opening)
    int side = 0;         // the pad's side: 0 the list, 1 the text (only while it overflows; D-pad right goes over)
    bool picked = false;  // the first row was picked on this opening (once: after a refused job the game clears its choice on purpose)
    float textAcc = 0;    // the right stick's scroll of the text not yet sent (pixels)
    // read right before the view draws, as the page read them while it drew (board_screen.cpp ReadFrame)
    int sel = -1; // the game's chosen row (-1 none: as after a refused job)
    Row rows[kRows];
    int jobTotal = 0, jobFirst = 0, jobRows = 0; // the base's offers, where the rows stand among them, how many lined up
    std::wstring reward, dest, title;            // the chosen job's head (MissionVendorMission*)
    std::wstring rejected;                       // the game's caption of a refused job ("Mission Rejected"), "" none
    std::wstring headline;                       // the chosen story's head (NewsStoryHeadline)
    std::wstring tickerText;                     // the market's ticker: the whole of what the base trades
    float* ticker = nullptr;                     // its offset (px): the screen's, running on across openings; the view keeps it within one width
    bool bar = false;                            // the text's own scroll bar stands (the text overflows)
    float barPos = 0, barLo = 0, barHi = 0;      // where it stands, its range
    bool textSide = false;                       // the pad on the text (side 1 and the bar stands)
    bool accept = false;                         // the game's ACCEPT stands

    bool Jobs() const { return mode == kJobs; }
};
} // namespace boardscreen
