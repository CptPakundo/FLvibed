#include "game/room.h"

#include "flexe.h"
#include "game/menu.h"
#include "str.h"

#include <cctype>
#include <sstream>

namespace room
{
static CharacterSource g_source = nullptr;
void SetCharacterSource(CharacterSource source) { g_source = source; }

typedef void(__fastcall* SlotFn)(void* self, void* edx, void* a1, void* a2);

static bool Valid(void* obj)
{
    __try
    {
        return obj && exe::VtCharacter.is(*static_cast<u32*>(obj));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool CallSlot(void* obj, int slot)
{
    __try
    {
        reinterpret_cast<SlotFn>((*static_cast<void***>(obj))[slot])(obj, nullptr, nullptr, nullptr);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void CursorName(char* out, size_t len)
{
    out[0] = 0;
    __try
    {
        const char* p = Mem<const char*>(exe::CursorName); // current cursor; name at +4
        if (p)
            strncpy_s(out, len, p + 4, _TRUNCATE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

std::vector<Character> List()
{
    std::vector<Character> out;
    if (!g_source)
        return out;
    void* objs[128];
    const int n = g_source(objs, 128);
    for (int i = 0; i < n; ++i)
    {
        if (!Valid(objs[i]))
            continue;
        // hover-enter picks the cursor from the character's type and offer; read it, then leave again
        char offer[64] = {};
        if (CallSlot(objs[i], 10))
        {
            CursorName(offer, sizeof(offer));
            CallSlot(objs[i], 11);
        }
        out.push_back({objs[i], offer});
    }
    return out;
}

bool Hover(void* obj) { return Valid(obj) && CallSlot(obj, 10); }
} // namespace room

namespace uiclick
{
// Research captures (user, 19 Oct 2026: "hide the game's HUD for the duration of capture"): clears the visible bit
// (+0x6C bit 0) of every visible control and remembers them; `false` restores exactly those.
static std::vector<u32> g_hidden;
int HideAll(bool hide)
{
    int n = 0;
    __try
    {
        if (hide)
        {
            const u32 head = Mem<u32>(exe::UiControls);
            u32 node = head ? Mem<u32>(head + 4) : 0;
            for (int guard = 0; node && node != head && guard < 5000; ++guard, node = Mem<u32>(node + 4))
            {
                const u32 ctl = Mem<u32>(node + 8);
                if (ctl && (Mem<u8>(ctl + 0x6C) & 1))
                {
                    *reinterpret_cast<u8*>(ctl + 0x6C) &= ~1;
                    g_hidden.push_back(ctl), ++n;
                }
            }
        }
        else
        {
            for (u32 ctl : g_hidden)
                *reinterpret_cast<u8*>(ctl + 0x6C) |= 1, ++n;
            g_hidden.clear();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return n;
}

// Every visible control's name (+0x18) that contains `part` (all when empty), with its address and vtable - research:
// which controls a window shows (the address is what ctlinfo / dealer take further).
static void CollectNames(char* buf, size_t cap, const char* part)
{
    size_t used = 0;
    __try
    {
        const u32 head = Mem<u32>(exe::UiControls);
        u32 node = head ? Mem<u32>(head + 4) : 0;
        for (int guard = 0; node && node != head && guard < 5000; ++guard, node = Mem<u32>(node + 4))
        {
            const u32 ctl = Mem<u32>(node + 8);
            if (!ctl || !(Mem<u8>(ctl + 0x6C) & 1))
                continue;
            char name[33] = {};
            memcpy(name, reinterpret_cast<const char*>(ctl + 0x18), 32);
            if (part && *part && !strstr(name, part))
                continue;
            const int n = sprintf_s(buf + used, cap - used, "%s ctl=%08X vt=%08X\n", name, ctl, Mem<u32>(ctl));
            if (n <= 0 || used + n + 80 >= cap)
                break;
            used += n;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

std::string VisibleNames(const char* part)
{
    std::vector<char> buf(32768, 0);
    CollectNames(buf.data(), buf.size(), part);
    return buf.data();
}

static u32 FindVisibleNth(const char* want, int nth)
{
    __try
    {
        const u32 head = Mem<u32>(exe::UiControls); // flat list of all controls: node {+0 prev, +4 next, +8 FlControl*}
        u32 node = head ? Mem<u32>(head + 4) : 0;
        for (int guard = 0; node && node != head && guard < 5000; ++guard, node = Mem<u32>(node + 4))
        {
            const u32 ctl = Mem<u32>(node + 8);
            if (ctl && (Mem<u8>(ctl + 0x6C) & 1) && strncmp(reinterpret_cast<const char*>(ctl + 0x18), want, 32) == 0 &&
                --nth <= 0)
                return ctl;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return 0;
}

// "Name#n" = the n-th visible control of that name, in the game's control list order (windows reuse row names: the
// commodity trader's dealer list NN_Dealer and the ship's cargo list NN_Inventory both have ItemButton1..: #2 is the
// cargo's, 22 Oct 2026).
u32 FindVisible(const char* want)
{
    const char* hash = strchr(want, '#');
    if (!hash)
        return FindVisibleNth(want, 1);
    char name[33] = {};
    strncpy_s(name, want, std::min<size_t>(32, static_cast<size_t>(hash - want)));
    return FindVisibleNth(name, std::max(1, atoi(hash + 1)));
}

// Exact signatures, as the game calls them (verified in the disassembly, 18 Oct 2026). Was: __fastcall stand-ins with
// the wrong argument counts (the id getter takes none, a window message takes three) - two stack mismatches that
// happened to cancel out.
typedef u32(__thiscall* GetFn)(u32 self);                                // vt[8]: owner window / control id
typedef u32(__thiscall* WinMsgFn)(u32 self, u32 msg, u32 id, u32 param); // window vt[0xA8]: message, ret 0Ch
typedef void(__thiscall* ActionFn)(u32 self);                            // control vt[0xB4]: its action

bool Click(u32 ctl)
{
    __try
    {
        // Button classes by mouse handler: text / popup-OK buttons 0x5A4380 (sound id +0x380, notify-owner
        // flag +0x388 bit 2, messages 0x27 then 0x26); icon buttons 0x55B950 (HUD, maneuvers, NN menu, base
        // rooms: sound id +0x374, the action vt[0xB4] = IconButtonActivate).
        void** vt = *reinterpret_cast<void***>(ctl);
        const u32 handler = reinterpret_cast<u32>(vt[0x4C / 4]);
        // Front-end buttons (main menu 0x5664A0: enabled +0xE6, id +0xEC; character select and other front-end
        // screens 0x59A3D0: enabled +0x498, id +0x4A4): on release, parent->vt[0x5C](0x3004, id, 0); the screen
        // plays the click sound itself.
        if (exe::MainMenuButtonClick.is(handler) || exe::FrontButtonClick.is(handler))
        {
            const bool main = exe::MainMenuButtonClick.is(handler);
            const u32 parent = Mem<u32>(ctl + 4);
            if (!parent || !Mem<u8>(ctl + (main ? 0xE6 : 0x498)))
                return false;
            reinterpret_cast<WinMsgFn>((*reinterpret_cast<void***>(parent))[0x5C / 4])(
                parent, 0x3004, Mem<u32>(ctl + (main ? 0xEC : 0x4A4)), 0);
            return true;
        }
        if (exe::ToggleButtonClick.is(handler))
        {
            // Toggle buttons (ToggleButtonControl, vt 0x5E1764: the equipment dealer's EquipToggle N mount boxes, the
            // Options screen's opt_toggle N): the game's handler picks the click sound by the state (+0x3A0 -> the id at
            // +0x3C4 off / +0x3C8 on) and hands over to the icon button handler, whose release runs the control's action
            // vt[0xB4] = ToggleButtonActivate (the command, else msg 0x26 to the owner window - the dealer mounts /
            // unmounts; then the state flips and the owner gets msg 0x30 with it). Signed, else fails closed (25 Oct 2026).
            if (!exe::ToggleButtonActivate.is(reinterpret_cast<u32>(vt[0xB4 / 4])))
                return false;
            const u32 sound = Mem<u32>(ctl + (Mem<u32>(ctl + 0x3A0) ? 0x3C8 : 0x3C4));
            if (sound != 0xFFFFFFFF)
                hk::Call<void(__cdecl*)(u32)>(exe::PlayUiSound, sound);
            reinterpret_cast<ActionFn>(vt[0xB4 / 4])(ctl);
            return true;
        }
        if (exe::IconButtonClick.is(handler))
        {
            // Icon buttons: their own action, as the game's mouse handler runs it on a click while the button is not in
            // its other mode (+0x330 bit 0x80, set and cleared by the hover logic: then the handler sends 0x27, which
            // e.g. the NavBar ignores). Was: our click followed that bit - with no real hover it could stay set, and
            // room buttons (Launch, Bar, ...) were ignored for the rest of the session (18 Oct 2026). The action must
            // be the signed function: another class or an unknown build fails closed.
            if (!exe::IconButtonActivate.is(reinterpret_cast<u32>(vt[0xB4 / 4])))
                return false;
            const u32 sound = Mem<u32>(ctl + 0x374);
            if (sound != 0xFFFFFFFF)
                hk::Call<void(__cdecl*)(u32)>(exe::PlayUiSound, sound);
            reinterpret_cast<ActionFn>(vt[0xB4 / 4])(ctl);
            return true;
        }
        if (!exe::TextButtonClick.is(handler))
            return false;
        const u32 sound = Mem<u32>(ctl + 0x380);
        if (sound != 0xFFFFFFFF)
            hk::Call<void(__cdecl*)(u32)>(exe::PlayUiSound, sound);
        if (Mem<u8>(ctl + 0x388) & 2)
        {
            // Text buttons notify their owner window: 0x27, and 0x26 if that was not handled (as the game does).
            const u32 parent = Mem<u32>(ctl + 4);
            if (!parent)
                return false;
            const u32 win = reinterpret_cast<GetFn>((*reinterpret_cast<void***>(parent))[8 / 4])(parent);
            if (!win)
                return false;
            const WinMsgFn msg = reinterpret_cast<WinMsgFn>((*reinterpret_cast<void***>(win))[0xA8 / 4]);
            const u32 id = reinterpret_cast<GetFn>(vt[8 / 4])(ctl);
            if (!msg(win, 0x27, id, 0))
                msg(win, 0x26, id, 0);
        }
        else
            reinterpret_cast<ActionFn>(vt[0xB4 / 4])(ctl);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

float Scroll(u32 ctl, float delta, bool pages, float* min, float* max)
{
    __try
    {
        void** vt = *reinterpret_cast<void***>(ctl);
        if (!exe::ScrollBarClick.is(reinterpret_cast<u32>(vt[0x4C / 4])))
            return -1;
        float& pos = *reinterpret_cast<float*>(ctl + 0x3C8);
        pos += pages ? delta * Mem<float>(ctl + 0x3CC) : delta;
        hk::Call<void(__thiscall*)(u32)>(exe::ScrollBarClamp, ctl);
        hk::Call<void(__thiscall*)(u32)>(exe::ScrollBarLayout, ctl);
        // the bar's own notifications after a page click (0x597143..0x597178)
        if (const u32 parent = Mem<u32>(ctl + 4))
        {
            reinterpret_cast<WinMsgFn>((*reinterpret_cast<void***>(parent))[0x5C / 4])(parent, 0xF005, ctl, 0);
            if (const u32 win = reinterpret_cast<GetFn>((*reinterpret_cast<void***>(parent))[8 / 4])(parent))
                reinterpret_cast<WinMsgFn>((*reinterpret_cast<void***>(win))[0xA8 / 4])(
                    win, 0x3E, ctl, static_cast<u32>(static_cast<int>(pos)));
        }
        if (min)
            *min = Mem<float>(ctl + 0x3C0);
        if (max)
            *max = Mem<float>(ctl + 0x3C4);
        return pos;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}
} // namespace uiclick

namespace room
{
bool Unhover(void* obj) { return Valid(obj) && CallSlot(obj, 11); }
bool Talk(void* obj) { return Valid(obj) && CallSlot(obj, 12); }

// NavBar +0x37C: MSVC6 std::list (head node at +0x380) of the room UI's hotspot records. Node: +0 next,
// +0x10 label (std::string data, length at +0x14), +0x20 FlControl, +0x24 hotspot (+0 name, +0x10 behaviour).
struct RawHotspot
{
    char label[48], hotspot[48], control[33];
    u32 behaviour, ctl;
};

// The visible NavBar, only if it is the game's NavBar class (its fields are read by fixed offsets: another build or a
// UI mod's control of that name fails closed instead of reading garbage).
static u32 NavBar()
{
    const u32 nav = uiclick::FindVisible("NavBar");
    return nav && exe::VtNavBar.is(Mem<u32>(nav)) ? nav : 0;
}

static int ReadRaw(RawHotspot* out, int max)
{
    int n = 0;
    __try
    {
        const u32 nav = NavBar();
        const u32 head = nav ? Mem<u32>(nav + 0x380) : 0;
        u32 r = head ? Mem<u32>(head) : 0;
        for (int guard = 0; r && r != head && n < max && guard < 64; ++guard, r = Mem<u32>(r))
        {
            RawHotspot& h = out[n];
            memset(&h, 0, sizeof(h));
            const u32 len = Mem<u32>(r + 0x14);
            const char* label = Mem<const char*>(r + 0x10);
            if (label && len < sizeof(h.label))
                memcpy(h.label, label, len);
            h.ctl = Mem<u32>(r + 0x20);
            if (h.ctl && !exe::VtIconButton.is(Mem<u32>(h.ctl)))
                h.ctl = 0; // not a room button as we know it: listed, never clicked
            if (h.ctl)
                memcpy(h.control, reinterpret_cast<const char*>(h.ctl + 0x18), 32);
            const u32 spot = Mem<u32>(r + 0x24);
            if (spot)
            {
                if (const char* name = Mem<const char*>(spot))
                    strncpy_s(h.hotspot, name, _TRUNCATE);
                h.behaviour = Mem<u32>(spot + 0x10);
            }
            ++n;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return n;
}

std::vector<Hotspot> Hotspots()
{
    RawHotspot raw[32];
    std::vector<Hotspot> out;
    const int n = ReadRaw(raw, 32);
    for (int i = 0; i < n; ++i)
        out.push_back({raw[i].label, raw[i].hotspot, raw[i].control, raw[i].behaviour, raw[i].ctl});
    return out;
}

bool NNWindowBusy()
{
    // the manager's close (NNWindowClose) returns while TransitionRunning(+0x330) or (+0x350): both signed, so the
    // offsets and the byte are the game's (any mismatch = cannot tell = not busy, today's behaviour)
    if (!exe::NNManager.ok() || !exe::NNWindowClose.ok() || !exe::TransitionRunning.ok())
        return false;
    __try
    {
        const u32 mgr = Mem<u32>(exe::NNManager);
        return mgr && (Mem<u8>(mgr + 0x330 + 0x18) || Mem<u8>(mgr + 0x350 + 0x18));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool NNWindowClosing()
{
    if (!NNWindowBusy())
        return false;
    __try
    {
        const u32 mgr = Mem<u32>(exe::NNManager);
        return mgr && !Mem<u32>(mgr + 0x3A8);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// 0 ready, else 1 global flag, 2 / 3 NavBar sub-objects, 4 room busy, 5 no NavBar, 6 fault, 7 scene, 8 unresolved,
// 9 the NeuroNet window manager's open / close slide (a dealer click then opens nothing: 26 Oct 2026)
static int NavBusyCode()
{
    __try
    {
        if (!exe::UiBusy.ok() || !exe::RoomInfo.ok() || !exe::RoomInfoOwner.ok())
            return 8; // cannot tell (Freelancer.exe symbols unresolved): never report ready
        if (Mem<u8>(exe::UiBusy))
            return 1;
        // The game's own "in cutscene" first (the cinematic letterbox, exe::IsPlayerInCutscene): room entry transitions
        // and room cutscenes - USER_CANCEL skips both. M04 Benford's Van Pelt scene keeps the room buttons shown and at
        // times shows no NavBar: read as ready / "no navbar", it was sat through (~3.5 min a run, until 21 Oct 2026).
        // Probed: 0 in a plain room, 1 in the Juni bar scene, Van Pelt's and room changes.
        if (exe::IsPlayerInCutscene.ok() && hk::Call<int(__cdecl*)()>(exe::IsPlayerInCutscene))
            return 7;
        const u32 nav = NavBar();
        if (!nav)
            return 5;
        if (Mem<u8>(nav + 0x32C + 0x18))
            return 2;
        if (Mem<u8>(nav + 0x34C + 0x18))
            return 3;
        if (NNWindowBusy())
            return 9;
        const u32 info = hk::Call<u32(__thiscall*)(u32)>(exe::RoomInfo, static_cast<u32>(exe::RoomInfoOwner.get()));
        if (info && Mem<u8>(info + 0x1338))
            return 4;
        // A room scene (story / bar cutscene) hides every room button (state bits 0-1 cleared) while the NavBar
        // itself stays shown.
        static RawHotspot raw[32];
        const int n = ReadRaw(raw, 32);
        bool anyShown = false;
        for (int i = 0; i < n; ++i)
            anyShown |= raw[i].ctl && (Mem<u8>(raw[i].ctl + 0x6C) & 1);
        if (n > 0 && !anyShown)
            return 7;
        return 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 6;
    }
}

std::string NavBusy()
{
    static const char* kWhy[] = {"",          "ui-flag 67DCC8", "navbar+32C", "navbar+34C", "room+1338",
                                 "no navbar", "fault",          "scene",      "unresolved", "nn-window"};
    return kWhy[NavBusyCode()];
}

using str::Lower;

// `dealer` probe of one object (SEH-guarded: research addresses, the object's class unknown)
static void DealerProbe(u32 obj, int depth, char* line, size_t size)
{
    __try
    {
        if (IsBadReadPtr(reinterpret_cast<void*>(obj), 0x980))
        {
            sprintf_s(line, size, "d%d %08X (short object)", depth, obj);
            return;
        }
        const u32 list = Mem<u32>(obj + 0x360);
        int n = sprintf_s(line, size, "d%d %08X vt %08X +3A4 %d +978 %d +360 %08X", depth, obj, Mem<u32>(obj),
                          Mem<i32>(obj + 0x3A4), Mem<i32>(obj + 0x978), list);
        const u32 cands[2] = {obj, list};
        for (int i = 0; i < 2 && n > 0; ++i)
        {
            const u32 c = cands[i];
            if (!c || IsBadReadPtr(reinterpret_cast<void*>(c), 0x980))
                continue;
            const i32 side = Mem<i32>(c + 0x978), sel = Mem<i32>(c + 0x3A4);
            if ((side == 1 || side == 3) && sel >= 0 && sel < 1000)
                n += sprintf_s(line + n, size - n, " | %s sel %d good %u", i ? "list" : "self", sel,
                               hk::Call<u32(__thiscall*)(u32)>(exe::DealerSelectedGood, c));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf_s(line, size, "d%d %08X fault", depth, obj);
    }
}

bool Command(const std::string& cmd, const std::string& rest, std::string* reply)
{
    if (menu::Command(cmd, rest, reply)) // menu, saves, loadsave (core/game/menu.cpp)
        return true;
    std::ostringstream out;
    if (cmd == "uihide") // uihide 1|0: hide every visible UI control (the HUD) / restore them (research captures)
    {
        *reply = "uihide " + std::to_string(uiclick::HideAll(rest == "1")) + " controls";
        return true;
    }
    if (cmd == "uinames") // uinames [part]: every visible control (name, address, vtable) - research
    {
        *reply = uiclick::VisibleNames(rest.c_str());
        return true;
    }
    if (cmd == "visible") // visible <ControlName>: whether a control with that name is visible (state for tests)
    {
        *reply = uiclick::FindVisible(rest.c_str()) ? "visible " + rest : "not visible " + rest;
        return true;
    }
    if (cmd == "ctlinfo") // ctlinfo <ControlName>: addresses for reverse engineering (control, parent, owner window)
    {
        const u32 ctl = uiclick::FindVisible(rest.c_str());
        if (!ctl)
        {
            *reply = "no visible control " + rest;
            return true;
        }
        char line[256];
        const u32 parent = Mem<u32>(ctl + 4);
        const u32 owner = parent ? reinterpret_cast<u32(__fastcall*)(u32, void*)>(
                                       (*reinterpret_cast<void***>(parent))[8 / 4])(parent, nullptr)
                                 : 0;
        sprintf_s(line, "control %08X vt %08X mouse %08X | parent %08X vt %08X | owner %08X vt %08X", ctl,
                  Mem<u32>(ctl), (*reinterpret_cast<u32**>(ctl))[0x4C / 4], parent, parent ? Mem<u32>(parent) : 0,
                  owner, owner ? Mem<u32>(owner) : 0);
        *reply = line;
        return true;
    }
    if (cmd == "dealer") // dealer [ControlName]: research probe of the dealer window around a row control (20 Oct 2026)
    {
        // The dealer's buy (0x47EAB0, thiscall(count) on the dealer window) buys the good of its list object's selected
        // row: list = window+0x360, +0x3A4 selected index (-1 none), +0x978 1/3 = the side shown; 0x483580 (thiscall on
        // the list) returns that row's good id. Prints the candidates up the control's parent chain.
        const u32 ctl = uiclick::FindVisible(rest.empty() ? "ItemButton1" : rest.c_str());
        if (!ctl)
        {
            *reply = "no visible control " + rest;
            return true;
        }
        u32 obj = ctl;
        for (int depth = 0; depth < 6 && obj; ++depth)
        {
            char line[300];
            DealerProbe(obj, depth, line, sizeof line);
            out << line << "\n";
            obj = IsBadReadPtr(reinterpret_cast<void*>(obj), 8) ? 0 : Mem<u32>(obj + 4);
        }
        *reply = out.str();
        return true;
    }
    if (cmd == "scroll") // scroll <ControlName> <delta> [page]: native scroll bar move (rows, or pages with "page")
    {
        std::istringstream in(rest);
        std::string name, unit;
        float delta = 0;
        in >> name >> delta >> unit;
        const u32 ctl = uiclick::FindVisible(name.c_str());
        float lo = 0, hi = 0;
        const float pos = ctl ? uiclick::Scroll(ctl, delta, unit == "page", &lo, &hi) : -1;
        char line[160];
        sprintf_s(line,
                  !ctl      ? "no visible control %s"
                  : pos < 0 ? "not a scroll bar: %s"
                            : "scrolled %s pos=%.1f range=%.1f..%.1f",
                  name.c_str(), pos, lo, hi);
        *reply = line;
        return true;
    }
    if (cmd == "click") // click <ControlName>: native button click (no cursor), e.g. click MMOkButton
    {
        const u32 ctl = uiclick::FindVisible(rest.c_str());
        const bool slide = ctl && NNWindowBusy(); // (read before the click: a close starts the close slide)
        *reply = !ctl                  ? "no visible control " + rest
                 : uiclick::Click(ctl) ? "clicked " + rest
                                       : "not a supported button: " + rest;
        if (slide) // the game drops a NeuroNet window's close (and opens no window) during its slides: say so, retry later
            *reply += " (nn-window busy: a close is dropped by the game now)";
        return true;
    }
    if (cmd == "hotspots") // room buttons by function: label, control (numbered per room), hotspot, behaviour
    {
        const std::vector<Hotspot> hs = Hotspots();
        for (const Hotspot& h : hs)
        {
            char line[200];
            sprintf_s(line, "%-14s %-15s %-34s behaviour=%u\n", h.label.c_str(), h.control.c_str(), h.hotspot.c_str(),
                      h.behaviour);
            out << line;
        }
        const std::string busy = NavBusy();
        out << "hotspots " << hs.size() << (busy.empty() ? " ready" : " busy: " + busy);
        *reply = out.str();
        return true;
    }
    if (cmd == "goto") // goto <label | hotspot name part>: native click on that room button, e.g. goto bar
    {
        Goto(rest, reply);
        return true;
    }
    if (cmd == "talkers") // base-room characters and what each offers (native, no cursor)
    {
        const std::vector<Character> cs = List();
        out << "talkers " << cs.size() << "\n";
        for (size_t i = 0; i < cs.size(); ++i)
            out << i << " " << cs[i].obj << " " << cs[i].offer << "\n";
        *reply = out.str();
        return true;
    }
    if (cmd == "talk") // talk <index> | talk <offer part, e.g. story>: talk to a room character natively
    {
        const std::vector<Character> cs = List();
        const bool byIndex = !rest.empty() && isdigit(static_cast<u8>(rest[0]));
        *reply = "no such character";
        for (size_t i = 0; i < cs.size(); ++i)
            if (byIndex ? i == static_cast<size_t>(atoi(rest.c_str())) : cs[i].offer.find(rest) != std::string::npos)
            {
                *reply = Talk(cs[i].obj) ? "talking to " + std::to_string(i) + " " + cs[i].offer : "talk failed";
                break;
            }
        return true;
    }
    return false;
}

bool Goto(const std::string& want, std::string* what)
{
    const std::string busy = NavBusy();
    if (!busy.empty())
    {
        *what = "busy: " + busy; // the game would swallow the click
        return false;
    }
    const std::vector<Hotspot> hs = Hotspots();
    const std::string w = Lower(want);
    for (int pass = 0; pass < 2; ++pass) // exact label first, then part of a label or hotspot name
        for (const Hotspot& h : hs)      // (mission-added buttons such as IDS_HOTSPOT_LAUNCH have only a label)
            if (h.ctl && (pass == 0 ? Lower(h.label) == w
                                    : Lower(h.label).find(w) != std::string::npos ||
                                          Lower(h.hotspot).find(w) != std::string::npos))
            {
                const bool ok = uiclick::Click(h.ctl);
                *what = (ok ? "clicked " : "not a supported button: ") + h.control + " (" + h.label + ")";
                return ok;
            }
    *what = hs.empty() ? "no hotspots (not in a base room?)" : "no hotspot " + want;
    return false;
}
} // namespace room
