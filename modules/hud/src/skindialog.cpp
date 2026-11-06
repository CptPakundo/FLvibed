// HUD 2's dialogs - see skindialog.h. The children by class: docs\FREELANCER_RE.md "Dialogs" (decompiled; a probe per class is owed, UI_HUD2_SKIN.md 15 step 1).
#include "skindialog.h"

#include "game/room.h"
#include "skin_rules.h"
#include "skinkit.h"
#include "ui/hud2_kit.h"
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
};
std::vector<Open> g_open;
u32 g_hovered = 0; // the control under the focus at the last draw

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
} // namespace

bool IsDialog(int fam) { return fam == kFamConfirm || fam == kFamExit || fam == kFamError || fam == kFamNetOp || fam == kFamPrompt; }

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
            g_open.erase(g_open.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

// Enter answers with the chip in focus, not the game's YES: on a risky dialog the focus rests on the safe answer, and a word in focus that Enter ignores would lose
// the player's progress. The game's own click, by the button's own action; the release is swallowed with the press. Off a dialog with two answers: the game's.
void Clear() { g_open.clear(), g_hovered = 0; }

// The answer in focus: what the pointer or the pad is on, else the dialog's own start. One rule for the chip drawn lit and for Enter: a focus the player cannot see,
// on a dialog that can exit the game, was a fault
static u32 FocusOf(const Open& o, u32 hovered)
{
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
        held = true;
        return true;
    }
    return false;
}

bool Draw(const Win& w, u32 hovered)
{
    g_hovered = hovered;
    const Open* o = Find(w.root);
    if (!o)
        return false;
    using namespace hud2kit;
    Box a, s;
    const bool hasAction = o->action != 0, hasSafe = o->safe != 0;
    if ((hasAction && (!RectOf(o->action, a) || !a.box)) || (hasSafe && (!RectOf(o->safe, s) || !s.box)))
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
    in.titleW = o->title.empty() ? 0 : TextWidth(o->title, skinkit::Px(kHeaderTitle));
    const float room = g_W * 0.45f;
    const float costW = o->cost.empty() ? 0 : TextWidth(o->cost, skinkit::Px(kBodyLow));
    in.costW = std::min(costW, room);
    in.costLines = room > 0 ? std::min(3, 1 + static_cast<int>(costW / room)) : 1;
    DialogBox b;
    if (!DialogFit(in, b))
        return false;
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

// (the topmost open dialog: the last that opened)
const wchar_t* ActionWord() { return !g_open.empty() && g_open.back().verbs ? g_open.back().actionWord.c_str() : nullptr; }
const wchar_t* SafeWord() { return !g_open.empty() && g_open.back().verbs ? g_open.back().safeWord.c_str() : nullptr; }
bool FocusAction() { return !g_open.empty() && g_open.back().focusAction; }
} // namespace skindialog
