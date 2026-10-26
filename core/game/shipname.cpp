// FLvibed - a ship archetype's display name (shipname.h).
#include "game/shipname.h"

#include "text.h"

#include <unordered_map>

namespace shipname
{
typedef const void*(__cdecl* GetShipFn)(u32);       // Archetype::GetShip
typedef u32(__thiscall* RootIdFn)(const void*);     // Archetype::Root::get_id

// The archetype's ids_name (guarded, POD only): 0 when the archetype is unknown or not the one asked for.
static u32 ShipIds(GetShipFn getShip, RootIdFn rootId, u32 arch)
{
    __try
    {
        const u8* s = static_cast<const u8*>(getShip(arch));
        return s && rootId(s) == arch ? *reinterpret_cast<const u32*>(s + 0x14) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

std::wstring Name(u32 shipArch)
{
    static std::unordered_map<u32, std::wstring> cache;
    static GetShipFn getShip = nullptr;
    static RootIdFn rootId = nullptr;
    if (!shipArch)
        return L"";
    const auto it = cache.find(shipArch);
    if (it != cache.end())
        return it->second;
    if (!getShip || !rootId)
    {
        const HMODULE c = GetModuleHandleA("common.dll");
        if (!c)
            return L"";
        getShip = reinterpret_cast<GetShipFn>(GetProcAddress(c, "?GetShip@Archetype@@YAPAUShip@1@I@Z"));
        rootId = reinterpret_cast<RootIdFn>(GetProcAddress(c, "?get_id@Root@Archetype@@QBEIXZ"));
        if (!getShip || !rootId)
            return cache[shipArch] = L"";
    }
    const u32 ids = ShipIds(getShip, rootId, shipArch);
    std::wstring t = ids ? text::Ids(ids) : std::wstring();
    while (!t.empty() && iswspace(t.back()))
        t.pop_back();
    return cache[shipArch] = t;
}
} // namespace shipname
