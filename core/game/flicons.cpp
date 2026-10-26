#include "flicons.h"

#include "log.h"
#include "render.h"
#include "utf.h"

#include <fstream>
#include <sstream>

namespace flicons
{
// The cells by meaning (tools\uimock\hud2.py ICON; the sheets' grids: Montage64 4 x 4 of 256 px, Montage32 8 x 8 of
// 128 px, HUD_targetingelement in 256 px pieces).
struct Cell
{
    int sheet; // 0 Montage64, 1 Montage32, 2 HUD_targetingelement
    int x, y, size;
};
static const Cell kCells[Count] = {
    {0, 0 * 256, 0 * 256, 256}, // Cruise
    {0, 1 * 256, 0 * 256, 256}, // Map
    {0, 0 * 256, 1 * 256, 256}, // Free
    {0, 2 * 256, 1 * 256, 256}, // Goto
    {0, 0 * 256, 2 * 256, 256}, // Dock
    {0, 2 * 256, 2 * 256, 256}, // Formation
    {0, 0 * 256, 3 * 256, 256}, // Comms
    {0, 3 * 256, 3 * 256, 256}, // Player
    {1, 0 * 128, 6 * 128, 128}, // Fighter
    {1, 7 * 128, 4 * 128, 128}, // Enemy
    {1, 7 * 128, 6 * 128, 128}, // Station
    {1, 0 * 128, 7 * 128, 128}, // JumpGate
    {1, 5 * 128, 6 * 128, 128}, // Lane
    {1, 2 * 128, 6 * 128, 128}, // Waypoint
    {1, 7 * 128, 7 * 128, 128}, // Planet
    {1, 1 * 128, 7 * 128, 128}, // Transport
    {1, 4 * 128, 4 * 128, 128}, // Tractor
    {1, 7 * 128, 5 * 128, 128}, // Important
    {1, 0 * 128, 4 * 128, 128}, // Info
    {1, 5 * 128, 4 * 128, 128}, // Next
    {1, 4 * 128, 6 * 128, 128}, // Star
    {1, 4 * 128, 7 * 128, 128}, // Mine
    {1, 6 * 128, 6 * 128, 128}, // Group
    {1, 5 * 128, 7 * 128, 128}, // Loot
    {1, 2 * 128, 5 * 128, 128}, // List
    {2, 0, 768, 256},           // Bracket
    {2, 0, 256, 256},           // Arrow
    {2, 256, 256, 256},         // Lead
};
static const char* const kSheets[3] = {"montage64", "montage32", "hud_targetingelement"};

constexpr int kCols = 8, kRows = 4;      // the atlas grid (28 cells)
constexpr int kBig = 128, kSmall = 64;   // cell sizes of the two atlases
constexpr int kBigW = kCols * kBig, kBigH = kRows * kBig, kSmallW = kCols * kSmall, kSmallH = kRows * kSmall;

static volatile LONG g_state = 0; // 0 idle, 1 loading, 2 pixels ready, 3 failed, 4 built
static HANDLE g_thread = nullptr;
static std::vector<u32> g_bigPx, g_smallPx; // the worker's output (read by the game thread after state 2)
static std::string g_source, g_why;         // which file / why it failed (the worker writes before the state change)
static int g_sheetPx = 0;                   // the sheets' size read (1024 HDE, 256 vanilla)
static render::Image g_big, g_small;
static void* g_device = nullptr;

// ---------------------------------------------------------------- the worker: file -> pixels
static u32 U32(const std::vector<u8>& b, size_t o) { return o + 4 <= b.size() ? *reinterpret_cast<const u32*>(&b[o]) : 0; }

// The MIP0 leaves of the three sheets, by a walk of the UTF tree (only those copied: the file is 18 MB).
static bool FindSheets(const std::vector<u8>& b, std::vector<u8> out[3])
{
    if (b.size() < 56 || memcmp(b.data(), "UTF ", 4) != 0)
        return false;
    const u32 nodeOff = U32(b, 8), strOff = U32(b, 24), dataOff = U32(b, 36);
    auto name = [&](u32 o)
    {
        std::string s;
        for (size_t i = strOff + o; i < b.size() && b[i]; ++i)
            s += static_cast<char>(tolower(b[i]));
        return s;
    };
    struct Item
    {
        u32 rel;
        std::string parent; // the directory's name (the texture's)
    };
    std::vector<Item> stack = {{0, ""}};
    int guard = 0;
    while (!stack.empty() && ++guard < 200000)
    {
        const Item it = stack.back();
        stack.pop_back();
        for (u32 rel = it.rel; ++guard < 200000;)
        {
            const size_t n = nodeOff + rel;
            if (n + 28 > b.size())
                break;
            const std::string nm = name(U32(b, n + 4));
            if (U32(b, n + 8) & 0x10)
                stack.push_back({U32(b, n + 16), nm});
            else if (nm == "mip0")
                for (int s = 0; s < 3; ++s)
                    if (it.parent == kSheets[s])
                    {
                        const size_t data = dataOff + U32(b, n + 16), size = U32(b, n + 24);
                        if (data + size <= b.size())
                            out[s].assign(b.begin() + data, b.begin() + data + size);
                    }
            const u32 next = U32(b, n);
            if (!next)
                break;
            rel = next;
        }
    }
    return !out[0].empty() || !out[1].empty() || !out[2].empty();
}

static u32 Rgb565(u16 c)
{
    const u32 r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return ((r * 255 / 31) << 16) | ((g * 255 / 63) << 8) | (b * 255 / 31);
}

// DDS with DXT1 / DXT3 / DXT5 (or 32-bit uncompressed) to top-down 0xAARRGGBB.
static bool DecodeDds(const std::vector<u8>& d, int& w, int& h, std::vector<u32>& argb)
{
    if (d.size() < 128 || memcmp(d.data(), "DDS ", 4) != 0)
        return false;
    h = static_cast<int>(U32(d, 12)), w = static_cast<int>(U32(d, 16));
    const u32 pfFlags = U32(d, 80), fourcc = U32(d, 84), bits = U32(d, 88);
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192)
        return false;
    argb.assign(static_cast<size_t>(w) * h, 0);
    const u8* p = d.data() + 128;
    const u8* end = d.data() + d.size();
    if (!(pfFlags & 4) && bits == 32) // uncompressed BGRA
    {
        if (p + static_cast<size_t>(w) * h * 4 > end)
            return false;
        for (size_t i = 0; i < argb.size(); ++i, p += 4)
            argb[i] = (static_cast<u32>(p[3]) << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
        return true;
    }
    int kind = 0;
    if (fourcc == (0x00545844u | (static_cast<u32>('1') << 24)))
        kind = 1;
    else if (fourcc == (0x00545844u | (static_cast<u32>('3') << 24)))
        kind = 3;
    else if (fourcc == (0x00545844u | (static_cast<u32>('5') << 24)))
        kind = 5;
    if (!kind)
        return false;
    const int bw = (w + 3) / 4, bh = (h + 3) / 4, blockBytes = kind == 1 ? 8 : 16;
    if (p + static_cast<size_t>(bw) * bh * blockBytes > end)
        return false;
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx, p += blockBytes)
        {
            u8 alpha[16];
            const u8* c = p;
            if (kind == 3)
            {
                for (int i = 0; i < 16; ++i)
                    alpha[i] = static_cast<u8>(((p[i / 2] >> ((i & 1) * 4)) & 0xF) * 17);
                c = p + 8;
            }
            else if (kind == 5)
            {
                u8 a[8] = {p[0], p[1]};
                if (a[0] > a[1])
                    for (int i = 1; i < 7; ++i)
                        a[i + 1] = static_cast<u8>(((7 - i) * a[0] + i * a[1]) / 7);
                else
                {
                    for (int i = 1; i < 5; ++i)
                        a[i + 1] = static_cast<u8>(((5 - i) * a[0] + i * a[1]) / 5);
                    a[6] = 0, a[7] = 255;
                }
                unsigned long long idx = 0;
                for (int i = 0; i < 6; ++i)
                    idx |= static_cast<unsigned long long>(p[2 + i]) << (8 * i);
                for (int i = 0; i < 16; ++i)
                    alpha[i] = a[(idx >> (3 * i)) & 7];
                c = p + 8;
            }
            else
                memset(alpha, 255, sizeof(alpha));
            const u16 c0 = static_cast<u16>(c[0] | (c[1] << 8)), c1 = static_cast<u16>(c[2] | (c[3] << 8));
            u32 col[4] = {Rgb565(c0), Rgb565(c1), 0, 0};
            auto mix = [](u32 x, u32 y, int wx, int wy, int div)
            {
                u32 o = 0;
                for (int s = 0; s < 24; s += 8)
                    o |= ((((x >> s) & 0xFF) * wx + ((y >> s) & 0xFF) * wy) / div) << s;
                return o;
            };
            bool punch = false;
            if (c0 > c1 || kind != 1)
                col[2] = mix(col[0], col[1], 2, 1, 3), col[3] = mix(col[0], col[1], 1, 2, 3);
            else
                col[2] = mix(col[0], col[1], 1, 1, 2), col[3] = 0, punch = true;
            const u32 idx = c[4] | (c[5] << 8) | (c[6] << 16) | (static_cast<u32>(c[7]) << 24);
            for (int i = 0; i < 16; ++i)
            {
                const int x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                if (x >= w || y >= h)
                    continue;
                const u32 k = (idx >> (2 * i)) & 3;
                const u32 a = punch && k == 3 ? 0 : alpha[i];
                argb[static_cast<size_t>(y) * w + x] = (a << 24) | col[k];
            }
        }
    return true;
}

// One sheet cell resampled into an atlas cell: white, alpha = coverage; on the icon sheets a dark pixel counts as none
// (their thin dark outlines: the mock-up's luminance cut at 30), the targeting element is alpha only. The cell grid is
// in fractions of the sheet (`k` = the sheet's size / 1024: HDE's sheets are 1024, vanilla's 256 - the same grid, the
// same meanings, verified 25 Oct 2026): box-filtered down when the sheet's cell is larger than the atlas cell, sampled
// bilinearly up when smaller.
static u32 Coverage(u32 p, bool lumaCut)
{
    u32 a = p >> 24;
    if (lumaCut && (((p >> 16) & 0xFF) * 299 + ((p >> 8) & 0xFF) * 587 + (p & 0xFF) * 114) / 1000 <= 30)
        a = 0;
    return a;
}

static void Blit(const std::vector<u32>& src, int sw, int sh, const Cell& c, bool lumaCut, std::vector<u32>& dst,
                 int dw, int dx, int dy, int cell, float k)
{
    const int cx = static_cast<int>(c.x * k + 0.5f), cy = static_cast<int>(c.y * k + 0.5f),
              cs = static_cast<int>(c.size * k + 0.5f);
    if (cs < 2 || cx + cs > sw || cy + cs > sh)
        return;
    const int f = cs / cell;
    for (int y = 0; y < cell; ++y)
        for (int x = 0; x < cell; ++x)
        {
            u32 a = 0;
            if (f >= 1) // down: the mean over the f x f source pixels
            {
                u32 sum = 0;
                for (int yy = 0; yy < f; ++yy)
                    for (int xx = 0; xx < f; ++xx)
                        sum += Coverage(src[static_cast<size_t>(cy + y * f + yy) * sw + cx + x * f + xx], lumaCut);
                a = sum / (f * f);
            }
            else // up: bilinear between the four nearest source pixels (inside the cell)
            {
                const float fx = std::min(std::max((x + 0.5f) * cs / cell - 0.5f, 0.0f), cs - 1.001f);
                const float fy = std::min(std::max((y + 0.5f) * cs / cell - 0.5f, 0.0f), cs - 1.001f);
                const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
                const float tx = fx - x0, ty = fy - y0;
                auto at = [&](int xx, int yy)
                { return static_cast<float>(Coverage(src[static_cast<size_t>(cy + yy) * sw + cx + xx], lumaCut)); };
                const float v = (at(x0, y0) * (1 - tx) + at(x0 + 1, y0) * tx) * (1 - ty) +
                                (at(x0, y0 + 1) * (1 - tx) + at(x0 + 1, y0 + 1) * tx) * ty;
                a = static_cast<u32>(v + 0.5f);
            }
            dst[static_cast<size_t>(dy + y) * dw + dx + x] = a ? (a << 24) | 0xFFFFFF : 0;
        }
}

static bool Decode(const std::vector<u8>& d, int& w, int& h, std::vector<u32>& px)
{
    return utf::DecodeTga(d, w, h, px) || DecodeDds(d, w, h, px);
}

static bool LoadFile(const std::wstring& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    const std::vector<u8> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<u8> leaf[3];
    if (!FindSheets(b, leaf))
        return false;
    std::vector<u32> big(static_cast<size_t>(kBigW) * kBigH, 0), smallPx(static_cast<size_t>(kSmallW) * kSmallH, 0);
    int got = 0, sheetPx = 0;
    for (int s = 0; s < 3; ++s)
    {
        int w = 0, h = 0;
        std::vector<u32> px;
        if (leaf[s].empty() || !Decode(leaf[s], w, h, px))
            continue;
        // the grids are fractions of the (square) sheet: the cell boxes scale with it (HDE 1024, vanilla 256)
        if (w != h || w < 128 || (w & (w - 1)) != 0)
            continue;
        const float k = w / 1024.0f;
        for (int i = 0; i < Count; ++i)
            if (kCells[i].sheet == s)
            {
                const int col = i % kCols, row = i / kCols;
                Blit(px, w, h, kCells[i], s != 2, big, kBigW, col * kBig, row * kBig, kBig, k);
                Blit(px, w, h, kCells[i], s != 2, smallPx, kSmallW, col * kSmall, row * kSmall, kSmall, k);
                ++got;
            }
        sheetPx = std::max(sheetPx, w);
    }
    if (!got)
        return false;
    g_bigPx.swap(big);
    g_smallPx.swap(smallPx);
    g_sheetPx = sheetPx;
    return true;
}

static DWORD WINAPI Worker(LPVOID)
{
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe;
    const size_t slash = dir.find_last_of(L'\\');
    dir = slash == std::wstring::npos ? L"" : dir.substr(0, slash + 1); // the EXE folder
    const std::wstring data = dir + L"..\\DATA\\INTERFACE\\";
    for (const wchar_t* f : {L"buttontextures_flaticons.txm", L"buttontextures.txm"})
        if (LoadFile(data + f))
        {
            for (const wchar_t* p = f; *p; ++p)
                g_source += static_cast<char>(*p);
            InterlockedExchange(&g_state, 2);
            return 0;
        }
    g_why = "no flat icon sheet (DATA\\INTERFACE\\buttontextures_flaticons.txm / buttontextures.txm) could be read";
    InterlockedExchange(&g_state, 3);
    return 0;
}

bool Ready(void* device)
{
    const LONG st = g_state;
    if (st == 4)
    {
        if (device == g_device && g_big.texture)
            return true;
        // a new device: rebuild from the kept pixels
        render::FreeImage(g_big), render::FreeImage(g_small);
        InterlockedExchange(&g_state, 2);
        return false;
    }
    if (st == 0)
    {
        InterlockedExchange(&g_state, 1);
        g_thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (!g_thread)
        {
            g_why = "worker thread not started";
            InterlockedExchange(&g_state, 3);
        }
        return false;
    }
    if (st == 2 && device)
    {
        if (render::BuildImage(g_big, g_bigPx, kBigW, kBigH) && render::BuildImage(g_small, g_smallPx, kSmallW, kSmallH))
        {
            g_device = device;
            InterlockedExchange(&g_state, 4);
            Log("flicons: atlas built from %s (%d icons, %d px sheets)", g_source.c_str(), static_cast<int>(Count), g_sheetPx);
            return true;
        }
        g_why = "texture not created";
        InterlockedExchange(&g_state, 3);
        Log("flicons: %s", g_why.c_str());
    }
    if (st == 3)
    {
        static bool logged = false;
        if (!logged)
            logged = true, Log("flicons: off (%s): HUD 2 draws its own shapes", g_why.c_str());
    }
    return false;
}

bool Uv(Icon icon, float sizePx, void** texture, float uv[4])
{
    if (g_state != 4 || icon < 0 || icon >= Count)
        return false;
    const bool smallPx = sizePx <= kSmall * 1.1f;
    const render::Image& img = smallPx ? g_small : g_big;
    if (!img.texture)
        return false;
    const int cell = smallPx ? kSmall : kBig;
    const float atlasW = static_cast<float>(smallPx ? kSmallW : kBigW), atlasH = static_cast<float>(smallPx ? kSmallH : kBigH);
    const int col = icon % kCols, row = icon / kCols;
    // half a texel in: linear filtering never samples the neighbour cell
    uv[0] = (col * cell + 0.5f) / atlasW, uv[1] = (row * cell + 0.5f) / atlasH;
    uv[2] = ((col + 1) * cell - 0.5f) / atlasW, uv[3] = ((row + 1) * cell - 0.5f) / atlasH;
    *texture = img.texture;
    return true;
}

void Shutdown()
{
    if (g_thread)
    {
        WaitForSingleObject(g_thread, 10000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    render::FreeImage(g_big);
    render::FreeImage(g_small);
    g_device = nullptr;
    if (g_state == 4)
        InterlockedExchange(&g_state, 2);
}

std::string Status()
{
    static const char* kNames[] = {"idle", "loading", "decoded", "failed", "ready"};
    std::ostringstream o;
    const LONG st = g_state;
    o << "flicons " << (st >= 0 && st <= 4 ? kNames[st] : "?") << " source=" << (g_source.empty() ? "-" : g_source) << " sheets=" << g_sheetPx;
    if (st == 3)
        o << " why=" << g_why;
    return o.str();
}
} // namespace flicons
