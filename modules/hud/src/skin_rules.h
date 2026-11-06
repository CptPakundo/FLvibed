// HUD 2's skin: the language in numbers and pure rules (modules\hud\docs\UI_HUD2_SKIN.md 4, 6.2-6.7, 8). Host-free: no game, no drawing, so
// tests\skin_rules_test.cpp holds it. Sizes are fractions of the screen's height, Normal's. skinkit.* draws from these; skindialog.*, skinload.*,
// skincursor.* and skinmenu.* take their tables from here.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

namespace skinrules
{
// ---- the primitives' sizes (section 4)
constexpr float kPlateFill = 0.85f, kDialogFill = 0.92f, kRimAlpha = 0.70f;
constexpr float kChamfer = 0.012f;
constexpr float kHeaderTitle = 0.040f, kHeaderTag = 0.024f, kHeaderRule = 0.0012f;
constexpr float kSection = 0.024f;
constexpr float kBodyLow = 0.030f, kBodyHigh = 0.032f;
constexpr float kRowMin = 0.046f;
constexpr float kChipLow = 0.046f, kChipHigh = 0.066f, kChipWord = 0.030f;
constexpr float kFocusFill = 0.32f, kFocusBar = 0.005f, kFocusGap = 0.008f, kFocusEdge = 0.002f;
constexpr float kScrollTrack = 0.005f;
constexpr float kScrimModal = 0.70f, kScrimMap = 0.78f;
constexpr float kTooltipBody = 0.026f;
constexpr float kMaxFlashHz = 3.0f; // the owner's limit for every flash cue

// A chip's height: at least the low end, never above the high end, whatever the native hit box behind it measures.
inline float ChipHeight(float hitBoxH) { return std::min(kChipHigh, std::max(kChipLow, hitBoxH)); }
// A flash cue's rate is allowed.
inline bool FlashOk(float hz) { return hz > 0 && hz < kMaxFlashHz; }

// ---- the grammar (section 4, R12): what carries HUD 2's flavour onto a new screen
// Pips for any quantity: how many of n segments a share lights. Anything above zero lights one, so a nearly empty quantity never reads as none.
inline int PipsLit(float share, int n)
{
    if (n <= 0 || !(share > 0))
        return 0;
    const float s = std::min(1.0f, share);
    return std::max(1, std::min(n, static_cast<int>(s * n + 0.5f)));
}
// The accent edge faces the eye: a plate's edge is on the side nearest the reticle or the reading start. A plate left of the reference has it on
// its right, one right of it on its left; a dialog keeps its centred plate's top edge (R10).
enum class Edge
{
    Left,
    Right,
    Top
};
inline Edge AccentEdge(float plateCentreX, float eyeX, bool dialog)
{
    if (dialog)
        return Edge::Top;
    return plateCentreX < eyeX ? Edge::Right : Edge::Left;
}
// The HOLD ring: the share of the ring filled; a released hold shows nothing.
inline float HoldShare(float held, float needed, bool down)
{
    if (!down || needed <= 0 || held <= 0)
        return 0;
    return std::min(1.0f, held / needed);
}
// Which of a control's kinds takes the focused word in INK (R9): text controls, through +0x348. Box buttons and rich text keep today's.
enum class WordClass
{
    TextControl,
    BoxButton,
    RichText,
    SkinChip
};
inline bool WordTakesInk(WordClass c) { return c == WordClass::TextControl || c == WordClass::SkinChip; }

// The text control that carries the focused row's word (R9), by the focused control's name; false when the row has none. `out` holds at least 40.
inline bool FocusWord(const char* focus, char* out, size_t outLen)
{
    struct Map
    {
        const char *row, *word;
        bool pad2; // the word's number has two digits, the row's may have one
    };
    static const Map kMap[] = {{"PauseButtonBox", "PauseButtonText", false},
                               {"MissionVendorSummaryBox", "MissionVendorSummaryTitle", false},
                               {"ItemView", "ItemName", true},
                               {"ItemButton", "ItemName", true}};
    for (const Map& m : kMap)
    {
        const size_t n = strlen(m.row);
        if (strncmp(focus, m.row, n) != 0 || focus[n] < '0' || focus[n] > '9')
            continue;
        const int number = atoi(focus + n);
        const int wrote = m.pad2 ? snprintf(out, outLen, "%s%02d", m.word, number) : snprintf(out, outLen, "%s%d", m.word, number);
        return wrote > 0 && static_cast<size_t>(wrote) < outLen;
    }
    return false;
}

// ---- motion (6.7)
enum Motion : int
{
    MotionFull = 0,
    MotionFades = 1,
    MotionNone = 2
};
inline int ClampMotion(int v) { return v < MotionFull || v > MotionNone ? MotionFull : v; }
constexpr float kEaseInS = 0.150f, kEaseOutS = 0.100f;
// The scrim's level (0..1 of its peak) one step on: it eases in and out, input never waits for it. skin_motion 2 cuts.
inline float EaseStep(float level, bool open, float dt, int motion)
{
    if (ClampMotion(motion) == MotionNone)
        return open ? 1.0f : 0.0f;
    const float step = dt / (open ? kEaseInS : kEaseOutS);
    return open ? std::min(1.0f, level + step) : std::max(0.0f, level - step);
}

// ---- skin_native (section 8): a comma list of family labels that stay native. Blanks around a name are ignored, case counts for nothing.
inline bool NativeListed(const std::string& list, const char* label)
{
    if (!label || !*label)
        return false;
    size_t at = 0;
    while (at <= list.size())
    {
        size_t end = list.find(',', at);
        if (end == std::string::npos)
            end = list.size();
        size_t a = at, b = end;
        while (a < b && (list[a] == ' ' || list[a] == '\t'))
            ++a;
        while (b > a && (list[b - 1] == ' ' || list[b - 1] == '\t'))
            --b;
        if (b > a && b - a == strlen(label))
        {
            bool same = true;
            for (size_t i = 0; i < b - a; ++i)
                same = same && tolower(static_cast<unsigned char>(list[a + i])) == tolower(static_cast<unsigned char>(label[i]));
            if (same)
                return true;
        }
        at = end + 1;
    }
    return false;
}

// ---- dialogs (6.2)
enum class DialogKind
{
    Quit,       // NN_Confirm, the game's text 1883
    Delete,     // NN_Confirm, the game's text 1884 with the save's name
    ExitMenu,   // NN_Exit from the main menu
    ExitInGame, // NN_Exit in a game (Alt-F4): the same object, told by the screen behind it
    Abort,      // the map's abort question: the game's own text is the cost
    Other       // any other: the game's own words in the same primitive
};
struct DialogRow
{
    DialogKind kind;
    const wchar_t* title;
    const wchar_t* cost;   // null: the cost is read from the game (the save's name, the abort's own text)
    const wchar_t* action; // the game's order: action left, safe right
    const wchar_t* safe;
    bool focusAction; // nothing is at risk: the focus starts on the action
    bool amber;       // progress can be lost: the amber top edge; red stays with hostility
};
inline const DialogRow* Row(DialogKind k)
{
    static const DialogRow rows[] = {
        {DialogKind::Quit, L"QUIT TO THE MAIN MENU?", L"Progress since your last save will be lost.", L"QUIT ANYWAY", L"KEEP PLAYING", false, true},
        {DialogKind::Delete, L"DELETE THIS SAVE?", nullptr, L"DELETE", L"KEEP SAVE", false, true},
        {DialogKind::ExitMenu, L"EXIT FREELANCER?", L"Back to the desktop.", L"EXIT GAME", L"STAY", true, false},
        {DialogKind::ExitInGame, L"EXIT FREELANCER?", L"Progress since your last save will be lost.", L"EXIT ANYWAY", L"KEEP PLAYING", false, true},
        {DialogKind::Abort, L"ABORT THE MISSION?", nullptr, L"ABORT", L"KEEP GOING", false, true},
    };
    for (const DialogRow& r : rows)
        if (r.kind == k)
            return &r;
    return nullptr;
}
// The row for NN_Confirm's text. The dialog keeps no question id, so the text is the key, and a text that is not the English one (a mod, another
// language) gives Other: the game's own words stay. `name` takes the save's name out of the delete question.
inline DialogKind ConfirmKind(const wchar_t* text, std::wstring* name)
{
    if (!text)
        return DialogKind::Other;
    if (wcscmp(text, L"QUIT TO MAIN MENU") == 0)
        return DialogKind::Quit;
    static const wchar_t kDelete[] = L"ARE YOU SURE YOU WANT TO DELETE ";
    const size_t head = wcslen(kDelete), len = wcslen(text);
    if (len > head + 1 && wcsncmp(text, kDelete, head) == 0 && text[len - 1] == L'?')
    {
        if (name)
            name->assign(text + head, len - head - 1);
        return DialogKind::Delete;
    }
    return DialogKind::Other;
}
inline DialogKind ExitKind(const wchar_t* text, bool mainMenuBehind)
{
    if (!text || wcscmp(text, L"EXIT GAME?") != 0)
        return DialogKind::Other;
    return mainMenuBehind ? DialogKind::ExitMenu : DialogKind::ExitInGame;
}

// A dialog's plate around the native hit boxes: the verbs stand on those boxes, so the chips decide the height and the plate grows round them and
// its words, centred on the chips' middle (R10). Pixels. False when it would not fit the screen: that dialog stays the game's.
struct DialogBox
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0; // the plate
    float titleY = 0, costY = 0;          // the centre lines
};
struct DialogIn
{
    float W = 0, H = 0;
    float chipsX0 = 0, chipsX1 = 0, chipsY0 = 0, chipsY1 = 0; // the chips' outer bounds; 0 height: a dialog with no buttons
    float centreY = 0;                                        // with no chips: the dialog's own middle
    float titleW = 0, costW = 0;
    int costLines = 1;
};
inline bool DialogFit(const DialogIn& in, DialogBox& out)
{
    const float pad = 0.030f * in.H, title = kHeaderTitle * in.H, line = kBodyHigh * in.H, gap = 0.016f * in.H;
    const bool chips = in.chipsY1 > in.chipsY0;
    const float mid = chips ? (in.chipsX0 + in.chipsX1) / 2 : in.W / 2;
    const float halfWords = std::max(in.titleW, in.costW) / 2 + pad;
    const float half = std::max(chips ? (in.chipsX1 - in.chipsX0) / 2 + pad : 0.0f, halfWords);
    const float costH = in.costW > 0 ? line * std::max(1, in.costLines) : 0;
    const float words = title + (costH > 0 ? gap + costH : 0);
    float top, bottom;
    if (chips)
        bottom = in.chipsY1 + pad, top = in.chipsY0 - gap * 1.5f - words - pad;
    else
        top = in.centreY - words / 2 - pad, bottom = in.centreY + words / 2 + pad;
    if (mid - half < 0 || mid + half > in.W || top < 0 || bottom > in.H)
        return false;
    out.x0 = mid - half, out.x1 = mid + half, out.y0 = top, out.y1 = bottom;
    out.titleY = top + pad + title / 2;
    out.costY = out.titleY + title / 2 + gap + line / 2;
    return true;
}

// ---- the loading screen, form C (6.4)
constexpr float kLoadBarAfterS = 1.5f;
constexpr int kLoadPips = 24;
constexpr float kLoadGapS = 2.0f; // brackets further apart than this are another load
// What the screen shows after `elapsed` seconds of a load.
inline bool LoadBarShows(float elapsed) { return elapsed >= kLoadBarAfterS; }
// The native bar's share read as progress: the game has no 0..1 value, only a fill that wraps, so a wrap holds full: never a looped animation.
inline float LoadShare(float was, float now)
{
    if (!(now >= 0) || now > 1)
        return was;
    return std::max(was, now);
}

// ---- the cursor set (6.6, decisions\D1b_cursor.jpg): the 19 names of DATA\mouse.ini, one meaning each
enum class CursorShape
{
    Arrow,
    Stretch,
    Reticle,
    Bubble
};
enum class CursorTint
{
    Chrome,
    Neutral,
    Hostile,
    Friend,
    Amber
};
enum CursorForm : unsigned
{
    FormPlain = 0,
    FormCorners = 1, // hostile
    FormSpokes = 2,  // firing
    FormBar = 4      // cannot fire
};
struct CursorRow
{
    const char* name;
    CursorShape shape;
    CursorTint tint;
    unsigned form;
    const wchar_t* word; // a bubble's
};
constexpr int kCursorCount = 19;
inline const CursorRow* CursorRows()
{
    static const CursorRow rows[kCursorCount] = {
        {"arrow", CursorShape::Arrow, CursorTint::Chrome, FormPlain, nullptr},
        {"stretch", CursorShape::Stretch, CursorTint::Chrome, FormPlain, nullptr},
        {"cross", CursorShape::Reticle, CursorTint::Chrome, FormPlain, nullptr},
        {"neutral", CursorShape::Reticle, CursorTint::Neutral, FormPlain, nullptr},
        {"hostile", CursorShape::Reticle, CursorTint::Hostile, FormCorners, nullptr},
        {"friendly", CursorShape::Reticle, CursorTint::Friend, FormPlain, nullptr},
        {"fire", CursorShape::Reticle, CursorTint::Hostile, FormSpokes, nullptr},
        {"fire_neutral", CursorShape::Reticle, CursorTint::Neutral, FormSpokes, nullptr},
        {"fire_friendly", CursorShape::Reticle, CursorTint::Friend, FormSpokes, nullptr},
        {"no_fire", CursorShape::Reticle, CursorTint::Amber, FormBar, nullptr},
        {"talk_mission", CursorShape::Bubble, CursorTint::Chrome, FormPlain, L"JOB"},
        {"talk_info", CursorShape::Bubble, CursorTint::Chrome, FormPlain, L"INFO"},
        {"talk_rumor", CursorShape::Bubble, CursorTint::Chrome, FormPlain, L"RUMOR"},
        {"talk_story", CursorShape::Bubble, CursorTint::Chrome, FormPlain, L"STORY"},
        {"talk_bribe", CursorShape::Bubble, CursorTint::Chrome, FormPlain, L"BRIBE"},
        {"talk_blowoff", CursorShape::Bubble, CursorTint::Chrome, FormPlain, L"..."},
        {"talk_equip_dealer", CursorShape::Bubble, CursorTint::Chrome, FormPlain, L"GEAR"},
        {"talk_commodity_dealer", CursorShape::Bubble, CursorTint::Chrome, FormPlain, L"GOODS"},
        {"talk_ship_dealer", CursorShape::Bubble, CursorTint::Chrome, FormPlain, L"SHIPS"},
    };
    return rows;
}
// The row of the game's current cursor name; null for a name the table does not know (a mod's): that cursor stays the game's.
inline const CursorRow* CursorFor(const char* name)
{
    if (!name)
        return nullptr;
    const CursorRow* rows = CursorRows();
    for (int i = 0; i < kCursorCount; ++i)
#ifdef _WIN32
        if (_stricmp(rows[i].name, name) == 0)
#else
        if (strcmp(rows[i].name, name) == 0)
#endif
            return &rows[i];
    return nullptr;
}
// The game's software cursor scales with the screen's width, so this one does: 0.052 H at 16:9, the size of the frames.
constexpr float kCursorSizeW = 0.052f * 9.0f / 16.0f;
inline float CursorSize(float screenW) { return kCursorSizeW * screenW; }

// ---- CONTINUE (6.3, D9): one press loads the latest save
struct SaveStamp
{
    const char* file = nullptr;
    uint64_t time = 0; // FILETIME
    bool hidden = false;
};
// The newest listed save's index, -1 with none: no save, no CONTINUE. A later entry wins a tie, as the newest written.
inline int LatestSave(const SaveStamp* saves, int count)
{
    int best = -1;
    for (int i = 0; i < count; ++i)
        if (saves[i].file && saves[i].file[0] && !saves[i].hidden && (best < 0 || saves[i].time >= saves[best].time))
            best = i;
    return best;
}
// The six places of the menu: the game's five buttons keep the first and last native lines; CONTINUE takes the first, the five spread down to the
// last. `slot` 0 is CONTINUE, 1..5 the game's five.
inline float MenuSlotY(float firstY, float lastY, int slot) { return firstY + (lastY - firstY) * static_cast<float>(slot) / 5.0f; }
constexpr float kContinueTimeoutS = 6.0f; // the load screen's slide is 2.4 s: past this CONTINUE gives up and leaves it open

// ---- base text (rows 29-31, 41, 42; R1)
// One rung by resolution, 800 first: a rung doubles the stem at 800 lines, 1.35x at 4K. Rows at pitch 0.065 (the ship dealer's) take none.
constexpr float kRungHeight = 900.0f;
inline int NameRungs(float screenH, float rowPitch)
{
    if (rowPitch < 0.066f)
        return 0;
    return screenH <= kRungHeight ? 1 : 0;
}

// ---- one frame for the five windows (R8): one top line and left edge. x in fractions of W, y of H.
constexpr float kFrameTop = 0.115f;
constexpr float kFrameCentreY = 0.54f; // a NeuroNet window's centre: midway from the docked strip's tabs to the prompts
inline float FrameLeft(float sideW) { return sideW; }
} // namespace skinrules
