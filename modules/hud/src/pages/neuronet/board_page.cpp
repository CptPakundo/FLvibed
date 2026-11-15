// FLvibed - the job board's and the news's page descriptions (audit 70b F3d / F3e; basepage.cpp's board as it was): each claims
// its window and leaves it where it stands (these two answer no move of their own, and their picture stands by it; the shell
// places their text and controls round the panel); the model is the window's rows, the chosen one's head and the text's bar
// (core\game\screens\board_screen.*). Their input is their own (pagerun.h PageInput), two sides (issue 45, the owner): D-pad
// left the list - the stick and D-pad up / down step it; D-pad right the text, while it overflows - the stick and D-pad up /
// down scroll it; a held A accepts the chosen job (the docked pages' one hold); the wheel scrolls the text; a click takes a row.
#include "board_page.h"

#include "../pagelist.h"
#include "../pagerun.h"
#include "game/screens/board_screen.h"
#include "ui/glyphs.h"
#include "ui/winplates.h"

#include <cmath>

namespace boardpage
{
using boardscreen::Model;

static void InitJobs(void* m) { new (m) Model(), static_cast<Model*>(m)->mode = boardscreen::kJobs; }
static void InitNews(void* m) { new (m) Model(), static_cast<Model*>(m)->mode = boardscreen::kNews; }
static void Done(void* m) { static_cast<Model*>(m)->~Model(); }
static bool Read(void* m, u32 root) { return boardscreen::Read(*static_cast<Model*>(m), root); }
static void Prepare(void* m, u32) { boardscreen::ReadFrame(*static_cast<Model*>(m)); }
static void Visit(const void* m, page::FieldVisitor& v)
{
    const Model& r = *static_cast<const Model*>(m);
    v.Field("mode", r.mode);
    v.Field("side", r.side);
    v.Field("sel", r.sel);
}

// The pad on the text: its side, while the text overflows (asked of the game at that moment, as before).
static bool TextSide(const Model& m) { return m.side == 1 && boardscreen::TextBar(m.mode) != 0; }
static void Scroll(float px) { pagerun::Act(boardscreen::kActText, boardscreen::FloatArg(px)); }

// The right stick's step on the list (a step a push, then repeats while it is held): its direction last frame and the time to
// its next repeat, one for both pages and kept across openings, as basepage.cpp's were.
static int g_rsWas = 0;
static float g_rsRepeat = 0;

static int Prompts(const void* model, views::PagePrompt* out, int max)
{
    using namespace glyphs;
    const Model& m = *static_cast<const Model*>(model);
    const bool jobs = m.Jobs();
    int n = 0;
    auto add = [&](int id, const wchar_t* word, int id2 = -1, bool hold = false, float progress = 0)
    {
        if (n < max)
            out[n++] = {id, id2, word, hold, progress};
    };
    // one side's words, only the D-pad direction that goes over (the dealers' rule)
    if (!TextSide(m))
    {
        add(DU, jobs ? L"JOB" : L"STORY", DD);
        if (boardscreen::TextBar(m.mode))
            add(DR, L"READ");
    }
    else
    {
        add(DL, jobs ? L"JOBS" : L"STORIES");
        add(RS, L"SCROLL");
    }
    // (only with a job chosen: after a refusal the game keeps its button shown with none, and its ACCEPT does nothing then)
    if (jobs && boardscreen::AcceptStands() && boardscreen::Sel(m.mode) >= 0)
        add(A, L"ACCEPT", -1, true, pageinput::HoldProgress(pagerun::PurchaseHold()));
    return n;
}

static unsigned Pad(void* model, unsigned pressed, unsigned held, float dt)
{
    Model& m = *static_cast<Model*>(model);
    const bool canText = boardscreen::TextBar(m.mode) != 0;
    if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)
        m.side = 0;
    if ((pressed & XINPUT_GAMEPAD_DPAD_RIGHT) && canText)
        m.side = 1;
    const int rs = (held & views::kPagePadRsUp) ? -1 : (held & views::kPagePadRsDown) ? 1 : 0;
    if (!TextSide(m))
    {
        int step = (pressed & XINPUT_GAMEPAD_DPAD_UP) ? -1 : (pressed & XINPUT_GAMEPAD_DPAD_DOWN) ? 1 : 0;
        if (rs != g_rsWas)
            step = rs, g_rsRepeat = 0.35f;
        else if (rs && (g_rsRepeat -= dt) <= 0)
            step = rs, g_rsRepeat = 0.12f;
        if (step)
            pagerun::Act(boardscreen::kActStep, step);
        m.textAcc = 0;
    }
    else
    {
        const float H = winplates::g_H;
        if (pressed & (XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN))
            Scroll(((pressed & XINPUT_GAMEPAD_DPAD_UP) ? -0.11f : 0.11f) * H);
        m.textAcc = rs ? m.textAcc + rs * 0.9f * H * dt : 0;
        if (fabsf(m.textAcc) >= 0.02f * H)
            Scroll(m.textAcc), m.textAcc = 0;
    }
    g_rsWas = rs;
    const bool hold = (held & XINPUT_GAMEPAD_A) != 0; // (the docked pages' one hold: pagerun::PurchaseHold)
    if (pageinput::StepHoldIf(pagerun::PurchaseHold(), hold, dt, m.Jobs()))
        pagerun::Act(boardscreen::kActAccept, 0);
    return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
}

// The wheel scrolls the text (the game's own bar).
static void Wheel(void*, int notches)
{
    if (notches)
        Scroll(-0.08f * winplates::g_H * notches);
}

static const page::PageInput kInput = {Prompts, Pad, Wheel, nullptr, Prepare};

const page::PageDesc kJobs = {
    {kJobsId, "neuronet", page::Kind::Page, {boardscreen::kJobsClass, boardscreen::kJobsName, nullptr, nullptr}, page::Park::Hosted, page::Pause::None,
     nullptr, 0, nullptr, 0, boardscreen::kJobsActions, boardscreen::kJobsActionCount, nullptr, nullptr, 0, &kInput},
    {sizeof(Model), InitJobs, Read, boardscreen::Stamp, Visit, nullptr, 0, Done},
    View,
};
PAGE_REGISTER(kJobs);
const page::PageDesc kNews = {
    {kNewsId, "neuronet", page::Kind::Page, {boardscreen::kNewsClass, boardscreen::kNewsName, nullptr, nullptr}, page::Park::Hosted, page::Pause::None,
     nullptr, 0, nullptr, 0, boardscreen::kNewsActions, boardscreen::kNewsActionCount, nullptr, nullptr, 0, &kInput},
    {sizeof(Model), InitNews, Read, boardscreen::Stamp, Visit, nullptr, 0, Done},
    View,
};
PAGE_REGISTER(kNews);
} // namespace boardpage
