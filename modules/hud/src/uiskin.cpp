// Extended HUD 2's skin over the game's non-flight windows - see uiskin.h. The design and the build rules:
// modules\hud\docs\UI_HUD2_EXTENDED.md 9-10 (10.1 pinned); the geometry, colours and order: tools\uimock\hud2x.py (its plates are
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

#include "game/fl.h"
#include "flexe.h"
#include "game/room.h"
#include "ui/glyphs.h"
#include "safemem.h"
#include "hudhost.h"
#include "skin_rules.h"
#include "skindialog.h"
#include "skinkit.h"
#include "skinload.h"
#include "skinmenu.h"
#include "skinover.h"
#include "ui/hud_rows.h"
#include "ui/hud2_kit.h"
#include "log.h"
#include "navnew.h"
#include "navskin.h"
#include "nnstrip.h"
#include "ui/ui.h"
#include "ui/uiopt.h"
#include "ui/winplates.h"
#include "uistrip.h"
#include "windowtext.h"
#include "game/deckmode.h"
#include "game/render.h"

#include <sstream>
#include <vector>

namespace uiskin
{
namespace
{
using namespace hud2kit;
using namespace winplates;

Context g_c;
float g_dt = 0, g_scrim = 0, g_dialogScrim = 0, g_frameScrim = 0; // the frame's step; the eased scrims, 0..1 of their peak
const int kFamPlayerInfo = FamilyIndex("playerinfo");
bool g_applied = false; // something is written
u32 g_reclears = 0, g_ownRefused = 0;
float g_lastMs = 0;
u32 g_scenes = 0;
// The native ToolTip hidden (bit 1) while uistrip's character card replaces it - only then.
u32 g_tip = 0, g_tipHides = 0;

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
    // the .page text button: +0x84 = -1 takes its frame and keeps caption, hit and click (probe 6)
    if (exe::VtFrontTextButton.is(vt))
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

// The nav map's frame: its draw 0x4926D0 never reaches the base draw whole but calls the art half 0x5A06A0 itself, which skips a handle of -1 (probe 1, 12a).
void HideMapFrame(Win& w)
{
    u32 h = 0;
    for (const Own& o : w.own)
        if (o.ctl == w.root)
            return;
    if (R32(w.root + kOwnMesh, h) && h != kNoMesh && W32(w.root + kOwnMesh, kNoMesh))
        w.own.push_back({w.root, h});
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

// The own-art hides (+0x84) of a window (ui_style 1 only).
void HideOwnAll(Win& w)
{
    const FamilyDef& f = kFamilies[w.fam];
    if (!g_on || (g_scene && !f.modal))
        return;
    Box rb;
    // (the mission box: its art goes only while its plate can stand in - the root's own rect known; else native)
    if (f.ownRoot && (w.fam != kFamPopup || (RectOf(w.root, rb) && rb.box)))
        HideOwn(w, w.root);
    if (w.partner)
        HideOwn(w, w.partner);
    for (const char* n : f.own)
        if (n)
            if (const u32 c = Child(w.root, n))
                HideOwn(w, c);
    g_applied = true;
}

// One window fails, one window goes native (UI_HUD2_SKIN.md 3.1, 8): a window that fell keeps the game's look until it closes; it is built anew at the next
// open, which tries again. skin_native holds the same by name, for good.
std::vector<u32> g_fell;
ULONGLONG g_fellAt = 0;
bool Fell(u32 root) { return std::find(g_fell.begin(), g_fell.end(), root) != g_fell.end(); }
bool Listed(const char* label) { return skinrules::NativeListed(hudrows::g.skinNative, label); }
bool Native(const Win& w) { return Fell(w.root) || Listed(kFamilies[w.fam].label); }

// A window's writes, from the registry's Resolve while the skin is on.
void WriteWindow(Win& w)
{
    if (Native(w))
        return;
    const FamilyDef& f = kFamilies[w.fam];
    const u32 roots[2] = {w.root, w.partner};
    std::vector<u32> words;
    if (skindialog::IsDialog(w.fam))
    {
        bool menu = false;
        for (const Win& m : g_wins)
            menu |= m.fam == kFamMainMenu && Shown(m.root);
        if (!skindialog::Prepare(w, menu, words))
            return;
    }
    for (const char* n : f.art)
        if (n)
            for (u32 r : roots)
                if (r)
                    HideArt(w, Child(r, n));
    for (u32 c : words)
        HideArt(w, c);
    HideOwnAll(w);
    if (w.fam == kFamMainMenu) // its direct children: every `bImg` (art)
    {
        u32 c = 0;
        R32(w.root + kChild, c);
        for (int guard = 0; c && guard < 64; ++guard)
        {
            u32 vt = 0;
            if (R32(c, vt) && exe::VtImage.is(vt) && NameIs(c, "bImg"))
                HideArt(w, c);
            if (!R32(c + kSibling, c))
                break;
        }
        Recolour(w, Child(w.root, "Version"), Rgba(kDim, 0xFF)); // the corner's one line, DIM
    }
    if (w.fam == kFamNavMap)
    {
        navskin::Hides hides;
        if (navskin::Bind(w.root, hides))
        {
            for (int i = 0; i < hides.nArt; ++i)
                HideArt(w, hides.art[i]);
            HideMapFrame(w);
        }
    }
    for (const char* n : f.titles)
        if (n)
            for (u32 r : roots)
                if (r)
                    Recolour(w, Child(r, n), w.fam == kFamPopup ? Rgba(kInk, 0xFF) : kTitleColour);
    g_applied = true;
}

// The class rule (UI_HUD2_SKIN.md 3.3, 12a row 6) for what has no family: a `.page` screen, a mod's included. Art by bit 1, a text button's frame by +0x84: each
// only where its rectangle is known, for a plate never stands without its art hidden nor art hidden without its plate. A control of any other class stays
// visible. Pages rebuild on entry, so it runs at each new generation of the tree.
Win g_page;
u32 g_pageGen = 0xFFFFFFFFu;
void PageRule()
{
    if (g_pageGen == g_gen || Listed("page"))
        return;
    g_pageGen = g_gen;
    u32 head = 0, node = 0;
    if (!exe::UiControls.ok() || !R32(static_cast<u32>(exe::UiControls.get()), head) || !head || !R32(head + 4, node))
        return;
    // what the game rebuilt is gone with its controls; what stands keeps its memory, for its write can only go back through it
    const auto gone = [](u32 ctl)
    {
        u32 vt = 0;
        u8 st = 0;
        return !R32(ctl, vt) || !(exe::VtPageObject.is(vt) || exe::VtFrontTextButton.is(vt)) || !R8(ctl + kState, st) || (st & 4);
    };
    g_page.art.erase(std::remove_if(g_page.art.begin(), g_page.art.end(), [&](const Art& a) { return gone(a.ctl); }), g_page.art.end());
    g_page.own.erase(std::remove_if(g_page.own.begin(), g_page.own.end(), [&](const Own& o) { return gone(o.ctl); }), g_page.own.end());
    for (int n = 0; node && node != head && n < 8000; ++n)
    {
        u32 ctl = 0, vt = 0;
        u8 st = 0;
        if (!R32(node + 8, ctl) || !R32(node + 4, node))
            break;
        if (!ctl || !R32(ctl, vt) || !R8(ctl + kState, st) || (st & 4) || !(st & 1))
            continue;
        const bool art = exe::VtPageObject.is(vt), button = exe::VtFrontTextButton.is(vt);
        if (!art && !button)
            continue;
        bool family = false;
        for (const Win& w : g_wins)
            family |= IsAncestor(w.root, ctl);
        Box b;
        if (family || !RectOf(ctl, b) || !b.box)
            continue;
        if (art)
            HideArt(g_page, ctl);
        else
            HideOwn(g_page, ctl);
    }
}

void PagePaint()
{
    for (const Art& a : g_page.art)
    {
        Box b;
        if (Shown(a.ctl) && RectOf(a.ctl, b) && b.box)
            skinkit::Plate(b.x0, b.y0, b.x1, b.y1, kTLBR);
    }
    const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
    for (const Own& o : g_page.own)
    {
        Box b;
        if (FullyShown(o.ctl) && RectOf(o.ctl, b) && b.box)
        {
            skinkit::ChipGlass(b.x0, b.y0, b.x1, b.y1);
            if (Enabled(o.ctl) && mx >= b.x0 && mx <= b.x1 && my >= b.y0 && my <= b.y1)
                skinkit::Focus(b.x0, b.y0, b.x1, b.y1);
        }
    }
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
        !safemem::Read(ctl + kPos, pos, sizeof(pos)) || !safemem::Read(ctl + kOrient, orient, sizeof(orient)))
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

// The focused word in INK (UI_HUD2_SKIN.md 4, R9): one text control at a time, through the same +0x348 as the titles. Only a text control takes it, and it is
// written each frame it differs, since the game recolours a hovered row itself; what the game wrote last is what goes back.
struct FocusWord
{
    u32 ctl = 0, before = 0;
    char name[40] = {};
} g_word;
const u32 kInkColour = Rgba(kInk, 0xFF);

void RestoreWord()
{
    u32 c = 0;
    if (g_word.ctl && NameIs(g_word.ctl, g_word.name) && R32(g_word.ctl + kColour, c) && c == kInkColour)
        W32(g_word.ctl + kColour, g_word.before);
    g_word.ctl = 0;
}

void FocusWordStep(u32 focus)
{
    char name[33] = {}, word[40] = {};
    u32 parent = 0, ctl = 0, vt = 0, c = 0;
    if (focus && RBytes(focus + kName, name, 32) && skinrules::FocusWord(name, word, sizeof(word)) && R32(focus + kParent, parent))
        ctl = Child(parent, word);
    if (!ctl || !R32(ctl, vt) || !exe::VtTitleText.is(vt) || !R32(ctl + kColour, c))
        ctl = 0;
    if (ctl != g_word.ctl)
        RestoreWord();
    if (!ctl)
        return;
    if (c != kInkColour && W32(ctl + kColour, kInkColour))
        g_word.ctl = ctl, g_word.before = c, strcpy_s(g_word.name, word);
}

// a cached window still exists: its vtable, not deleted (+0x6C bit 2)
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
    // on the pad, under a modal menu, the native button help goes too (a stale "Go to the options
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
    // the Shadow tier (UI_HUD2_SKIN.md 6.6): the game's box hidden only while its text reads; drawn again by skinover. Not under a card, which carries the words
    static u32 shadow = 0, shadowGen = 0xFFFFFFFFu;
    if (shadowGen != g_gen || (shadow && !NameIs(shadow, "ToolTip")))
    {
        shadow = static_cast<u32>(ui::FindControl("ToolTip")), shadowGen = g_gen;
        u32 vt = 0;
        if (shadow && (!R32(shadow, vt) || !exe::VtToolTip.is(vt)))
            shadow = 0;
    }
    const bool read = !want && shadow && !Listed("tooltip") && skinover::TipRead(shadow);
    if (read)
        want = shadow;
    else if (want || !shadow)
        skinover::TipRead(0);
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
    skindialog::Clear();
    skinmenu::GiveBack();
    RestoreWord();
    navskin::Unbind();
    RestoreTip();
    Restore(g_page);
    g_pageGen = 0xFFFFFFFFu;
    for (Win& w : g_wins)
        if (Valid(w.root, w.vt))
            Restore(w);
    g_wins.clear();
    g_applied = false;
}

void Fall(Win& w, const char* why)
{
    Restore(w);
    skindialog::Forget(w.root);
    g_fell.push_back(w.root);
    g_fellAt = GetTickCount64();
    Log("skin: %s goes native until it closes (%s)", kFamilies[w.fam].label, why);
}

// Windows that closed may try again at their next open.
void PruneFell()
{
    for (size_t i = g_fell.size(); i-- > 0;)
    {
        bool live = false;
        for (const Win& w : g_wins)
            live |= w.root == g_fell[i];
        if (!live)
            g_fell.erase(g_fell.begin() + static_cast<std::ptrdiff_t>(i));
    }
    skindialog::Prune();
}

// every frame: the art bits the game re-showed cleared again (the NavBar's frame / tab do it each frame)
void ReclearList(const std::vector<Art>& art)
{
    for (const Art& a : art)
    {
        u8 st = 0;
        if (R8(a.ctl + kState, st) && (st & 2) && StartsWithName(a.ctl, a.name))
            W8(a.ctl + kState, static_cast<u8>(st & ~2)), ++g_reclears;
    }
}
void Reclear()
{
    for (Win& w : g_wins)
    {
        // the map's info card is made on the first selection, after Bind: picked up here when it appears (the owner, 7 Nov 2026: still over VIEW with a waypoint)
        if (w.fam == kFamNavMap && Shown(w.root) && !Native(w) && !navskin::NativeAsked())
        {
            for (const char* name : {"NavMapInfoCard", "NavMapInfoText", "NavMapInfoTitle", "NavMapMissionObjectiveScroll"})
                HideArt(w, Child(w.root, name));
            // the game's waypoint list: HUD 2's ROUTE section shows it (navskin Waypoints; the owner, 7 Nov 2026)
            for (int i = 1; i <= 8; ++i)
            {
                char box[40], text[40];
                sprintf_s(box, "NavMapMissionObjective%d", i);
                sprintf_s(text, "NavMapMissionObjectiveText%d", i);
                HideArt(w, Child(w.root, box));
                HideArt(w, Child(w.root, text));
            }
        }
        ReclearList(w.art);
    }
    ReclearList(g_page.art);
}

// The first sight of a window with no family says so, once: "complete" is a checked claim, and an unknown window stays native (UI_HUD2_SKIN.md 3.3).
void NoteUnknownRoots()
{
    static u32 seenGen = 0xFFFFFFFFu;
    static std::set<std::string> seen;
    if (seenGen == g_gen)
        return;
    seenGen = g_gen;
    for (uintptr_t r : ui::VisibleRoots())
    {
        bool known = false;
        for (const Win& w : g_wins)
            known |= w.root == static_cast<u32>(r);
        if (known)
            continue;
        u32 vt = 0;
        const std::string name = ui::ControlName(r);
        if (R32(static_cast<u32>(r), vt) && seen.insert(name).second)
            Log("skin: native window %s vt 0x%08X", name.c_str(), vt);
    }
}

// `uiskin seams`: every shown control drawing art nobody hid or claimed. It walks the tree on request and rests on no counter: key capture and jettison never move
// the generation (12a row 12). A static image or a page object counts as art; so does an icon or page button that still has its frame.
std::string Seams()
{
    std::ostringstream o;
    u32 head = 0, node = 0;
    if (!exe::UiControls.ok() || !R32(static_cast<u32>(exe::UiControls.get()), head) || !head || !R32(head + 4, node))
        return "no control list";
    int count = 0;
    for (int n = 0; node && node != head && n < 8000; ++n)
    {
        u32 ctl = 0, vt = 0, mesh = kNoMesh;
        if (!R32(node + 8, ctl) || !R32(node + 4, node))
            break;
        if (!ctl || !FullyShown(ctl) || !R32(ctl, vt))
            continue;
        bool art = exe::VtImage.is(vt) || exe::VtPageObject.is(vt);
        if (!art && (exe::VtIconButton.is(vt) || exe::VtFrontTextButton.is(vt)))
            art = R32(ctl + kOwnMesh, mesh) && mesh != kNoMesh;
        if (!art)
            continue;
        bool up = true; // a hidden parent hides it
        u32 p = ctl;
        for (int i = 0; p && i < 32; ++i)
        {
            up = up && FullyShown(p);
            if (!R32(p + kParent, p))
                break;
        }
        if (!up)
            continue;
        ++count;
        if (count <= 200)
        {
            char line[96];
            sprintf_s(line, "  seam %s vt 0x%08X\n", ui::ControlName(ctl), vt);
            o << line;
        }
    }
    return "seams " + std::to_string(count) + "\n" + o.str();
}

// ---------------------------------------------------------------- drawing (fractions of H)
inline float Hf(float f) { return f * g_H; }

using skinkit::Rim;
inline void Glass(float x0, float y0, float x1, float y1, int corners, float fill = 0.85f) { skinkit::Plate(x0, y0, x1, y1, corners, fill); }
inline void RuleAt(float x0, float x1, float y, float a = 130 / 255.0f) { skinkit::Rule(x0, x1, y, a); }
inline void FocusFill(float x0, float y0, float x1, float y1, float fill = 0.32f) { skinkit::Focus(x0, y0, x1, y1, fill); }
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

// Focus = the game's selection where the window keeps one (the job board and the ship dealer marked the
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
    // the job board: the selected row is the focus whatever is hovered (at 21:9 the game's hover sat on ACCEPT
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

u32 g_focusCtl = 0; // the control the focus stands on this frame: its word goes to INK

// The drawn element of the hovered control (the one focus style: modules\hud\docs\UI_HUD2_EXTENDED.md 10 "Focus").
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
    g_focusCtl = hov;
    return true;
}

// The bottom of a rich text as it reads now (pixels; 0 unknown): its projected top + the text's own height, at most the
// box's (past that the game's scroll bar takes over). RichTextWin (exe::VtText): +0x35C the box's height, +0x380 the laid
// out text's height, read 4 Nov 2026 at 1280x800: 187 and 110 for five lines (FREELANCER_RE "Job board"). GitHub issue 13: the
// box runs 51 px past the plate's fixed foot there, so a job of seven or eight lines ran out of the window.
int g_jobHeightTest = 0; // debug `uiskin jobheight <px>`: the text's height as read (0: the game's). A raw text set on this
                         // control ended the game on 4 Nov 2026 (its vt[0xA8] message 0x1D): never write text to it that way
int g_platesPainted = 0; // this window's, this frame: art hidden with no plate standing is the fault section 8 forbids
void PaintPlate(float x0, float y0, float x1, float y1, const PlateDef& p)
{
    ++g_platesPainted;
    Glass(x0, y0, x1, y1, p.corners, p.fill);
    if (p.rule > 0)
        RuleAt(x0, x1, y0 + Hf(p.rule));
    if (p.div1 > 0)
        RuleAt(x0, x1, y0 + Hf(p.div1), 110 / 255.0f);
    if (p.div2 > 0)
        RuleAt(x0, x1, y0 + Hf(p.div2), 110 / 255.0f);
}
void PaintGlass(float x0, float y0, float x1, float y1, int corners, float fill) { Glass(x0, y0, x1, y1, corners, fill); }
void PaintUnderline(float x, float y)
{
    const float u = Hf(0.0045f);
    Line(x - Hf(0.030f), y - u * 1.6f, x + Hf(0.030f), y - u * 1.6f, u, Argb(kActive, 1));
}
void PaintBumpers(float x0, float x1, float cy)
{
    Glyph(glyphs::LB, x0 - Hf(0.030f), cy, Hf(0.032f), Argb(kChrome, 1));
    Glyph(glyphs::RB, x1 + Hf(0.030f), cy, Hf(0.032f), Argb(kChrome, 1));
}
const Ops kOps = {WriteWindow, Restore, PaintPlate, TabPlate, PaintGlass, PaintUnderline, PaintBumpers};

bool DrawWindow(const Win& w, u32 hov) // true: it drew the focus
{
    if (Native(w))
        return false;
    if (skindialog::IsDialog(w.fam)) // (their own pass; a dialog that stays the game's keeps today's mark on the confirm alone)
    {
        if (skindialog::Skinned(w.root) || w.fam != kFamConfirm)
            return skindialog::Skinned(w.root);
    }
    WindowPass pass;
    WindowPlates(w, pass);
    const int page = pass.page;
    const u32 currentTab = pass.currentTab;
    const bool haveTabFocus = pass.haveTabFocus && !(w.fam == kFamMainMenu && skinmenu::Focused()); // (one focus per screen: CONTINUE has it)
    Box tabFocus = pass.tabFocus;
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
    // (FLvibed's own page draws its own focus and owns the pad: no second mark on the game's tabs)
    if (w.fam == kFamOptions && page == 2)
        return false;
    // (never on the current page's tab - in game it is not disabled, its yellow caption marks it)
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

// One family's pass under its own guard: a fault there stops that window, not the module.
int DrawGuarded(const Win& w, u32 hov) // 1 it drew the focus, 0 not, -1 a fault
{
    __try
    {
        return DrawWindow(w, hov) ? 1 : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

// How many frames a window with art hidden may stand with no plate: a slide or a rebuild can lose an anchor for a moment.
constexpr int kNoPlateFrames = 30;
struct NoPlate
{
    u32 root;
    int frames;
};
std::vector<NoPlate> g_noPlate;
bool PlateMissing(const Win& w)
{
    bool anyPlate = false;
    for (const PlateDef& p : kFamilies[w.fam].plates)
        anyPlate |= p.anchor != nullptr;
    if (!anyPlate || (w.art.empty() && w.own.empty()))
        return false;
    for (size_t i = 0; i < g_noPlate.size(); ++i)
        if (g_noPlate[i].root == w.root)
        {
            if (g_platesPainted > 0)
            {
                g_noPlate.erase(g_noPlate.begin() + static_cast<std::ptrdiff_t>(i));
                return false;
            }
            return ++g_noPlate[i].frames >= kNoPlateFrames;
        }
    if (g_platesPainted == 0)
        g_noPlate.push_back({w.root, 1});
    return false;
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
    // (every frame: at 4 Hz the highlight stood for frames on a spot its control had left, the owner, 6 Nov 2026)
    const u32 hit = static_cast<u32>(ui::HitTest(static_cast<float>(fl::MouseX()), static_cast<float>(fl::MouseY())));
    const u32 focus = hit ? hit : static_cast<u32>(ui::Hovered());
    char name[16] = {};
    // (the bar settling: the cursor still stands where the last room left it, and a box flashed on a room button on the way)
    // (and with a window open the pad's focus is the window's: the cursor crossed a room button on its way to a row still sliding in, and the box flashed on it)
    if (focus && (winplates::BarBusy() || (g_c.pad && winplates::WindowOpen())) && RBytes(focus + kName, name, 15) && (!strncmp(name, "ButtonControl", 13) || !strncmp(name, "RoomControl", 11)))
        return 0;
    return focus;
}

void Draw(void* device)
{
    g_haveOverFocus = false;
    // (multiplayer's CHAT and GROUP are NeuroNet windows no family knows: the strip stands on them all the same)
    if ((g_wins.empty() && nnstrip::ScrimAlpha() <= 0) || !device || g_sh < 100)
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
    skinkit::SetScreen(g_W, g_H);
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
    // the full-screen scrim (the black planes are hidden): it eases in and out, and input never waits for it
    g_scrim = skinrules::EaseStep(g_scrim, g_modalOpen, g_dt, hudrows::g.skinMotion);
    skinkit::Scrim(skinrules::kScrimModal, g_scrim);
    // R8's one scrim behind the NeuroNet windows (a dealer's NN_Inventory is the dealer's, no window of this family)
    bool neural = false;
    for (const Win& w : g_wins)
        neural |= (w.fam == kFamInventory || w.fam == kFamLog || w.fam == kFamPlayerInfo || w.fam == kFamHelp) && Shown(w.root) && !Native(w);
    neural |= nnstrip::ScrimAlpha() > 0; // (the strip knows the map)
    g_frameScrim = skinrules::EaseStep(g_frameScrim, neural && !g_scene, g_dt, hudrows::g.skinMotion);
    if (!g_modalOpen)
        skinkit::Scrim(navnew::Grown() ? skinrules::kScrimFullMap : skinrules::kScrimMap, g_frameScrim); // (one scrim from MAP to INVENTORY: R8; the map's number, the owner's)
    // the docked frame while the game draws it (not under a modal menu: the NavBar is not drawn while paused)
    // (not in a NeuroNet window's close: the game hides the bar's and the box's art that frame while their controls
    // still read shown - the empty plates stood one frame, 26 Oct 2026, research\experience\runs\dealer_close_2026-10-26)
    if (bar && !g_modalOpen && !g_scene && FullyShown(bar->root) && !room::NNWindowClosing())
    {
        g_dockedFrame = !conversation; // (the focus on the bar's buttons: not while hidden)
        g_dockedLane = true;
        DockedPlates(*bar, nn && Shown(nn->root) ? nn : nullptr, !conversation);
    }
    for (Win& w : g_wins)
        if (w.fam == kFamOptions && Shown(w.root))
            OneCurrentTab(w);
    const u32 hov = FocusedControl();
    g_focusCtl = 0;
    bool focused = false, window = false;
    for (Win& w : g_wins)
        if (w.fam != kFamNavBar && w.fam != kFamNNBox && Shown(w.root) && (!g_scene || kFamilies[w.fam].modal) &&
            !(w.fam == kFamMainMenu && g_modalOpen)) // (the main menu's plates never over a modal screen's scrim)
        {
            g_platesPainted = 0;
            const int drew = DrawGuarded(w, hov);
            if (drew < 0)
                Fall(w, "a fault in its pass");
            else if (!Native(w) && PlateMissing(w))
                Fall(w, "no plate could stand");
            focused |= drew > 0, window = true;
        }
    // one focus mark per screen while a window owns the input, no mark on the room bar / its sub-row
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
    FocusWordStep(g_focusCtl);
    PagePaint();
    {
        u32 menu = 0;
        for (const Win& w : g_wins)
            if (w.fam == kFamMainMenu && Shown(w.root) && !g_modalOpen && !Native(w))
                menu = w.root;
        if (menu)
            skinmenu::Draw(menu);
    }
    const Win* map = nullptr;
    for (const Win& w : g_wins)
        if (w.fam == kFamNavMap)
            map = &w;
    navskin::Draw(g_scene || g_modalOpen ? nullptr : map, g_c.pad);
    if (!g_scene)
        windowtext::DrawMarquees(); // (the dealers' long row names)
    SetKeepDestAlpha();
    End();
}
} // namespace

void Update(float dt, const Context& c)
{
    g_dt = dt;
    g_c = c;
    g_padDevice = c.pad;
    g_ops = &kOps;
}

bool On() { return g_c.enabled && g_c.style == 1; }
bool PadOwns() { return g_c.pad; }

void RenderUnder(void* device)
{
    perf::Scope ps("uiskin");
    const double t0 = perf::Now();
    static bool wasOn = false;
    nnstrip::BeginPass();
    navskin::BeginPass();
    g_on = On();
    if (!g_on)
    {
        if (g_applied || g_tip || wasOn)
            RestoreAll(), g_genSeen = 0xFFFFFFFFu;
        wasOn = false;
        winplates::ClassicPass(device, g_c.enabled, g_c.pad, g_c.style == 0);
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
    skinload::Ended(); // (no UI pass runs in a load)
    Refresh();
    PruneFell();
    NoteUnknownRoots();
    PageRule();
    if (navskin::TakeFailure()) // (one window fails, one window goes native: its writes back at once)
        for (Win& w : g_wins)
            if (w.fam == kFamNavMap && Valid(w.root, w.vt))
                Restore(w);
    bool mapUp = false;
    for (const Win& w : g_wins)
        mapUp |= w.fam == kFamNavMap;
    if (!mapUp)
        navskin::Unbind();
    Reclear();
    ApplyTip();
    {
        const Win* menu = nullptr;
        for (const Win& w : g_wins)
            if (w.fam == kFamMainMenu && Shown(w.root) && !Native(w))
                menu = &w;
        static const std::vector<u32> none;
        skinmenu::Layout(menu ? menu->root : 0, menu ? menu->tabs : none, !g_scene);
    }
    Geometry(device);
    Draw(device);
    g_lastMs =static_cast<float>((perf::Now() - t0) * 1000.0);
}

bool ModalOpen()
{
    // (only while the skin already runs: its first frame on, or one going off, answers false - RenderUnder owns the
    // switch and the restore)
    if (!g_on || !On())
        return false;
    Refresh();
    for (const Win& w : g_wins)
        if ((kFamilies[w.fam].modal || skindialog::IsDialog(w.fam)) && Valid(w.root, w.vt) && Shown(w.root))
            return true;
    return false;
}

void DrawMap(void* device)
{
    if (!g_on || !device)
        return;
    int sw = 0, sh = 0;
    render::SetDevice(device);
    if (!render::ScreenSize(sw, sh) || sh < 100 || !Begin(device, sw, sh, std::max(8, static_cast<int>(0.026f * sh + 0.5f))))
        return;
    SetAlpha(1), SetOffset(0, 0);
    skinkit::SetScreen(static_cast<float>(sw), static_cast<float>(sh));
    navskin::DrawOver();
    nnstrip::Draw(); // (over the game's UI: the map's viewport runs under it)
    End();
}

void DrawModal(void* device)
{
    if (!g_on || !device)
        return;
    bool open = false;
    for (const Win& w : g_wins)
        open |= skindialog::Skinned(w.root) && Shown(w.root);
    const ULONGLONG now = GetTickCount64();
    const bool note = g_fellAt && now - g_fellAt < 4000;
    g_dialogScrim = skinrules::EaseStep(g_dialogScrim, open, g_dt, hudrows::g.skinMotion);
    if (g_dialogScrim <= 0 && !note)
        return;
    int sw = 0, sh = 0;
    render::SetDevice(device);
    if (!render::ScreenSize(sw, sh) || sh < 100 || !Begin(device, sw, sh, std::max(8, static_cast<int>(0.026f * sh + 0.5f))))
        return;
    SetAlpha(1), SetOffset(0, 0);
    skinkit::SetScreen(static_cast<float>(sw), static_cast<float>(sh));
    skinkit::Scrim(skinrules::kScrimModal, g_dialogScrim);
    const u32 hov = open ? FocusedControl() : 0;
    for (Win& w : g_wins)
        if (skindialog::Skinned(w.root) && Shown(w.root) && !skindialog::Draw(w, hov))
            Fall(w, "its boxes cannot be read or do not fit");
    if (note) // a window in the game's ornate look would read as a new bug: one line says it is on purpose
        Text(sw * 0.05f, sh * 0.90f, L"THIS WINDOW KEEPS THE GAME'S LOOK", skinkit::Px(0.024f), Argb(kAmber, 1));
    End();
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
    skinkit::SetScreen(static_cast<float>(sw), static_cast<float>(sh));
    // (over the dialog's text: a lighter fill than under the glass, the edges and bar carry the mark)
    FocusFill(g_overFocus.x0, g_overFocus.y0, g_overFocus.x1, g_overFocus.y1, 0.22f);
    End();
}

void GiveBack() { RestoreAll(); }

void Shutdown()
{
    RestoreAll();
    g_genSeen = 0xFFFFFFFFu;
    g_ops = nullptr;
    g_on = false;
    g_c = {};
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
    if (what == "seams") // what is still drawn in the game's art: empty is the claim "complete" makes
        return Seams();
    if (what == "cursorsheet") // cursorsheet <0|1>: every flight reticle drawn in a row across the middle (debug, 7 Nov 2026)
    {
        int on = 0;
        in >> on;
        skinover::Sheet(on != 0);
        return std::string("uiskin cursorsheet ") + (on ? "1" : "0");
    }
    if (what == "loadplace") // loadplace <0|1>: the loading screen tries the player's base and system (the probe of UI_HUD2_SKIN.md 6.4)
    {
        int on = 0;
        in >> on;
        skinload::SetPlaceProbe(on != 0);
        return skinload::Status();
    }
    if (what == "rescan")
    {
        g_genSeen = 0xFFFFFFFFu;
        return "uiskin: rescan next frame";
    }
    if (what == "jobheight") // jobheight <px>: the job text's height as the plate reads it, 0 = the game's (nothing written to the game)
    {
        int px = 0;
        in >> px;
        g_jobHeightTest = std::max(0, px);
        return "uiskin jobheight " + std::to_string(g_jobHeightTest);
    }
    if (what == "windows" || what == "state" || what.empty())
    {
        o << "uiskin on=" << g_on << " style=" << g_c.style << " pad=" << g_c.pad << " gen=" << g_gen
          << (g_genHost ? " (host v23)" : " (poll 500 ms)") << " rescans=" << g_rescans << " reclears=" << g_reclears
          << " ownRefused=" << g_ownRefused << " reposed=" << g_reposed << "/" << g_reposeFaults << " reposeRefused=" << g_reposeRefused << " modal=" << g_modalOpen << " docked=" << g_dockedFrame
          << " scene=" << g_scene << " scenes=" << g_scenes << " tip=" << (g_tip ? 1 : 0) << " tipHides=" << g_tipHides
          << " ms=" << g_lastMs << " windows=" << g_wins.size() << " page=" << g_page.art.size() << "/" << g_page.own.size() << " fell=" << g_fell.size() << " "
          << skinmenu::Status() << " " << skinload::Status() << " " << skinover::Status();
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
    return "usage: uiskin state | windows | restore | rescan | seams | loadplace <0|1>";
}

} // namespace uiskin
