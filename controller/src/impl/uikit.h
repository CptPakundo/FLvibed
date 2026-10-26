// The row kit of FLvibed's own screens (docs\UI_PLAN.md 3.2 "The row kit", docs\UI_BUILD.md): the game's fonts as
// fractions of the screen height, one input abstraction over the pad (Gamepad), the keyboard (the host's raw keys) and
// the game's cursor, and the row renderers of the Options screen's grammar (toggle, picker, slider, key, button) drawn
// through core\game\uidraw.* / render.*. The options page (options_page.cpp) is its first user; the pad screens of
// phase D build on it. Nothing here is in pixels of a fixed resolution: callers pass the screen height.
#pragma once
#include "common.h"
#include "game/render.h"

#include <string>
#include <vector>

namespace uikit
{
// ------------------------------------------------------------------ fonts
struct Fonts
{
    render::Font title;   // page titles and section headers (Agency FB, ~0.028 H)
    render::Font label;   // row labels, the control words, button captions (Agency FB, ~0.024 H)
    render::Font body;    // the rollover help, value captions (Arial Unicode MS, ~0.016 H)
    int sh = 0;           // built for this screen height
    int percent = 100;    // and this text_size
    bool ok = false;
};
// The fonts for the screen height (rebuilt when it or text_size changes; the game's faces when its files load).
const Fonts& FontsFor(int sh, int textSizePercent);
void FreeFonts(); // before the logic unloads (the textures belong to this DLL)
std::wstring Widen(const char* utf8);
std::string Narrow(const std::wstring& s);

// ------------------------------------------------------------------ keys
// The name of a virtual key as the `key` rows show and store it ("K", "F10", "Insert", "Num 5"); "" when unnamed.
std::string KeyName(int vk);
// A key name back to its virtual key; 0 when unknown.
int KeyFromName(const std::string& name);

// ------------------------------------------------------------------ input
struct UiInput
{
    // edges (with auto-repeat on up / down / left / right while held)
    bool up = false, down = false, left = false, right = false;
    bool accept = false, back = false, tabPrev = false, tabNext = false, pageUp = false, pageDown = false;
    bool anyPad = false; // the pad produced something this frame (the device that drives the focus)
    bool anyKey = false;
    // the pointer: the game's cursor in screen pixels; click = the left button's press edge, held = down now
    float px = 0, py = 0;
    bool pointer = false, click = false, held = false;
    int capturedVk = 0; // while capturing a key: the key pressed this frame (0 none)
};

class Input
{
  public:
    void Init();
    void Reset(); // forget held keys (the page closed)
    // From the host's OnRawKey (the game's thread, before the game acts). Returns true when the key is one the page
    // uses (to be swallowed while it is open): navigation keys, and every key while capturing.
    bool PushKey(int vk, bool down, bool capturing);
    // Once per frame: reads the pad and the queued keys into edges with repeat, the game's cursor and its left button.
    UiInput Poll(float dt, bool capturing);
    bool PadConnected() const;
    // (D-MENUS, 25 Oct 2026) The pad's other buttons as of the last Poll, for the pad screens that need more than the
    // page's grammar (X hold = delete, Y = keyboard, Menu = done, the right stick's scroll): edges and down states.
    struct PadExtra
    {
        bool x = false, y = false, menu = false, view = false, lb = false, rb = false, l3 = false, r3 = false;
        bool xDown = false, yDown = false, lbDown = false, rbDown = false, aDown = false, bDown = false;
        bool menuDown = false, viewDown = false;
        float xHeld = 0; // seconds X has been down (0 up)
        float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
        bool connected = false;
    };
    PadExtra Extra() const;

  private:
    struct KeyEvent
    {
        int vk;
        bool down;
    };
    void* pad_ = nullptr; // Gamepad (owned; the type stays out of this header)
    std::vector<KeyEvent> keys_;
    bool keyDown_[256] = {};
    float repeat_[4] = {};  // up, down, left, right: time until the next repeat while held
    bool prevClick_ = false;
    float backHold_ = 0;
};

// ------------------------------------------------------------------ row renderers
// A row's geometry: its band (y, h) across the frame, the label column at labelX, the control column ending at
// controlRight; sh = the screen height (the controls' sizes are fractions of it). All inside a render pass.
struct RowGeom
{
    float y, h;        // the row band (top, height)
    float labelX;      // where the label starts
    float controlLeft; // the control column (its left edge) ...
    float controlRight; // ... and right edge
    float sh;
};
struct RowStyle
{
    bool focused = false;  // the focus frame around the band
    bool enabled = true;   // false: greyed (a parent row is off)
    bool changed = false;  // the dot before the label (differs from the default)
    const wchar_t* note = nullptr; // "AFTER RESTART" / "AT THE NEXT LOAD" after the value
};
void RowLabel(const Fonts& f, const RowGeom& g, const RowStyle& s, const std::wstring& label);
void RowToggle(const Fonts& f, const RowGeom& g, const RowStyle& s, bool on);
void RowPicker(const Fonts& f, const RowGeom& g, const RowStyle& s, const std::wstring& word);
// t = the value's position 0..1; caption = the value text left of the track
void RowSlider(const Fonts& f, const RowGeom& g, const RowStyle& s, float t, const std::wstring& caption);
void RowKey(const Fonts& f, const RowGeom& g, const RowStyle& s, const std::wstring& keyName, bool capturing);
// A text button (the game's MMOk look): the framed box with its caption centred; lit = the focus / hover state.
void Button(const Fonts& f, float x, float y, float w, float h, float sh, const std::wstring& caption, bool lit,
            bool enabled);
// A sub-tab button (the Controls page's SHIP CONTROLS / USER INTERFACE): selected = the yellow caption.
void TabButton(const Fonts& f, float x, float y, float w, float h, float sh, const std::wstring& caption,
               bool selected, bool lit);
// The hit areas the renderers use, so the mouse agrees with the drawing.
struct PickerHit
{
    float x, w, cell; // the box's left edge and width, the arrow cell's width at its right end
};
PickerHit PickerArea(const RowGeom& g);
struct SliderHit
{
    float x, w; // the track's left edge and width
};
SliderHit SliderArea(const RowGeom& g);
} // namespace uikit
