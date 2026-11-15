// FLvibed design bench - the display list writer (fldl_write.h): the atlas, the probes and the fingerprints; the list
// itself is the kit's (core\ui\hud2_kit_fldl.cpp).
#include "fldl_write.h"

#include "game/fontraster.h"

#include <algorithm>
#include <map>
#include <sstream>

namespace fldl
{
bool WriteFile(const std::string& path, const std::string& bytes)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f)
        return false;
    const bool ok = fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    return fclose(f) == 0 && ok;
}

// ---------------------------------------------------------------- the atlas
static u32 Crc(const u8* p, size_t n, u32 c = 0xFFFFFFFFu)
{
    for (size_t i = 0; i < n; ++i)
    {
        c ^= p[i];
        for (int k = 0; k < 8; ++k)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return c;
}
static void Be32(std::string& s, u32 v)
{
    for (int k = 3; k >= 0; --k)
        s += static_cast<char>((v >> (8 * k)) & 0xFF);
}
static void Chunk(std::string& png, const char* type, const std::string& data)
{
    Be32(png, static_cast<u32>(data.size()));
    std::string body = std::string(type, 4) + data;
    png += body;
    Be32(png, ~Crc(reinterpret_cast<const u8*>(body.data()), body.size()));
}
// Deflate (RFC 1951) in one block with the fixed Huffman codes; the only matches tried are against the previous pixel
// (distance 4) and the row above (distance `stride`): an atlas is mostly runs of empty or solid texels, so that is
// enough (a 4K atlas from ~13 MB stored to well under 1 MB) with no library and a few lines.
struct Bits
{
    std::string out;
    u32 acc = 0;
    int n = 0;
    void Put(u32 v, int bits) // LSB first
    {
        acc |= v << n, n += bits;
        while (n >= 8)
            out += static_cast<char>(acc & 0xFF), acc >>= 8, n -= 8;
    }
    void Code(u32 code, int bits) // a Huffman code, MSB first
    {
        u32 r = 0;
        for (int i = 0; i < bits; ++i)
            r |= ((code >> i) & 1u) << (bits - 1 - i);
        Put(r, bits);
    }
    void Lit(int v)
    {
        if (v < 144)
            Code(0x30 + v, 8);
        else if (v < 256)
            Code(0x190 + v - 144, 9);
        else if (v < 280)
            Code(v - 256, 7);
        else
            Code(0xC0 + v - 280, 8);
    }
    void Flush()
    {
        if (n)
            out += static_cast<char>(acc & 0xFF), acc = 0, n = 0;
    }
};
static void Match(Bits& b, int len, int dist)
{
    static const int kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const int kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const int kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const int kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    int li = 28;
    while (kLenBase[li] > len)
        --li;
    b.Lit(257 + li);
    b.Put(static_cast<u32>(len - kLenBase[li]), kLenExtra[li]);
    int di = 29;
    while (kDistBase[di] > dist)
        --di;
    b.Code(static_cast<u32>(di), 5);
    b.Put(static_cast<u32>(dist - kDistBase[di]), kDistExtra[di]);
}
static std::string Deflate(const std::string& raw, int stride)
{
    Bits b;
    b.Put(1, 1), b.Put(1, 2); // the last block, fixed codes
    const u8* p = reinterpret_cast<const u8*>(raw.data());
    const size_t n = raw.size();
    for (size_t i = 0; i < n;)
    {
        int bestLen = 0, bestDist = 0;
        for (int d : {4, stride})
        {
            if (d <= 0 || d > 32768 || static_cast<size_t>(d) > i)
                continue;
            int len = 0;
            while (len < 258 && i + len < n && p[i + len] == p[i + len - d])
                ++len;
            if (len > bestLen)
                bestLen = len, bestDist = d;
        }
        if (bestLen >= 3)
            Match(b, bestLen, bestDist), i += bestLen;
        else
            b.Lit(p[i]), ++i;
    }
    b.Lit(256);
    b.Flush();
    return b.out;
}

// RGBA, 8 bits, no filter, zlib (Deflate above), an optional tEXt comment.
static std::string Png(int w, int h, const std::vector<u32>& argb, const std::string& comment)
{
    std::string raw;
    raw.reserve(static_cast<size_t>(h) * (1 + static_cast<size_t>(w) * 4));
    for (int y = 0; y < h; ++y)
    {
        raw += '\0';
        for (int x = 0; x < w; ++x)
        {
            const u32 c = argb[static_cast<size_t>(y) * w + x];
            raw += static_cast<char>((c >> 16) & 0xFF), raw += static_cast<char>((c >> 8) & 0xFF);
            raw += static_cast<char>(c & 0xFF), raw += static_cast<char>(c >> 24);
        }
    }
    std::string z = "\x78\x01";
    u32 a = 1, b = 0;
    for (unsigned char c : raw)
        a = (a + c) % 65521, b = (b + a) % 65521;
    z += Deflate(raw, 1 + w * 4);
    Be32(z, (b << 16) | a);
    std::string png = "\x89PNG\r\n\x1a\n";
    std::string ihdr;
    Be32(ihdr, static_cast<u32>(w)), Be32(ihdr, static_cast<u32>(h));
    ihdr += std::string("\x08\x06\x00\x00\x00", 5);
    Chunk(png, "IHDR", ihdr);
    if (!comment.empty())
        Chunk(png, "tEXt", std::string("Comment") + '\0' + comment);
    Chunk(png, "IDAT", z);
    Chunk(png, "IEND", "");
    return png;
}

bool WriteAtlas(const std::string& base, bool priv, std::string& error)
{
    struct Place
    {
        const kitlist::Texture* t;
        const kitlist::Box* b;
        int x, y;
    };
    std::vector<Place> places;
    int W = 2048;
    for (const auto& t : kitlist::Textures())
        for (const kitlist::Box& b : t->boxes)
            W = std::max(W, b.w + 4);
    int x = 2, y = 2, rowH = 0;
    for (const auto& t : kitlist::Textures())
        for (const kitlist::Box& b : t->boxes)
        {
            if (x + b.w + 2 > W)
                x = 2, y += rowH + 2, rowH = 0;
            places.push_back({t.get(), &b, x, y});
            x += b.w + 2;
            rowH = std::max(rowH, b.h);
        }
    const int H = y + rowH + 2;
    if (static_cast<long long>(W) * H > (1ll << 28))
        return error = "the atlas would pass 256 M texels", false;
    std::vector<u32> img(static_cast<size_t>(W) * H, 0);
    std::string entries;
    for (const Place& p : places)
    {
        for (int yy = 0; yy < p.b->h; ++yy)
            memcpy(&img[static_cast<size_t>(p.y + yy) * W + p.x], &p.t->argb[static_cast<size_t>(p.b->y + yy) * p.t->w + p.b->x],
                   static_cast<size_t>(p.b->w) * 4);
        entries += (entries.empty() ? "\n " : ",\n ") + Quote(p.b->name) + ": [" + std::to_string(p.x) + ", " +
                   std::to_string(p.y) + ", " + std::to_string(p.b->w) + ", " + std::to_string(p.b->h) + "]";
    }
    const std::string file = base.substr(base.find_last_of("\\/") + 1) + ".png";
    if (!WriteFile(base + ".png", Png(W, H, img, priv ? Marker() : "")))
        return error = "cannot write " + base + ".png", false;
    const std::string manifest = "{\"schema\": \"flatlas/0\", \"privacy\": " + Quote(priv ? Marker() : "public") +
                                 ", \"image\": " + Quote(file) + ", \"size\": [" + std::to_string(W) + ", " +
                                 std::to_string(H) + "],\n\"entries\": {" + entries + "}}\n";
    if (!WriteFile(base + ".json", manifest))
        return error = "cannot write " + base + ".json", false;
    return true;
}

std::string ProbesJson(const std::vector<Probe>& probes)
{
    std::string o = "[";
    for (size_t i = 0; i < probes.size(); ++i)
    {
        const Probe& p = probes[i];
        o += (i ? ",\n " : "\n ") + std::string("{\"name\": ") + Quote(p.name) + ", \"x\": " + std::to_string(p.x) +
             ", \"y\": " + std::to_string(p.y) + ", \"rgb\": [" + std::to_string((p.rgb >> 16) & 0xFF) + ", " +
             std::to_string((p.rgb >> 8) & 0xFF) + ", " + std::to_string(p.rgb & 0xFF) + "], \"tol\": " +
             std::to_string(p.tol) + (p.a >= 0 ? ", \"a\": " + std::to_string(p.a) : std::string()) + "}";
    }
    return o + "]\n";
}

// ---------------------------------------------------------------- fingerprints
std::string Prints()
{
    std::vector<std::string> blocks;
    for (const auto& t : kitlist::Textures())
        if (t->font)
            blocks.push_back(t->print);
    std::sort(blocks.begin(), blocks.end());
    std::string o;
    for (const std::string& b : blocks)
        o += b;
    return o;
}

// "font <face> px=<n> weight=<n>": a block's key
static std::string Key(const std::string& block)
{
    std::istringstream in(block);
    std::string a, b, c, d;
    in >> a >> b >> c >> d;
    return a + " " + b + " " + c + " " + d;
}

int ComparePrints(const std::string& gameDump, std::string& diff, std::string& missing, int& differing)
{
    std::multimap<std::string, std::string> game;
    std::istringstream in(gameDump);
    std::string line, block;
    auto flush = [&]
    {
        if (!block.empty())
            game.emplace(Key(block), block);
        block.clear();
    };
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.rfind("font ", 0) == 0)
            flush();
        if (!line.empty() && line[0] != '#') // (a comment line: the private marker of the bench's own file)
            block += line + "\n";
    }
    flush();
    int compared = 0;
    differing = 0;
    for (const auto& t : kitlist::Textures())
    {
        if (!t->font)
            continue;
        const std::string key = Key(t->print);
        auto range = game.equal_range(key);
        if (range.first == range.second)
        {
            missing += "  " + key + "\n";
            continue;
        }
        ++compared;
        bool same = false;
        for (auto it = range.first; it != range.second && !same; ++it)
            same = it->second == t->print;
        if (same)
            continue;
        ++differing;
        std::istringstream a(t->print), g(range.first->second);
        std::string la, lg;
        while (std::getline(a, la))
        {
            if (!std::getline(g, lg))
                lg = "(end)";
            if (la != lg)
            {
                diff += "  " + t->set + ": bench '" + la + "'\n" + std::string(t->set.size() + 4, ' ') + "game  '" + lg + "'\n";
                break;
            }
        }
    }
    return compared;
}
} // namespace fldl
