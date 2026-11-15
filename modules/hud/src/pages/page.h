// FLvibed - the HUD 2 page framework's descriptions, pure (audit 70b 2.1-2.2): four kinds of page (Page, Dialog, Hosted,
// Map) share one core description; the registry's pure half (which page claims a game window) and the stack (a page plus
// at most one dialog, input to the top). The runtime (pagerun, F2a) holds the game's half: the window registry's scan,
// the park, the frame. No game include; the game's symbols appear only as pointers (hk::Sym, core\hook.h).
// tests\page_test.cpp holds the registry, the stack and the prompts' derivation.
#pragma once
#include "game/screens/action.h"
#include "pageinput.h"
#include "promptlane.h"

#include <cstddef>
#include <cstdint>
#include <ostream>

namespace page
{
enum class Kind : uint8_t
{
    Page,   // full screen; the game's window parked by its own setter, or none
    Dialog, // over a page or flight, takes all input, answers its opener
    Hosted, // the game's screen stays alive and on screen (a state machine, a slide, 3D), its controls hidden
    Map,    // the game's own transform and camera: only the core (claim, input, prompts, actions) is the framework's
};

enum class Park : uint8_t
{
    Root,   // the window's root moved off screen by its own setter, given back on leave
    Hosted, // the window stays; its controls hidden
    None,   // no game window
    Away,   // the root moved off screen by its own setter and never given back: the page's window closes by its own close
            // button when the page goes (the docked windows: basepage.cpp closes it when HUD 2 leaves; audit 70b F3a)
};

struct PageInput; // pagerun.h: a page's own prompts, pad, wheel and clicks, where its input does not fit the bindings

// Today's rules as values (70b 2.1): what the page does to the world, and how its prompts give way.
enum class Pause : uint8_t
{
    None,
    World,      // single player's world pause (nnpage's, while the page stands)
    FlightLock, // flight input locked, the world runs
};

// Which game window a page claims: its class (the vtable) and, where one class serves two pages, the test that tells
// them apart (the final stage: NN_Dealer's kind byte). partnerVt: the window it works with (a dealer's partner).
struct Claim
{
    const hk::Sym* vt;
    const char* name;
    bool (*Is)(uint32_t root); // null: every window of the class
    const hk::Sym* partnerVt;
};

struct PageCore
{
    const char* id;
    const char* family;
    Kind kind;
    Claim claim;
    Park park;
    Pause pause;
    const pageinput::Binding* pad;
    int nPad;
    const pageinput::Binding* keys;
    int nKeys;
    const screens::ActionDesc* actions;
    int nActions;
    const char* parent; // B / Back returns there (a staged page); null: B closes
    // A page whose text scrolls (INFO): the D-pad up / down and the right stick move its first line, a press a line, a hold
    // by this repeat (pageinput.h); its prompt SCROLL stands first while the text is longer than the page. Null: none.
    const pageinput::Repeat* scroll;
    uint32_t padTakes; // the pad buttons the page takes from the game besides its bindings (a press there does nothing)
    // The page's own input (pagerun.h PageInput), called instead of the bindings: the docked pages' prompts and pad (words that
    // change, holds with their fill, two sides, the triggers and sticks) do not fit pageinput::Binding yet (audit 70b F3a).
    // Null: the bindings.
    const PageInput* input = nullptr;
};

// The model's field list for the test surface (`page model`): one call per field.
struct FieldVisitor
{
    virtual void Field(const char* name, double value) = 0;
    virtual void Text(const char* name, const wchar_t* value) = 0;

protected:
    ~FieldVisitor() = default;
};

struct ModelOps
{
    size_t size;
    void (*Init)(void* model);
    bool (*Read)(void* model, uint32_t root); // from the client's stores; root: the parked window (0 none)
    uint64_t (*Stamp)();                      // cheap, each frame: Read again when it changes
    void (*Visit)(const void* model, FieldVisitor& v);
    void (*Game)(uint32_t root, std::wostream& out); // the game's own window, for the comparison
    uint32_t windowFields;                           // how many fields still come from the parked window (borrowed)
    void (*Done)(void* model);                       // the model's end (Init constructs it in the runtime's storage)
};

struct ViewIn;
struct ViewOut;
struct PageState;
typedef void (*ViewFn)(const ViewIn& in, const void* model, PageState& state, ViewOut& out);

enum class Opener : uint8_t
{
    Callback,      // the game's callback with its cookie (0x4A6630's)
    ParentMessage, // parent->vt[0x5C](0x3006, n)
    PopUp,         // IServer PopUpDialog: only for a box the server opened
};

struct HostRect
{
    float x, y, w, h; // the hosted screen's area, fractions of the screen
};

struct PageDesc
{
    PageCore core;
    ModelOps model;
    ViewFn view;
};
struct DialogDesc
{
    PageCore core;
    ModelOps model;
    ViewFn view;
    Opener opener;
    bool loop; // runs its own wait (NN_Prompt, "please wait")
};
struct HostedDesc
{
    PageCore core;
    ModelOps model;
    ViewFn view;
    HostRect rect;
};
struct MapDesc
{
    PageCore core;
};

// ---------------------------------------------------------------- the registry's pure half
// Pages register their class at init; the runtime resolves each Claim::vt and looks a window up by its vtable value.
// One class can serve two pages (told apart by Is); a window no page claims shows natively (another mod's class).
class Registry
{
public:
    static constexpr int kMax = 64;
    // false: full, or this page is already registered for this class
    bool Add(uint32_t vt, const PageCore* core)
    {
        if (!vt || !core || n_ >= kMax)
            return false;
        for (int i = 0; i < n_; ++i)
            if (e_[i].vt == vt && e_[i].core == core)
                return false;
        e_[n_].vt = vt, e_[n_].core = core, ++n_;
        return true;
    }
    // The page that claims the window `root` of class `vt`: the first registered whose Is passes; null: none.
    const PageCore* Find(uint32_t vt, uint32_t root) const
    {
        for (int i = 0; i < n_; ++i)
            if (e_[i].vt == vt && (!e_[i].core->claim.Is || e_[i].core->claim.Is(root)))
                return e_[i].core;
        return nullptr;
    }
    int Count() const { return n_; }
    void Clear() { n_ = 0; }

private:
    struct Entry
    {
        uint32_t vt;
        const PageCore* core;
    };
    Entry e_[kMax] = {};
    int n_ = 0;
};

// ---------------------------------------------------------------- the stack
// A page plus at most one dialog; input goes to the top; a dialog closes back to its page.
class Stack
{
public:
    bool OpenPage(const PageCore* p)
    {
        if (!p || p->kind == Kind::Dialog || dialog_)
            return false; // a dialog is answered first
        page_ = p;
        return true;
    }
    bool OpenDialog(const PageCore* d)
    {
        if (!d || d->kind != Kind::Dialog || dialog_)
            return false; // one dialog at a time
        dialog_ = d;
        return true;
    }
    void CloseDialog() { dialog_ = nullptr; }
    void CloseAll() { page_ = dialog_ = nullptr; }
    const PageCore* Top() const { return dialog_ ? dialog_ : page_; }
    const PageCore* Page() const { return page_; }
    const PageCore* Dialog() const { return dialog_; }

private:
    const PageCore* page_ = nullptr;
    const PageCore* dialog_ = nullptr;
};

// ---------------------------------------------------------------- the prompts
// A page's prompts: its bindings with a word whose action may run now (May == 0), and its way out (a binding without an
// action, Close / Back) always. Writes the binding indices into `out`; returns the count (at most promptlane::kMax).
inline int Prompts(const PageCore& p, const pageinput::Binding* b, int n, const void* model, int arg, int* out)
{
    int k = 0;
    for (int i = 0; i < n && k < promptlane::kMax; ++i)
    {
        if (!b[i].word)
            continue;
        bool show = b[i].role != pageinput::Role::Action;
        if (!show)
            for (int a = 0; a < p.nActions; ++a)
                if (p.actions[a].id == b[i].action)
                {
                    show = !p.actions[a].May || p.actions[a].May(model, arg) == 0;
                    break;
                }
        if (show)
            out[k++] = i;
    }
    return k;
}
} // namespace page
