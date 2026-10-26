#include "flhash.h"

namespace flhash
{
static const u32* Table()
{
    static u32 t[256];
    static bool built = false;
    if (!built)
    {
        for (u32 i = 0; i < 256; ++i)
        {
            u32 x = i;
            for (int b = 0; b < 8; ++b)
                x = (x & 1) ? (x >> 1) ^ (0xA001u << 14) : x >> 1;
            t[i] = x;
        }
        built = true;
    }
    return t;
}

u32 CreateId(const char* nickname)
{
    const u32* t = Table();
    u32 h = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(nickname); p && *p; ++p)
    {
        const u32 c = (*p >= 'A' && *p <= 'Z') ? *p + ('a' - 'A') : *p; // ASCII lower case, locale-free
        h = (h >> 8) ^ t[(h ^ c) & 0xFF];
    }
    h = (h >> 24) | ((h >> 8) & 0xFF00) | ((h << 8) & 0xFF0000) | (h << 24);
    return (h >> 2) | 0x80000000u;
}
} // namespace flhash
