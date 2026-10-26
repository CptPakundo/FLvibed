// core\game\utf.cpp (pure: a file in, leaves out): a synthetic UTF file laid out as the reader documents it (header
// offsets +8 nodes / +24 strings / +36 data; node +0 next, +4 name, +8 flags 0x10 = directory, +16 child or data, +24
// size) and synthetic TGA images (24-bit bottom-up, 32-bit top-down, an RLE one refused).
#include "test.h"

#include "game/utf.h"

#include <cstring>

static void Put32(std::vector<u8>& b, size_t at, u32 v)
{
    if (b.size() < at + 4)
        b.resize(at + 4);
    memcpy(&b[at], &v, 4);
}

void Test_utf()
{
    // strings: "" (root), "Texture library", "MIP0", "other"
    const char strings[] = "\0Texture library\0MIP0\0other\0";
    const u32 sRoot = 0, sLib = 1, sMip = 17, sOther = 22;
    const u8 data[] = {1, 2, 3, 4, 9, 8, 7};
    const u32 nodeOff = 56, nodeSize = 44;
    const u32 strOff = nodeOff + 4 * nodeSize;
    const u32 dataOff = strOff + sizeof(strings);
    std::vector<u8> b(dataOff + sizeof(data));
    memcpy(&b[0], "UTF ", 4);
    Put32(b, 8, nodeOff);
    Put32(b, 24, strOff);
    Put32(b, 36, dataOff);
    auto node = [&](u32 i, u32 next, u32 name, u32 flags, u32 childOrData, u32 size)
    {
        const u32 n = nodeOff + i * nodeSize;
        Put32(b, n + 0, next);
        Put32(b, n + 4, name);
        Put32(b, n + 8, flags);
        Put32(b, n + 16, childOrData);
        Put32(b, n + 24, size);
    };
    node(0, 0, sRoot, 0x10, 1 * nodeSize, 0); // root directory -> its first child
    node(1, 0, sLib, 0x10, 2 * nodeSize, 0);  // "Texture library" directory
    node(2, 3 * nodeSize, sMip, 0x80, 0, 4);  // leaf MIP0: data[0..4), next sibling
    node(3, 0, sOther, 0x80, 4, 3);           // leaf other: data[4..7)
    memcpy(&b[strOff], strings, sizeof(strings));
    memcpy(&b[dataOff], data, sizeof(data));

    const std::string path = TempPath("utf_test.utf");
    CHECK(WriteBytes(path, b.data(), b.size()));
    std::wstring wpath(path.begin(), path.end());
    const std::vector<utf::Leaf> leaves = utf::Leaves(wpath);
    CHECK(leaves.size() == 2);
    const utf::Leaf* mip = nullptr;
    const utf::Leaf* other = nullptr;
    for (const utf::Leaf& l : leaves)
        if (l.path == "\\\\Texture library\\MIP0")
            mip = &l;
        else if (l.path == "\\\\Texture library\\other")
            other = &l;
    CHECK(mip != nullptr);
    CHECK(other != nullptr);
    if (mip)
        CHECK(mip->data == std::vector<u8>({1, 2, 3, 4}));
    if (other)
        CHECK(other->data == std::vector<u8>({9, 8, 7}));
    // a leaf whose data runs past the file is dropped, the rest stays
    node(3, 0, sOther, 0x80, 4, 300);
    CHECK(WriteBytes(path, b.data(), b.size()));
    CHECK(utf::Leaves(wpath).size() == 1);
    // not a UTF file / missing file
    CHECK(WriteTextFile(path, "BINI not a utf file, long enough to pass the size check............"));
    CHECK(utf::Leaves(wpath).empty());
    CHECK(utf::Leaves(L"Z:\\no\\such\\file.utf").empty());
    DeleteFileA(path.c_str());

    // TGA: 2 x 2, 24-bit, bottom-up (file row 0 is the image's bottom row); pixels as B, G, R
    std::vector<u8> tga(18, 0);
    tga[2] = 2;
    tga[12] = 2, tga[14] = 2, tga[16] = 24;
    const u8 px24[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // bottom row: (x0) BGR 01 02 03, (x1) 04 05 06
                       0x11, 0x12, 0x13, 0x14, 0x15, 0x16}; // top row
    tga.insert(tga.end(), px24, px24 + sizeof(px24));
    int w = 0, h = 0;
    std::vector<u32> argb;
    CHECK(utf::DecodeTga(tga, w, h, argb));
    CHECK(w == 2 && h == 2 && argb.size() == 4);
    if (argb.size() == 4)
    {
        CHECK(argb[0] == 0xFF131211u); // top-left: the file's second row, R 13 G 12 B 11
        CHECK(argb[1] == 0xFF161514u);
        CHECK(argb[2] == 0xFF030201u); // bottom-left: the file's first row
        CHECK(argb[3] == 0xFF060504u);
    }
    // 32-bit top-down with alpha
    std::vector<u8> tga32(18, 0);
    tga32[2] = 2;
    tga32[12] = 1, tga32[14] = 2, tga32[16] = 32, tga32[17] = 0x20;
    const u8 px32[] = {0x01, 0x02, 0x03, 0x80, 0x11, 0x12, 0x13, 0x40};
    tga32.insert(tga32.end(), px32, px32 + sizeof(px32));
    CHECK(utf::DecodeTga(tga32, w, h, argb));
    CHECK(w == 1 && h == 2);
    if (argb.size() == 2)
    {
        CHECK(argb[0] == 0x80030201u);
        CHECK(argb[1] == 0x40131211u);
    }
    // refused: RLE (type 10), a truncated image
    std::vector<u8> rle = tga;
    rle[2] = 10;
    CHECK(!utf::DecodeTga(rle, w, h, argb));
    std::vector<u8> cut(tga.begin(), tga.end() - 1);
    CHECK(!utf::DecodeTga(cut, w, h, argb));
}
