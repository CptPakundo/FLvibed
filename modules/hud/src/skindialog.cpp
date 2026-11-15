// HUD 2's dialogs - see skindialog.h. The children by class: docs\FREELANCER_RE.md "Dialogs" (decompiled; a probe per class is owed, UI_HUD2_SKIN.md 15 step 1).
#include "skindialog.h"

#include "dimouse.h"
#include "flexe.h"
#include "game/room.h"
#include "skin_rules.h"
#include "skinkit.h"
#include "ui/hud2_kit.h"
#include "ui/ui.h"
#include "uiskin.h"

#include <string>

namespace skindialog
{
namespace
{
using namespace winplates;
using namespace skinrules;

struct Open
{
    u32 root = 0;
    u32 action = 0, safe = 0; // the buttons; a single answer stands in `safe`
    std::wstring title, cost, actionWord, safeWord;
    bool amber = false, focusAction = false, verbs = false;
    bool overFlight = false; // the cargo demand: the flight runs on under it, so its keys stay the game's (no arrows)
};
std::vector<Open> g_open;
u32 g_hovered = 0; // the control under the focus at the last draw
// An arrow key's focus move (ArrowKey): the game's cursor driven onto that answer's button, the pad's way (padmenus SnapTo), held a moment until the game's
// next mouse reads have it there, then let go so the real mouse moves it again. 0: none running. Re-aimed at every draw of the hold: the key that hands the
// device from the pad to the keyboard also clears the virtual mouse at the next frame (controller device.cpp), which dropped the first move.
u32 g_snap = 0, g_snapRoot = 0;
float g_snapX = 0, g_snapY = 0;
ULONGLONG g_snapAt = 0;
constexpr ULONGLONG kSnapHoldMs = 150;
void EndSnap()
{
    if (!g_snap)
        return;
    dimouse::ReleaseTarget();
    g_snap = g_snapRoot = 0;
}

Open* Find(u32 root)
{
    for (Open& o : g_open)
        if (o.root == root)
            return &o;
    return nullptr;
}

bool Read(u32 root, const char* name, std::wstring& out, std::vector<u32>& hide)
{
    const u32 c = Child(root, name);
    if (!c || !TextOf(c, out))
        return false;
    hide.push_back(c);
    return true;
}

// The two-button dialogs: the question, then the row or the game's words
bool Answers(Open& o, u32 root, DialogKind kind, const std::wstring& text, const std::wstring& name, const char* yes, const char* no, const char* yesText,
             const char* noText, std::vector<u32>& hide)
{
    o.action = Child(root, yes), o.safe = Child(root, no);
    if (!o.action || !o.safe || !Read(root, yesText, o.actionWord, hide) || !Read(root, noText, o.safeWord, hide))
        return false;
    if (const DialogRow* r = Row(kind))
    {
        o.title = r->title;
        o.cost = r->cost ? r->cost : name;
        o.actionWord = r->action, o.safeWord = r->safe;
        o.amber = r->amber, o.focusAction = r->focusAction, o.verbs = true;
    }
    else
        o.cost = text; // the game's whole question, and its own YES / NO
    return true;
}

// A chip stands on its hit box, at least the chip's height.
void ChipRect(Box& b)
{
    const float h = ChipHeight((b.y1 - b.y0) / g_H) * g_H;
    b.y0 = b.cy - h / 2, b.y1 = b.cy + h / 2;
}

// The mission message box in its drop-cargo form (a pirate's demand, a scan's): its two buttons, made only in that form (the builder 0x48A4C0, mode 4).
bool DropForm(u32 root) { return winplates::CargoDemand(root); }

// Its words: the box keeps them as a FmtStr and shows them in a rich text, which has no reader; the game's own formatter makes the same text.
bool FormatDemand(wchar_t* out, int max)
{
    if (!exe::FmtStrFormat.ok() || !exe::MissionMessageFmt.ok() || max < 2)
        return false;
    out[0] = 0;
    __try
    {
        hk::Call<int(__fastcall*)(const void*, void*, wchar_t*, int)>(exe::FmtStrFormat, reinterpret_cast<const void*>(exe::MissionMessageFmt.get()), nullptr, out, max - 1);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    out[max - 1] = 0;
    return out[0] != 0;
}

// Where a skinned dialog stands (pixels): its chips on the game's buttons and the plate round them and its words. The one rule for the draw and for what the
// flight HUD keeps off (FlightBox). False: a button has no box, or the plate does not fit the screen.
bool Fit(const Open& o, Box& a, Box& s, DialogBox& b)
{
    using namespace hud2kit;
    const bool hasAction = o.action != 0, hasSafe = o.safe != 0;
    if ((hasAction && (!RectOf(o.action, a) || !a.box)) || (hasSafe && (!RectOf(o.safe, s) || !s.box)))
        return false;
    if (hasAction)
        ChipRect(a);
    if (hasSafe)
        ChipRect(s);
    DialogIn in;
    in.W = g_W, in.H = g_H, in.centreY = g_H / 2;
    if (hasSafe)
    {
        in.chipsX0 = hasAction ? a.x0 : s.x0, in.chipsX1 = s.x1;
        in.chipsY0 = hasAction ? std::min(a.y0, s.y0) : s.y0, in.chipsY1 = hasAction ? std::max(a.y1, s.y1) : s.y1;
    }
    in.titleW = o.title.empty() ? 0 : TextWidth(o.title, skinkit::Px(kHeaderTitle));
    const float room = g_W * 0.45f;
    const float costW = o.cost.empty() ? 0 : TextWidth(o.cost, skinkit::Px(kBodyLow));
    in.costW = std::min(costW, room);
    in.costLines = room > 0 ? std::min(3, 1 + static_cast<int>(costW / room)) : 1;
    return DialogFit(in, b);
}
} // namespace

bool IsDialog(int fam, u32 root)
{
    return fam == kFamConfirm || fam == kFamExit || fam == kFamError || fam == kFamNetOp || fam == kFamPrompt || (fam == kFamPopup && DropForm(root));
}

bool Pauses(int fam) { return fam != kFamPopup; }

bool Prepare(const Win& w, bool mainMenuBehind, std::vector<u32>& hide)
{
    Open o;
    o.root = w.root;
    bool ok = false;
    std::wstring text, name;
    if (w.fam == kFamConfirm)
    {
        ok = Read(w.root, "Confirm_delTxt", text, hide) &&
             Answers(o, w.root, ConfirmKind(text.c_str(), &name), text, name, "Confirm_delBtnYes", "Confirm_delBtnNo", "Confirm_delBtnYesText", "Confirm_delBtnNoText", hide);
    }
    else if (w.fam == kFamExit)
    {
        ok = Read(w.root, "ExitText", text, hide) &&
             Answers(o, w.root, ExitKind(text.c_str(), mainMenuBehind), text, name, "ExitButtonBox0", "ExitButtonBox1", "ExitButtonText0", "ExitButtonText1", hide);
    }
    else if (w.fam == kFamError)
    {
        o.safe = Child(w.root, "ErrorOkayButton");
        ok = o.safe && Read(w.root, "ErrorDialogTitle", o.title, hide) && Read(w.root, "Text", o.cost, hide) && Read(w.root, "ErrorOkayText", o.safeWord, hide);
    }
    else if (w.fam == kFamNetOp)
        ok = Read(w.root, "ExitText", o.cost, hide);
    else if (w.fam == kFamPopup && DropForm(w.root))
    {
        // the cargo demand: the demanding ship's name over the game's words, DROP CARGO and CANCEL as the game words them; its X (the same as CANCEL) goes with the art
        wchar_t demand[512];
        std::wstring who;
        ok = FormatDemand(demand, 512) && Answers(o, w.root, DialogKind::Other, demand, name, "DropCargoButton", "DCCancelButton", "DropCargoText", "DCCancelText", hide);
        o.overFlight = true;
        if (ok)
        {
            if (Read(w.root, "MissionMessageTitle", who, hide))
                o.title = who;
            for (const char* n : {"MissionMessageText", "MissionCloseButton"})
                if (const u32 c = Child(w.root, n))
                    hide.push_back(c);
        }
    }
    // NN_Prompt: PromptText is rich text, whose string has no known field, and the caption controls of PromptYes / PromptNo are unnamed: nothing to read yet
    if (Open* have = Find(w.root))
        *have = o;
    else if (ok)
        g_open.push_back(o);
    if (!ok)
    {
        Forget(w.root);
        hide.clear();
    }
    return ok;
}

bool Skinned(u32 root) { return Find(root) != nullptr; }

void Forget(u32 root)
{
    if (root == g_snapRoot)
        EndSnap();
    for (size_t i = g_open.size(); i-- > 0;)
        if (g_open[i].root == root)
            g_open.erase(g_open.begin() + static_cast<std::ptrdiff_t>(i));
}

void Prune()
{
    for (size_t i = g_open.size(); i-- > 0;)
    {
        bool live = false;
        for (const Win& w : g_wins)
            live |= w.root == g_open[i].root;
        if (!live)
        {
            if (g_open[i].root == g_snapRoot)
                EndSnap();
            g_open.erase(g_open.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
}

// Enter answers with the chip in focus, not the game's YES: on a risky dialog the focus rests on the safe answer, and a word in focus that Enter ignores would lose
// the player's progress. The game's own click, by the button's own action; the release is swallowed with the press. Off a dialog with two answers: the game's.
void Clear() { EndSnap(), g_open.clear(), g_hovered = 0; }

// The answer in focus: what the pointer or the pad is on (an arrow key's move while the cursor is still on its way), else the dialog's own start. One rule for
// the chip drawn lit and for Enter: a focus the player cannot see, on a dialog that can exit the game, was a fault
static u32 FocusOf(const Open& o, u32 hovered)
{
    if (g_snap && g_snapRoot == o.root)
        return g_snap;
    return hovered == o.action || hovered == o.safe ? hovered : o.focusAction ? o.action : o.safe;
}

bool EnterKey(int vk, bool down)
{
    static bool held = false;
    if (vk != VK_RETURN)
        return false;
    if (!down)
    {
        const bool was = held;
        held = false;
        return was;
    }
    if (!uiskin::On()) // (the key-up above stays ours: the Enter that closed a dialog never reaches the game by half)
        return false;
    for (const Open& o : g_open)
    {
        if (!o.action || !o.safe || !FullyShown(o.root))
            continue;
        const u32 target = FocusOf(o, g_hovered);
        if (!uiclick::Click(target))
            return false;
        EndSnap();
        held = true;
        return true;
    }
    return false;
}

// The arrow keys move the focus between the two answers (the owner, 14 Nov 2026: the quit questions could not be worked by them), the way the pad's D-pad
// moves it: the game's cursor onto the answer's own button, so the chip drawn lit, Enter and a click all act on the same answer. Not on the cargo demand (the
// flight's keys stay the game's). The release of a key taken here is swallowed with its press; any other key, or no two-answer dialog shown: the game's.
bool ArrowKey(int vk, bool down)
{
    static unsigned heldMask = 0; // the arrows whose press was taken (bit per arrow)
    DialogArrow a;
    switch (vk)
    {
    case VK_LEFT: a = DialogArrow::Left; break;
    case VK_RIGHT: a = DialogArrow::Right; break;
    case VK_UP: a = DialogArrow::Up; break;
    case VK_DOWN: a = DialogArrow::Down; break;
    default: return false;
    }
    const unsigned bit = 1u << static_cast<int>(a);
    if (!down)
    {
        const bool was = (heldMask & bit) != 0;
        heldMask &= ~bit;
        return was;
    }
    if (!uiskin::On())
        return false;
    for (const Open& o : g_open)
    {
        if (!o.action || !o.safe || o.overFlight || !FullyShown(o.root))
            continue;
        const u32 target = ArrowToAction(a, FocusOf(o, g_hovered) == o.action) ? o.action : o.safe;
        Box b;
        if (!RectOf(target, b) || !b.box)
            return false;
        dimouse::MoveTo(b.cx, b.cy);
        g_snap = target, g_snapRoot = o.root, g_snapX = b.cx, g_snapY = b.cy, g_snapAt = GetTickCount64();
        heldMask |= bit;
        return true;
    }
    return false;
}

bool Draw(const Win& w, u32 hovered)
{
    g_hovered = hovered;
    if (g_snap && GetTickCount64() - g_snapAt >= kSnapHoldMs) // (the cursor has stood on the button for the game's reads: the real mouse's again)
        EndSnap();
    else if (g_snap)
        dimouse::MoveTo(g_snapX, g_snapY);
    const Open* o = Find(w.root);
    if (!o)
        return false;
    Box a, s;
    DialogBox b;
    if (!Fit(*o, a, s, b))
        return false;
    const bool hasAction = o->action != 0, hasSafe = o->safe != 0;
    skinkit::Dialog(b, o->title, o->cost, o->amber);
    if (hasAction)
    {
        skinkit::ChipState st;
        st.focused = FocusOf(*o, hovered) == o->action;
        skinkit::Chip(a.x0, a.y0, a.x1, a.y1, o->actionWord, st);
    }
    if (hasSafe)
    {
        skinkit::ChipState st;
        st.focused = FocusOf(*o, hovered) == o->safe;
        skinkit::Chip(s.x0, s.y0, s.x1, s.y1, o->safeWord, st);
    }
    return true;
}

bool FlightBox(float rect[4], std::wstring& accept, std::wstring& cancel)
{
    for (const uintptr_t r : ui::VisibleRoots())
    {
        const u32 root = static_cast<u32>(r);
        if (!winplates::CargoDemand(root))
            continue;
        Box a, s;
        DialogBox b;
        if (const Open* o = Find(root); o && Fit(*o, a, s, b))
        {
            rect[0] = b.x0, rect[1] = b.y0, rect[2] = b.x1, rect[3] = b.y1;
            accept = o->actionWord, cancel = o->safeWord;
            return true;
        }
        // (not ours: the game's own box and its own words)
        Box g;
        if (!RectOf(root, g) || !g.box || !TextOf(Child(root, "DropCargoText"), accept) || !TextOf(Child(root, "DCCancelText"), cancel))
            return false;
        rect[0] = g.x0, rect[1] = g.y0, rect[2] = g.x1, rect[3] = g.y1;
        return true;
    }
    return false;
}

// (the topmost open dialog: the last that opened)
const wchar_t* ActionWord() { return !g_open.empty() && g_open.back().verbs ? g_open.back().actionWord.c_str() : nullptr; }
const wchar_t* SafeWord() { return !g_open.empty() && g_open.back().verbs ? g_open.back().safeWord.c_str() : nullptr; }
bool FocusAction() { return !g_open.empty() && g_open.back().focusAction; }
} // namespace skindialog
