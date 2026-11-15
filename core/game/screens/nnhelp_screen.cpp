// The INFO page's text, composed by HUD 2 from the game's data with the game's own appenders (nnhelp_screen.h). The order of
// the calls follows the game's NN_Help builders, read in its code 12 Nov 2026 (docs\FREELANCER_RE.md "The INFO page's
// composers"): the docked and space bodies of 0x474B90 and the target card 0x4754A0. Every string the window would add
// goes through the same appender (0x57E2C0 for a plain string, 0x57DA40 for an infocard, 0x476080 for a market), so the
// paragraphs, their order and the market's lines are the game's by construction.
#include "nnhelp_screen.h"

#include "game/fl.h"
#include "game/nntabs.h"
#include "game/nnwin.h"
#include "flexe.h"
#include "hook.h"
#include "safemem.h"
#include "text.h"
#include "ui/winplates.h"

namespace infobody
{
static u32 R32(u32 a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}
// ---- Common.dll's exports the builders use (by name, as the game imports them)
struct Fns
{
    void(__thiscall* rdlMake)(void*) = nullptr;
    void(__thiscall* rdlFree)(void*) = nullptr;
    void(__thiscall* writerMake)(void*) = nullptr;
    bool(__thiscall* write)(void*, const void*, char*, u32, u32&) = nullptr;
    float(__thiscall* hit)(const void*) = nullptr;
    float(__thiscall* maxHit)(const void*) = nullptr;
    u32(__thiscall* groupName)(const void*) = nullptr;
    int(__cdecl* affiliation)(const int&, u32&, bool) = nullptr;
    u32(__cdecl* factionCard)(u32) = nullptr;
    int(__cdecl* vibeCard)(const int&, u32&) = nullptr;
    const void*(__cdecl* goodOf)(u32) = nullptr;
    const void*(__cdecl* system)(u32) = nullptr;
    u32(__cdecl* createId)(const char*) = nullptr;
    // the game's C runtime (MSVCRT.dll, as Freelancer.exe imports it): its swprintf prints the card's "%d%\n" as the
    // window does, its free releases the pilot name's buffer that the game allocated
    int(__cdecl* crtPrint)(wchar_t*, const wchar_t*, ...) = nullptr;
    void(__cdecl* crtFree)(void*) = nullptr;
    bool ok = false;
};
static const Fns& F()
{
    static Fns f;
    static bool tried = false;
    if (!tried)
    {
        tried = true;
        const HMODULE c = GetModuleHandleA("common.dll");
        if (!c)
            return f;
        auto get = [c](auto& slot, const char* name) { slot = reinterpret_cast<std::remove_reference_t<decltype(slot)>>(GetProcAddress(c, name)); };
        get(f.rdlMake, "??0RenderDisplayList@@QAE@XZ");
        get(f.rdlFree, "??1RenderDisplayList@@QAE@XZ");
        get(f.writerMake, "??0BinaryRDLWriter@@QAE@XZ");
        get(f.write, "?write_buffer@BinaryRDLWriter@@UAE_NABVRenderDisplayList@@PADIAAI@Z");
        get(f.hit, "?get_hit_pts@CSimple@@QBEMXZ");
        get(f.maxHit, "?get_max_hit_pts@CSimple@@QBEMXZ");
        get(f.groupName, "?get_group_name@CShip@@QBEIXZ");
        get(f.affiliation, "?GetAffiliation@Vibe@Reputation@@YAHABHAAI_N@Z");
        get(f.factionCard, "?get_info_card@Reputation@@YAII@Z");
        get(f.vibeCard, "?GetInfocard@Vibe@Reputation@@YAHABHAAI@Z");
        get(f.goodOf, "?find_by_archetype@GoodList@@YAPBUGoodInfo@@I@Z");
        get(f.system, "?get_system@Universe@@YAPBUISystem@1@I@Z");
        get(f.createId, "?CreateID@@YAIPBD@Z");
        if (const HMODULE crt = GetModuleHandleA("msvcrt.dll"))
        {
            f.crtPrint = reinterpret_cast<int(__cdecl*)(wchar_t*, const wchar_t*, ...)>(GetProcAddress(crt, "swprintf"));
            f.crtFree = reinterpret_cast<void(__cdecl*)(void*)>(GetProcAddress(crt, "free"));
        }
        f.ok = f.crtPrint && f.crtFree && f.rdlMake && f.rdlFree && f.writerMake && f.write && f.hit && f.maxHit && f.groupName && f.affiliation && f.factionCard && f.vibeCard &&
               f.goodOf && f.system && f.createId && exe::RdlAppendCard.ok() && exe::RdlAppendText.ok() && exe::InfoMarket.ok() && exe::FindObject.ok();
    }
    return f;
}

// A virtual call by the slot's byte offset (thiscall), as the builders make them on ISystem, IZone, IObj and the object.
template <class R, class... A> static R VThis(const void* obj, u32 off, A... a)
{
    typedef R(__thiscall * Fn)(const void*, A...);
    return reinterpret_cast<Fn>(R32(R32(reinterpret_cast<u32>(obj)) + off))(obj, a...);
}
// ---- the builders' accumulator: the game's is 0x66FC60 (0x1000 characters); this one is the mod's
constexpr size_t kAcc = 0x1000;
static wchar_t g_acc[kAcc];
static wchar_t g_title[256];
static void Copy(const wchar_t* s) { wcsncpy_s(g_acc, s, _TRUNCATE); }
static void Cat(const wchar_t* s) { wcsncat_s(g_acc, s, _TRUNCATE); }
static void CatIds(u32 ids) { Cat(text::Ids(ids).c_str()); } // (the game's 0x4347E0: the string table)
// "%d%\n" (the card's format at 0x5D0A48) through the game's own swprintf, so the number reads as the window's does
static void CatNumber(int n)
{
    wchar_t s[64] = {};
    F().crtPrint(s, L"%d%\n", n);
    s[63] = 0;
    Cat(s);
}
static void Flush(void* rdl) { hk::Call<void(__cdecl*)(const wchar_t*, int, void*, int)>(exe::RdlAppendText, g_acc, static_cast<int>(wcslen(g_acc)), rdl, 0); }
static void Card(u32 ids, void* rdl) { hk::Call<bool(__cdecl*)(u32, void*)>(exe::RdlAppendCard, ids, rdl); }
static void Market(void* rdl, u32 base, bool c, bool e, bool s) { hk::Call<void(__cdecl*)(void*, u32, int, int, int)>(exe::InfoMarket, rdl, base, c ? 1 : 0, e ? 1 : 0, s ? 1 : 0); }
static bool MapEntry(u32 ids, u32& second, u32& equipment, u32& commodities, u32& ships)
{
    const u32* e = nullptr;
    if (!hk::Call<bool(__cdecl*)(u32, const u32**)>(exe::InfocardMapFind, ids, &e) || !e)
        return false;
    second = e[0], equipment = e[1], commodities = e[2], ships = e[3];
    return true;
}
static u8 VisitMap(u32 id)
{
    u8 b = 0; // (the game's out byte is not cleared first when the id is absent: 0 here)
    hk::Call<bool(__cdecl*)(u32, u8*, int, int)>(exe::VisitLookup, id, &b, 0, 0);
    return b;
}
static void TitleFrom(const wchar_t* s)
{
    wcsncpy_s(g_title, s, _TRUNCATE);
    CharUpperW(g_title); // (the game's CharUpperW, 0x616420)
}
// "%s, %s System." (IDS 1372) with the two names: only %s is filled, as the game's swprintf would with two strings
static void FormatTwo(const wchar_t* fmt, const wchar_t* a, const wchar_t* b)
{
    g_acc[0] = 0;
    int n = 0;
    for (const wchar_t* p = fmt; *p; ++p)
    {
        if (p[0] == L'%' && p[1] == L's')
        {
            Cat(n == 0 ? a : n == 1 ? b : L"");
            ++n, ++p;
            continue;
        }
        const wchar_t c[2] = {*p, 0};
        Cat(c);
    }
}

// ---- the docked body (0x474E43..0x4751A8)
static void Docked(void* rdl)
{
    const u32 sysIds = fl::PlayerSystem() && F().system(fl::PlayerSystem()) ? VThis<u32>(F().system(fl::PlayerSystem()), 0x0C) : 0;
    const u32 baseIds = hk::Call<u32(__thiscall*)(u32)>(exe::BaseNameIds, static_cast<u32>(exe::RoomInfoOwner.get()));
    if (!baseIds || !sysIds)
        return; // (the window's body stays empty)
    FormatTwo(text::Ids(1372).c_str(), text::Ids(baseIds).c_str(), text::Ids(sysIds).c_str());
    Flush(rdl);
    // the base's own solar: the first system record whose base is the player's, then (no card) one whose dock_with is
    const void* sys = F().system(fl::PlayerSystem());
    const u32 key = fl::PlayerBase();
    const u32 count = VThis<u32>(sys, 0x3C);
    alignas(8) static unsigned char rec[0x400];
    u32 info = 0;
    u8 visit = 0;
    for (int pass = 0; pass < 2 && !info; ++pass)
        for (u32 i = 0; i < count; ++i)
        {
            memset(rec, 0, sizeof(rec));
            if (!VThis<bool>(sys, 0x40, i, static_cast<void*>(rec)))
                continue;
            u32 field;
            memcpy(&field, rec + (pass == 0 ? 0x88 : 0x80), 4);
            if (field != key)
                continue;
            memcpy(&info, rec + 0x58, 4);
            visit = rec[0x114] | VisitMap(F().createId(reinterpret_cast<const char*>(rec + 4)));
            break;
        }
    if (!info)
    {
        // a base no system object names (a mission's): Universe\MissionCreatedSolars.ini's table
        hk::Call<void(__cdecl*)()>(exe::MissionSolarsLoad);
        const u32 begin = Mem<u32>(exe::MissionSolarTable), end = begin ? R32(static_cast<u32>(exe::MissionSolarTable.get()) + 4) : 0;
        for (u32 r = begin; r && r + 0x14 <= end; r += 0x14)
            if (R32(r) == key)
            {
                info = R32(r + 0x10);
                break;
            }
    }
    Copy(L"\n\n");
    Flush(rdl);
    Card(info, rdl);
    u32 second, equipment, commodities, ships;
    if (!MapEntry(info, second, equipment, commodities, ships))
        return;
    Market(rdl, key, (visit & 0x04) && commodities, (visit & 0x08) && equipment, (visit & 0x10) && ships);
    if ((visit & 0x02) && second)
    {
        Copy(L"<RDL><TEXT><PARA/></TEXT></RDL>");
        Flush(rdl);
        Card(second, rdl);
    }
}

// ---- the space body (0x4751AD..0x47530C)
static void Space(void* rdl)
{
    void* ship = fl::PlayerIObj();
    const void* sys = fl::PlayerSystem() ? F().system(fl::PlayerSystem()) : nullptr;
    if (!ship || !sys)
        return;
    Copy(text::Ids(VThis<u32>(sys, 0x0C)).c_str());
    CatIds(1377); // " System, "
    const float* pos = VThis<const float*>(ship, 0);
    wchar_t sector[64] = {};
    hk::Call<void(__cdecl*)(const float*, wchar_t*, int)>(exe::SectorText, pos, sector, 64);
    Cat(sector);
    Flush(rdl);
    const u32 zone = hk::Call<u32(__cdecl*)(const void*, const float*, int, int, int)>(exe::ZoneAtPoint, sys, VThis<const float*>(ship, 0), 1, 0, 0);
    if (!zone)
        return;
    Copy(L"\n\n");
    Flush(rdl);
    Card(VThis<u32>(reinterpret_cast<void*>(zone), 0x84), rdl);
    Copy(L"<RDL><TEXT><PARA/></TEXT></RDL>");
    Flush(rdl);
    Card(VThis<u32>(reinterpret_cast<void*>(zone), 0x88), rdl);
}

// ---- the target card (0x4754A0); false: the object is not one the card takes (the window stays as it was)
static bool Target(void* rdl, void* obj)
{
    const u32 simple = VThis<u32>(obj, 0x150);
    if (!simple || (R32(simple + 0x4C) & 3) != 3)
        return false;
    const u32 types = R32(simple + 0xE0), cls = R32(simple + 0x4C);
    const u8 visit = hk::Call<u8(__cdecl*)(u32)>(exe::ObjectVisit, simple);
    if (!(visit & 1))
    {
        TitleFrom(text::Ids(hk::Call<u32(__cdecl*)(u32)>(exe::ObjectUnknownIds, simple)).c_str());
        Copy(text::Ids(55023).c_str());
        Flush(rdl);
        return true;
    }
    TitleFrom(text::Ids(hk::Call<u32(__cdecl*)(u32)>(exe::ObjectNameIds, simple)).c_str());
    g_acc[0] = 0;
    void* player = fl::PlayerIObj();
    struct
    {
        wchar_t* buf;
        int cap;
    } pilot = {nullptr, 0};
    hk::Call<size_t(__cdecl*)(u32, void*)>(exe::PilotName, simple, &pilot);
    if (pilot.buf && pilot.buf[0])
    {
        CatIds(1108); // "Pilot: "
        Cat(pilot.buf);
        Cat(L"\n");
    }
    if (pilot.buf)
        F().crtFree(pilot.buf); // (the game malloc-ed it in its CRT: its free)
    if (player && (types & 0x7F0100))
    {
        const int r = hk::Call<int(__cdecl*)(void*, void*)>(exe::AttitudeBand, player, obj);
        CatIds(1109); // "ATTITUDE: "
        CatIds(r < 0 ? 1126 : r > 0 ? 1128 : 1127);
        Cat(L"\n");
    }
    if (types & 0x7F0000)
    {
        const float maxHit = F().maxHit(reinterpret_cast<void*>(simple));
        CatIds(1111); // "Health: "
        CatNumber(maxHit > 0 ? static_cast<int>(F().hit(reinterpret_cast<void*>(simple)) / maxHit * 100) : 0);
        if ((cls & 0x103) == 0x103)
        {
            float a = 0, b = 0;
            hk::Call<void(__cdecl*)(u32, float*, float*, u8*)>(exe::ShieldLevels, simple, &a, &b, nullptr);
            if (a > 0)
            {
                CatIds(1124); // "Shield: "
                CatNumber(static_cast<int>(b / a * 100));
            }
        }
    }
    u32 kept = 0;
    if ((cls & 0x503) == 0x503)
    {
        const u32 group = F().groupName(reinterpret_cast<void*>(simple));
        if (group)
        {
            CatIds(1110); // "Faction: "
            CatIds(group);
            Cat(L"\n");
            const int vibe = static_cast<int>(R32(simple + 0x104));
            u32 aff = 0;
            F().affiliation(vibe, aff, false);
            const u32 card = F().factionCard(aff);
            if (card)
            {
                Cat(L"\n");
                kept = card;
            }
        }
    }
    if (types & 0x200)
    {
        const u32 manager = Mem<u32>(exe::WaypointManager);
        u8 isPlayer = 0;
        int index = 0;
        if (manager && hk::Call<bool(__thiscall*)(u32, u8*, int*)>(exe::WaypointShown, manager, &isPlayer, &index))
        {
            if (isPlayer)
            {
                CatIds(1102); // "Player Waypoint"
                CatNumber(index + 1);
            }
            else if (const u32 o = hk::Call<u32(__cdecl*)(int)>(exe::ClientObjective, index))
            {
                wchar_t s[0x1000] = {};
                hk::Call<void(__thiscall*)(u32, wchar_t*, int)>(exe::FmtStrFormat, o, s, 0x1000);
                Cat(s);
                Cat(L"\n");
            }
        }
    }
    if (types & 0x10000000)
    {
        TitleFrom(g_acc); // (the window's title becomes the text block so far: empty for loot)
        const u32 item = (cls & 0x803) == 0x803 ? R32(simple + 0xEC) : 0;
        if (item)
        {
            const void* good = F().goodOf(R32(item + 8));
            kept = good ? R32(reinterpret_cast<u32>(good) + 0x8C) : R32(item + 0x18);
        }
    }
    if (g_acc[0])
        Flush(rdl);
    if (kept)
        Card(kept, rdl);
    if ((cls & 0x103) == 0x103)
    {
        const int vibe = static_cast<int>(R32(simple + 0x104));
        u32 card = 0;
        F().vibeCard(vibe, card);
        if (!card)
            return true;
        Card(card, rdl);
        Copy(L"\n");
        Flush(rdl);
        const u32 base = R32(simple + 0x160);
        u32 second, equipment, commodities, ships;
        if (MapEntry(card, second, equipment, commodities, ships) && base)
        {
            Market(rdl, base, visit & 0x04, visit & 0x08, visit & 0x10); // (the map's three fields are not read here)
            Copy(L"\n");
            Flush(rdl);
            if ((visit & 0x02) && second)
            {
                Card(second, rdl);
                Copy(L"\n");
                Flush(rdl);
            }
        }
    }
    return true;
}

// ---- the case, as 0x474B90 picks it, and the builders under one guard (a fault fails closed: Case::None)
static int Build(void* rdl, u32 targetId)
{
    __try
    {
        g_acc[0] = 0, g_title[0] = 0;
        if (fl::PlayerBase())
        {
            Docked(rdl);
            return static_cast<int>(Case::Docked);
        }
        if (targetId)
        {
            void* obj = hk::Call<void*(__cdecl*)(u32, int)>(exe::FindObject, targetId, 0);
            if (obj && Target(rdl, obj))
                return static_cast<int>(Case::Target);
            return static_cast<int>(Case::TargetGone);
        }
        if (!fl::PlayerIObj())
            return static_cast<int>(Case::None);
        Space(rdl);
        return static_cast<int>(Case::Space);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return static_cast<int>(Case::None);
    }
}
// The list written out as records (BinaryRDLWriter: `this` only holds its vtable)
static u32 Write(const void* rdl, char* out, u32 cap)
{
    __try
    {
        void* writer[4] = {};
        F().writerMake(writer);
        u32 written = 0;
        return F().write(writer, rdl, out, cap, written) ? written : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}
static bool MakeList(void* rdl)
{
    __try
    {
        F().rdlMake(rdl);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
static void FreeList(void* rdl)
{
    __try
    {
        F().rdlFree(rdl);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}
static std::vector<inforules::Para> Read(const void* rdl)
{
    static std::vector<char> buf(256 * 1024);
    const u32 n = Write(rdl, buf.data(), static_cast<u32>(buf.size()));
    return inforules::Parse(reinterpret_cast<const unsigned char*>(buf.data()), n);
}

Body Compose(u32 targetId)
{
    Body b;
    if (!F().ok)
        return b;
    alignas(8) unsigned char rdl[64] = {}; // (a RenderDisplayList is 0xC bytes)
    if (!MakeList(rdl))
        return b;
    b.kase = static_cast<Case>(Build(rdl, targetId));
    b.target = b.kase == Case::Target || b.kase == Case::TargetGone ? targetId : 0;
    b.title = g_title[0] || b.kase == Case::Target ? std::wstring(g_title) : text::Ids(905); // CURRENT INFORMATION
    if (b.kase != Case::None && b.kase != Case::TargetGone)
        b.paras = Read(rdl);
    FreeList(rdl);
    return b;
}

bool ReadWindow(u32 helpRoot, Body& out)
{
    out = Body();
    const u32 textCtl = helpRoot ? R32(helpRoot + 0x330) : 0, titleCtl = helpRoot ? R32(helpRoot + 0x334) : 0;
    if (!textCtl || !exe::VtText.is(R32(textCtl)) || !F().ok)
        return false;
    winplates::TextOf(titleCtl, out.title);
    out.paras = Read(reinterpret_cast<const void*>(textCtl + 0x488)); // (the window's own list: only read)
    return true;
}

std::wstring Probe(u32 id)
{
    void* obj = id && exe::FindObject.ok() ? hk::Call<void*(__cdecl*)(u32, int)>(exe::FindObject, id, 0) : nullptr;
    const u32 simple = obj ? VThis<u32>(obj, 0x150) : 0;
    wchar_t s[160];
    swprintf_s(s, L"obj=0x%X simple=0x%X cls=0x%X types=0x%X visit=%d nick=0x%X", reinterpret_cast<u32>(obj), simple, simple ? R32(simple + 0x4C) : 0,
               simple ? R32(simple + 0xE0) : 0, simple ? hk::Call<u8(__cdecl*)(u32)>(exe::ObjectVisit, simple) : -1, simple ? R32(simple + 0xB0) : 0);
    return s;
}

int VisitByte(u32 id)
{
    u8 b = 0;
    return id && hk::Call<bool(__cdecl*)(u32, u8*, int, int)>(exe::VisitLookup, id, &b, 0, 0) ? b : -1;
}

std::wstring Dump(const Body& b, bool styles)
{
    std::wstring s = L"title=" + b.title;
    for (const inforules::Para& p : b.paras)
    {
        s += L"\n|" + p.Text();
        const std::wstring st = styles ? inforules::StyleLine(p) : std::wstring();
        if (!st.empty())
            s += L"\n" + st;
    }
    return s;
}

// ---- the test card (TestCard): one infocard alone, through the game's own appender
static u32 g_testCard = 0; // the next open's card (0 none)
bool TestCard(u32 ids)
{
    g_testCard = TestSwitch() ? ids : 0;
    return g_testCard != 0;
}
static bool AppendCard(u32 ids, void* rdl)
{
    __try
    {
        Card(ids, rdl);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
static Body ComposeCard(u32 ids)
{
    Body b;
    if (!F().ok)
        return b;
    alignas(8) unsigned char rdl[64] = {};
    if (!MakeList(rdl))
        return b;
    b.kase = Case::Target; // (so the view draws the title, as a target's card)
    b.title = L"INFOCARD " + std::to_wstring(ids);
    if (AppendCard(ids, rdl))
        b.paras = Read(rdl);
    FreeList(rdl);
    return b;
}

// ---- the page's reads (audit 70b 2.2)
u32 Target() { return fl::PlayerBase() ? 0 : fl::TargetId(); }
uint64_t Stamp() { return Target(); }
bool Read(Model& m)
{
    if (g_testCard && m.fresh) // (only at an open: the model is new at each)
    {
        m.body = ComposeCard(g_testCard);
        g_testCard = 0, m.fresh = false;
        return true;
    }
    const u32 target = Target();
    if (!Composes(m, target))
        return false;
    Body b = Compose(target);
    if (!Takes(m, b.kase))
        return false;
    m.body = std::move(b);
    m.fresh = false;
    return true;
}

const hk::Sym* const kWindowClass = &exe::VtHelp;

// ---- INFO's actions (audit 70b 2.3)
// SHOW ON MAP: the NeuroNet's own map tab, the command the game's tab button sends (core\game\nnwin.cpp); borrowed in both
// modes, as it is the game's own handler, and it asks nothing of the server.
static bool ShowOnMap(u32, int) { return nnwin::Go(nntabs::kCmdMap); }

const screens::ActionDesc kActions[] = {
    {kActShowOnMap, "map", L"SHOW ON MAP", nullptr, nullptr, nullptr, ShowOnMap, nullptr, screens::Route::Borrowed,
     screens::Route::Borrowed, nullptr, false, screens::Answer::Now, 0, nullptr, nullptr},
};
const int kActionCount = sizeof(kActions) / sizeof(kActions[0]);
} // namespace infobody
