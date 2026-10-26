#include "utf.h"

#include <fstream>

namespace utf
{
static u32 U32(const std::vector<u8>& b, size_t o)
{
    return o + 4 <= b.size() ? *reinterpret_cast<const u32*>(&b[o]) : 0;
}

std::vector<Leaf> Leaves(const std::wstring& file)
{
    std::vector<Leaf> out;
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return out;
    const std::vector<u8> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (b.size() < 56 || memcmp(b.data(), "UTF ", 4) != 0)
        return out;
    // Header: +8 node block offset, +24 string block offset, +36 data block offset. Node: +0 next sibling (relative),
    // +4 name (string offset), +8 flags (0x10 directory), +16 first child (directory) or data offset (leaf), +24 size.
    const u32 nodeOff = U32(b, 8), strOff = U32(b, 24), dataOff = U32(b, 36);
    auto name = [&](u32 o)
    {
        std::string s;
        for (size_t i = strOff + o; i < b.size() && b[i]; ++i)
            s += static_cast<char>(b[i]);
        return s;
    };
    struct Item
    {
        u32 rel;
        std::string prefix;
    };
    std::vector<Item> stack = {{0, ""}};
    int guard = 0;
    while (!stack.empty() && ++guard < 100000)
    {
        Item it = stack.back();
        stack.pop_back();
        for (u32 rel = it.rel;;)
        {
            const size_t n = nodeOff + rel;
            if (n + 28 > b.size())
                break;
            const std::string path = it.prefix + "\\" + name(U32(b, n + 4));
            if (U32(b, n + 8) & 0x10)
                stack.push_back({U32(b, n + 16), path});
            else
            {
                const size_t data = dataOff + U32(b, n + 16), size = U32(b, n + 24);
                if (data + size <= b.size())
                    out.push_back({path, std::vector<u8>(b.begin() + data, b.begin() + data + size)});
            }
            const u32 next = U32(b, n);
            if (!next || ++guard > 100000)
                break;
            rel = next;
        }
    }
    return out;
}

bool DecodeTga(const std::vector<u8>& d, int& w, int& h, std::vector<u32>& argb)
{
    if (d.size() < 18 || d[2] != 2 || d[1] != 0)
        return false;
    const int idLen = d[0], bpp = d[16];
    w = d[12] | (d[13] << 8);
    h = d[14] | (d[15] << 8);
    const bool topDown = (d[17] & 0x20) != 0;
    const int bytes = bpp / 8;
    if ((bytes != 3 && bytes != 4) || w <= 0 || h <= 0 || 18 + idLen + static_cast<size_t>(w) * h * bytes > d.size())
        return false;
    argb.resize(static_cast<size_t>(w) * h);
    const u8* p = d.data() + 18 + idLen;
    for (int y = 0; y < h; ++y)
    {
        const int dy = topDown ? y : h - 1 - y;
        for (int x = 0; x < w; ++x, p += bytes)
            argb[static_cast<size_t>(dy) * w + x] = (bytes == 4 ? static_cast<u32>(p[3]) << 24 : 0xFF000000u) |
                                                    (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[1]) << 8) |
                                                    p[0];
    }
    return true;
}
} // namespace utf
