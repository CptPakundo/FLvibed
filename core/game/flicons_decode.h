// FLvibed - the decoding half of core\game\flicons with no device and no thread: the game's flat icon file (a UTF tree
// holding the sheets Montage64, Montage32 and HUD_targetingelement as TGA or DDS) to the two atlases of white-on-alpha
// cells flicons uploads (flicons.h says which cells and why). flicons.cpp reads the file on its worker thread with
// ReadAtlas; a program with no game (the design bench, ..\..\..\audit_2026-11-12\71b_bench_design_v2.md) decodes the
// same file synchronously. Split out of flicons.cpp unchanged (bench B1a, 12 Nov 2026).
#pragma once
#include "common.h"
#include "flicons.h"

#include <string>
#include <vector>

namespace flicons
{
constexpr int kCols = 8, kRows = 4;    // the atlas grid (28 cells)
constexpr int kBig = 128, kSmall = 64; // cell sizes of the two atlases
constexpr int kBigW = kCols * kBig, kBigH = kRows * kBig, kSmallW = kCols * kSmall, kSmallH = kRows * kSmall;

struct Atlas
{
    std::vector<u32> bigPx, smallPx; // kBigW x kBigH and kSmallW x kSmallH, 0xAARRGGBB top-down; icon i at cell
                                     // (i % kCols, i / kCols)
    int sheetPx = 0;                 // the largest sheet decoded (1024 HDE, 256 vanilla)
};

// The flat icon files under DATA\INTERFACE, in the order they are tried (HDE's, then vanilla's).
constexpr const wchar_t* kSheetFiles[2] = {L"buttontextures_flaticons.txm", L"buttontextures.txm"};

// Whether a draw `sizePx` tall takes the small atlas; an icon's UV rectangle (u0, v0, u1, v1) in that atlas, half a
// texel in, so linear filtering never samples the neighbour cell (flicons::Uv's rule; the bench's too).
inline bool SmallFor(float sizePx) { return sizePx <= kSmall * 1.1f; }
inline void CellUv(Icon icon, bool smallAtlas, float uv[4]) // (not `small`: a macro of Windows' rpcndr.h)
{
    const int cell = smallAtlas ? kSmall : kBig;
    const float atlasW = static_cast<float>(smallAtlas ? kSmallW : kBigW),
                atlasH = static_cast<float>(smallAtlas ? kSmallH : kBigH);
    const int col = icon % kCols, row = icon / kCols;
    uv[0] = (col * cell + 0.5f) / atlasW, uv[1] = (row * cell + 0.5f) / atlasH;
    uv[2] = ((col + 1) * cell - 0.5f) / atlasW, uv[3] = ((row + 1) * cell - 0.5f) / atlasH;
}

// The atlases from the bytes of a flat icon .txm; false when no sheet was found or no cell could be filled.
bool DecodeAtlas(const std::vector<u8>& file, Atlas& out);
// The file read whole, then DecodeAtlas; false when it cannot be read either.
bool ReadAtlas(const std::wstring& path, Atlas& out);
} // namespace flicons
