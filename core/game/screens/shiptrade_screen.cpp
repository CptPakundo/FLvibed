// The ship dealer page's reads and actions: shiptrade_screen.h. The reads are basepage.cpp's ReadShips, the ship dealer's part
// of its Update and what its DrawShips read as it drew; the actions its ShipPick / ShipBuy and the close; the 3D view's
// ViewBack, ShipTurn, ShipZoom and HoldWheel; all moved unchanged (audit 70b F3c).
#include "shiptrade_screen.h"

#include "parked.h"
#include "screen_rules.h"
#include "game/baseinfo.h"
#include "game/equip_rules.h"
#include "game/fl.h"
#include "game/gamedata.h"
#include "game/hostcalls.h"
#include "game/room.h"
#include "flexe.h"
#include "hook.h"
#include "safemem.h"
#include "text.h"
#include "ui/views.h"
#include "ui/winplates.h"

#include <cmath>

namespace shiptradescreen
{
// The window's own fields (docs\research\neuronet-windows-facts.md): the ships' prices +0x3A8, their count +0x3CC, the chosen
// +0x3D0, their status +0x3FC (1 the level, 2 the standing), their level +0x408, the budget (cash and the trade-in) +0x3A4.
constexpr u32 kPrice = 0x3A8, kCount = 0x3CC, kChosen = 0x3D0, kStatus = 0x3FC, kLevel = 0x408, kBudget = 0x3A4;
static u32 g_soonAt = 0; // a pick asks the ships to be read again at this time (GetTickCount), 0 none

static u32 R32(u32 a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}

static void ReadShips(Model& m, u32 ship)
{
    std::vector<Offer> offers;
    const int n = std::min(3, static_cast<int>(R32(ship + kCount)));
    for (int i = 0; i < n; ++i)
    {
        Offer o;
        o.price = static_cast<int>(R32(ship + kPrice + 4 * i));
        const int status = static_cast<int>(R32(ship + kStatus + 4 * i));
        o.refusal = status == 1 ? 1 : status == 2 ? 2 : o.price > static_cast<int>(R32(ship + kBudget)) ? 3 : 0;
        o.level = static_cast<int>(R32(ship + kLevel + 4 * i));
        char name[40];
        sprintf_s(name, "ShipTraderShipName%d", i);
        winplates::TextOf(uiclick::FindVisible(name), o.name);
        sprintf_s(name, "ShipTraderShipClass%d", i);
        winplates::TextOf(uiclick::FindVisible(name), o.kind);
        offers.push_back(o);
    }
    m.offers.swap(offers);
    m.sel = static_cast<int>(R32(ship + kChosen));
    if (m.sel >= static_cast<int>(m.offers.size()))
        m.sel = -1;
}

// ---- the actions: the window's own buttons
// A ship chosen: its frame clicked; the ships read again soon (the window's choice shows in its fields).
static bool Pick(u32, int i)
{
    char name[40];
    sprintf_s(name, "ShipTraderShipNameFrame%d", i);
    const u32 c = uiclick::FindVisible(name);
    if (c)
        uiclick::Click(c);
    g_soonAt = (GetTickCount() + 50) | 1;
    return c != 0;
}
static int BuyMay(const void* model, int)
{
    const Model& m = *static_cast<const Model*>(model);
    const Offer* o = m.Chosen();
    return o && o->refusal == 0 ? 0 : 1; // (no ship chosen, or the game would refuse it: CanBuy's model half)
}
static bool Buy(u32, int)
{
    if (!uiclick::FindVisible("ShipTraderBuyButton") || room::NNWindowBusy())
        return false;
    uiclick::Click(uiclick::FindVisible("ShipTraderBuyButton"));
    return true;
}
static bool Close(u32, int) { return screens::PressClose(kCloseButton); }

const screens::ActionDesc kActions[] = {
    {kActPick, "pick", L"SHIP", nullptr, nullptr, nullptr, Pick, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr, false,
     screens::Answer::Now, 0, nullptr, nullptr},
    {kActBuy, "buy", L"BUY THIS SHIP", nullptr, BuyMay, nullptr, Buy, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr, false,
     screens::Answer::Now, 0, nullptr, nullptr},
    {kActClose, "close", L"CLOSE", nullptr, nullptr, nullptr, Close, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr, false,
     screens::Answer::Now, 0, nullptr, nullptr},
};
const int kActionCount = sizeof(kActions) / sizeof(kActions[0]);
const hk::Sym* const kWindowClass = &exe::VtShipTrader;

// ---- the reads
uint64_t Stamp()
{
    static uint64_t frames = 0;
    return ++frames;
}

bool Read(Model& m, u32 root)
{
    if (room::NNWindowBusy() || !root)
        return false;
    const u32 now = GetTickCount();
    if (g_soonAt)
        m.rowsDue = g_soonAt, g_soonAt = 0;
    if (m.rowsDue != 0 && static_cast<int>(now - m.rowsDue) < 0) // (0: the open's first read, at once)
        return false;
    m.rowsDue = (now + 400) | 1;
    ReadShips(m, root);
    if (m.sel < 0 && !m.offers.empty())
        m.cardFirst = 0, Pick(root, 0);
    if (g_soonAt) // (the pick's "soon" is this read's next)
        m.rowsDue = g_soonAt, g_soonAt = 0;
    return true;
}

void ReadFrame(Model& m, u32 root)
{
    m.devicePad = views::DevicePad();
    m.credits = baseinfo::Credits(), m.budget = static_cast<int>(R32(root + kBudget));
    m.own = baseinfo::ShipName();
    m.canBuy = CanBuy(m);
    m.need.clear(), m.nStats = 0, m.compare = false;
    const Offer* o = m.Chosen();
    if (!o)
        return;
    // the chosen ship's need against the player's own value, in the equipment card's form (issue 56; the row has no room for it
    // beside the class word). A refusal by standing has no number of the game's: the standing with the base's owners alone, in
    // the equipment rows' form.
    const int level = baseinfo::PlayerLevel();
    const gamedata::BaseFacts& base = gamedata::Base(fl::PlayerBase());
    float standing = 0;
    if (o->refusal == 1 && level >= 0)
        m.need = L"NEEDS LEVEL " + std::to_wstring(o->level) + L" \xB7 YOU: LEVEL " + std::to_wstring(level);
    else if (o->refusal == 2 && !base.owner.empty() && baseinfo::Standing(base.owner.c_str(), standing))
        m.need = L"REFUSED \xB7 YOU: REP " + screens::RepWord(standing);
    // the card: a new ship's from its top
    const gamedata::ShipFacts s = gamedata::ShipNamed(o->name), mine = gamedata::Ship(baseinfo::ShipArch());
    if (s.arch != m.cardArch)
    {
        m.cardArch = s.arch, m.cardFirst = 0;
        m.cardText = text::InfoCard(s.infoIds[1]);
    }
    // the numbers against the player's
    m.compare = mine.ok;
    const auto stat = [&](const wchar_t* label, float value, float theirs)
    {
        Stat& r = m.stats[m.nStats++];
        r.label = label, r.mine = equiprules::ValueWord(value), r.diff.clear(), r.verdict = kSame;
        if (!m.compare)
            return;
        const equiprules::Stat e{label, value, 1, L""};
        const equiprules::Verdict j = equiprules::Judge(e, theirs);
        wchar_t b[24];
        if (j == equiprules::Verdict::Same)
            swprintf_s(b, L"=");
        else
            swprintf_s(b, L"%+.0f", value - theirs);
        r.diff = b, r.verdict = j == equiprules::Verdict::Better ? kBetter : j == equiprules::Verdict::Worse ? kWorse : kSame;
    };
    stat(L"ARMOR", s.hull, mine.hull);
    stat(L"CARGO SPACE", s.hold, mine.hold);
    stat(L"GUN MOUNTS", static_cast<float>(s.guns), static_cast<float>(mine.guns));
    stat(L"TURRET MOUNTS", static_cast<float>(s.turrets), static_cast<float>(mine.turrets));
    stat(L"BEST GUN CLASS", static_cast<float>(s.gunClass), static_cast<float>(mine.gunClass));
    stat(L"BEST SHIELD CLASS", static_cast<float>(s.shieldClass), static_cast<float>(mine.shieldClass));
    stat(L"NANOBOTS", s.nanobots, mine.nanobots);
    stat(L"SHIELD BATTERIES", s.batteries, mine.batteries);
    stat(L"TORPEDO MOUNTS", static_cast<float>(s.torpedoes), static_cast<float>(mine.torpedoes));
}

bool CanBuy(const Model& m) { return m.sel >= 0 && uiclick::FindVisible("ShipTraderBuyButton") != 0 && m.offers[m.sel].refusal == 0; }

// ---- the 3D view
// The game's 3D view stays the game's: the window goes off the screen, the view comes back to the page's place, a hole in the
// ground over it. (it has no rectangle: it goes where the window goes, by its own position call, so it is called back)
void ViewBack()
{
    const u32 view = uiclick::Find("ShipTrader3DShip");
    float pos[3] = {};
    if (!view || !safemem::Read(view + 0x60, pos, sizeof(pos)) || fabsf(pos[0]) < 0.01f)
        return;
    pos[0] = 0;
    reinterpret_cast<void(__thiscall*)(u32, const float*)>(R32(R32(view) + 0x14))(view, pos);
}

// The pad's stick turns the game's ship view as its mouse drag does (ShipViewControl, exe::VtShipView; drag 0x584C80, decompiled 7 Nov 2026): yaw +0x520 and
// pitch +0x524 in degrees, wrapped to +-360, yaw's sense flipped past 180 of pitch; copied to the live YPR +0x37C / +0x380; the matrix +0x358 it draws by
// rebuilt the same way (Ry(yaw), then Rx(pitch); roll stays 0). No easing pair: what is written stands, and a later mouse drag goes on from it.
void Turn(float rx, float ry, float dt)
{
    constexpr float kDead = 0.2f, kYawRate = 180.0f, kPitchRate = 120.0f, kDeg = 3.14159265f / 180.0f;
    const float sx = fabsf(rx) > kDead ? rx : 0, sy = fabsf(ry) > kDead ? ry : 0;
    const u32 view = uiclick::Find("ShipTrader3DShip");
    if ((sx == 0 && sy == 0) || !view || !exe::VtShipView.is(R32(view)))
        return;
    float yp[2] = {};
    if (!safemem::Read(view + 0x520, yp, sizeof(yp)) || !std::isfinite(yp[0]) || !std::isfinite(yp[1]))
        return;
    const auto wrap = [](float a) { while (a > 360) a -= 360; while (a < -360) a += 360; return a; };
    const float yaw = wrap(yp[0] + (fabsf(yp[1]) > 180 ? -1.0f : 1.0f) * sx * kYawRate * dt), pitch = wrap(yp[1] - sy * kPitchRate * dt); // (stick down as a drag down)
    float mtx[9] = {};
    const float cy = cosf(yaw * kDeg), syw = sinf(yaw * kDeg), cp = cosf(pitch * kDeg), sp = sinf(pitch * kDeg);
    mtx[4] = 1, mtx[0] = mtx[8] = cy, mtx[2] = syw, mtx[6] = -syw;
    for (int k = 0; k < 3; ++k)
    {
        const float a = mtx[3 + k], b = mtx[6 + k];
        mtx[3 + k] = cp * a - sp * b, mtx[6 + k] = a * sp + cp * b;
    }
    const auto put = [view](u32 off, float v) { *reinterpret_cast<float*>(static_cast<uintptr_t>(view + off)) = v; }; // (the view's own fields, read just above)
    put(0x520, yaw), put(0x524, pitch), put(0x37C, yaw), put(0x380, pitch);
    for (u32 i = 0; i < 9; ++i)
        put(0x358 + i * 4, mtx[i]);
}

// The ship's zoom is HDE's, not the game's (flplusplus: its handler in the view's wheel slot vt[0x48] adds speed x notches to +0x3EC, negative = farther,
// clamped by its ini's min / max; research\repos\flplusplus src\shippreviewscroll.cpp). The triggers move that field each frame, smoothly (the owner, 7 Nov 2026:
// the wheel's notches were stepped). No such plugin in the slot: no zoom, no prompt.
u32 ViewCtl()
{
    const u32 view = uiclick::Find("ShipTrader3DShip");
    return view && exe::VtShipView.is(R32(view)) ? view : 0;
}
bool Zoomable()
{
    const u32 view = ViewCtl();
    HMODULE in = nullptr;
    return view && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                      reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(R32(R32(view) + 0x48))), &in) &&
           in != GetModuleHandleA(nullptr);
}
void Zoom(float lt, float rt, float dt)
{
    static bool read = false;
    static float speed = 3, nearest = 0, farthest = 100;
    if (!read)
    {
        read = true;
        char path[MAX_PATH] = {}, v[32] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        if (char* slash = strrchr(path, '\\'))
            strcpy_s(slash + 1, path + MAX_PATH - (slash + 1), "flplusplus.ini");
        const auto num = [&](const char* key, float def)
        {
            GetPrivateProfileStringA("flplusplus", key, "", v, sizeof(v), path);
            return v[0] ? static_cast<float>(atof(v)) : def;
        };
        speed = num("ship_preview_scrolling_speed", 3), nearest = num("ship_preview_scrolling_min_distance", 0), farthest = std::max(nearest, num("ship_preview_scrolling_max_distance", 100));
        GetPrivateProfileStringA("flplusplus", "ship_preview_scrolling_inverse", "", v, sizeof(v), path);
        if (!_stricmp(v, "true") || !strcmp(v, "1"))
            speed = -speed;
    }
    const float t = (rt > 0.1f ? rt : 0) - (lt > 0.1f ? lt : 0); // RT in (the field toward 0), LT out
    const u32 view = ViewCtl();
    float z = 0;
    if (t == 0 || !Zoomable() || !safemem::Read(view + 0x3EC, &z, 4) || !std::isfinite(z))
        return;
    z = std::max(-farthest, std::min(-nearest, z + t * speed * 8.0f * dt)); // (8 notches' worth a second at a full trigger)
    *reinterpret_cast<float*>(static_cast<uintptr_t>(view + 0x3EC)) = z;
}

bool OverShip()
{
    const float H = winplates::g_H, W = winplates::g_W, mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
    return fabsf(mx - W / 2) <= kShipHoleHalfW * H && my >= kShipHoleTop * H && my <= kShipHoleFoot * H;
}

static bool g_wheelHeld = false;
void HoldWheel(bool on)
{
    if (on != g_wheelHeld && hostcalls::g.HoldRealWheel)
        hostcalls::g.HoldRealWheel(on), g_wheelHeld = on;
}
} // namespace shiptradescreen
