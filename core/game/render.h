// Minimal Direct3D 8 text and box renderer for the overlay (called from Present).
// Glyphs are rasterized with GDI at the exact pixel size into one atlas texture (grayscale
// antialiasing, coverage stored as alpha), so text stays sharp at any resolution or DPI.
// The font types, the rasteriser and the glyph lookup need no device: fontraster.h (included here).
#pragma once
#include "common.h"
#include "fontraster.h"

#include <string>
#include <vector>

namespace render
{
// A texture for Sprite (managed pool: survives Reset; tied to the device it was built on).
struct Image
{
    void* texture = nullptr; // IDirect3DTexture8
    void* device = nullptr;
    int w = 0, h = 0;
};

void SetDevice(void* device);
// Builds `img` from top-down 0xAARRGGBB pixels on the current device; false without a device.
bool BuildImage(Image& img, const std::vector<u32>& argb, int w, int h);
// Writes w x h top-down 0xAARRGGBB pixels into `img` at (x, y) (a managed texture's rectangle locked: no read-back); false
// without the device it was built on or outside the image.
bool UpdateImage(Image& img, int x, int y, int w, int h, const u32* argb);
void FreeImage(Image& img);
// `img` stands on the current device (UpdateImage's check): a cache of cells inside it is still valid.
bool ImageOnDevice(const Image& img);
void OnDeviceLost();
void OnDeviceReset();
// The device Resets this logic has seen (OnDeviceLost calls): whoever holds something for one device generation compares it
// (modules\hud\src\modelslot.cpp frees its model holders on a change).
u32 DeviceResets();

// (Re)builds `font` for the given characters (RasterizeFont, then the atlas uploaded as a managed texture); false if
// the device is missing or the font failed.
bool BuildFont(Font& font, const wchar_t* face, int px, int weight, const std::wstring& chars);
void FreeFont(Font& font);
// Research (debug `hud2 fontdump <file>`): every font built so far that holds an atlas - face, size, metrics, each
// glyph's box, advance and UV, hashes of the coverage copy and of the uploaded texture - sorted, to compare two builds.
// The file holds the font's metrics: write it under a git-ignored folder only. Returns a one-line summary.
std::string FontDump(const std::string& path);
// Research (the design bench's recorder, `hud2 bench record`): the font BuildFont filled whose atlas is `texture`
// (nullptr: none), and its fingerprint block as FontDump writes it (the uploaded texture read back for its hash).
const Font* FontOfTexture(const void* texture);
std::string FontPrintOf(const Font& font);

// Back buffer size in pixels (0 if unknown).
bool ScreenSize(int& w, int& h);
// The device's current viewport (D3DVIEWPORT8 X, Y, Width, Height): what the game is rendering into at this moment -
// in a letterboxed scene the picture rectangle between the bars, if the game set it so. False without a device.
bool Viewport(int& x, int& y, int& w, int& h);
// A region of the back buffer as 0xAARRGGBB pixels (call in Present, before drawing overlays); false if unsupported.
bool GrabRegion(int x, int y, int w, int h, std::vector<u32>& out);
// The readback is safe on this device (the system's Direct3D 8; not under a D3D9 wrapper, where it breaks every later
// Reset - render.cpp): player features check it first; research captures may grab anyway.
bool ReadbackSafe();

// One overlay pass: Begin, any number of Box/Text calls, End (draws everything in one batch).
bool Begin(const Font& font);
// A pass without text (Box / Gradient / Sprite only).
bool BeginPlain();
// Software target: premultiplied BGRA pixels (w x h), e.g. for a layered window over a movie.
bool BeginSoftware(const Font& font, u32* pixels, int w, int h);
// Switches the font for the following Text / Box calls (a new draw batch on the Direct3D target).
bool SetFont(const Font& font);
void Box(float x, float y, float w, float h, u32 argb);
// A box with a colour per corner (top-left, top-right, bottom-right, bottom-left), interpolated: soft edges, vignettes.
void Gradient(float x, float y, float w, float h, u32 tl, u32 tr, u32 br, u32 bl);
void Text(float x, float y, const std::wstring& s, u32 argb);
// Text at `scale` times the current font's size from (x, y), unsnapped and linearly filtered (smooth size changes;
// scale <= 1 keeps it sharp - draw from a larger font). The software target draws it at the font's size.
void TextScaled(float x, float y, const std::wstring& s, u32 argb, float scale);
// A textured quad centred on (cx, cy), w x h pixels, rotated by `angle` radians, tinted by argb (alpha included);
// additive = the game's effect blending (SRCALPHA / ONE: it lights up), else ordinary alpha blending. Linear filtering.
// Direct3D target only.
void Sprite(const Image& img, float cx, float cy, float w, float h, float angle, u32 argb, bool additive);
// A textured quad with its four corners given (xy, uv: corner 0..3 in order around it) and a colour per corner.
void Quad(const Image& img, const float xy[8], const float uv[8], const u32 argb[4], bool additive);
// A zoom over a range of what was drawn (the subtitles' stack fit): ZoomBegin starts a new batch at the current vertex
// and returns its index; ZoomEnd(from, s, ox, oy) moves every vertex drawn since to o + (v - o) * s and draws those
// batches linearly filtered (Text drawn after it is point-filtered again, as after TextScaled). s >= 1 (or <= 0): nothing.
// Direct3D target only (the software target - movies - draws unzoomed). Not nested.
size_t ZoomBegin();
void ZoomEnd(size_t from, float s, float ox, float oy);
// ownScene: wrap the draw in BeginScene/EndScene (from Present); false inside the game's scene.
void End(bool ownScene);
} // namespace render
