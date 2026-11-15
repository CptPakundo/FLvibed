// The model slot (modelslot.h): the ship view's own draw (0x584470: lights, rectangle, field of view, viewport, Z clear,
// projection, Z states, the renderer's draw, then everything back) with the slot's mesh holders, one camera and three lights,
// as the model probe proved it (audit 48_; docs\FREELANCER_RE.md "A model drawn with no game control"). Every call is a signed
// symbol (core\flexe.h, the "R3b model probe" rows); an unresolved one leaves every slot Pending and draws nothing.
#include "modelslot.h"
#include "common.h"
#include "flexe.h"
#include "game/fl.h"
#include "game/hostcalls.h"
#include "game/render.h"
#include "log.h"

namespace modelslot
{
namespace
{
constexpr int kHolder = 0x40; // the game's holder is 0x28 bytes (ShipView +0x330..+0x358)
constexpr int kCamera = 0x100; // the camera object reaches +0xB8 (ctor 0x40F150; the ship view reserves 0xC4)
constexpr int kLight = 0xC8;  // a light object (the ship view's operator new of 0xC8)

alignas(16) unsigned char g_holder[kSlots][kHolder];
alignas(16) unsigned char g_camera[kCamera];
alignas(16) unsigned char g_light[3][kLight]; // never deleted: the game's deleting destructor would free them to its heap
void* g_lightList[3] = {};
bool g_cameraReady = false, g_lightsReady = false;
bool g_broken = false; // a symbol, the camera, the lights or the state's restore failed: nothing is drawn again until a reload

Table g_table;
World g_world;
bool g_worldKnown = false;
double g_lastPass = 0;

// per slot, from its load
int g_handle[kSlots];
double g_loadMs[kSlots];
float g_boundsA[kSlots][4], g_boundsB[kSlots][4], g_distance[kSlots];
bool g_drawnLast[kSlots]; // drawn at the last pass (Status Ready against Hidden)

Stats g_stats;
double g_usSum = 0;

bool Symbols()
{
    return exe::MeshInit.ok() && exe::MeshLoad.ok() && exe::MeshFree.ok() && exe::CamInit.ok() && exe::CamSetFov.ok() &&
           exe::CamSetRect.ok() && exe::CamSetClip.ok() && exe::CamViewport.ok() && exe::CamProject.ok() &&
           exe::RenderZRestore.ok() && exe::LightsClear.ok() && exe::LightAmbient.ok() && exe::LightsSet.ok() &&
           exe::LightInit.ok() && exe::LightSetType.ok() && exe::LightSetDir.ok() && exe::LightSetColor.ok() &&
           exe::EngineIface.ok() && exe::PipelineIface.ok() && exe::RendererIface.ok() && exe::RenderGetInt.ok() &&
           exe::RenderSetCamera.ok() && exe::RenderSetViewport.ok() && exe::UiCamera.ok();
}

// An engine interface: the import slot holds the address of DALib's variable, which holds the interface.
void* Iface(hk::Sym& s)
{
    void** slot = reinterpret_cast<void**>(s.get());
    void** var = slot ? reinterpret_cast<void**>(*slot) : nullptr;
    return var ? *var : nullptr;
}

// ---- the engine calls, each in a small guarded helper (no C++ objects under __try)

bool LoadOne(unsigned char* holder, const char* file, int* handle)
{
    __try
    {
        reinterpret_cast<void(__fastcall*)(void*)>(exe::MeshInit.get())(holder);
        *reinterpret_cast<int*>(holder + 0x10) = -1; // the anim handle (the ship view's ctor)
        for (int i = 0; i < 5; ++i)
            reinterpret_cast<float*>(holder + 0x14)[i] = 1.0f; // the tint (0x492250 fills it while loading)
        const bool ok = reinterpret_cast<bool(__thiscall*)(void*, const char*, const char*, char)>(exe::MeshLoad.get())(
            holder, file, nullptr, 0);
        *handle = *reinterpret_cast<int*>(holder + 4);
        return ok && *handle != -1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// 0x59EEE0 frees the anim handle and the instance only when set, and clears both: a second free is harmless (FREELANCER_RE.md).
bool FreeOne(unsigned char* holder)
{
    __try
    {
        reinterpret_cast<void(__fastcall*)(void*)>(exe::MeshFree.get())(holder);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool Bounds(void* engine, int handle, float* a, float* b)
{
    __try
    {
        VCall<int>(engine, 0x88 / 4, handle, 0, a, b);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool SetupCamera(unsigned char* cam)
{
    __try
    {
        reinterpret_cast<void(__fastcall*)(void*)>(exe::CamInit.get())(cam);
        reinterpret_cast<void(__thiscall*)(void*, float, float)>(exe::CamSetClip.get())(cam, 0.1f, 10000.0f); // the ship view's 0x586310
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The ship view's three directional lights (its ctor 0x5836A0): white from (1, 1, -1), blue-grey from the other two.
bool SetupLights()
{
    static const float kDir[3][3] = {{1, 1, -1}, {-0.965f, 0.259f, 0.965f}, {-1, -1, 1}};
    static const unsigned char kColor[3][4] = {{255, 255, 255, 255}, {50, 70, 100, 255}, {50, 70, 100, 255}};
    __try
    {
        for (int i = 0; i < 3; ++i)
        {
            void* l = g_light[i];
            reinterpret_cast<void(__thiscall*)(void*, int)>(exe::LightInit.get())(l, -1);
            reinterpret_cast<void(__thiscall*)(void*, int, int)>(exe::LightSetType.get())(l, 3, 0);
            const float* d = kDir[i];
            const float n = 1.0f / sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            const float dir[3] = {-d[0] * n, -d[1] * n, -d[2] * n}; // the ctor stores the negated unit vector
            reinterpret_cast<void(__thiscall*)(void*, const float*)>(exe::LightSetDir.get())(l, dir);
            reinterpret_cast<void(__thiscall*)(void*, const unsigned char*)>(exe::LightSetColor.get())(l, kColor[i]);
            void** vt = *reinterpret_cast<void***>(l);
            reinterpret_cast<void(__thiscall*)(void*)>(vt[1])(l); // what the ctor calls last (its update)
            g_lightList[i] = l;
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The lights' destructor (vt[0] 0x410BA0; flag 0 = no free: they are static) takes each out of the Pipeline's own list,
// which holds them by address: before the logic DLL that holds them is unloaded.
bool DropLights()
{
    __try
    {
        reinterpret_cast<void(__cdecl*)()>(exe::LightsClear.get())();
        for (int i = 0; i < 3; ++i)
        {
            void** vt = *reinterpret_cast<void***>(g_light[i]);
            reinterpret_cast<void(__thiscall*)(void*, int)>(vt[0])(g_light[i], 0);
            g_lightList[i] = nullptr;
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// One model in its rectangle: placed (xf: modelslot_rules.h Place), the lights, the camera on the rectangle and its field of
// view, its viewport, the rectangle's depth cleared, the projection, the Z states, the renderer's draw, the Z states back.
// The rectangle goes BEFORE the field of view (the ship view's order is the other way, for a rectangle that never changes):
// CamSetRect keeps the horizontal angle and CamSetFov sets the vertical one from the rectangle's aspect, so in this order the
// field of view holds for every rectangle whatever the one before was (for squares both orders give the same camera).
bool DrawOne(void* engine, void* pipe, void* rend, unsigned char* cam, int handle, const int rect[4], const float xf[12], float fov,
             unsigned flags, unsigned char ambient)
{
    __try
    {
        VCall<int>(engine, 0x9C / 4, handle, xf);
        VCall<int>(engine, 0x6C / 4, handle, 0, 0);
        hk::Call<void(__cdecl*)()>(exe::LightsClear);
        const unsigned char amb[3] = {ambient, ambient, ambient};
        hk::Call<void(__cdecl*)(const unsigned char*)>(exe::LightAmbient, amb);
        hk::Call<void(__cdecl*)(void**, unsigned, void*, int)>(exe::LightsSet, g_lightList, 3u, cam, 0);
        reinterpret_cast<void(__thiscall*)(void*, const int*, int)>(exe::CamSetRect.get())(cam, rect, 1);
        reinterpret_cast<void(__thiscall*)(void*, float)>(exe::CamSetFov.get())(cam, fov);
        hk::Call<void(__cdecl*)(void*, const char*, int)>(exe::CamViewport, cam, "modelslot", 0);
        const int* r = VCall<const int*>(cam, 0x14 / 4);
        const int local[4] = {0, 0, r[2] - r[0] - 1, r[3] - r[1] - 1};
        VCall<int>(pipe, 0x3C / 4, 2, local); // the rectangle's depth
        hk::Call<void(__cdecl*)(void*)>(exe::CamProject, cam);
        VCall<int>(pipe, 0xF0 / 4, 7, 1);    // Z on
        VCall<int>(pipe, 0xF0 / 4, 0x17, 4); // less or equal
        VCall<int>(pipe, 0xF0 / 4, 0xE, 1);  // Z written
        VCall<int>(rend, 0x1C / 4, cam, handle, 1.0f, flags, 0, 0);
        hk::Call<void(__cdecl*)(int)>(exe::RenderZRestore, 0xB71);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The game's state after a view's draw (0x584470's tail): Z states, the UI camera, the viewport, the lights.
bool Restore(void* pipe, const int vp[4])
{
    __try
    {
        hk::Call<void(__cdecl*)()>(exe::LightsClear); // first: a fault below must not leave the list on the slot's lights
        VCall<int>(pipe, 0xF0 / 4, 7, 0);
        VCall<int>(pipe, 0xF0 / 4, 0x17, 8);
        VCall<int>(pipe, 0xF0 / 4, 0xE, 0);
        hk::Call<void(__cdecl*)(int)>(exe::RenderZRestore, 0xB71);
        hk::Call<void(__cdecl*)(uintptr_t)>(exe::RenderSetCamera, exe::UiCamera.get());
        hk::Call<void(__cdecl*)(int, int, int, int, const char*, int)>(exe::RenderSetViewport, vp[0], vp[1], vp[2], vp[3],
                                                                             "modelslot", 0);
        hk::Call<void(__cdecl*)()>(exe::LightsClear);
        const unsigned char white[3] = {255, 255, 255};
        hk::Call<void(__cdecl*)(const unsigned char*)>(exe::LightAmbient, white);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// ---- the table's acts

void FreeSlot(int i)
{
    if (g_table.slots[i].held && !FreeOne(g_holder[i]))
        ++g_stats.faults, g_stats.why = "free faulted";
    g_table.Freed(i);
    g_handle[i] = -1, g_drawnLast[i] = false;
}

void Load(int i, void* engine)
{
    if (g_table.slots[i].held)
        FreeSlot(i);
    const double t0 = perf::Now();
    int h = -1;
    const bool ok = LoadOne(g_holder[i], g_table.slots[i].ask.file, &h);
    if (!ok)
    {
        FreeOne(g_holder[i]); // what a failed load left (its anim handle)
        g_stats.why = std::string("load failed: ") + g_table.slots[i].ask.file;
        Log("[MODELSLOT] load failed: %s", g_table.slots[i].ask.file);
    }
    g_table.Loaded(i, ok);
    g_handle[i] = ok ? h : -1;
    g_loadMs[i] = (perf::Now() - t0) * 1000.0;
    memset(g_boundsA[i], 0, sizeof(g_boundsA[i])), memset(g_boundsB[i], 0, sizeof(g_boundsB[i]));
    if (ok && !Bounds(engine, h, g_boundsA[i], g_boundsB[i]))
        ++g_stats.faults, g_stats.why = "bounds faulted";
}

// Every holder freed, the asks kept (each loads again at its next ask).
void FreeHeld(const char* why)
{
    const int held = g_table.Held();
    for (int i = 0; i < kSlots; ++i)
        if (g_table.slots[i].held)
            FreeSlot(i);
    g_table.Forget(); // a failed load is tried again in the new world too
    if (held)
    {
        ++g_stats.releases, g_stats.lastWhy = why;
        Log("[MODELSLOT] freed %d holder(s): %s (loads %u, frees %u)", held, why, g_table.loads, g_table.frees);
    }
}

World ReadWorld(double now)
{
    World w;
    w.loads = hostcalls::g.GameLoads ? hostcalls::g.GameLoads() : 0;
    w.base = fl::PlayerBase();
    w.system = fl::PlayerSystem();
    w.resets = render::DeviceResets();
    render::ScreenSize(w.w, w.h);
    w.at = now;
    return w;
}

void Draw(double now, float dt)
{
    void* engine = Iface(exe::EngineIface);
    void* pipe = Iface(exe::PipelineIface);
    void* rend = Iface(exe::RendererIface);
    if (!engine || !pipe || !rend)
        return; // (no engine yet: the asks wait)
    if (!g_cameraReady && !(g_cameraReady = SetupCamera(g_camera)))
    {
        ++g_stats.faults, g_stats.why = "camera faulted", g_broken = true;
        return;
    }
    if (!g_lightsReady && !(g_lightsReady = SetupLights()))
    {
        ++g_stats.faults, g_stats.why = "lights faulted", g_broken = true;
        return;
    }
    int vp[4] = {};
    hk::Call<void(__cdecl*)(int, int*)>(exe::RenderGetInt, 0xBA2, vp);
    if (vp[2] <= 0 || vp[3] <= 0)
        return;
    const Rect viewport{vp[0], vp[1], vp[2], vp[3]};
    perf::Scope ps("hud.modelslot");
    const double t0 = perf::Now();
    int drawn = 0;
    bool any = false;
    for (int i = 0; i < kSlots; ++i)
        g_drawnLast[i] = false;
    for (int i = 0; i < kSlots; ++i)
    {
        const Act act = g_table.Next(i, now);
        if (act == Act::None || act == Act::Release)
            continue; // (released before the draw: Pass)
        if (act == Act::Load)
        {
            Load(i, engine);
            if (g_table.slots[i].state != State::Ready)
                continue;
        }
        Slot& s = g_table.slots[i];
        s.turned += s.ask.spin * dt;
        if (!Drawable(s.ask.rect, viewport, s.ask.clip))
            continue; // a rectangle off the screen or out of its clip: Hidden (the engine would keep the last viewport)
        int rect[4];
        Corners(s.ask.rect, rect);
        g_distance[i] = Distance(g_boundsA[i][0], s.ask.fill, s.ask.fov, static_cast<float>(s.ask.rect.w) / static_cast<float>(s.ask.rect.h));
        float xf[12];
        Place(s.ask.yaw + s.turned, s.ask.pitch, g_boundsB[i], g_distance[i], xf);
        any = true;
        const int amb = s.ask.ambient < 0 ? 0 : s.ask.ambient > 255 ? 255 : s.ask.ambient;
        if (!DrawOne(engine, pipe, rend, g_camera, g_handle[i], rect, xf, s.ask.fov, s.ask.flags, static_cast<unsigned char>(amb)))
        {
            ++g_stats.faults, g_stats.why = std::string("draw faulted: ") + s.ask.file;
            g_table.DrawFailed(i);
            break; // the state goes back below
        }
        g_drawnLast[i] = true, ++drawn;
    }
    g_stats.drawn = drawn;
    if (!any)
        return;
    if (!Restore(pipe, vp))
    {
        ++g_stats.faults, g_stats.why = "restore faulted", g_broken = true;
        Log("[MODELSLOT] the game's state did not restore: the slots stop until a reload");
        return;
    }
    const double us = (perf::Now() - t0) * 1e6;
    ++g_stats.frames, g_usSum += us, g_stats.usLast = us, g_stats.usAvg = g_usSum / g_stats.frames;
    if (us > g_stats.usMax)
        g_stats.usMax = us;
    memcpy(g_stats.vpBefore, vp, sizeof(vp));
    hk::Call<void(__cdecl*)(int, int*)>(exe::RenderGetInt, 0xBA2, g_stats.vpAfter);
    render::Viewport(g_stats.d3dAfter[0], g_stats.d3dAfter[1], g_stats.d3dAfter[2], g_stats.d3dAfter[3]);
    g_stats.vpSame = g_stats.vpSame && !memcmp(g_stats.vpAfter, vp, sizeof(vp));
}
} // namespace

bool Show(unsigned key, const Model& m)
{
    if (!m.file || !m.file[0])
        return false;
    Ask a;
    strncpy_s(a.file, m.file, _TRUNCATE);
    a.rect = m.rect, a.clip = m.clip;
    a.yaw = m.yaw, a.pitch = m.pitch, a.spin = m.spin, a.fill = m.fill, a.fov = m.fov, a.flags = m.flags, a.ambient = m.ambient;
    if (g_table.Show(key, a, perf::Now()) >= 0)
        return true;
    g_stats.why = "every place taken";
    return false;
}

void Drop(unsigned key) { g_table.Drop(key); }

Status StatusOf(unsigned key)
{
    const int i = g_table.Find(key);
    if (i < 0)
        return Status::None;
    switch (g_table.slots[i].state)
    {
    case State::Ready: return g_drawnLast[i] ? Status::Ready : Status::Hidden;
    case State::Failed: return Status::Failed;
    default: return Status::Pending;
    }
}

void Pass()
{
    if (!g_table.Used() && !g_table.Held())
    {
        g_worldKnown = false, g_stats.drawn = 0; // nothing asked: nothing to check, no cost
        return;
    }
    g_stats.drawn = 0;
    g_table.BeginPass();
    ++g_stats.passes;
    const double now = perf::Now();
    const float dt = g_lastPass > 0 && now - g_lastPass < 0.5 ? static_cast<float>(now - g_lastPass) : 0.0f;
    g_lastPass = now;
    // the keys no page asks for any more give their holder and their place back, whatever else this pass can do
    for (int i = 0; i < kSlots; ++i)
        if (g_table.Next(i, now) == Act::Release)
        {
            FreeSlot(i);
            g_table.Released(i);
        }
    // the world's change first: even a broken slot gives its holders back with the scene they were asked in
    const World w = ReadWorld(now);
    const Why why = g_worldKnown ? Changed(g_world, w) : Why::Same;
    g_world = w, g_worldKnown = true;
    if (why != Why::Same)
        FreeHeld(WhyName(why));
    g_stats.symbols = Symbols();
    if (g_broken || !g_stats.symbols)
    {
        if (!g_broken)
            g_stats.why = "a symbol did not resolve";
        return; // every ask stays Pending: the pages keep their flat fallback
    }
    Draw(now, dt);
    g_stats.used = g_table.Used(), g_stats.held = g_table.Held();
    g_stats.loads = g_table.loads, g_stats.frees = g_table.frees, g_stats.failures = g_table.failures, g_stats.refused = g_table.refused;
}

void Release(const char* why)
{
    FreeHeld(why);
    g_worldKnown = false;
    g_stats.held = g_table.Held(), g_stats.frees = g_table.frees;
}

void Shutdown()
{
    FreeHeld("shutdown");
    for (int i = 0; i < kSlots; ++i)
        g_table.Released(i);
    if (g_lightsReady)
    {
        DropLights();
        g_lightsReady = false;
    }
    g_worldKnown = false;
}

const Stats& GetStats()
{
    g_stats.symbols = Symbols();
    g_stats.used = g_table.Used(), g_stats.held = g_table.Held();
    g_stats.loads = g_table.loads, g_stats.frees = g_table.frees, g_stats.failures = g_table.failures, g_stats.refused = g_table.refused;
    return g_stats;
}

void ResetCost()
{
    g_stats.frames = 0, g_usSum = 0, g_stats.usLast = g_stats.usAvg = g_stats.usMax = 0, g_stats.vpSame = true;
}

bool SlotInfo(unsigned key, int* handle, double* loadMs, float boundsA[4], float boundsB[4], float* distance)
{
    const int i = g_table.Find(key);
    if (i < 0)
        return false;
    *handle = g_table.slots[i].held ? g_handle[i] : -1;
    *loadMs = g_loadMs[i];
    memcpy(boundsA, g_boundsA[i], sizeof(g_boundsA[i]));
    memcpy(boundsB, g_boundsB[i], sizeof(g_boundsB[i]));
    *distance = g_distance[i];
    return true;
}
} // namespace modelslot
