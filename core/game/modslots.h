// FLvibed - the cut modules' places in the logic (docs\MODULES_PLAN.md: a module is a namespace, a folder and a switch). The frame calls
// a slot at the very place it called the module before the cut, so the order is unchanged; a module that is off is an empty slot.
// A module exports one table; core\logic\modules.cpp fills the slots from the [Modules] rows when the logic starts.
// A fault in a module stops that module alone: from its Init on every way in, slot or view, is guarded (`Guard` below), and a stopped module is an empty slot and
// an empty view, so the rest of the frame behaves as with that module off. Its Attach only publishes its view, and runs bare.
#pragma once
#include "common.h"

#include <istream>
#include <string>
#include <type_traits>

namespace modslots
{
enum Slot : int
{
    kNpc,
    kSpeedrun,
    kAutopilot,
    kHud,
    kSlots
};

struct Module
{
    void (*Init)();
    void (*Shutdown)();
    void (*Frame)(float dt, const char* mode, bool paused);
    void (*Draw)();
    // Its debug verbs: true when `op` is its own, with the reply.
    bool (*Command)(const std::string& op, std::istream& in, std::string& reply);
    void (*NewGame)(bool speedrun); // the New Game click of a session
    void (*Attach)();               // at Start, before the entry's settings load: its views
    void (*Detach)();               // at Stop
    // After a fault in it or in its entry, once, at the top of the next frame: what it changed in the game goes back. Its state may be corrupt: no drawing, no
    // texture work. Phase 0 touches no heap, phase 1 may.
    void (*Fault)(int phase);
    bool (*Key)(int vk, bool down); // a raw key for a screen of its own over the game's; true: the game never sees it
};

inline const Module* g[kSlots] = {};
inline const Module* started[kSlots] = {}; // what Start filled: Shutdown and Stop reach a stopped module too
inline constexpr const char* kNames[kSlots] = {"npc", "speedrun", "autopilot", "hud"};
inline unsigned g_fault[kSlots] = {}; // the exception that stopped it, 0 = runs
inline int g_testFault[kSlots] = {};  // debug verb `fault <module> frame|draw`: its FaultPoint with that number faults

// The recorder's setting (`npc_stall_log`): its row is still in the controller's registry, whose section holds it.
inline bool g_npcLog = true;

// The fault's handler, out of the module's stack: the slot and its views empty at once, the FATAL line, the give-back due at the next frame's top.
void Stopped(int slot, unsigned code);
// At the top of the logic's frame: what a stopped module changed in the game goes back. One compare when nothing stopped.
void GiveBack();
// `fault <module> frame|draw`, `fault status`; the reply. False when `what` names no module.
bool FaultVerb(const std::string& what, const std::string& where, std::string& reply);
// A place in a module where the test verb breaks it on purpose: 1 its frame, 2 its draw.
inline void FaultPoint(int slot, int where)
{
    if (g_testFault[slot] != where)
        return;
    g_testFault[slot] = 0;
    *static_cast<volatile int*>(nullptr) = 0;
}

inline void InitOf(int slot)
{
    unsigned code = 0;
    __try
    {
        g[slot]->Init();
    }
    __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        Stopped(slot, code);
    }
}
inline void Init()
{
    for (int s = 0; s < kSlots; ++s)
        if (g[s] && g[s]->Init)
            InitOf(s);
}
// One step of a fault's give-back: a second fault loses that step, not the rest.
inline void Try(void (*step)(int), int phase)
{
    __try
    {
        step(phase);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}
inline void TryCall(void (*step)())
{
    __try
    {
        step();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}
inline void Shutdown()
{
    for (int s = 0; s < kSlots; ++s)
        if (started[s] && started[s]->Shutdown)
        {
            if (g_fault[s])
                TryCall(started[s]->Shutdown); // (its state may be corrupt: a reload after its fault must still go on)
            else
                started[s]->Shutdown();
        }
}
inline void Frame(int slot, float dt, const char* mode, bool paused)
{
    const Module* m = g[slot];
    if (!m || !m->Frame)
        return;
    unsigned code = 0;
    __try
    {
        m->Frame(dt, mode, paused);
    }
    __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        Stopped(slot, code);
    }
}
inline void Draw(int slot)
{
    const Module* m = g[slot];
    if (!m || !m->Draw)
        return;
    unsigned code = 0;
    __try
    {
        m->Draw();
    }
    __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        Stopped(slot, code);
    }
}
inline void NewGameOf(int slot, bool speedrun)
{
    unsigned code = 0;
    __try
    {
        g[slot]->NewGame(speedrun);
    }
    __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        Stopped(slot, code);
    }
}
inline void NewGame(bool speedrun)
{
    for (int s = 0; s < kSlots; ++s)
        if (g[s] && g[s]->NewGame)
            NewGameOf(s, speedrun);
}
inline bool KeyOf(int slot, int vk, bool down)
{
    unsigned code = 0;
    __try
    {
        return g[slot]->Key(vk, down);
    }
    __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        Stopped(slot, code);
        return false;
    }
}
inline bool Key(int vk, bool down)
{
    for (int s = 0; s < kSlots; ++s)
        if (g[s] && g[s]->Key && KeyOf(s, vk, down))
            return true;
    return false;
}
inline bool CommandOf(int slot, const std::string& op, std::istream& in, std::string& reply)
{
    unsigned code = 0;
    __try
    {
        return g[slot]->Command(op, in, reply);
    }
    __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        Stopped(slot, code);
        return false;
    }
}
inline bool Command(const std::string& op, std::istream& in, std::string& reply)
{
    for (int s = 0; s < kSlots; ++s)
        if (g[s] && g[s]->Command && CommandOf(s, op, in, reply))
            return true;
    return false;
}

// A module's view publishes this in place of the function: `Guard<kHud, hud2::Update>::Call`. A stopped module answers as an absent one: the value's default.
template <int Slot, auto Fn> struct Guard;
template <int Slot, class R, class... A, R (*Fn)(A...)> struct Guard<Slot, Fn>
{
    static void Run(A... a)
    {
        unsigned code = 0;
        __try
        {
            Fn(a...);
        }
        __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
        {
            Stopped(Slot, code);
        }
    }
    static void Keep(R* out, A... a) { *out = Fn(a...); } // (apart: a value with a destructor may not meet __try)
    static void RunTo(R* out, A... a)
    {
        unsigned code = 0;
        __try
        {
            Keep(out, a...);
        }
        __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
        {
            Stopped(Slot, code);
        }
    }
    static R Call(A... a)
    {
        if constexpr (std::is_void_v<R>)
        {
            if (g[Slot])
                Run(a...);
        }
        else
        {
            R out{};
            if (g[Slot])
                RunTo(&out, a...);
            return out;
        }
    }
};

// Fills and clears the slots, from whichever entry runs (the controller's, or core's without it). Init and Shutdown above run in that
// entry's own order, after the settings load. A stopped module is detached with the rest.
void Start();
void Stop();
} // namespace modslots
