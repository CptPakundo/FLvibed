// Freelancer UTF files (DATA\FX\*.txm texture libraries, models, voice banks) and the uncompressed TGA images inside
// texture libraries (Texture library\<name>\MIP0). Format: docs\FREELANCER_RE.md / tools\utf-list.ps1.
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace utf
{
struct Leaf
{
    std::string path; // "\Texture library\kioncanring\MIP0"
    std::vector<u8> data;
};

// Every leaf of a UTF file (empty when the file is missing or not UTF).
std::vector<Leaf> Leaves(const std::wstring& file);

// Decodes an uncompressed TGA (type 2, 24 or 32 bit) to top-down 0xAARRGGBB pixels. False for other formats.
bool DecodeTga(const std::vector<u8>& d, int& w, int& h, std::vector<u32>& argb);
} // namespace utf
