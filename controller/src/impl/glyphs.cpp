#include "glyphs.h"

#include "game/gamefont.h"
#include "log.h"
#include "uiopt.h"

#include <map>
#include <sstream>

namespace glyphs
{
static const char* kNames[COUNT] = {"A",  "B",  "X",  "Y",  "LB", "RB", "LT", "RT", "LS",
                                    "RS", "L3", "R3", "DU", "DD", "DL", "DR", "VIEW", "MENU"};

Id FromName(const char* name)
{
    for (int i = 0; name && i < COUNT; ++i)
        if (_stricmp(kNames[i], name) == 0)
            return static_cast<Id>(i);
    return COUNT;
}
const char* Name(Id id) { return id >= 0 && id < COUNT ? kNames[id] : "?"; }
const char* FamilyName(Family f)
{
    switch (f)
    {
    case Family::Xbox: return "xbox";
    case Family::PlayStation: return "playstation";
    case Family::Nintendo: return "nintendo";
    }
    return "?";
}

// ---------------------------------------------------------------- family detection (RawInput HID vendor ids)
static Family g_detected = Family::Xbox;
static Family g_current = Family::Xbox;
static bool g_detectedOnce = false;
static bool g_realWas = false;
static u32 g_vendorsSeen = 0;

static void Detect()
{
    // Every HID game controller the system lists (RawInput; XInput itself hides the vendor): a Sony or Nintendo pad
    // among them picks its family, else Xbox (an XInput pad with no HID match reads as Xbox too).
    UINT n = 0;
    if (GetRawInputDeviceList(nullptr, &n, sizeof(RAWINPUTDEVICELIST)) != 0 || n == 0)
    {
        g_detected = Family::Xbox;
        return;
    }
    std::vector<RAWINPUTDEVICELIST> list(n);
    n = GetRawInputDeviceList(list.data(), &n, sizeof(RAWINPUTDEVICELIST));
    Family f = Family::Xbox;
    int pads = 0;
    g_vendorsSeen = 0;
    for (UINT i = 0; i < n && i < list.size(); ++i)
    {
        if (list[i].dwType != RIM_TYPEHID)
            continue;
        RID_DEVICE_INFO info = {};
        info.cbSize = sizeof(info);
        UINT size = sizeof(info);
        if (GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICEINFO, &info, &size) == static_cast<UINT>(-1))
            continue;
        // generic desktop page, joystick (4) or gamepad (5)
        if (info.hid.usUsagePage != 1 || (info.hid.usUsage != 4 && info.hid.usUsage != 5))
            continue;
        ++pads;
        g_vendorsSeen = g_vendorsSeen * 65536u + (info.hid.dwVendorId & 0xFFFF);
        if (info.hid.dwVendorId == 0x054C)
            f = Family::PlayStation;
        else if (info.hid.dwVendorId == 0x057E && f != Family::PlayStation)
            f = Family::Nintendo;
    }
    if (f != g_detected || !g_detectedOnce)
        Log("glyphs: %d HID game controller(s), family %s (vendors 0x%X)", pads, FamilyName(f), g_vendorsSeen);
    g_detected = f;
    g_detectedOnce = true;
}

void Update(bool realPadConnected)
{
    if (!g_detectedOnce || realPadConnected != g_realWas)
        Detect();
    g_realWas = realPadConnected;
    const int style = uiopt::Get().glyphStyle;
    g_current = style == 1 ? Family::Xbox : style == 2 ? Family::PlayStation : style == 3 ? Family::Nintendo : g_detected;
}

Family Current() { return g_current; }

Look LookFor(bool hud2Surface)
{
    const int look = uiopt::Get().glyphLook;
    if (look == 1)
        return Look::Coloured;
    if (look == 2)
        return Look::Hud2;
    return hud2Surface ? Look::Hud2 : Look::Coloured; // 0 automatic: each kit its own look (before glyph_look)
}

u32 FaceRgb(Family f, Id id)
{
    if (id < A || id > Y)
        return 0;
    static const u32 kXbox[4] = {0x6DBE45, 0xE24A3A, 0x3C8EE0, 0xF2C230};
    static const u32 kPs[4] = {0x7E9BE6, 0xE8655A, 0xD58FD8, 0x6FCF97}; // cross, circle, square, triangle
    return f == Family::Xbox ? kXbox[id] : f == Family::PlayStation ? kPs[id] : kPressedArm;
}

// ---------------------------------------------------------------- rasterisation (GDI, 4x supersampled)
// A glyph is two layers in the atlas (docs\FREELANCER_RE.md "HUD panels" for the colours, TASK_GLYPHS_HUD.md 5 for the
// rules): the FIXED layer = the body (the HUD's panel base (13, 11, 26) at 0.8 coverage, so the glyph backs itself only
// within its own silhouette) and what keeps its family colour (Xbox A B X Y letters, the PlayStation shapes, the
// Nintendo letters); the TINT layer = everything in the HUD's chrome (the outlines, the letters on bumpers / triggers /
// sticks in the HUD's own face Agency FB, the View / Menu marks, the D-pad's cross and its pressed arm), drawn white and
// tinted at draw time: the chrome blue normally, the game's active yellow when the control it badges is lit. No grey
// discs: the only disc is the face button's body in the panel base.
constexpr int kSS = 4; // supersampling factor
constexpr u8 kBodyCov = 204; // the body's coverage (0.8): the HUD's panels are translucent too

struct Canvas
{
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    u32* px = nullptr;
    int w = 0, h = 0;
    bool Make(int cw, int ch)
    {
        w = cw, h = ch;
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h; // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        dc = CreateCompatibleDC(nullptr);
        void* bits = nullptr;
        bmp = dc ? CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
        if (!bmp)
            return false;
        px = static_cast<u32*>(bits);
        SelectObject(dc, bmp);
        SetBkMode(dc, TRANSPARENT);
        return true;
    }
    void Clear() { memset(px, 0, static_cast<size_t>(w) * h * 4); }
    ~Canvas()
    {
        if (dc)
            DeleteDC(dc);
        if (bmp)
            DeleteObject(bmp);
    }
};

// One drawing pass draws the glyph twice: colours on the colour canvas, the coverage (grey) on the mask canvas.
struct Painter
{
    HDC dc;
    bool mask;
    int S; // the glyph cell in supersampled pixels
    COLORREF C(COLORREF c, u8 cov = 255) const { return mask ? RGB(cov, cov, cov) : c; }
    void Fill(COLORREF c, int shape, int x0, int y0, int x1, int y1, int r = 0, u8 cov = 255)
    {
        HBRUSH b = CreateSolidBrush(C(c, cov));
        HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
        if (shape == 0)
            Ellipse(dc, x0, y0, x1 + 1, y1 + 1);
        else
            RoundRect(dc, x0, y0, x1 + 1, y1 + 1, r, r);
        SelectObject(dc, ob), SelectObject(dc, op);
        DeleteObject(b);
    }
    void Ring(COLORREF c, int x0, int y0, int x1, int y1, int width)
    {
        HPEN p = CreatePen(PS_SOLID, width, C(c));
        HGDIOBJ op = SelectObject(dc, p), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, x0, y0, x1 + 1, y1 + 1);
        SelectObject(dc, op), SelectObject(dc, ob);
        DeleteObject(p);
    }
    void RoundFrame(COLORREF c, int x0, int y0, int x1, int y1, int r, int width)
    {
        HPEN p = CreatePen(PS_SOLID, width, C(c));
        HGDIOBJ op = SelectObject(dc, p), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, x0, y0, x1 + 1, y1 + 1, r, r);
        SelectObject(dc, op), SelectObject(dc, ob);
        DeleteObject(p);
    }
    void Line(COLORREF c, int x0, int y0, int x1, int y1, int width)
    {
        HPEN p = CreatePen(PS_SOLID, width, C(c));
        HGDIOBJ op = SelectObject(dc, p);
        MoveToEx(dc, x0, y0, nullptr);
        LineTo(dc, x1, y1);
        SelectObject(dc, op);
        DeleteObject(p);
    }
    // penWidth > 0: the outline only; 0: filled (at coverage cov)
    void Poly(COLORREF c, const POINT* pts, int n, int penWidth, u8 cov = 255)
    {
        HPEN p = CreatePen(PS_SOLID, penWidth, C(c));
        HBRUSH b = CreateSolidBrush(C(c, cov));
        HGDIOBJ op = SelectObject(dc, penWidth > 0 ? p : GetStockObject(NULL_PEN)),
                ob = SelectObject(dc, penWidth > 0 ? GetStockObject(NULL_BRUSH) : b);
        Polygon(dc, pts, n);
        SelectObject(dc, op), SelectObject(dc, ob);
        DeleteObject(p), DeleteObject(b);
    }
    void Label(COLORREF c, const wchar_t* text, int cx, int cy, float em, const wchar_t* face, int weight = FW_BOLD)
    {
        HFONT f = CreateFontW(-static_cast<int>(em * S), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, face);
        HGDIOBJ of = SelectObject(dc, f);
        SetTextColor(dc, C(c));
        RECT r = {cx - S, cy - S, cx + S, cy + S};
        DrawTextW(dc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
        SelectObject(dc, of);
        DeleteObject(f);
    }
};

// The HUD's palette: the panel base (the body), white (the tint layer, coloured at draw time), the Nintendo letters.
static COLORREF Ref(u32 rgb) { return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF); }
static const COLORREF kBase = RGB(13, 11, 26), kWhite = RGB(255, 255, 255), kLight = Ref(kPressedArm);

enum Layer
{
    Fixed = 0,
    Tint = 1
};

// glyph_look's HUD 2 glyphs on the classic surfaces: hud2kit::Glyph's style in the atlas's square cell - the body in
// the panel base, everything else in the TINT layer (ring, the family's letter / shape, the pill's name, the D-pad's
// outline and pressed arm) with HUD 2's proportions (ring 0.075 of the height, letters in the HUD's face at 0.56, the
// PlayStation shapes' 0.07 stroke, bumpers and triggers one chamfered pill, sticks one ring). View / Menu: the common
// painter with the HUD 2 ring (false).
static bool PaintHud2(Painter& p, Family f, Id id, Layer layer, int ox, int oy)
{
    const int S = p.S;
    auto Px = [&](float u) { return ox + static_cast<int>(u * S + 0.5f); };
    auto Py = [&](float v) { return oy + static_cast<int>(v * S + 0.5f); };
    const int rim = std::max(2, static_cast<int>(S * 0.075f));
    const bool fixed = layer == Fixed;
    const wchar_t* hud = gamefont::LabelFace();
    const int cx = Px(0.5f), cy = Py(0.5f);
    if (id >= A && id <= Y)
    {
        if (fixed)
        {
            p.Fill(kBase, 0, Px(0.04f), Py(0.04f), Px(0.96f), Py(0.96f), 0, kBodyCov);
            return true;
        }
        p.Ring(kWhite, Px(0.04f) + rim / 2, Py(0.04f) + rim / 2, Px(0.96f) - rim / 2, Py(0.96f) - rim / 2, rim);
        if (f == Family::PlayStation)
        {
            const int s = static_cast<int>(S * 0.193f), w2 = std::max(2, static_cast<int>(S * 0.07f));
            if (id == A)
                p.Line(kWhite, cx - s, cy - s, cx + s, cy + s, w2), p.Line(kWhite, cx - s, cy + s, cx + s, cy - s, w2);
            else if (id == B)
                p.Ring(kWhite, cx - s, cy - s, cx + s, cy + s, w2);
            else if (id == X)
            {
                const POINT q[4] = {{cx - s, cy - s}, {cx + s, cy - s}, {cx + s, cy + s}, {cx - s, cy + s}};
                p.Poly(kWhite, q, 4, w2);
            }
            else
            {
                const POINT t[3] = {{cx, cy - s * 11 / 10}, {cx + s * 21 / 20, cy + s * 3 / 4}, {cx - s * 21 / 20, cy + s * 3 / 4}};
                p.Poly(kWhite, t, 3, w2);
            }
        }
        else
        {
            static const wchar_t* kXbox[4] = {L"A", L"B", L"X", L"Y"};
            static const wchar_t* kNin[4] = {L"B", L"A", L"Y", L"X"};
            p.Label(kWhite, (f == Family::Nintendo ? kNin : kXbox)[id], cx, cy, 0.56f, hud);
        }
        return true;
    }
    if (id >= LB && id <= RT)
    {
        // HUD 2's chamfered pill (1.3 x 0.78 of the height there), fitted to the square cell's width
        const float w = 0.96f, h = w * 0.78f / 1.3f, c = w * 0.2f / 1.3f;
        const float x0 = 0.5f - w / 2, x1 = 0.5f + w / 2, y0 = 0.5f - h / 2, y1 = 0.5f + h / 2;
        const POINT q[8] = {{Px(x0 + c), Py(y0)}, {Px(x1 - c), Py(y0)}, {Px(x1), Py(y0 + c)}, {Px(x1), Py(y1 - c)},
                            {Px(x1 - c), Py(y1)}, {Px(x0 + c), Py(y1)}, {Px(x0), Py(y1 - c)}, {Px(x0), Py(y0 + c)}};
        if (fixed)
            p.Poly(kBase, q, 8, 0, kBodyCov);
        else
        {
            p.Poly(kWhite, q, 8, rim);
            const bool ps = f == Family::PlayStation, nin = f == Family::Nintendo;
            static const wchar_t* kT[3][4] = {{L"LB", L"RB", L"LT", L"RT"}, {L"L1", L"R1", L"L2", L"R2"}, {L"L", L"R", L"ZL", L"ZR"}};
            p.Label(kWhite, kT[ps ? 1 : nin ? 2 : 0][id - LB], cx, cy, 0.42f, hud);
        }
        return true;
    }
    if (id >= LS && id <= R3)
    {
        if (fixed)
            p.Fill(kBase, 0, Px(0.04f), Py(0.04f), Px(0.96f), Py(0.96f), 0, kBodyCov);
        else
        {
            p.Ring(kWhite, Px(0.04f) + rim / 2, Py(0.04f) + rim / 2, Px(0.96f) - rim / 2, Py(0.96f) - rim / 2, rim);
            p.Label(kWhite, (id == LS || id == L3) ? L"L" : L"R", cx, cy, 0.52f, hud);
        }
        return true;
    }
    if (id >= DU && id <= DR)
    {
        const float t = 0.17f, e = 0.46f;
        const POINT cross[12] = {{Px(0.5f - t), Py(0.5f - e)}, {Px(0.5f + t), Py(0.5f - e)}, {Px(0.5f + t), Py(0.5f - t)},
                                 {Px(0.5f + e), Py(0.5f - t)}, {Px(0.5f + e), Py(0.5f + t)}, {Px(0.5f + t), Py(0.5f + t)},
                                 {Px(0.5f + t), Py(0.5f + e)}, {Px(0.5f - t), Py(0.5f + e)}, {Px(0.5f - t), Py(0.5f + t)},
                                 {Px(0.5f - e), Py(0.5f + t)}, {Px(0.5f - e), Py(0.5f - t)}, {Px(0.5f - t), Py(0.5f - t)}};
        if (fixed)
            p.Poly(kBase, cross, 12, 0, kBodyCov);
        else
        {
            p.Poly(kWhite, cross, 12, rim);
            // the lit arm in the tint (hud2kit::Glyph's rectangle)
            const float dx = id == DL ? -1.0f : id == DR ? 1.0f : 0.0f, dy = id == DU ? -1.0f : id == DD ? 1.0f : 0.0f;
            const float ax = 0.5f + dx * (e + t) * 0.5f, ay = 0.5f + dy * (e + t) * 0.5f, lw = rim / static_cast<float>(S);
            const float hw = dx != 0 ? (e - t) * 0.5f - lw : t - lw, hh = dy != 0 ? (e - t) * 0.5f - lw : t - lw;
            p.Fill(kWhite, 1, Px(ax - hw), Py(ay - hh), Px(ax + hw), Py(ay + hh));
        }
        return true;
    }
    return false;
}

// Draws layer `layer` of glyph `id` of family `f` into the cell at (ox, oy) of size S (supersampled pixels); `hud2` =
// glyph_look's HUD 2 style (PaintHud2) instead of the coloured one.
static void Paint(Painter& p, Family f, Id id, Layer layer, int ox, int oy, bool hud2)
{
    if (hud2 && PaintHud2(p, f, id, layer, ox, oy))
        return;
    const int S = p.S;
    auto Px = [&](float u) { return ox + static_cast<int>(u * S + 0.5f); };
    auto Py = [&](float v) { return oy + static_cast<int>(v * S + 0.5f); };
    const int rim = hud2 ? std::max(2, static_cast<int>(S * 0.075f)) // HUD 2's ring
                         : std::max(2, S / 18); // the outline (the HUD's frame line: ~3 px at 70 px)
    const int stroke = std::max(2, S / 12);  // the PlayStation shapes
    const int mark = std::max(2, S / 16);    // the View / Menu marks, the press chevrons
    const bool fixed = layer == Fixed;
    const wchar_t* hud = gamefont::LabelFace(); // Agency FB when the game's fonts load
    switch (id)
    {
    case A:
    case B:
    case X:
    case Y:
    {
        if (fixed)
        {
            p.Fill(kBase, 0, Px(0.04f), Py(0.04f), Px(0.96f), Py(0.96f), 0, kBodyCov);
            const int cx = Px(0.5f), cy = Py(0.5f);
            const COLORREF fc = Ref(FaceRgb(f, id)); // the family colour (one table with hud2kit::Glyph's coloured look)
            if (f != Family::PlayStation)
            {
                static const wchar_t* kXbox[4] = {L"A", L"B", L"X", L"Y"};
                static const wchar_t* kNin[4] = {L"B", L"A", L"Y", L"X"};
                p.Label(fc, (f == Family::Nintendo ? kNin : kXbox)[id], cx, cy, 0.64f, L"Arial");
            }
            else
            {
                const int r = static_cast<int>(S * 0.22f);
                if (id == A) // cross
                {
                    p.Line(fc, cx - r, cy - r, cx + r, cy + r, stroke);
                    p.Line(fc, cx - r, cy + r, cx + r, cy - r, stroke);
                }
                else if (id == B) // circle
                    p.Ring(fc, cx - r, cy - r, cx + r, cy + r, stroke);
                else if (id == X) // square
                {
                    const POINT q[4] = {{cx - r, cy - r}, {cx + r, cy - r}, {cx + r, cy + r}, {cx - r, cy + r}};
                    p.Poly(fc, q, 4, stroke);
                }
                else // triangle
                {
                    const POINT t[3] = {{cx, cy - r - stroke / 2}, {cx + r + stroke / 3, cy + r - stroke / 2},
                                        {cx - r - stroke / 3, cy + r - stroke / 2}};
                    p.Poly(fc, t, 3, stroke);
                }
            }
        }
        else
            p.Ring(kWhite, Px(0.04f), Py(0.04f), Px(0.96f), Py(0.96f), rim);
        break;
    }
    case LB:
    case RB:
    {
        // a pill the cell's width (the bumper's shape), its name in the HUD's face
        const int r = Py(0.82f) - Py(0.18f);
        if (fixed)
            p.Fill(kBase, 1, Px(0.02f), Py(0.18f), Px(0.98f), Py(0.82f), r, kBodyCov);
        else
        {
            p.RoundFrame(kWhite, Px(0.02f), Py(0.18f), Px(0.98f), Py(0.82f), r, rim);
            const wchar_t* t = f == Family::PlayStation ? (id == LB ? L"L1" : L"R1")
                               : f == Family::Nintendo  ? (id == LB ? L"L" : L"R")
                                                        : (id == LB ? L"LB" : L"RB");
            p.Label(kWhite, t, Px(0.5f), Py(0.5f), 0.5f, hud);
        }
        break;
    }
    case LT:
    case RT:
    {
        // the trigger: a tall rounded body
        const int r = S / 3;
        if (fixed)
            p.Fill(kBase, 1, Px(0.18f), Py(0.04f), Px(0.82f), Py(0.96f), r, kBodyCov);
        else
        {
            p.RoundFrame(kWhite, Px(0.18f), Py(0.04f), Px(0.82f), Py(0.96f), r, rim);
            const wchar_t* t = f == Family::PlayStation ? (id == LT ? L"L2" : L"R2")
                               : f == Family::Nintendo  ? (id == LT ? L"ZL" : L"ZR")
                                                        : (id == LT ? L"LT" : L"RT");
            p.Label(kWhite, t, Px(0.5f), Py(0.5f), 0.46f, hud);
        }
        break;
    }
    case LS:
    case RS:
    case L3:
    case R3:
    {
        // the stick: the base ring and the cap with its side letter; a press (L3 / R3): the cap pushed down under
        // two chevrons
        const bool press = id == L3 || id == R3;
        const float dy = press ? 0.07f : 0.0f;
        if (fixed)
            p.Fill(kBase, 0, Px(0.19f), Py(0.19f + dy), Px(0.81f), Py(0.81f + dy), 0, kBodyCov);
        else
        {
            p.Ring(kWhite, Px(0.05f), Py(0.05f), Px(0.95f), Py(0.95f), std::max(1, rim / 2));
            p.Ring(kWhite, Px(0.19f), Py(0.19f + dy), Px(0.81f), Py(0.81f + dy), rim);
            p.Label(kWhite, (id == LS || id == L3) ? L"L" : L"R", Px(0.5f), Py(0.5f + dy), 0.46f, hud);
            if (press)
            {
                p.Line(kWhite, Px(0.36f), Py(0.04f), Px(0.5f), Py(0.15f), mark);
                p.Line(kWhite, Px(0.5f), Py(0.15f), Px(0.64f), Py(0.04f), mark);
            }
        }
        break;
    }
    case DU:
    case DD:
    case DL:
    case DR:
    {
        // one D-pad: the cross (the body in the base, the outline in chrome), the pressed arm filled bright
        const float a = 0.17f; // half the arm width
        const POINT cross[12] = {{Px(0.5f - a), Py(0.04f)},     {Px(0.5f + a), Py(0.04f)},     {Px(0.5f + a), Py(0.5f - a)},
                                 {Px(0.96f), Py(0.5f - a)},     {Px(0.96f), Py(0.5f + a)},     {Px(0.5f + a), Py(0.5f + a)},
                                 {Px(0.5f + a), Py(0.96f)},     {Px(0.5f - a), Py(0.96f)},     {Px(0.5f - a), Py(0.5f + a)},
                                 {Px(0.04f), Py(0.5f + a)},     {Px(0.04f), Py(0.5f - a)},     {Px(0.5f - a), Py(0.5f - a)}};
        if (fixed)
        {
            p.Poly(kBase, cross, 12, 0, kBodyCov);
            const int in = rim; // the pressed arm, white (D2R's highlight), inside the outline up to the centre
            if (id == DU)
                p.Fill(kLight, 1, Px(0.5f - a) + in, Py(0.04f) + in, Px(0.5f + a) - in, Py(0.5f - a * 0.3f));
            else if (id == DD)
                p.Fill(kLight, 1, Px(0.5f - a) + in, Py(0.5f + a * 0.3f), Px(0.5f + a) - in, Py(0.96f) - in);
            else if (id == DL)
                p.Fill(kLight, 1, Px(0.04f) + in, Py(0.5f - a) + in, Px(0.5f - a * 0.3f), Py(0.5f + a) - in);
            else
                p.Fill(kLight, 1, Px(0.5f + a * 0.3f), Py(0.5f - a) + in, Px(0.96f) - in, Py(0.5f + a) - in);
        }
        else
            p.Poly(kWhite, cross, 12, rim);
        break;
    }
    case VIEW:
    case MENU:
    {
        if (fixed)
        {
            p.Fill(kBase, 0, Px(0.04f), Py(0.04f), Px(0.96f), Py(0.96f), 0, kBodyCov);
            break;
        }
        p.Ring(kWhite, Px(0.04f), Py(0.04f), Px(0.96f), Py(0.96f), rim);
        const int cx = Px(0.5f), cy = Py(0.5f);
        if (f == Family::Nintendo)
            p.Label(kWhite, id == VIEW ? L"\u2212" : L"+", cx, cy, 0.7f, L"Arial", FW_NORMAL);
        else if (id == MENU) // three lines (Xbox Menu, PlayStation Options)
        {
            for (int i = -1; i <= 1; ++i)
                p.Line(kWhite, Px(0.3f), cy + i * S / 6, Px(0.7f), cy + i * S / 6, mark);
        }
        else if (f == Family::PlayStation) // Create / Share: a small screen with its corner mark
        {
            const POINT q[4] = {{Px(0.3f), Py(0.34f)}, {Px(0.7f), Py(0.34f)}, {Px(0.7f), Py(0.66f)}, {Px(0.3f), Py(0.66f)}};
            p.Poly(kWhite, q, 4, mark);
            p.Line(kWhite, Px(0.5f), Py(0.44f), Px(0.5f), Py(0.6f), mark);
        }
        else // Xbox View: two overlapping windows
        {
            const POINT a[4] = {{Px(0.28f), Py(0.4f)}, {Px(0.58f), Py(0.4f)}, {Px(0.58f), Py(0.68f)}, {Px(0.28f), Py(0.68f)}};
            const POINT b[4] = {{Px(0.42f), Py(0.3f)}, {Px(0.72f), Py(0.3f)}, {Px(0.72f), Py(0.58f)}, {Px(0.42f), Py(0.58f)}};
            p.Poly(kWhite, a, 4, mark);
            p.Poly(kWhite, b, 4, mark);
        }
        break;
    }
    default: break;
    }
}

// ---------------------------------------------------------------- atlases
constexpr int kCells = COUNT * 2; // the fixed layer's cells, then the tint layer's
struct Atlas
{
    render::Image img;
    int cell = 0;           // glyph size in pixels
    int cols = 6, rows = 6; // 2 x 18 cells
    float u[kCells], v[kCells], du = 0, dv = 0;
    void* device = nullptr;
};
static std::map<u32, Atlas> g_atlases; // key: hud2 look << 24 | family << 16 | cell
static u32 g_builds = 0;

static u32 PowerOfTwo(u32 v)
{
    u32 p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

static Atlas* Get(Family f, int cell, void* device, bool hud2)
{
    cell = std::max(8, (cell + 1) & ~1); // even sizes: fewer atlases, the same look
    const u32 key = (hud2 ? 1u << 24 : 0u) | (static_cast<u32>(f) << 16) | static_cast<u32>(cell);
    auto it = g_atlases.find(key);
    if (it != g_atlases.end() && it->second.img.texture && it->second.device == device)
        return &it->second;
    Atlas a;
    a.cell = cell;
    a.device = device;
    const int pad = 2;
    const int cw = cell + pad;
    const int W = static_cast<int>(PowerOfTwo(a.cols * cw)), H = static_cast<int>(PowerOfTwo(a.rows * cw));
    const int S = cell * kSS;
    Canvas colour, mask;
    if (!colour.Make(a.cols * cw * kSS, a.rows * cw * kSS) || !mask.Make(colour.w, colour.h))
        return nullptr;
    colour.Clear(), mask.Clear();
    Painter pc{colour.dc, false, S}, pm{mask.dc, true, S};
    for (int i = 0; i < kCells; ++i)
    {
        const int ox = (i % a.cols) * cw * kSS + pad * kSS / 2, oy = (i / a.cols) * cw * kSS + pad * kSS / 2;
        const Layer layer = i < COUNT ? Fixed : Tint;
        const Id id = static_cast<Id>(i % COUNT);
        Paint(pc, f, id, layer, ox, oy, hud2);
        Paint(pm, f, id, layer, ox, oy, hud2);
    }
    // downsample: alpha = coverage, colour = the covered pixels' mean
    std::vector<u32> out(static_cast<size_t>(W) * H, 0);
    for (int y = 0; y < a.rows * cw; ++y)
        for (int x = 0; x < a.cols * cw; ++x)
        {
            u32 cov = 0, r = 0, g = 0, b = 0;
            for (int sy = 0; sy < kSS; ++sy)
                for (int sx = 0; sx < kSS; ++sx)
                {
                    const size_t i = static_cast<size_t>(y * kSS + sy) * colour.w + (x * kSS + sx);
                    const u32 m = mask.px[i] & 0xFF; // grey on black: the coverage
                    if (!m)
                        continue;
                    cov += m;
                    const u32 c = colour.px[i];
                    r += ((c >> 16) & 0xFF) * m, g += ((c >> 8) & 0xFF) * m, b += (c & 0xFF) * m;
                }
            if (!cov)
                continue;
            const u32 alpha = cov / (kSS * kSS);
            out[static_cast<size_t>(y) * W + x] = (alpha << 24) | ((r / cov) << 16) | ((g / cov) << 8) | (b / cov);
        }
    if (!render::BuildImage(a.img, out, W, H))
        return nullptr;
    for (int i = 0; i < kCells; ++i)
    {
        a.u[i] = static_cast<float>((i % a.cols) * cw + pad / 2) / W;
        a.v[i] = static_cast<float>((i / a.cols) * cw + pad / 2) / H;
    }
    a.du = static_cast<float>(cell) / W;
    a.dv = static_cast<float>(cell) / H;
    ++g_builds;
    render::FreeImage(g_atlases[key].img);
    g_atlases[key] = a;
    return &g_atlases[key];
}

static void* g_device = nullptr;
void SetDevice(void* device) { g_device = device; }

float Width(Id, float h) { return h; } // square cells (the wide shapes sit inside their cell)

bool Draw(Id id, float cx, float cy, float h, float alpha, u32 tintRgb)
{
    if (id < 0 || id >= COUNT || h < 4)
        return false;
    Atlas* a = Get(g_current, static_cast<int>(h + 0.5f), g_device, LookFor(false) == Look::Hud2);
    if (!a)
        return false;
    const float x0 = cx - h * 0.5f, y0 = cy - h * 0.5f, x1 = x0 + h, y1 = y0 + h;
    const float xy[8] = {x0, y0, x1, y0, x1, y1, x0, y1};
    const u32 al = static_cast<u32>(Clampf(alpha, 0, 1) * 255 + 0.5f) << 24;
    for (int layer = 0; layer < 2; ++layer)
    {
        const int cell = id + layer * COUNT;
        const float uv[8] = {a->u[cell],         a->v[cell],         a->u[cell] + a->du, a->v[cell],
                             a->u[cell] + a->du, a->v[cell] + a->dv, a->u[cell],         a->v[cell] + a->dv};
        const u32 c = al | (layer ? (tintRgb & 0xFFFFFF) : 0xFFFFFF);
        const u32 col[4] = {c, c, c, c};
        render::Quad(a->img, xy, uv, col, false);
    }
    return true;
}

float Text(const render::Font& font, float x, float yTop, const std::wstring& s, u32 argb, bool measure)
{
    const float gh = font.lineHeight > 0 ? font.lineHeight * 1.0f : font.px * 1.15f;
    const float alpha = (argb >> 24) / 255.0f;
    float pen = x;
    size_t i = 0;
    while (i < s.size())
    {
        const size_t open = s.find(L'{', i);
        const size_t close = open == std::wstring::npos ? open : s.find(L'}', open);
        const std::wstring plain = s.substr(i, (open == std::wstring::npos ? s.size() : open) - i);
        if (!plain.empty())
        {
            if (!measure)
                render::Text(pen, yTop, plain, argb);
            pen += static_cast<float>(render::TextWidth(font, plain));
        }
        if (open == std::wstring::npos || close == std::wstring::npos)
            break;
        char name[8] = {};
        for (size_t k = open + 1, n = 0; k < close && n < 7; ++k)
            name[n++] = static_cast<char>(s[k]);
        const Id id = FromName(name);
        if (id != COUNT)
        {
            if (!measure)
                Draw(id, pen + gh * 0.5f, yTop + gh * 0.5f, gh, alpha);
            pen += Width(id, gh) + gh * 0.15f;
        }
        i = close + 1;
    }
    return pen - x;
}

void Shutdown()
{
    for (auto& a : g_atlases)
        render::FreeImage(a.second.img);
    g_atlases.clear();
}

std::string Status()
{
    std::ostringstream o;
    o << "glyphs family=" << FamilyName(g_current) << " detected=" << FamilyName(g_detected) << " style="
      << uiopt::Get().glyphStyle << " look=" << uiopt::Get().glyphLook << " vendors=0x" << std::hex << g_vendorsSeen << std::dec
      << " atlases=" << g_atlases.size() << " builds=" << g_builds;
    return o.str();
}
} // namespace glyphs
