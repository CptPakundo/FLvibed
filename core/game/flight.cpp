#include "game/flight.h"

#include "fldll.h"
#include "flexe.h"
#include "game/space.h"

#include <cmath>
#include <sstream>

namespace flight
{
// ------------------------------------------------------------------ closed-form motion (m dv/dt = F - d v)
float Thrust::SpeedAfter(float v0, float t) const
{
    if (!ok())
        return v0;
    return top() + (v0 - top()) * std::exp(-t / tau());
}

float Thrust::DistanceAfter(float v0, float t) const
{
    if (!ok())
        return v0 * t;
    return top() * t + (v0 - top()) * tau() * (1.0f - std::exp(-t / tau()));
}

float Thrust::TimeToSpeed(float v0, float v1) const
{
    if (!ok())
        return -1;
    const float a = v0 - top(), b = v1 - top();
    // v approaches the top speed from v0's side and never crosses it: v1 must lie between v0 and the top speed.
    if (a == 0 || b == 0 || (a > 0) != (b > 0) || std::fabs(b) > std::fabs(a))
        return std::fabs(a - b) < 1e-3f ? 0.0f : -1.0f;
    return tau() * std::log(a / b);
}

float Thrust::DistanceToSpeed(float v0, float v1) const
{
    const float t = TimeToSpeed(v0, v1);
    return t < 0 ? -1.0f : DistanceAfter(v0, t);
}

// ------------------------------------------------------------------ the game's numbers
enum ThrustType
{
    kNormal = 1,
    kThruster = 2,
    kCruise = 4,
};
constexpr int kEquipEngine = 1 << 17;   // EquipmentClass Engine
constexpr int kEquipThruster = 1 << 10; // EquipmentClass Thruster
constexpr uintptr_t kEqObjEquipManager = 0xE4;

struct Api
{
    void(__thiscall* forces)(const void* ship, float throttle, int type, float* force, float* drag) = nullptr;
    float(__cdecl* mass)(const void* obj) = nullptr;
    void*(__thiscall* equipFirst)(void* mgr, u32 cls) = nullptr;
    float(__thiscall* chargeTime)(const void* engine) = nullptr;
    float(__thiscall* chargeElapsed)(const void* engine) = nullptr;
    bool(__thiscall* cruiseEngaged)(const void* engine) = nullptr;
    // axis 0 / 1 / 2 (the archetype's angular drag vector component), input -1..1
    void(__thiscall* angForces)(const void* ship, u32 axis, float input, float* torque, float* drag) = nullptr;
    bool ok = false;
};

static const Api& GetApi()
{
    static Api a;
    static bool done = false;
    HMODULE m = GetModuleHandleA("common.dll");
    if (done || !m)
        return a;
    done = true;
    auto get = [m](const char* n) { return reinterpret_cast<void*>(GetProcAddress(m, n)); };
    a.forces = reinterpret_cast<decltype(a.forces)>(
        get("?collect_force_components@CShip@@ABEXMW4ThrustEquipType@IObject@@AAM1@Z"));
    a.mass = reinterpret_cast<decltype(a.mass)>(get("?GetMass@PhySys@@YAMPBUCObject@@@Z"));
    a.equipFirst = reinterpret_cast<decltype(a.equipFirst)>(get("?FindFirst@CEquipManager@@QAEPAVCEquip@@I@Z"));
    a.chargeTime = reinterpret_cast<decltype(a.chargeTime)>(get("?GetCruiseChargeTime@CEEngine@@QBEMXZ"));
    a.chargeElapsed = reinterpret_cast<decltype(a.chargeElapsed)>(get("?GetCruiseChargeTimeElapsed@CEEngine@@QBEMXZ"));
    a.cruiseEngaged = reinterpret_cast<decltype(a.cruiseEngaged)>(get("?IsCruiseEngaged@CEEngine@@QBE_NXZ"));
    a.angForces = reinterpret_cast<decltype(a.angForces)>(get("?collect_angular_force_components@CShip@@ABEXIMAAM0@Z"));
    a.ok = a.forces && a.mass && a.equipFirst;
    return a;
}

// The player's CShip, only if it is one (Common.dll's exported vtable).
static void* PlayerShip()
{
    void* iobj = hk::Call<void*(__cdecl*)()>(exe::GetPlayerIObj);
    if (!iobj)
        return nullptr;
    const auto cobject = dll::ObjCobject.get<dll::ObjCobjectFn>(iobj); // IObjInspect::cobject() by name
    if (!cobject)
        return nullptr;
    void* ship = nullptr;
    __try
    {
        ship = cobject(iobj);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
    return hk::IsExportedClass(ship, "common.dll", "??_7CShip@@6BCEqObj@@@") ? ship : nullptr;
}

static bool ReadThrust(const Api& a, const void* ship, float mass, float throttle, int type, Thrust& t)
{
    __try
    {
        a.forces(ship, throttle, type, &t.force, &t.drag);
        t.mass = mass;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void ReadEngine(const Api& a, void* ship, Model& m)
{
    __try
    {
        u8* mgr = static_cast<u8*>(ship) + kEqObjEquipManager;
        if (void* engine = a.equipFirst(mgr, kEquipEngine);
            engine && hk::IsExportedClass(engine, "common.dll", "??_7CEEngine@@6BCInternalEquip@@@"))
        {
            if (a.chargeTime)
                m.cruiseChargeTime = a.chargeTime(engine);
            if (a.chargeElapsed)
                m.cruiseChargeElapsed = a.chargeElapsed(engine);
            if (a.cruiseEngaged)
                m.cruiseEngaged = a.cruiseEngaged(engine);
        }
        m.hasBurner = a.equipFirst(mgr, kEquipThruster) != nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

// The slower of pitch and yaw at full input (axes 0 and 1; 2 is roll), rad/s; 0 if unreadable.
static float ReadTurnRate(const Api& a, const void* ship)
{
    if (!a.angForces)
        return 0;
    float rate = 0;
    __try
    {
        for (u32 axis = 0; axis < 2; ++axis)
        {
            float torque = 0, drag = 0;
            a.angForces(ship, axis, 1.0f, &torque, &drag);
            if (torque > 0 && drag > 0)
                rate = rate > 0 ? std::min(rate, torque / drag) : torque / drag;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
    return rate;
}

bool ReadModel(Model& m)
{
    m = Model();
    const Api& a = GetApi();
    void* ship = a.ok ? PlayerShip() : nullptr;
    if (!ship)
        return false;
    __try
    {
        m.mass = a.mass(ship);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    if (m.mass <= 0 || !ReadThrust(a, ship, m.mass, 1, kNormal, m.engine) ||
        !ReadThrust(a, ship, m.mass, -1, kNormal, m.reverse) || !ReadThrust(a, ship, m.mass, 1, kCruise, m.cruise))
        return false;
    ReadEngine(a, ship, m);
    m.turnRate = ReadTurnRate(a, ship);
    if (m.hasBurner)
        ReadThrust(a, ship, m.mass, 1, kThruster, m.burner);
    static const float* cruiseSpeed = reinterpret_cast<const float*>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?CRUISING_SPEED@EngineEquipConsts@@3MA"));
    static const float* cruiseAccelTime = reinterpret_cast<const float*>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?CRUISE_ACCEL_TIME@EngineEquipConsts@@3MA"));
    if (cruiseSpeed && cruiseAccelTime && *cruiseAccelTime > 0)
    {
        m.cruiseSpeed = *cruiseSpeed;
        m.cruiseAccel = *cruiseSpeed / *cruiseAccelTime;
    }
    m.ok = m.engine.ok() && m.reverse.ok();
    return m.ok;
}

float BrakeDistance(const Model& m, float v, float vArrive)
{
    if (v <= vArrive)
        return 0;
    const float d = m.reverse.DistanceToSpeed(v, vArrive);
    return d < 0 ? 0 : d;
}

// One straight run, simulated in small steps with the game's laws: `useCruise` = charge + linear ramp + cruise, else
// the afterburner (engine if none); braking by full reverse once the way left equals the braking distance.
static float SimulateRun(const Model& m, float v0, float distance, float vArrive, bool useCruise, float chargeDone,
                         float dt = 0.005f)
{
    const Thrust& fast = m.hasBurner && m.burner.ok() ? m.burner : m.engine;
    float v = v0, x = 0, t = 0, charge = chargeDone;
    bool braking = false;
    while (x < distance && t < 600)
    {
        if (!braking && distance - x <= BrakeDistance(m, v, vArrive) + v * dt)
            braking = true;
        if (braking)
        {
            if (v <= vArrive)
            {
                // At the arrival speed before the point: hold it (engine eases off) for the rest of the way; a stop
                // (vArrive 0) is there - the braking began one step's travel early (was: the last metre or so at
                // 1 m/s, ~0.9 s too long on every stopping run, 18 Oct 2026).
                if (vArrive >= 1)
                    t += (distance - x) / vArrive;
                break;
            }
            const float v1 = m.reverse.SpeedAfter(v, dt);
            x += m.reverse.DistanceAfter(v, dt);
            v = v1;
        }
        else if (useCruise && charge < m.cruiseChargeTime)
        {
            charge += dt; // charging: the engine's top speed
            const float v1 = m.engine.SpeedAfter(v, dt);
            x += m.engine.DistanceAfter(v, dt);
            v = v1;
        }
        else if (useCruise)
        {
            const float v1 = std::min(m.cruiseSpeed, std::max(v, m.engine.top()) + m.cruiseAccel * dt);
            x += (v + v1) * 0.5f * dt;
            v = v1;
        }
        else
        {
            const float v1 = fast.SpeedAfter(v, dt);
            x += fast.DistanceAfter(v, dt);
            v = v1;
        }
        t += dt;
    }
    return t;
}

RunPlan PlanRun(const Model& m, float v0, float distance, float vArrive, float chargeDone, float turnAngle, bool coarse)
{
    RunPlan p;
    if (!m.ok || distance <= 0)
        return p;
    // The turn first (no progress counted along the way during it), the charge running meanwhile.
    const float turn = m.TurnTime(turnAngle);
    const float dt = coarse ? 0.05f : 0.005f;
    p.burnerTime = turn + SimulateRun(m, v0, distance, vArrive, false, 0, dt);
    if (m.cruiseChargeTime > 0 && m.cruiseSpeed > 0)
    {
        p.cruiseTime = turn + SimulateRun(m, v0, distance, vArrive, true, chargeDone + turn, dt);
        p.cruise = p.cruiseTime < p.burnerTime;
    }
    return p;
}

// Time for a force-law run to cover d from v0 (Newton on x(t) = d; x is increasing and convex past the start).
static float ForceLawTime(const Thrust& th, float v0, float d)
{
    if (!th.ok() || th.top() <= 0)
        return d / std::max(1.0f, v0);
    float t = d / th.top() + std::max(0.0f, (th.top() - v0) / th.top()) * th.tau();
    for (int i = 0; i < 6; ++i)
    {
        const float f = th.DistanceAfter(v0, t) - d, v = th.SpeedAfter(v0, t);
        if (v <= 0)
            break;
        t -= f / v;
        if (t < 0)
            t = 0;
    }
    return t;
}

// Replacing the last stretch at speed v by a full-reverse stop: the extra seconds (distance d_b covered in t_b).
static float StopExtra(const Model& m, float v)
{
    if (!m.reverse.ok() || v <= 0)
        return 0;
    const float tb = m.reverse.TimeToSpeed(v, 0), db = m.reverse.DistanceToSpeed(v, 0);
    return tb > 0 && db > 0 ? tb - db / v : 0;
}

float QuickRunTime(const Model& m, float v0, float distance, bool stop)
{
    if (!m.ok || distance <= 0)
        return 0;
    const Thrust& fast = m.hasBurner && m.burner.ok() ? m.burner : m.engine;
    float best = ForceLawTime(fast, v0, distance) + (stop ? StopExtra(m, fast.top()) : 0);
    if (m.cruiseChargeTime > 0 && m.cruiseSpeed > 0 && m.cruiseAccel > 0)
    {
        // Charge at the engine's top speed, ramp to the cruise speed, cruise.
        const float vE = m.engine.top(), tc = m.cruiseChargeTime;
        const float dc = m.engine.DistanceAfter(v0, tc);
        float t;
        if (distance <= dc)
            t = ForceLawTime(m.engine, v0, distance);
        else
        {
            const float v1 = std::max(vE, m.engine.SpeedAfter(v0, tc));
            const float tr = std::max(0.0f, (m.cruiseSpeed - v1) / m.cruiseAccel);
            const float dr = (v1 + m.cruiseSpeed) * 0.5f * tr;
            const float rest = distance - dc;
            if (rest <= dr) // still ramping: rest = v1 t + a t^2 / 2
                t = tc + (std::sqrt(v1 * v1 + 2 * m.cruiseAccel * rest) - v1) / m.cruiseAccel;
            else
                t = tc + tr + (rest - dr) / m.cruiseSpeed;
        }
        best = std::min(best, t + (stop ? StopExtra(m, m.cruiseSpeed) : 0));
    }
    return best;
}

// ------------------------------------------------------------------ flight recorder
static FILE* g_rec = nullptr;
static LARGE_INTEGER g_recStart, g_recFreq;
static long g_recLines = 0;
static std::string g_recPath;

// The closest surface-to-surface gap to anything the ship can hit within 300 m (negative: contact).
static float MinGap(const float pos[3])
{
    float minGap = 9999;
    const float myRadius = space::PlayerRadius();
    for (const space::Obstacle& o : space::Obstacles(pos, 300))
    {
        const float d[3] = {o.pos[0] - pos[0], o.pos[1] - pos[1], o.pos[2] - pos[2]};
        minGap = std::min(minGap, std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) - o.radius - myRadius);
    }
    return minGap;
}

void RecordFrame()
{
    if (!g_rec)
        return;
    space::Pilot p;
    if (!space::ReadPilot(p))
        return;
    float v[3] = {};
    if (void* iobj = hk::Call<void*(__cdecl*)()>(exe::GetPlayerIObj))
        if (const auto vel = dll::ObjGetVelocity.get<dll::ObjVectorFn>(iobj))
        {
            __try
            {
                vel(iobj, v);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const double t = static_cast<double>(now.QuadPart - g_recStart.QuadPart) / g_recFreq.QuadPart;
    const float minGap = MinGap(p.pos);
    float ls[3], ld[3];
    u32 laneNext = 0;
    if (!space::TargetLaneStart(ls, ld, laneNext))
        laneNext = 0;
    fprintf(g_rec, "%.4f,%.1f,%.1f,%.1f,%.2f,%.2f,%.2f,%.2f,%d,%d,%u,%u,%d,%u,%d,%u,%d,%.1f\n", t, p.pos[0], p.pos[1],
            p.pos[2], v[0], v[1], v[2], std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]), p.op, p.lane ? 1 : 0,
            p.laneEntry, p.laneExit, p.cruise ? 1 : 0, p.system, space::TargetLaneSide(), laneNext, p.jumping ? 1 : 0,
            minGap);
    ++g_recLines;
}

std::string RecordCommand(const std::string& rest)
{
    std::istringstream in(rest);
    std::string op, file;
    in >> op >> file;
    if (op == "start")
    {
        if (g_rec)
            fclose(g_rec);
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH); // Freelancer.exe: the record goes beside it
        if (char* slash = strrchr(path, '\\'))
            slash[1] = 0;
        g_recPath = std::string(path) + (file.empty() ? "FLRecord.csv" : file);
        g_rec = _fsopen(g_recPath.c_str(), "w", _SH_DENYWR);
        if (!g_rec)
            return "rec: cannot open " + g_recPath;
        fprintf(g_rec, "t,x,y,z,vx,vy,vz,speed,op,lane,entry,exit,cruise,system,laneside,lanenext,jumping,mingap\n");
        QueryPerformanceFrequency(&g_recFreq);
        QueryPerformanceCounter(&g_recStart);
        g_recLines = 0;
        return "rec started " + g_recPath;
    }
    if (op == "stop")
    {
        if (g_rec)
            fclose(g_rec);
        g_rec = nullptr;
        return "rec stopped " + std::to_string(g_recLines) + " lines " + g_recPath;
    }
    return std::string("rec ") + (g_rec ? "on " : "off ") + std::to_string(g_recLines) + " lines " + g_recPath;
}

static float Const(const char* name)
{
    HMODULE c = GetModuleHandleA("common.dll");
    const float* p = c ? reinterpret_cast<const float*>(GetProcAddress(c, name)) : nullptr;
    return p ? *p : NAN;
}

std::string Describe()
{
    Model m;
    if (!ReadModel(m))
        return "not flying (or the flight model is unreadable)";
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(2);
    auto line = [&](const char* name, const Thrust& t)
    {
        o << name << ": force " << t.force << " drag " << t.drag << " top " << t.top() << " m/s tau " << t.tau()
          << " s\n";
    };
    o << "mass " << m.mass << "\n";
    line("engine", m.engine);
    line("reverse", m.reverse);
    if (m.hasBurner)
        line("burner", m.burner);
    else
        o << "burner: none\n";
    line("cruise", m.cruise);
    o << "cruise charge " << m.cruiseChargeTime << " s (elapsed " << m.cruiseChargeElapsed << ", engaged "
      << m.cruiseEngaged << ")\n";
    o << "constants: CRUISING_SPEED " << Const("?CRUISING_SPEED@EngineEquipConsts@@3MA") << " CRUISE_ACCEL_TIME "
      << Const("?CRUISE_ACCEL_TIME@EngineEquipConsts@@3MA") << " CRUISE_STEADY_TIME "
      << Const("?CRUISE_STEADY_TIME@EngineEquipConsts@@3MA") << " CRUISE_DRAG "
      << Const("?CRUISE_DRAG@EngineEquipConsts@@3MA") << " CRUISE_DISRUPT_TIME "
      << Const("?CRUISE_DISRUPT_TIME@EngineEquipConsts@@3MA") << "\n";
    // Worked examples: full reverse from the burner's top speed; burner from rest to 95 % of its top speed.
    const float vb = m.hasBurner ? m.burner.top() : m.engine.top();
    o << "stop from " << vb << " m/s by full reverse to 0: " << m.reverse.TimeToSpeed(vb, 0) << " s, "
      << m.reverse.DistanceToSpeed(vb, 0) << " m\n";
    if (m.hasBurner)
        o << "burner 0 -> " << 0.95f * vb << " m/s: " << m.burner.TimeToSpeed(0, 0.95f * vb) << " s, "
          << m.burner.DistanceToSpeed(0, 0.95f * vb) << " m\n";
    o << "turn rate " << m.turnRate << " rad/s (" << m.turnRate * 57.2958f << " deg/s)\n";
    o << "cruise law: " << m.cruiseAccel << " m/s^2 to " << m.cruiseSpeed << " m/s after the charge\n";
    // Plans from rest to a stop, by distance: which way is faster.
    for (float d : {500.0f, 1000.0f, 2000.0f, 3000.0f, 5000.0f, 10000.0f, 20000.0f})
    {
        const RunPlan p = PlanRun(m, 0, d, 0);
        o << "run " << d << " m: burner " << p.burnerTime << " s, cruise " << p.cruiseTime << " s -> "
          << (p.cruise ? "cruise" : "burner") << " | closed form " << QuickRunTime(m, 0, d, true) << " s\n";
    }
    return o.str();
}
} // namespace flight
