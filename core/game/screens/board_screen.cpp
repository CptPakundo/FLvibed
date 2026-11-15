// The job board's and the news's reads and actions: board_screen.h. The reads are what basepage.cpp's DrawBoard read as it drew
// and the first pick of its Update; the actions its BoardPick, BoardStep, BoardAccept, TextScroll and the close; all moved
// unchanged (audit 70b F3d / F3e).
#include "board_screen.h"

#include "parked.h"
#include "screen_rules.h"
#include "game/baseinfo.h"
#include "game/fl.h"
#include "game/gamedata.h"
#include "game/hostcalls.h"
#include "game/hudinfo.h"
#include "game/room.h"
#include "flexe.h"
#include "safemem.h"
#include "ui/winplates.h"

#include <cstdio>

namespace boardscreen
{
static float g_ticker = 0; // the ticker's offset, px (one across openings, as it was)

static u32 R32(u32 a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}

static u32 Box(int mode, int i)
{
    char n[48];
    sprintf_s(n, mode == kJobs ? "MissionVendorSummaryBox%d" : "NewsHeadlineBox%d", i);
    return uiclick::FindVisible(n);
}
static bool RowSelected(u32 box) { return box && (R32(box + 0x35C) & 0x10) != 0; }
int Sel(int mode)
{
    for (int i = 0; i < kRows; ++i)
        if (RowSelected(Box(mode, i)))
            return i;
    return -1;
}
const char* TextName(int mode) { return mode == kJobs ? "MissionVendorMissionFullText" : "NewsStoryText"; }
u32 TextBar(int mode)
{
    const u32 t = uiclick::FindVisible(TextName(mode));
    return t ? R32(t + 0x3F0) : 0;
}
bool AcceptStands() { return uiclick::FindVisible("MissionVendorAcceptButton") != 0; }

// ---- the actions: the window's own controls
static bool PickIn(int mode, int i)
{
    const u32 c = Box(mode, i);
    if (c)
        uiclick::Click(c);
    return c != 0;
}
// D-pad up / down: the next row; the list's edge: the game's list moves under the same row (the job board's)
static bool StepIn(int mode, int by)
{
    const int sel = Sel(mode), next = sel + by;
    if (sel < 0) // (no selection, as after a refusal: either key starts at the first row)
        return PickIn(mode, 0);
    if (next >= 0 && Box(mode, next) && next < kRows)
        return PickIn(mode, next);
    if (mode == kJobs)
        if (const u32 bar = uiclick::FindVisible("MissionVendorSummaryScroll"))
        {
            uiclick::Scroll(bar, static_cast<float>(by), false, nullptr, nullptr);
            return PickIn(mode, sel);
        }
    return false;
}
static bool TextIn(int mode, int px)
{
    const u32 bar = TextBar(mode);
    if (bar)
        uiclick::Scroll(bar, ArgFloat(px), false, nullptr, nullptr);
    return bar != 0;
}
template <int Mode> static bool Pick(u32, int i) { return PickIn(Mode, i); }
template <int Mode> static bool Step(u32, int by) { return StepIn(Mode, by); }
template <int Mode> static bool Text(u32, int px) { return TextIn(Mode, px); }
static bool Accept(u32, int)
{
    const u32 c = uiclick::FindVisible("MissionVendorAcceptButton");
    if (!c || room::NNWindowBusy())
        return false;
    uiclick::Click(c);
    return true;
}
static bool CloseJobs(u32, int) { return screens::PressClose(kJobsClose); }
static bool CloseNews(u32, int) { return screens::PressClose(kNewsClose); }

#define BOARD_ACTION(id, name, word, fn)                                                                                                        \
    {id, name, word, nullptr, nullptr, nullptr, fn, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr, false, screens::Answer::Now, 0, \
     nullptr, nullptr}
const screens::ActionDesc kJobsActions[] = {
    BOARD_ACTION(kActPick, "pick", L"JOB", Pick<kJobs>),        BOARD_ACTION(kActStep, "step", L"JOB", Step<kJobs>),
    BOARD_ACTION(kActText, "text", L"SCROLL", Text<kJobs>),     BOARD_ACTION(kActAccept, "accept", L"ACCEPT", Accept),
    BOARD_ACTION(kActClose, "close", L"CLOSE", CloseJobs),
};
const int kJobsActionCount = sizeof(kJobsActions) / sizeof(kJobsActions[0]);
const screens::ActionDesc kNewsActions[] = {
    BOARD_ACTION(kActPick, "pick", L"STORY", Pick<kNews>),
    BOARD_ACTION(kActStep, "step", L"STORY", Step<kNews>),
    BOARD_ACTION(kActText, "text", L"SCROLL", Text<kNews>),
    BOARD_ACTION(kActClose, "close", L"CLOSE", CloseNews),
};
const int kNewsActionCount = sizeof(kNewsActions) / sizeof(kNewsActions[0]);
#undef BOARD_ACTION
const hk::Sym* const kJobsClass = &exe::VtMissionVendor;
const hk::Sym* const kNewsClass = &exe::VtNews;

// ---- the reads
uint64_t Stamp()
{
    static uint64_t frames = 0;
    return ++frames;
}

bool Read(Model& m, u32)
{
    // the first row, once per opening (out of the slide: the window's rows stand then)
    if (!room::NNWindowBusy() && !m.picked && Box(m.mode, 0))
    {
        m.picked = true;
        if (Sel(m.mode) < 0)
            PickIn(m.mode, 0);
    }
    return false;
}

void Advance(float px) { g_ticker += px; }

void ReadFrame(Model& m)
{
    const bool jobs = m.Jobs();
    m.sel = Sel(m.mode);
    for (int i = 0; i < kRows; ++i)
        m.rows[i] = Row(), m.rows[i].box = Box(m.mode, i) != 0;
    m.jobTotal = m.jobFirst = m.jobRows = 0;
    m.reward.clear(), m.dest.clear(), m.title.clear(), m.rejected.clear(), m.headline.clear();
    if (jobs)
    {
        // each job's difficulty on its row, the number the game prints only in the chosen job's text ("Difficulty : n"; Shockwave, 7 Nov 2026: "without
        // needing to scroll"): the base's offers in the board's order from the host (core\host\events.cpp), lined up with the rows by their rewards. Rows
        // that do not line up show none: no number rather than another job's.
        u32 rewards[64];
        int diffs[64];
        const int n = hostcalls::OfferDifficulties(fl::PlayerBase(), rewards, diffs, 64);
        long long rowReward[kRows];
        int rows = 0;
        for (int i = 0; i < kRows; ++i)
        {
            char nm[48];
            sprintf_s(nm, "MissionVendorSummaryReward%d", i);
            std::wstring t;
            if (!m.rows[i].box || !winplates::TextOf(uiclick::FindVisible(nm), t))
                break;
            long long v = 0;
            for (const wchar_t ch : t)
                if (ch >= L'0' && ch <= L'9')
                    v = v * 10 + (ch - L'0');
            rowReward[rows++] = v;
        }
        for (int s = 0; rows > 0 && s + rows <= n; ++s)
        {
            bool match = true;
            for (int i = 0; i < rows && match; ++i)
                match = static_cast<long long>(rewards[s + i]) == rowReward[i];
            if (match)
            {
                for (int i = 0; i < rows; ++i)
                    m.rows[i].difficulty = diffs[s + i];
                m.jobTotal = n, m.jobFirst = s, m.jobRows = rows;
                break;
            }
        }
        for (int i = 0; i < kRows; ++i)
        {
            Row& r = m.rows[i];
            if (!r.box)
                continue;
            char n2[48];
            sprintf_s(n2, "MissionVendorIconFrame%d", i);
            r.type = screens::Upper(baseinfo::HintText(uiclick::FindVisible(n2)));
            sprintf_s(n2, "MissionVendorSummaryReward%d", i);
            winplates::TextOf(uiclick::FindVisible(n2), r.reward);
            sprintf_s(n2, "MissionVendorSummaryDest%d", i);
            winplates::TextOf(uiclick::FindVisible(n2), r.dest);
            sprintf_s(n2, "MissionVendorSummaryTitle%d", i);
            winplates::TextOf(uiclick::FindVisible(n2), r.title);
        }
        // the chosen one's head; a refused job: the game's own caption ("Mission Rejected") in its place, its control parked off the
        // screen with the others (basepage.cpp BoardPlace), so the page says it while the game shows it (audit 109_ 2d)
        winplates::TextOf(uiclick::FindVisible("MissionVendorMissionReward"), m.reward);
        winplates::TextOf(uiclick::FindVisible("MissionVendorMissionDest"), m.dest);
        winplates::TextOf(uiclick::FindVisible("MissionVendorMissionTitle"), m.title);
        if (m.reward.empty())
        {
            std::wstring rejected;
            if (winplates::TextOf(uiclick::FindVisible("MissionVendorMissionReject"), rejected))
                m.rejected = rejected;
        }
        m.accept = AcceptStands();
    }
    else
    {
        for (int i = 0; i < kRows; ++i)
            if (m.rows[i].box)
            {
                char n2[48];
                sprintf_s(n2, "NewsHeadlineHeadline%d", i);
                winplates::TextOf(uiclick::FindVisible(n2), m.rows[i].headline);
            }
        winplates::TextOf(uiclick::FindVisible("NewsStoryHeadline"), m.headline);
        // the market's ticker across the panel, the whole of what the base trades: the game's own control shows a slice of it
        static u32 tickBase = 0;
        static std::wstring tick;
        if (tickBase != fl::PlayerBase())
        {
            tickBase = fl::PlayerBase();
            tick.clear();
            for (const gamedata::Good& g : gamedata::Base(tickBase).goods)
                tick += screens::Upper(hudinfo::EquipName(g.arch)) + L" " + std::to_wstring(static_cast<int>(g.price + 0.5f)) + L"   ---   ";
        }
        if (tick.empty())
            winplates::TextOf(uiclick::FindVisible("NewsTicker"), tick), tickBase = 0;
        m.tickerText = tick;
        m.ticker = &g_ticker;
        m.accept = false;
    }
    // its bar, the game's own state
    const u32 bar = TextBar(m.mode);
    m.bar = bar != 0, m.barPos = m.barLo = m.barHi = 0;
    if (bar)
        m.barPos = uiclick::Scroll(bar, 0, false, &m.barLo, &m.barHi);
    m.textSide = m.side == 1 && bar != 0;
}
} // namespace boardscreen
