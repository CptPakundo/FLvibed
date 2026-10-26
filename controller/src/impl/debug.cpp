#include "debug.h"

#include "controller.h"
#include "../dimouse.h"
#include "../fl.h"
#include "hook.h"
#include "log.h"
#include "../shm.h"
#include "host.h"
#include "game/menu.h"
#include "game/room.h"
#include "game/ai.h"
#include "game/space.h"
#include "game/route.h"
#include "game/spawn.h"
#include "game/hitlook.h"
#include "options_page.h"
#include "reticlecue.h"
#include "shieldcue.h"
#include "../events.h"
#include "ui.h"
#include "device.h"
#include "padhud.h"
#include "hud2.h"
#include "flufaware.h"
#include "hudvis.h"
#include "uiskin.h"
#include "uistrip.h"
#include "game/flicons.h"
#include "livesplit.h"
#include "newgame.h"
#include "osk.h"
#include "padmenus.h"
#include "runtimer.h"
#include "goals.h"
#include "picker.h"
#include "wheel.h"
#include "padbases.h"
#include "padmap.h"

#include <sstream>

namespace debug
{
// ---------------------------------------------------------------- safe memory
static bool SafeCopy(void* dst, const void* src, size_t len)
{
    __try
    {
        memcpy(dst, src, len);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool SafeRead(uintptr_t addr, void* out, size_t len) { return SafeCopy(out, reinterpret_cast<void*>(addr), len); }

// ------------------------------------------------------------------ RE aids: object search, hardware data breakpoints
// (21 Oct 2026, for objects the UI tree does not list - the HUD's target arrows: who writes a field each frame)

static bool ScanRegion(const u32* p, size_t count, u32 value, std::vector<uintptr_t>& hits, size_t max)
{
    __try
    {
        for (size_t i = 0; i < count && hits.size() < max; ++i)
            if (p[i] == value)
                hits.push_back(reinterpret_cast<uintptr_t>(p + i));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static std::string FindDword(u32 value, u32 max)
{
    std::vector<uintptr_t> hits;
    MEMORY_BASIC_INFORMATION mi = {};
    uintptr_t a = 0x10000;
    while (a < 0x7FFF0000 && hits.size() < max && VirtualQuery(reinterpret_cast<void*>(a), &mi, sizeof(mi)))
    {
        const uintptr_t base = reinterpret_cast<uintptr_t>(mi.BaseAddress);
        if (mi.State == MEM_COMMIT && mi.Type == MEM_PRIVATE &&
            (mi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) && !(mi.Protect & PAGE_GUARD))
            ScanRegion(reinterpret_cast<const u32*>(base), mi.RegionSize / 4, value, hits, max);
        a = base + mi.RegionSize;
    }
    std::ostringstream o;
    o << hits.size() << " found";
    for (uintptr_t h : hits)
        o << " " << std::hex << h;
    return o.str();
}

static struct
{
    uintptr_t addr = 0;
    DWORD thread = 0;
    PVOID handler = nullptr;
    struct Hit
    {
        u32 eip, caller1, caller2;
        long count;
    } hits[64];
    long n = 0;
} g_watch;

static LONG CALLBACK WatchHandler(EXCEPTION_POINTERS* e)
{
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !(e->ContextRecord->Dr6 & 0xF))
        return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* c = e->ContextRecord;
    const u32 eip = static_cast<u32>(c->Eip);
    // two code-looking return addresses from the stack (the callers, roughly): values inside Freelancer.exe's code
    // section (hk::GetImage, the image the symbol table is resolved in)
    u32 calls[2] = {}, got = 0;
    const u32* sp = reinterpret_cast<const u32*>(c->Esp);
    const hk::Image& img = hk::GetImage();
    for (int i = 0; i < 64 && got < 2; ++i)
    {
        u32 v = 0;
        if (!SafeRead(reinterpret_cast<uintptr_t>(sp + i), &v, 4))
            break;
        if (v >= img.textVa && v < img.textVa + img.textSize)
            calls[got++] = v;
    }
    long slot = -1;
    for (long i = 0; i < g_watch.n; ++i)
        if (g_watch.hits[i].eip == eip && g_watch.hits[i].caller1 == calls[0])
        {
            slot = i;
            break;
        }
    if (slot < 0 && g_watch.n < 64)
        slot = g_watch.n++, g_watch.hits[slot] = {eip, calls[0], calls[1], 0};
    if (slot >= 0)
        ++g_watch.hits[slot].count;
    c->Dr6 = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
}

// Debug registers of the game thread are set from a helper thread (a thread cannot reliably set its own).
static bool SetWatchRegs(DWORD threadId, uintptr_t addr, int len, bool rw)
{
    HANDLE t = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, threadId);
    if (!t)
        return false;
    struct Args
    {
        HANDLE t;
        uintptr_t addr;
        int len;
        bool rw, ok;
    } args{t, addr, len, rw, false};
    HANDLE h = CreateThread(
        nullptr, 0,
        [](LPVOID p) -> DWORD
        {
            Args& a = *static_cast<Args*>(p);
            if (SuspendThread(a.t) == static_cast<DWORD>(-1))
                return 0;
            CONTEXT c = {};
            c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(a.t, &c))
            {
                c.Dr0 = a.addr;
                const DWORD lenBits = a.len == 4 ? 3 : a.len == 2 ? 1 : 0;
                c.Dr7 = a.addr ? (1u | ((a.rw ? 3u : 1u) << 16) | (lenBits << 18)) : 0;
                c.Dr6 = 0;
                a.ok = SetThreadContext(a.t, &c) != 0;
            }
            ResumeThread(a.t);
            return 0;
        },
        &args, 0, nullptr);
    if (h)
        WaitForSingleObject(h, 2000), CloseHandle(h);
    CloseHandle(t);
    return args.ok;
}

// The handler lives in this (hot-reloadable) logic: it must be gone before the logic unloads.
void StopWatch()
{
    if (g_watch.addr)
        SetWatchRegs(g_watch.thread, 0, 0, false);
    if (g_watch.handler)
        RemoveVectoredExceptionHandler(g_watch.handler);
    g_watch.handler = nullptr, g_watch.addr = 0;
}

static std::string WatchCommand(std::istringstream& in)
{
    std::string arg;
    in >> arg;
    std::ostringstream o;
    if (arg == "report")
    {
        o << "watch " << std::hex << g_watch.addr << std::dec << ": " << g_watch.n << " writer(s)\n";
        for (long i = 0; i < g_watch.n; ++i)
            o << std::hex << "eip " << g_watch.hits[i].eip << " callers " << g_watch.hits[i].caller1 << " "
              << g_watch.hits[i].caller2 << std::dec << " x" << g_watch.hits[i].count << "\n";
        return o.str();
    }
    // the game thread: the one that owns the game window
    const DWORD thread = GetWindowThreadProcessId(fl::MainWindow(), nullptr);
    if (arg == "stop")
    {
        StopWatch();
        return "stopped";
    }
    const uintptr_t addr = strtoul(arg.c_str(), nullptr, 0);
    int len = 4;
    std::string mode = "w";
    in >> len >> mode;
    if (!addr || (len != 1 && len != 2 && len != 4) || addr % len)
        return "usage: watch <addr aligned to len> [len 1|2|4] [w|rw] | report | stop";
    if (!g_watch.handler)
        g_watch.handler = AddVectoredExceptionHandler(1, WatchHandler);
    g_watch.n = 0, g_watch.addr = addr, g_watch.thread = thread;
    return SetWatchRegs(thread, addr, len, mode == "rw") ? "watching" : "could not set the debug registers";
}

static u32 SafeCall(uintptr_t fn, const u32* args, int n, bool& ok)
{
    u32 result = 0;
    ok = true;
    __try
    {
        typedef u32(__cdecl * F0)();
        typedef u32(__cdecl * F1)(u32);
        typedef u32(__cdecl * F2)(u32, u32);
        typedef u32(__cdecl * F3)(u32, u32, u32);
        typedef u32(__cdecl * F4)(u32, u32, u32, u32);
        switch (n)
        {
        case 0: result = reinterpret_cast<F0>(fn)(); break;
        case 1: result = reinterpret_cast<F1>(fn)(args[0]); break;
        case 2: result = reinterpret_cast<F2>(fn)(args[0], args[1]); break;
        case 3: result = reinterpret_cast<F3>(fn)(args[0], args[1], args[2]); break;
        default: result = reinterpret_cast<F4>(fn)(args[0], args[1], args[2], args[3]); break;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ok = false;
    }
    return result;
}

// ---------------------------------------------------------------- UI dump
static void DumpNode(std::ostringstream& out, uintptr_t ctl, int depth, int& budget, float ox, float oy)
{
    if (!ctl || budget-- <= 0)
        return;
    u32 vtable = 0, child = 0, state = 0, hint = 0;
    char name[33] = {};
    float pos[3] = {}, box[3] = {}, extra[3] = {};
    if (!SafeRead(ctl, &vtable, 4) || !SafeRead(ctl + 0xC, &child, 4))
        return;
    SafeRead(ctl + 0x18, name, 32);
    SafeRead(ctl + 0x6C, &state, 4);
    SafeRead(ctl + 0xA8, pos, 12);
    SafeRead(ctl + 0xB4, box, 12);
    SafeRead(ctl + 0xC0, extra, 12);
    SafeRead(ctl + 0xCC, &hint, 4);
    const float ax = ox + pos[0], ay = oy + pos[1];
    char line[480];
    sprintf_s(line,
              "%*s%-28s ctl=%08X vt=%08X st=%02X abs=(%.4f,%.4f) pos=(%.4f,%.4f,%.4f) box=(%.4f,%.4f,%.4f) "
              "c0=(%.3f,%.3f,%.3f) hint=%u\n",
              depth * 2, "", name, ctl, vtable, state & 0xFF, ax, ay, pos[0], pos[1], pos[2], box[0], box[1], box[2],
              extra[0], extra[1], extra[2], hint);
    out << line;
    if (depth > 40)
        return;
    // Children form a sibling chain starting at +0xC.
    u32 c = child, prev = 0;
    int guard = 0;
    while (c && c != prev && guard++ < 500)
    {
        DumpNode(out, c, depth + 1, budget, ax, ay);
        prev = c;
        u32 next = 0;
        if (!SafeRead(c + 0x8, &next, 4))
            break;
        c = next;
    }
}

std::string DumpUi()
{
    std::ostringstream out;
    int budget = 8000;
    // 0x67DBE8 is a flat list of every control; dump the trees of parentless ones.
    u32 node = Mem<u32>(exe::UiControls);
    int seen = 0, roots = 0;
    while (node && seen < 20000)
    {
        u32 next = 0, ctl = 0, parent = 1;
        if (!SafeRead(node + 4, &next, 4) || !SafeRead(node + 8, &ctl, 4))
            break;
        if (ctl && SafeRead(ctl + 4, &parent, 4) && parent == 0)
        {
            out << "== root " << roots++ << "\n";
            DumpNode(out, ctl, 1, budget, 0, 0);
        }
        node = next;
        ++seen;
    }
    std::string path = DllDir() + "FLController_ui.txt";
    if (FILE* f = fopen(path.c_str(), "w"))
    {
        fputs(out.str().c_str(), f);
        fclose(f);
    }
    return path;
}

// ---------------------------------------------------------------- memory snapshot/diff
static std::vector<u8> g_snap;
static uintptr_t g_snapAddr = 0;

// ---------------------------------------------------------------- command processing
static std::string Hex(uintptr_t addr, size_t len)
{
    std::ostringstream out;
    std::vector<u8> buf(len);
    if (!SafeRead(addr, buf.data(), len))
        return "read failed";
    char tmp[16];
    for (size_t i = 0; i < len; ++i)
    {
        if (i % 16 == 0)
        {
            sprintf_s(tmp, "%s%08X:", i ? "\n" : "", static_cast<u32>(addr + i));
            out << tmp;
        }
        sprintf_s(tmp, " %02X", buf[i]);
        out << tmp;
    }
    return out.str();
}

// `shot <command> <parkX> <parkY>` (research captures, user 19 Oct 2026): fire one shot with the cursor where it is
// (the guns aim at it), then park the cursor on the first frame after the shot left - the gun's power draw - so the
// hover marker is gone from every frame of the hit.
static struct
{
    int frames = -1, parkX = 0, parkY = 0, firedAt = -1;
    float power = -1;
} g_shot;

void Frame()
{
    if (g_shot.frames < 0)
        return;
    ++g_shot.frames;
    ai::Self s;
    const bool fired = ai::ReadSelf(s) && s.power >= 0 && s.power < g_shot.power - 0.0005f;
    if (fired || g_shot.frames > 60)
    {
        Mem<int>(exe::MouseX) = g_shot.parkX;
        Mem<int>(exe::MouseY) = g_shot.parkY;
        g_shot.firedAt = fired ? g_shot.frames : 0;
        Log("shot: %s, cursor parked at frame %d", fired ? "fired" : "no power draw in 60 frames", g_shot.frames);
        g_shot.frames = -1;
    }
}

std::string Execute(const std::string& line)
{
    std::istringstream in(line);
    std::string op;
    in >> op;
    std::ostringstream out;

    auto num = [&](u32 def = 0) -> u32
    {
        std::string t;
        if (!(in >> t))
            return def;
        return static_cast<u32>(strtoul(t.c_str(), nullptr, 0));
    };

    if (op == "ping")
        return "pong";
    // perf cpu: the logic's CPU time per frame per entry point (core\log.h perf::Scope) since the last `perf cpu`, then
    // resets. Bare `perf` stays the flight model (space::Command, core\game\flight.*).
    if (op == "perf")
    {
        std::istringstream peek(line);
        std::string first, second;
        peek >> first >> second;
        if (second == "cpu")
            return perf::Report();
    }
    if (op == "stall") // stall <ms>: test aid - the game thread spins this long (at most 60 s): a frozen game for the
    {                  // freeze watchdog (tools\flfreeze.ps1, the check-ins)
        const ULONGLONG ms = std::min<u32>(num(1000), 60000), until = GetTickCount64() + ms;
        volatile u32 spin = 0;
        while (GetTickCount64() < until)
            ++spin;
        return "stalled " + std::to_string(ms) + " ms";
    }
    if (op == "ship") // player kinematics: speed, turn rates in ship axes (deg/s), forward vector
    {
        fl::Kinematics k;
        if (!fl::PlayerKinematics(k))
            return "not in space";
        const float* m = k.rot;
        float local[3]; // R^T * w : angular velocity around the ship's own axes
        for (int i = 0; i < 3; ++i)
            local[i] = m[0 * 3 + i] * k.angVel[0] + m[1 * 3 + i] * k.angVel[1] + m[2 * 3 + i] * k.angVel[2];
        float lv[3]; // velocity in ship axes: +x right, +y up, -z forward
        for (int i = 0; i < 3; ++i)
            lv[i] = m[0 * 3 + i] * k.vel[0] + m[1 * 3 + i] * k.vel[1] + m[2 * 3 + i] * k.vel[2];
        const float r2d = 57.29578f;
        char tmp[400];
        sprintf_s(tmp,
                  "speed=%.1f pos=(%.0f,%.0f,%.0f)\nturn deg/s: x(pitch)=%.1f y(yaw)=%.1f z(roll)=%.1f\n"
                  "forward=(%.3f,%.3f,%.3f) throttle=%.2f\nlocal vel: right=%.1f up=%.1f forward=%.1f\n",
                  sqrtf(k.vel[0] * k.vel[0] + k.vel[1] * k.vel[1] + k.vel[2] * k.vel[2]), k.pos[0], k.pos[1], k.pos[2],
                  local[0] * r2d, local[1] * r2d, local[2] * r2d, -m[2], -m[5], -m[8], fl::GetThrottle(), lv[0], lv[1],
                  -lv[2]);
        std::string s = tmp;
        sprintf_s(tmp, "steer=(%.2f,%.2f)\n", ctl::LastSteerX(), ctl::LastSteerY());
        s += tmp;
        fl::Kinematics t;
        if (fl::TargetKinematics(t))
        {
            float lead[3] = {};
            const int rc = fl::TargetLeadPos(lead);
            sprintf_s(tmp, "target pos=(%.0f,%.0f,%.0f) lead rc=%d (%.0f,%.0f,%.0f)\n", t.pos[0], t.pos[1], t.pos[2],
                      rc, lead[0], lead[1], lead[2]);
            s += tmp;
            float st[3] = {}, sl[3] = {};
            fl::WorldToScreen(t.pos, st);
            fl::WorldToScreen(lead, sl);
            sprintf_s(tmp, "w2s target=(%.3f,%.3f,%.3f) lead=(%.3f,%.3f,%.3f)\n", st[0], st[1], st[2], sl[0], sl[1],
                      sl[2]);
            s += tmp;
            float att = 0;
            const bool ok = fl::TargetAttitude(att);
            const float d[3] = {t.pos[0] - k.pos[0], t.pos[1] - k.pos[1], t.pos[2] - k.pos[2]};
            sprintf_s(tmp, "target attitude=%s%.2f dist=%.0f\n", ok ? "" : "?", att,
                      sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
            s += tmp;
        }
        return s;
    }
    if (op == "config") // config [save|reload]: list options, save them to FLController.ini, or reload it
        return ctl::ConfigCommand(in);
    if (op == "ui") // ui options [open|close] | tab <n> | rows | press ... | focus <key> | state: the options page
        return options_page::Command(in);
    if (op == "ap") // ap ...: the Advanced Autopilot (autopilot.cpp)
        return ctl::AutopilotCommand(in);
    if (op == "bot") // bot fight|off|status: frame-rate combat test bot (hostile targets only)
        return ctl::BotCommand(in);
    if (op == "state")
        return ctl::DescribeState();
    if (op == "peek")
    {
        u32 a = num(), n = num(64);
        return Hex(a, std::min<u32>(n, 4096));
    }
    if (op == "peekf") // floats
    {
        u32 a = num(), n = num(4);
        for (u32 i = 0; i < std::min<u32>(n, 256); ++i)
        {
            float f;
            if (!SafeRead(a + i * 4, &f, 4))
                break;
            out << std::hex << (a + i * 4) << std::dec << ": " << f << "\n";
        }
        return out.str();
    }
    if (op == "poke")
    {
        u32 a = num();
        std::vector<u8> bytes;
        std::string t;
        while (in >> t)
            bytes.push_back(static_cast<u8>(strtoul(t.c_str(), nullptr, 16)));
        if (bytes.empty())
            return "no bytes";
        return hk::Write(a, bytes.data(), bytes.size()) ? "ok" : "write failed";
    }
    if (op == "commplaying") // the radio is playing a comm now: comm player +0x460 (byte, set by the game; 18 Oct 2026)
    {
        const uintptr_t p = reinterpret_cast<uintptr_t>(HostCommPlayer());
        u8 playing = 0;
        if (!p)
            return "comm-playing -1"; // unknown: no comm since start (or an older host)
        if (!SafeRead(p + 0x460, &playing, 1))
            return "comm-playing -1";
        return std::string("comm-playing ") + (playing ? "1" : "0");
    }
    if (op == "commstate") // the client comm player (host v9): its object and the words around its queue / playing flag
    {
        const uintptr_t p = reinterpret_cast<uintptr_t>(HostCommPlayer());
        if (!p)
            return "no comm player yet (no comm since start)";
        out << "player " << std::hex << p << std::dec << "\n";
        return out.str() + Hex(p + 0x440, 0x40);
    }
    if (op ==
        "findvt") // findvt <value> [max]: objects whose first dword is this value (a vtable): committed private RW memory
    {
        const u32 value = num(), max = num(16); // in this order (arguments are evaluated in no fixed order)
        return FindDword(value, std::min<u32>(max, 256));
    }
    if (op ==
        "watch") // watch <addr> [len 1|2|4] [w|rw] | report | stop: hardware data breakpoint on the game thread (RE)
        return WatchCommand(in);
    if (op == "snap")
    {
        g_snapAddr = num();
        u32 n = num(0x1000);
        g_snap.assign(n, 0);
        return SafeRead(g_snapAddr, g_snap.data(), n) ? "snapped" : "read failed";
    }
    if (op == "diff") // dword-granular diff against the last snapshot
    {
        std::vector<u8> now(g_snap.size());
        if (!SafeRead(g_snapAddr, now.data(), now.size()))
            return "read failed";
        int shown = 0;
        for (size_t i = 0; i + 4 <= now.size() && shown < 300; i += 4)
        {
            u32 a, b;
            memcpy(&a, &g_snap[i], 4);
            memcpy(&b, &now[i], 4);
            if (a != b)
            {
                char tmp[96];
                sprintf_s(tmp, "%08X: %08X -> %08X\n", static_cast<u32>(g_snapAddr + i), a, b);
                out << tmp;
                ++shown;
            }
        }
        g_snap = now;
        return shown ? out.str() : "no changes";
    }
    if (op == "cue") // cue status | cue hit|break|hull [damage] [munition arch] | cue dir <screen deg>|off: shield cue
    {
        std::string what;
        in >> what;
        if (what == "hullpts") // test aid: the damage edge acts on these hull points until `cue hullpts off`
        {
            std::string v;
            in >> v;
            return shieldcue::TestHull(v == "off" ? -1.0f : static_cast<float>(std::atof(v.c_str())));
        }
        if (what == "dir") // test aid: hits come from this screen direction (0 = right, 90 = down) until `cue dir off`
        {
            std::string v;
            in >> v;
            return shieldcue::TestDir(v != "off", v == "off" ? 0.0f : static_cast<float>(std::atof(v.c_str())));
        }
        float dmg = 50;
        u32 mun = 0;
        in >> dmg >> mun;
        if (what == "hit")
            return shieldcue::Test(0, dmg, mun);
        if (what == "break")
            return shieldcue::Test(1, dmg, mun);
        if (what == "hull")
            return shieldcue::Test(2, dmg, mun);
        if (what == "ring") // cue ring 0..3: engage, hand-back, nanobots, batteries
        {
            reticlecue::Pulse(static_cast<reticlecue::Kind>(std::clamp(static_cast<int>(dmg), 0, 3)));
            return reticlecue::Status();
        }
        return shieldcue::Status() + "\n" + reticlecue::Status();
    }
    if (op ==
        "hitlook") // hitlook self [damage] | shield <arch> [damage] | munition <arch> | attackers <id> | hull | stages
    {
        std::string what;
        in >> what;
        if (what == "self")
        {
            ai::Self s;
            if (!ai::ReadSelf(s))
                return "not flying";
            float dmg = 0;
            in >> dmg;
            return "shield arch " + std::to_string(s.shieldArch) + ": " +
                   hitlook::Describe(hitlook::Shield(s.shieldArch, dmg));
        }
        if (what == "shield")
        {
            const u32 arch = num();
            float dmg = 0;
            in >> dmg;
            return hitlook::Describe(hitlook::Shield(arch, dmg));
        }
        if (what == "munition")
            return hitlook::Describe(hitlook::Impact(num()));
        if (what == "hull" || what == "stages") // the player ship's hull hit look [damage] / its damage stages
        {
            ai::Self s;
            if (!ai::ReadSelf(s))
                return "not flying";
            if (what == "hull")
            {
                float dmg = 0;
                in >> dmg;
                return "ship arch " + std::to_string(s.shipArch) + ": " +
                       hitlook::Describe(hitlook::Hull(s.shipArch, dmg));
            }
            out << "ship arch " << s.shipArch << " hull " << s.hull * s.hullMax << "/" << s.hullMax << "\n";
            for (const auto& st : hitlook::DamageStages(s.shipArch))
            {
                out << "below " << st.hitPts << " hp: " << st.fuse << "\n";
                for (const auto* l : st.looks)
                    out << "  " << hitlook::Describe(l) << "\n";
            }
            return out.str();
        }
        if (what == "attackers") // a ship's guns and their impact looks
        {
            for (const auto& w : space::ShipWeapons(num()))
                out << "ammo " << w.ammoArch << ": " << hitlook::Describe(hitlook::Impact(w.ammoArch)) << "\n";
            return out.str().empty() ? "no weapons" : out.str();
        }
        return "usage: hitlook self [damage] | shield <arch> [damage] | munition <arch> | attackers <id> | hull "
               "[damage] | "
               "stages";
    }
    if (op == "drainshield") // drainshield <id>: test aid, the game's DrainShields
        return space::DrainShields(num()) ? "drained" : "failed";
    if (op == "shieldof") // shieldof <id>: the ship's shield points and the game's up flag
    {
        float cur = 0, max = 0;
        bool up = false;
        if (!space::ShieldOf(num(), cur, max, up))
            return "no shield";
        out << "shield=" << cur << "/" << max << " up=" << (up ? 1 : 0);
        return out.str();
    }
    if (op == "shot") // shot <command> <parkX> <parkY> | shot status
    {
        std::string name;
        in >> name;
        if (name == "status")
            return g_shot.frames >= 0 ? "waiting"
                                      : "parked at frame " + std::to_string(g_shot.firedAt) +
                                            (g_shot.firedAt ? " (fired)" : " (no shot)");
        const int cmd = fl::CmdFromName(name.c_str());
        ai::Self s;
        if (cmd < 0 || !(in >> g_shot.parkX >> g_shot.parkY))
            return "usage: shot <command> <parkX> <parkY> | shot status";
        if (!ai::ReadSelf(s) || s.power < 0)
            return "not flying";
        if (!exe::MouseX.ok() || !exe::MouseY.ok())
            return "cursor symbols unverified";
        g_shot.power = s.power;
        g_shot.frames = 0;
        g_shot.firedAt = -1;
        const u32 flags = fl::CommandFlags(cmd);
        if (flags & fl::CMDF_KEYDOWN)
            fl::FireCommand(cmd, true);
        if (flags & fl::CMDF_KEYUP)
            fl::FireCommand(cmd, false);
        return "armed (power " + std::to_string(s.power) + ")";
    }
    if (op == "fire" || op == "tap")
    {
        std::string name;
        in >> name;
        int cmd = fl::CmdFromName(name.c_str());
        if (cmd < 0)
            return "unknown command";
        if (op == "tap") // send the events a bound key would (per the command's keymap state)
        {
            const u32 flags = fl::CommandFlags(cmd);
            out << fl::CmdName(cmd) << " flags=" << flags;
            if (flags & fl::CMDF_KEYDOWN)
                out << " down->" << fl::FireCommand(cmd, true);
            if (flags & fl::CMDF_KEYUP)
                out << " up->" << fl::FireCommand(cmd, false);
            return out.str();
        }
        int down = static_cast<int>(num(1));
        out << fl::CmdName(cmd) << " -> " << fl::FireCommand(cmd, down != 0);
        return out.str();
    }
    if (op == "key") // key <vk> [down 0/1]  (vk as number, e.g. 0x1B)
    {
        u32 vk = num();
        std::string t;
        if (in >> t)
            return fl::SendKey(static_cast<int>(vk), atoi(t.c_str()) != 0) ? "handled" : "not handled";
        fl::TapKey(static_cast<int>(vk));
        return "tapped";
    }
    if (op == "post") // post <vk>: real WM_KEYDOWN/WM_CHAR/WM_KEYUP to the game window (e.g. 13 = Enter in chat)
    {
        const u32 vk = num();
        const HWND wnd = fl::MainWindow();
        const UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
        const UINT ch = MapVirtualKeyA(vk, MAPVK_VK_TO_CHAR);
        PostMessageA(wnd, WM_KEYDOWN, vk, 1 | (scan << 16));
        if (ch)
            PostMessageA(wnd, WM_CHAR, ch, 1 | (scan << 16));
        PostMessageA(wnd, WM_KEYUP, vk, 1 | (scan << 16) | 0xC0000000);
        return "posted";
    }
    if (op == "postkey") // postkey <vk> <0|1>: one real WM_KEYDOWN / WM_KEYUP (a modifier held across another key's post)
    {
        const u32 vk = num();
        const bool down = num(1) != 0;
        const UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
        PostMessageA(fl::MainWindow(), down ? WM_KEYDOWN : WM_KEYUP, vk,
                     1 | (scan << 16) | (down ? 0 : 0xC0000000));
        return down ? "posted down" : "posted up";
    }
    if (op == "type") // type <text...>  (rest of line, sent as WM_CHAR)
    {
        std::string rest;
        std::getline(in, rest);
        if (!rest.empty() && rest[0] == ' ')
            rest.erase(0, 1);
        for (char c : rest)
            fl::SendChar(static_cast<wchar_t>(static_cast<unsigned char>(c)));
        return "typed " + std::to_string(rest.size()) + " chars";
    }
    if (op == "held")
    {
        std::string name;
        in >> name;
        int cmd = fl::CmdFromName(name.c_str());
        if (cmd < 0)
            return "unknown command";
        fl::SetHeld(cmd, num(1) != 0);
        return "ok";
    }
    if (op == "trace")
    {
        const bool on = num(1) != 0;
        g_host->SetKeyTrace(on);
        return on ? "key trace on" : "key trace off";
    }
    if (op == "mouse") // mouse <x> <y> | mouse release
    {
        std::string t;
        in >> t;
        if (t == "release")
        {
            dimouse::ReleaseTarget();
            return "released";
        }
        float x = static_cast<float>(atof(t.c_str()));
        float y = static_cast<float>(num());
        dimouse::MoveTo(x, y);
        return "ok";
    }
    if (op == "mbtn") // mbtn <index> <0/1>
    {
        u32 i = num(), d = num(1);
        dimouse::SetButton(static_cast<int>(i), d != 0);
        return "ok";
    }
    if (op == "wheel")
    {
        dimouse::Wheel(static_cast<int>(num()));
        return "ok";
    }
    if (op == "dumpui")
        return DumpUi();
    if (op == "keymap") // dump Freelancer's loaded key command list
    {
        const u32 sentinel = Mem<u32>(exe::KeyCommands);
        u32 node = 0;
        SafeRead(sentinel, &node, 4);
        int count = 0;
        while (node && node != sentinel && count++ < 400)
        {
            u32 e[8] = {};
            SafeRead(node + 8, e, sizeof(e));
            char tmp[256];
            sprintf_s(tmp, "%-34s a1=%08X a2=%08X a3=%08X a4=%08X keys:", fl::CmdName(static_cast<int>(e[0])), e[1],
                      e[2], e[3], e[4]);
            out << tmp;
            for (u32 k = e[5]; k && k < e[6] && k < e[5] + 8 * 16; k += 8)
            {
                i32 key = 0;
                u32 mods = 0;
                SafeRead(k, &key, 4);
                SafeRead(k + 4, &mods, 4);
                sprintf_s(tmp, " %d/%X", key, mods);
                out << tmp;
            }
            out << "\n";
            u32 next = 0;
            if (!SafeRead(node, &next, 4))
                break;
            node = next;
        }
        return out.str();
    }
    if (op == "probe") // probe <step px>: discover controls via Freelancer's hit test
    {
        const float step = static_cast<float>(num(60));
        LARGE_INTEGER f, t0, t1;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&t0);
        std::vector<ui::Item> items = ui::Probe(step);
        QueryPerformanceCounter(&t1);
        out << "probe step=" << step << " took " << (t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart << " ms, "
            << items.size() << " controls\n";
        for (const ui::Item& it : items)
        {
            u32 vt = 0;
            SafeRead(it.ctl, &vt, 4);
            char tmp[200];
            sprintf_s(tmp, "%-26s ctl=%08X vt=%08X root=%08X center=(%.0f,%.0f) half=(%.0f,%.0f)\n", it.name,
                      static_cast<u32>(it.ctl), vt, static_cast<u32>(it.root), it.x, it.y, it.hw, it.hh);
            out << tmp;
        }
        return out.str();
    }
    if (op == "targetinfo") // the current target: space object id, position, attitude towards the player
    {
        fl::Kinematics k = {};
        float att = 0;
        const uintptr_t handle = fl::TargetHandle();
        if (!handle)
            return "no target";
        const u32 id = fl::TargetId();
        const bool hasPos = fl::TargetKinematics(k);
        const bool hasAtt = fl::TargetAttitude(att);
        float radius = 0;
        fl::TargetRadius(radius);
        char msg[220];
        sprintf_s(msg, "target handle=%08X id=%u pos=%s(%.0f,%.0f,%.0f) radius=%.0f attitude=%s",
                  static_cast<u32>(handle), id, hasPos ? "" : "?", k.pos[0], k.pos[1], k.pos[2], radius,
                  hasAtt ? std::to_string(att).c_str() : "?");
        return msg;
    }
    if (op == "destroytarget") // test aid: the current target explodes (pub::SpaceObj::Destroy, core/game/menu.cpp)
    {
        const u32 id = fl::TargetId();
        return id ? menu::DestroyObject(id) : "no target";
    }
    {
        // click, hotspots, goto, talkers, talk: shared with every module (core/game/room.cpp)
        std::string rest;
        std::getline(in >> std::ws, rest);
        std::string reply;
        if (room::Command(op, rest, &reply))
            return reply;
        if (space::Command(op, rest, &reply, events::IdsText)) // contacts, settarget (core/game/space.cpp)
            return reply;
        if (ai::Command(op, rest, &reply)) // ai ... : the game's AI on the player ship (core/game/ai.cpp)
            return reply;
        if (spawn::Command(op, rest, &reply)) // spawn ... : NPCs from npcships.ini (core/game/spawn.cpp)
            return reply;
        if (route::Command(op, rest, &reply)) // route, lanelaw: travel planning (core/game/route.cpp)
            return reply;
        // the shared commands took the rest of the line: rewind it for the commands below (`set`, `call` got an empty
        // line before, 19 Oct 2026)
        in.clear();
        in.str(line);
        in >> op;
    }
    if (op == "ctlrect") // ctlrect <name>: one visible control's projected rectangle, whatever its class (ui::ControlRect)
    {
        std::string name;
        std::getline(in, name);
        name.erase(0, name.find_first_not_of(' '));
        const u32 ctl = uiclick::FindVisible(name.c_str());
        ui::Item it;
        if (!ctl)
            return "no visible control " + name;
        if (!ui::ControlRect(ctl, it))
            return "no rectangle for " + name;
        char tmp[200];
        sprintf_s(tmp, "%s ctl=%08X root=%08X center=(%.1f,%.1f) half=(%.1f,%.1f) depth=%.3f left=%.1f top=%.1f right=%.1f bottom=%.1f",
                  it.name, ctl, static_cast<u32>(it.root), it.x, it.y, it.hw, it.hh, it.depth, it.x - it.hw, it.y - it.hh,
                  it.x + it.hw, it.y + it.hh);
        return tmp;
    }
    if (op == "hit") // hit <x> <y>: the control the game's own hit test finds there (answers out of focus too)
    {
        std::string sx, sy;
        in >> sx >> sy;
        const float x = static_cast<float>(atof(sx.c_str())), y = static_cast<float>(atof(sy.c_str()));
        const uintptr_t c = ui::HitTest(x, y);
        char tmp[160];
        sprintf_s(tmp, "hit (%.0f,%.0f) = %08X %s", x, y, static_cast<u32>(c), c ? ui::ControlName(c) : "-");
        return tmp;
    }
    if (op == "uiitems")
    {
        std::vector<ui::Item> items = ui::CollectAll(); // what D-pad navigation sees
        const uintptr_t hov = ui::Hovered();
        out << "hovered=" << ui::ControlName(hov) << " (" << std::hex << hov << std::dec << ") items=" << items.size()
            << "\n";
        for (const ui::Item& it : items)
        {
            char tmp[200];
            u32 vt = 0;
            SafeRead(it.ctl, &vt, 4);
            sprintf_s(tmp, "%-26s ctl=%08X vt=%08X root=%08X center=(%.0f,%.0f) half=(%.0f,%.0f) depth=%.3f\n", it.name,
                      static_cast<u32>(it.ctl), vt, static_cast<u32>(it.root), it.x, it.y, it.hw, it.hh, it.depth);
            out << tmp;
        }
        return out.str();
    }
    // ---- the HUD track's layer (padhud.cpp): padhud on|off|state; device = the input-device service's line
    if (op == "padhud")
        return padhud::Command(in);
    // ---- HUD 2 (the HUD2 track): hud2 on|off|auto|state|sim <combat|travel|cruise|dock|lane|off>
    if (op == "hud2")
        return hud2::Command(in);
    // ---- FLUF awareness (flufaware.h): fluf = the build, status, policy, text input / mouse, every open keep-out
    if (op == "fluf")
        return flufaware::Describe();
    // ---- Extended HUD 2 (the SKIN track): uiskin state | windows | restore | rescan
    if (op == "uiskin")
        return uiskin::Command(in);
    if (op == "uistrip") // Extended HUD 2's docked overlay (the STRIP track): uistrip [state] | data | research ... | hover <i>
        return uistrip::Command(in);
    if (op == "device")
        return device::Status();
    // ---- D-MENUS (25 Oct 2026): the menu screens on the pad, the OSK, the New Game prompt, speedrun mode, the timer
    if (op == "menus")
        return padmenus::Command(in);
    if (op == "osk")
        return osk::Command(in);
    if (op == "newgame")
        return newgame::Command(in);
    if (op == "livesplit")
        return livesplit::Command(in);
    if (op == "timer")
        return runtimer::Command(in);
    // ---- the phase D flight modals (D-FLIGHT track): picker ..., awheel ... (`wheel` is the mouse wheel above),
    // goals ... (each prints its rows)
    if (op == "picker")
        return picker::Command(in);
    if (op == "awheel")
        return wheel::Command(in);
    if (op == "goals")
        return goals::Command(in);
    // ---- the phase D base screens and the nav map (D-BASES track): bases state|rooms|chars, map state|dest
    if (op == "bases")
        return padbases::Command(in);
    if (op == "map")
        return padmap::Command(in);
    if (op == "call") // call <addr> [args...] (cdecl)
    {
        u32 fn = num();
        u32 args[4] = {};
        int n = 0;
        std::string t;
        while (n < 4 && (in >> t))
            args[n++] = static_cast<u32>(strtoul(t.c_str(), nullptr, 0));
        bool ok;
        u32 r = SafeCall(fn, args, n, ok);
        char tmp[64];
        sprintf_s(tmp, ok ? "eax=0x%08X" : "exception", r);
        return tmp;
    }
    if (op == "set")
        return ctl::SetOption(in);
    return "unknown op: " + op;
}

} // namespace debug
