// The debug surface: the type names, the player's server-side equipment list and the `space::Command` dispatcher the
// controller's debug console chains (contacts, weapons, rounds, pilot, waypoint, ...). Split from space.cpp on
// 24 Oct 2026 (a file move: the functions are unchanged).
#include "game/space_internal.h"

#include "fldll.h"
#include "flexe.h"
#include "game/flight.h"
#include "game/hudinfo.h"
#include "text.h"

#include <sstream>

namespace space
{
static std::string TypeName(u32 t)
{
    static const char* names[] = {"moon",     "planet",    "sun",       "blackhole",  "satellite", "dockring",
                                  "jumpgate", "tradelane", "station",   "waypoint",   "airlock",   "jumphole",
                                  "weapons",  "depot",     "nontarget", "missionsat", "fighter",   "freighter",
                                  "gunboat",  "cruiser",   "transport", "capital",    "mining"};
    if (t & kLoot)
        return "loot";
    std::string s;
    for (int i = 0; i < 23; ++i)
        if (t & (1u << i))
            s += (s.empty() ? "" : "|") + std::string(names[i]);
    return s.empty() ? "other" : s;
}

// The player's equipment list as the server keeps it (mounted launchers / guns / shield, and the cargo: ammo,
// nanobots, batteries, goods), readable docked too - the base stop's restock needs the mounted launchers and the ammo
// counts. server.dll's pub::Player::EnumerateCargo walks exactly this list (but reports only unmounted items): its own
// code says where the list is - the player array and count globals, the 0x418 record stride, the record's "in use"
// field +0x348, the list at record - 0x1A0 + 4 - and every byte of it used here is checked first (another build fails
// closed). Each node (MSVC6 std::list, value from +8): +0x0A u16 sub-object id, +0x0C archetype id, +0x10
// CacheString hardpoint (char*), +0x14 bool mounted, +0x18 float health, +0x1C int count. Lines "arch= id= count=
// mounted= hp= health=" (verified 20 Oct 2026).
static void EquipmentList(u32 client, char* out, size_t size)
{
    out[0] = 0;
    HMODULE srv = GetModuleHandleA("server.dll");
    const u8* f = srv ? reinterpret_cast<const u8*>(
                            GetProcAddress(srv, "?EnumerateCargo@Player@pub@@YAHABIAAUCargoEnumerator@2@@Z"))
                      : nullptr;
    // the instructions this reads its facts from (offset, bytes)
    static const struct
    {
        int at;
        const char* bytes;
        int n;
    } sig[] = {{0x11, "\x8B\x35", 2},
               {0x1E, "\x8B\x35", 2},
               {0x24, "\x69\xD2\x18\x04\x00\x00", 6},
               {0x2A, "\x8B\xBC\x32\x48\x03\x00\x00", 7},
               {0x3B, "\x8D\x8C\x31\x60\xFE\xFF\xFF", 7},
               {0x43, "\x8B\x59\x04", 3},
               {0x57, "\x8A\x46\x14", 3},
               {0x5E, "\x8B\x46\x0C", 3},
               {0x64, "\x8B\x4E\x1C", 3},
               {0x6F, "\x66\x8B\x46\x0A", 4}};
    __try
    {
        if (!f)
        {
            sprintf_s(out, size, "equipment: EnumerateCargo not exported");
            return;
        }
        for (const auto& s : sig)
            if (memcmp(f + s.at, s.bytes, s.n) != 0)
            {
                sprintf_s(out, size, "equipment: EnumerateCargo differs at +0x%X (unknown server.dll build)", s.at);
                return;
            }
        const u32 count = Mem<u32>(Mem<u32>(reinterpret_cast<u32>(f + 0x13)));
        const u32 arr = Mem<u32>(Mem<u32>(reinterpret_cast<u32>(f + 0x20)));
        if (!client || client - 1 >= count || !arr || !Mem<u32>(arr + (client - 1) * 0x418 + 0x348))
        {
            sprintf_s(out, size, "equipment: no player record for client %u", client);
            return;
        }
        const u32 head = Mem<u32>(arr + client * 0x418 - 0x1A0 + 4);
        int n = 0, used = 0;
        for (u32 node = Mem<u32>(head); node && node != head && n < 256; node = Mem<u32>(node), ++n)
        {
            const char* hp = Mem<const char*>(node + 0x10);
            const int w = sprintf_s(out + used, size - used, "arch=%u id=%u count=%d mounted=%d hp=%s health=%.2f\n",
                                    Mem<u32>(node + 0x0C), Mem<u16>(node + 0x0A), Mem<int>(node + 0x1C),
                                    Mem<u8>(node + 0x14) ? 1 : 0, hp && !IsBadStringPtrA(hp, 64) ? hp : "-",
                                    Mem<float>(node + 0x18));
            if (w <= 0 || used + w >= static_cast<int>(size) - 64)
                break;
            used += w;
        }
        sprintf_s(out + used, size - used, "equipment %d", n);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf_s(out, size, "equipment: fault reading the list");
    }
}

bool Command(const std::string& cmd, const std::string& rest, std::string* reply, std::string (*name)(u32 ids))
{
    if (cmd == "hudinfo") // hudinfo [wg|engine|lock|tractor|obj <id>|sys [id]|equip <arch>]: HUD 2's readers (hudinfo.h)
    {
        *reply = rest.empty() ? hudinfo::Status() : hudinfo::Research(rest);
        return true;
    }
    if (cmd ==
        "motion") // motion [id]: a ship's motion and agility (ShipMotionOf); no id = the current target, else ours
    {
        u32 id = static_cast<u32>(strtoul(rest.c_str(), nullptr, 0));
        if (!id)
        {
            const Api& a = GetApi();
            void* t = a.target ? a.target(PlayerCShip(), nullptr) : nullptr;
            const auto getId = t ? dll::ObjGetId.get<dll::ObjGetIdFn>(t) : nullptr;
            id = getId ? getId(t) : PlayerShipId();
        }
        ShipMotion m;
        char buf[320];
        if (!ShipMotionOf(id, m))
            sprintf_s(buf, "motion: no ship %u", id);
        else
            sprintf_s(
                buf,
                "motion id=%u arch=%u speed=%.1f vel=%.1f,%.1f,%.1f fwd=%.2f,%.2f,%.2f turn=%.3f rad/s (%.0f deg/s) "
                "strafe=%.1f m/s2 mass=%.0f",
                id, m.arch, m.speed, m.vel[0], m.vel[1], m.vel[2], m.fwd[0], m.fwd[1], m.fwd[2], m.turnRate,
                m.turnRate * 57.2958f, m.strafeAccel, m.mass);
        *reply = buf;
        return true;
    }
    if (cmd == "lanestart") // research: the targeted ring's lane hardpoints and get_lane_start both ways
    {
        void* me = PlayerIObj();
        void* ring = TargetIObj();
        if (!me || !ring)
        {
            *reply = "no target";
            return true;
        }
        std::ostringstream o;
        o.setf(std::ios::fixed);
        o.precision(1);
        o << "side (get_lane_direction) " << TargetLaneSide() << "\n";
        if (const auto hp = dll::ObjGetHardpoint.get<dll::ObjHardpointFn>(ring))
            for (const char* n : {"HpLeftLane", "HpRightLane", "HpMount"})
            {
                float pos[3] = {}, rot[16] = {};
                const int rc = CallHardpoint(hp, ring, n, pos, rot);
                o << n << " rc=" << rc << " pos " << pos[0] << " " << pos[1] << " " << pos[2] << "\n";
            }
        if (const auto ls = dll::ObjGetLaneStart.get<dll::ObjLaneStartFn>(me))
        {
            const struct
            {
                const char* what;
                void *self, *other;
            } tries[] = {{"ship.get_lane_start(ring)", me, ring}, {"ring.get_lane_start(ship)", ring, me}};
            for (const auto& t : tries)
            {
                const void* next = nullptr;
                float start[3] = {}, dir[3] = {};
                const int rc = CallLaneStart(ls, t.self, t.other, &next, start, dir);
                o << t.what << " rc=" << rc << " next=" << (next ? ObjId(const_cast<void*>(next)) : 0) << " start "
                  << start[0] << " " << start[1] << " " << start[2] << " dir " << dir[0] << " " << dir[1] << " "
                  << dir[2] << "\n";
            }
        }
        *reply = o.str();
        return true;
    }
    if (cmd == "perf") // perf: the player ship's flight model from the game (core\game\flight.*)
    {
        *reply = flight::Describe();
        return true;
    }
    if (cmd == "rec") // rec start [file] | stop | status: the flight recorder (core\game\flight.*)
    {
        *reply = flight::RecordCommand(rest);
        return true;
    }
    if (cmd == "dockpoints") // dockpoints: research - the current target's docking geometry, raw (get_dock_hardpoints)
    {
        void* me = PlayerIObj();
        void* target = nullptr;
        if (const auto getTarget = dll::ObjGetTarget.get<dll::ObjGetObjFn>(me))
            getTarget(me, &target);
        const auto dock = dll::ObjGetDockHardpoints.get<dll::ObjDockPointsFn>(target);
        if (!target || !dock)
        {
            *reply = target ? "get_dock_hardpoints not found" : "no target";
            return true;
        }
        std::ostringstream out;
        out << "target " << ObjId(target) << "\n";
        for (int i = 0; i < 6; ++i)
        {
            float t[3][32] = {}, radius = -1;
            int type = -1;
            const int rc = CallDockPoints(dock, target, i, &type, t, &radius);
            char line[700];
            int n = sprintf_s(line, "dock %d rc=%d type=%d radius=%.2f", i, rc, type, radius);
            for (int k = 0; k < 3; ++k)
            {
                n += sprintf_s(line + n, sizeof(line) - n, " | %s", k == 0 ? "mount" : k == 1 ? "p1" : "p2");
                for (int f = 9; f < 12; ++f) // position (Transform: 3x3 rotation, then position)
                    n += sprintf_s(line + n, sizeof(line) - n, " %.1f", t[k][f]);
            }
            out << line << "\n";
        }
        *reply = out.str();
        return true;
    }
    if (cmd ==
        "vtslots") // vtslots: the game interface virtuals FLvibed calls, with the slots found by name (core\fldll.h)
    {
        *reply = dll::Slots();
        return true;
    }
    if (cmd ==
        "save") // save [ids]: a native save now - server.dll pub::Save(client, ids), what missions' Act_Save calls;
    {           // the game names the file (SaveXXXX.fl) and describes it by string `ids` (default 1890)
        HMODULE srv = GetModuleHandleA("server.dll");
        const auto save =
            srv ? reinterpret_cast<int(__cdecl*)(u32, u32)>(GetProcAddress(srv, "?Save@pub@@YAHII@Z")) : nullptr;
        if (!save)
        {
            *reply = "save: server.dll pub::Save not found";
            return true;
        }
        if (!InSpace() && Mem<u32>(exe::PlayerBase) == 0)
        {
            *reply = "save: not in the game (space or a base)";
            return true;
        }
        const u32 ids = rest.empty() ? 1890 : static_cast<u32>(strtoul(rest.c_str(), nullptr, 10));
        char line[80];
        sprintf_s(line, "save ids=%u result=%d", ids, save(1, ids));
        *reply = line;
        return true;
    }
    if (cmd == "munition") // munition: the player's homing launchers' munition archetype as offset: u32 / float
    {
        std::ostringstream out;
        for (const Weapon& w : PlayerWeapons())
        {
            if (!w.homing || !w.projArch)
                continue;
            out << "munition " << w.hardpoint << " at " << std::hex << w.projArch << std::dec << "\n";
            for (u32 off = 0x40; off < 0xC0; off += 4)
            {
                char line[80];
                sprintf_s(line, "+%02X %08X %g\n", off, Mem<u32>(w.projArch + off), Mem<float>(w.projArch + off));
                out << line;
            }
        }
        *reply = out.str();
        return true;
    }
    if (cmd == "loot") // loot <nickname> [carried]: price (game good list / goods file) and the loot rule's drops
    {
        std::istringstream in(rest);
        std::string nick;
        int carried = 1;
        in >> nick >> carried;
        const Api& a = GetApi();
        const u32 id = a.createId && !nick.empty() ? a.createId(nick.c_str()) : 0;
        const auto& t = LootTable();
        const auto it = t.find(id);
        char line[320];
        sprintf_s(line, "loot %s id=%u game_price=%.1f price=%.1f table=%zu entry=%d worth=%.0f drops=%.2f",
                  nick.c_str(), id, GamePrice(a, id), Price(id), t.size(), it != t.end() ? 1 : 0, PlayerWorth(),
                  ExpectedDrops(id, PlayerWorth(), carried));
        *reply = line;
        return true;
    }
    if (cmd == "weapons") // weapons [ship id]: guns / launchers (equipment order), live refire state, ammo economy
    {
        const u32 ship = rest.empty() ? 0 : static_cast<u32>(strtoul(rest.c_str(), nullptr, 10));
        const std::vector<Weapon> list = ship ? ShipWeapons(ship) : PlayerWeapons();
        const float worth = PlayerWorth();
        std::ostringstream out;
        out << "weapons " << list.size() << " worth=" << worth << " slots=" << (g_fireSlotsHud ? "hud" : "hardpoint")
            << "\n";
        int i = 0;
        for (const Weapon& w : list)
        {
            char line[300];
            sprintf_s(line,
                      "%d subobj=%u active=%d refire=%.3f elapsed=%.3f power=%.2f muzzle=%.0f homing=%d dmg=%.1f "
                      "blast=%.0f ammo=%d ammo_arch=%u price=%.0f drops=%.2f hp=%s slot=%d life=%.2f seeker=%.0f/%.0f "
                      "turn=%.2f motor=%.2f/%.1f/%.2f\n",
                      ++i, w.subObj, w.active ? 1 : 0, w.refire, w.elapsed, w.power, w.muzzleVelocity, w.homing ? 1 : 0,
                      w.hullDamage, w.blastRadius, w.ammo, w.ammoArch, w.ammoArch ? Price(w.ammoArch) : -1.0f,
                      w.ammoArch ? ExpectedDrops(w.ammoArch, worth, w.ammo) : 0.0f, w.hardpoint, w.fireSlot, w.lifetime,
                      w.seekerRange, w.seekerFovDeg, w.maxTurn, w.motorDelay, w.motorAccel, w.motorTime);
            out << line;
        }
        out << "countermeasures=" << PlayerCountermeasures() << " mines=" << PlayerMines() << "\n";
        *reply = out.str();
        return true;
    }
    if (cmd == "cover") // cover <id> [dist..]: how many of its guns reach the player and 26 directions x distances
    {
        std::istringstream in(rest);
        u32 id = 0;
        in >> id;
        std::vector<float> dists;
        for (float d; in >> d;)
            dists.push_back(d);
        if (dists.empty())
            dists = {300, 600, 900, 1200};
        float c[3], rot[9];
        if (!id || !ObjectPose(id, c, rot))
        {
            *reply = "cover: no such object";
            return true;
        }
        struct P3
        {
            float v[3];
        };
        std::vector<P3> pts;
        if (void* me = PlayerCShip())
        {
            const Api& a = GetApi();
            float p[3], r[9];
            if (ObjectPose(ObjectId(a, me), p, r))
                pts.push_back({{p[0], p[1], p[2]}});
        }
        const size_t self = pts.size();
        for (int x = -1; x <= 1; ++x)
            for (int y = -1; y <= 1; ++y)
                for (int z = -1; z <= 1; ++z)
                {
                    if (!x && !y && !z)
                        continue;
                    const float l = sqrtf(static_cast<float>(x * x + y * y + z * z));
                    for (float d : dists) // in the object's own frame (x right, y up, z back)
                    {
                        const float lx = x / l * d, ly = y / l * d, lz = z / l * d;
                        pts.push_back({{c[0] + rot[0] * lx + rot[1] * ly + rot[2] * lz,
                                        c[1] + rot[3] * lx + rot[4] * ly + rot[5] * lz,
                                        c[2] + rot[6] * lx + rot[7] * ly + rot[8] * lz}});
                    }
                }
        std::vector<int> counts(pts.size());
        const int guns = GunsCovering(id, reinterpret_cast<const float (*)[3]>(pts.data()),
                                      static_cast<int>(pts.size()), counts.data());
        std::ostringstream out;
        out << "cover " << id << " guns=" << guns;
        if (self)
            out << " player=" << counts[0];
        out << "\n";
        size_t k = self;
        for (int x = -1; x <= 1; ++x)
            for (int y = -1; y <= 1; ++y)
                for (int z = -1; z <= 1; ++z)
                {
                    if (!x && !y && !z)
                        continue;
                    out << "dir " << x << "," << y << "," << z << ":";
                    for (size_t j = 0; j < dists.size(); ++j)
                        out << " " << counts[k++];
                    out << "\n";
                }
        *reply = out.str();
        return true;
    }
    if (cmd == "equipall") // equipall: the player's equipment per EquipmentClass bit (0..15): class, active, ammo
    {
        // Research (20 Oct 2026: PlayerCountermeasures read 0 with a dropper of 28 - no countermeasure was ever dropped).
        std::ostringstream out;
        for (int bit = 0; bit < 16; ++bit)
        {
            Weapon buf[16];
            const int n = InSpace() ? ReadWeapons(PlayerCShip(), buf, 16, 1 << bit) : 0;
            for (int i = 0; i < n; ++i)
                out << "bit " << bit << " hp=" << buf[i].hardpoint << " active=" << (buf[i].active ? 1 : 0)
                    << " ammo=" << buf[i].ammo << " arch=" << buf[i].archId << "\n";
        }
        out << "countermeasures=" << PlayerCountermeasures() << " mines=" << PlayerMines() << "\n";
        *reply = out.str();
        return true;
    }
    if (cmd == "missiles") // missiles: guided missiles / torpedoes in flight in the player's system, nearest first
    {
        const std::vector<Missile> list = Missiles();
        std::ostringstream out;
        out << "missiles " << list.size() << " player=" << Mem<u32>(exe::PlayerShipId) << "\n";
        for (const Missile& m : list)
        {
            char line[260];
            sprintf_s(line,
                      "%u dist=%.0f closing=%.0f target=%u%s owner=%u cd=%d homing=%d damage=%.0f blast=%.0f "
                      "pos=%.1f,%.1f,%.1f\n",
                      m.id, m.distance, m.closing, m.targetId, m.targetsPlayer ? " (player)" : "", m.owner,
                      m.cruiseDisruptor ? 1 : 0, m.homing ? 1 : 0, m.hullDamage, m.blastRadius, m.pos[0], m.pos[1],
                      m.pos[2]);
            out << line;
        }
        *reply = out.str();
        return true;
    }
    if (cmd == "contacts") // contacts [dock|ships|all]: objects of the player's system, nearest first
    {
        const std::vector<Contact> list = Contacts();
        if (list.empty())
        {
            float me[3];
            const Api& a = GetApi();
            *reply = !a.ok        ? "no contacts: Common.dll / server.dll exports missing"
                     : !InSpace() ? "no contacts: not in space"
                     : !Location(a, Mem<u32>(exe::PlayerShipId), me)
                         ? "no contacts: player location unknown (GetLocation " + std::to_string(g_lastLocation) + ")"
                         : "no contacts";
            return true;
        }
        const bool all = rest == "all", dockOnly = rest == "dock", ships = rest == "ships";
        std::ostringstream out;
        out << "contacts " << list.size() << " player=" << Mem<u32>(exe::PlayerShipId) << "\n";
        int shown = 0;
        for (const Contact& c : list)
        {
            if ((dockOnly && !c.dock) || (ships && !c.ship) || (!all && !c.targetable))
                continue;
            if (!all && ++shown > 40)
                break;
            char line[320];
            sprintf_s(line,
                      "%u %s dist=%.0f%s%s%s%s%s base=%u health=%.2f att=%.2f target=%u maxhit=%.0f good=0x%08X "
                      "units=%u fits=%d value=%.0f name=",
                      c.id, TypeName(c.type).c_str(), c.distance, c.dock ? " dock" : "", c.ship ? " ship" : "",
                      c.dockPort ? " docking" : "", c.jumping ? " jumping" : "", c.lane ? " lane" : "", c.base,
                      c.health, c.attitude, c.targetId, c.maxHit, c.lootGood, c.lootUnits, c.lootFits, c.lootValue);
            // ships: the pilot's name as the game shows it (the reputation's name record); else the object's name
            const std::wstring pilot = c.ship && c.rep ? RepName(c.rep) : std::wstring();
            out << line << (!pilot.empty() ? text::Utf8(pilot) : c.nameIds && name ? name(c.nameIds) : "") << "\n";
        }
        *reply = out.str();
        return true;
    }
    if (cmd == "equipment") // equipment [client]: the player's whole equipment list (mounted + cargo), docked or not
    {
        char buf[16384];
        EquipmentList(rest.empty() ? 1 : static_cast<u32>(strtoul(rest.c_str(), nullptr, 10)), buf, sizeof buf);
        *reply = buf;
        return true;
    }
    if (cmd == "worth") // the player's net worth the way the story code computes it (Content.dll: asset value + cash)
    {
        // server.dll pub::Player::GetAssetValue(const u32& client, float&) / InspectCash(const u32& client, int&); the
        // single-player client is 1 (a result code other than 0 is reported, never a guessed value).
        HMODULE srv = GetModuleHandleA("server.dll");
        const auto assets = srv ? reinterpret_cast<int(__cdecl*)(const u32&, float&)>(
                                      GetProcAddress(srv, "?GetAssetValue@Player@pub@@YAHABIAAM@Z"))
                                : nullptr;
        const auto cash = srv ? reinterpret_cast<int(__cdecl*)(const u32&, int&)>(
                                    GetProcAddress(srv, "?InspectCash@Player@pub@@YAHABIAAH@Z"))
                              : nullptr;
        if (!assets || !cash)
        {
            *reply = "worth: server.dll functions not found";
            return true;
        }
        const u32 client = rest.empty() ? 1 : static_cast<u32>(strtoul(rest.c_str(), nullptr, 10));
        float a = 0;
        int c = 0;
        const int ra = assets(client, a), rc = cash(client, c);
        char line[160];
        sprintf_s(line, "worth client=%u cash=%d assets=%.0f total=%.0f rc=%d,%d", client, c, a, a + c, ra, rc);
        *reply = line;
        return true;
    }
    if (cmd == "pilot") // the player ship's autopilot: maneuver op, lane, jump, formation leader, target, system
    {
        Pilot p;
        if (!ReadPilot(p))
        {
            *reply = "not in space";
            return true;
        }
        static const char* ops[] = {"null",         "buzz",       "goto",   "trail",     "flee",      "evade",
                                    "idle",         "dock",       "launch", "tradelane", "formation", "largeshipmove",
                                    "cruise",       "strafe",     "guide",  "face",      "loot",      "follow",
                                    "drasticevade", "freeflight", "delay"};
        // EngineEquipConsts::CRUISING_SPEED (constants.ini; HDE's console `cspd` changes it)
        static const float* cruiseSpeed = reinterpret_cast<const float*>(
            GetProcAddress(GetModuleHandleA("common.dll"), "?CRUISING_SPEED@EngineEquipConsts@@3MA"));
        const u32 arch = PlayerShipArch();
        char line[360];
        sprintf_s(line,
                  "pilot op=%d %s locked=%d lane=%d entry=%u exit=%u jumping=%d cruise=%d leader=%u target=%u "
                  "system=%u dockrange=%.0f cruisespeed=%.0f pos=(%.0f,%.0f,%.0f) avoid=%d arch=%u",
                  p.op, p.op >= 0 && p.op <= 20 ? ops[p.op] : "none", p.locked, p.lane, p.laneEntry, p.laneExit,
                  p.jumping, p.cruise, p.leader, p.target, p.system, DockRange(), cruiseSpeed ? *cruiseSpeed : 0.0f,
                  p.pos[0], p.pos[1], p.pos[2], exe::AutoAvoidance.ok() ? Mem<u8>(exe::AutoAvoidance) : -1, arch);
        *reply = line;
        return true;
    }
    if (cmd == "avoid") // avoid on|off: the player's auto avoidance, as USER_TOGGLE_AUTO_AVOIDANCE sets it
    {
        if (!InSpace() || !exe::AutoAvoidance.ok() || !exe::SetAutoAvoidance.ok())
        {
            *reply = "not in space (or auto avoidance unresolved)";
            return true;
        }
        if (rest == "on" || rest == "off")
        {
            const bool on = rest == "on";
            Mem<u8>(exe::AutoAvoidance) = on ? 1 : 0;
            hk::Call<void(__cdecl*)(bool)>(exe::SetAutoAvoidance, on);
        }
        *reply = std::string("auto avoidance ") + (Mem<u8>(exe::AutoAvoidance) ? "on" : "off");
        return true;
    }
    if (cmd == "area") // area: encounter level of the zones at the ship vs the player's rank
    {
        Area ar;
        char buf[128];
        if (!ReadArea(ar))
            *reply = "not in space";
        else
        {
            sprintf_s(buf, "area level=%d rank=%d zone=0x%08X", ar.level, ar.rank, ar.zone);
            *reply = buf;
        }
        return true;
    }
    if (cmd == "zonesalong") // zonesalong x1 y1 z1 x2 y2 z2 [step]: zones of the system along a segment
    {
        float a[3], b[3], step = 250;
        if (sscanf_s(rest.c_str(), "%f %f %f %f %f %f %f", &a[0], &a[1], &a[2], &b[0], &b[1], &b[2], &step) < 6 ||
            step < 10)
        {
            *reply = "usage: zonesalong x1 y1 z1 x2 y2 z2 [step >= 10]";
            return true;
        }
        if (!InSpace())
        {
            *reply = "not in space";
            return true;
        }
        int samples = 0;
        std::vector<ZoneSpan> spans;
        if (!ZonesAlong(a, b, step, samples, spans))
        {
            *reply = "zones unavailable (server.dll InZones)";
            return true;
        }
        std::ostringstream out;
        out << "zones samples=" << samples << "\n";
        for (const ZoneSpan& z : spans)
        {
            out << z.zone;
            for (const auto& r : z.ranges)
                out << " " << r.first << "-" << r.second;
            out << "\n";
        }
        *reply = out.str();
        return true;
    }
    if (cmd == "waypoint") // waypoint <x> <y> <z> | clear: the player's waypoint, selected as the target
    {
        if (rest == "clear")
        {
            ClearWaypoints();
            *reply = "waypoints cleared";
            return true;
        }
        if (rest == "restore") // the list as it was before `waypoint x y z` replaced it
        {
            const int n = RestoreWaypoints();
            *reply = n >= 0    ? "waypoints restored " + std::to_string(n)
                     : n == -2 ? "waypoints not restored: the list changed (mission)"
                               : "nothing to restore";
            return true;
        }
        if (rest == "first") // target the first waypoint of the list as it is (e.g. the mission objective's)
        {
            const u32 id = SelectFirstWaypoint();
            *reply = id ? "waypoint " + std::to_string(id) : "no waypoint (list empty or not in space)";
            return true;
        }
        float pos[3] = {};
        if (sscanf_s(rest.c_str(), "%f %f %f", &pos[0], &pos[1], &pos[2]) != 3)
        {
            *reply = "usage: waypoint <x> <y> <z> | clear";
            return true;
        }
        const u32 id = SetWaypoint(pos);
        *reply = id ? "waypoint " + std::to_string(id) : "waypoint refused";
        return true;
    }
    if (cmd == "bestpath") // research: bestpath <object id> | <x> <y> <z> [system id] - the nav map's best path
    {
        float end[3] = {};
        u32 object = 0, system = Mem<u32>(exe::PlayerSystem);
        Pilot p;
        if (!InSpace() || !ReadPilot(p) || !exe::PlotBestPath.ok())
        {
            *reply = "not in space (or PlotBestPath unresolved)";
            return true;
        }
        if (sscanf_s(rest.c_str(), "%f %f %f %u", &end[0], &end[1], &end[2], &system) < 3)
        {
            object = static_cast<u32>(strtoul(rest.c_str(), nullptr, 0));
            if (!object || !Location(GetApi(), object, end))
            {
                *reply = "usage: bestpath <object id> | <x> <y> <z> [system id]";
                return true;
            }
        }
        typedef void(__cdecl * PlotFn)(const float*, const float*, u32, u32);
        hk::Call<PlotFn>(exe::PlotBestPath, p.pos, end, object, system);
        char line[160];
        sprintf_s(line, "bestpath requested from (%.0f,%.0f,%.0f) to (%.0f,%.0f,%.0f) object=%u system=%u", p.pos[0],
                  p.pos[1], p.pos[2], end[0], end[1], end[2], object, system);
        *reply = line;
        return true;
    }
    if (cmd == "obstacles") // obstacles [range]: what the ship can hit nearby, nearest first (avoidance input)
    {
        Pilot p;
        if (!ReadPilot(p))
        {
            *reply = "not in space";
            return true;
        }
        const float range = rest.empty() ? 3000.0f : static_cast<float>(atof(rest.c_str()));
        std::vector<Obstacle> obs = Obstacles(p.pos, range);
        auto dist = [&](const Obstacle& o)
        {
            const float d[3] = {o.pos[0] - p.pos[0], o.pos[1] - p.pos[1], o.pos[2] - p.pos[2]};
            return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) - o.radius;
        };
        std::sort(obs.begin(), obs.end(), [&](const Obstacle& x, const Obstacle& y) { return dist(x) < dist(y); });
        int rocks = 0, mines = 0, solars = 0, ships = 0, debris = 0;
        for (const Obstacle& o : obs)
        {
            (o.mine ? mines : o.asteroid ? rocks : o.ship ? ships : solars)++;
            debris += o.breakable ? 1 : 0;
        }
        std::ostringstream o;
        o.setf(std::ios::fixed);
        o.precision(0);
        o << "obstacles within " << range << " m: " << obs.size() << " (rocks " << rocks << " - breakable debris "
          << debris << ", mines " << mines << ", solars " << solars << ", ships " << ships << "), ship radius "
          << PlayerRadius() << ", strict field: " << (StrictAt(p.pos).empty() ? "no" : StrictAt(p.pos)) << "\n";
        for (size_t i = 0; i < obs.size() && i < 12; ++i)
            o << " "
              << (obs[i].mine        ? "mine"
                  : obs[i].breakable ? "debris"
                  : obs[i].asteroid  ? "rock"
                  : obs[i].ship      ? "ship"
                                     : "solar")
              << " gap " << dist(obs[i]) << " radius " << obs[i].radius << " at " << obs[i].pos[0] << " "
              << obs[i].pos[1] << " " << obs[i].pos[2] << " id " << obs[i].id << "\n";
        *reply = o.str();
        return true;
    }
    if (cmd == "godmode") // godmode on|off|status: test aid - the game's own invincibility flag (command line -g)
    {
        if (!exe::GodMode.ok())
        {
            *reply = "godmode unresolved";
            return true;
        }
        if (rest == "on" || rest == "off")
            Mem<u8>(exe::GodMode) = rest == "on" ? 1 : 0;
        *reply = std::string("godmode ") + (Mem<u8>(exe::GodMode) ? "on" : "off");
        return true;
    }
    if (cmd == "attitude") // attitude <id>: the object's faction towards the player
    {
        float att = 0;
        const u32 id = static_cast<u32>(strtoul(rest.c_str(), nullptr, 0));
        *reply = AttitudeToPlayer(id, att) ? "attitude " + std::to_string(id) + " " + std::to_string(att)
                                           : "attitude unknown";
        return true;
    }
    if (cmd == "rounds") // rounds [horizon s]: gun rounds in flight toward the player (IncomingRounds) + all CBeams
    {
        std::istringstream args(rest);
        float horizon = 2.0f;
        u32 shield = 0;
        args >> horizon >> shield;
        std::vector<void*> beams;
        const Api& a = GetApi();
        if (a.ok && InSpace())
            Collect(a, kClassBeam, 0, beams);
        const auto rs = IncomingRounds(horizon, 0, shield);
        std::ostringstream o;
        float dmg = 0, sdmg = 0;
        for (const Round& r : rs)
            dmg += r.hullDamage, sdmg += r.shieldDamage;
        float mn[3] = {}, mx[3] = {};
        if (ShipExtents(PlayerCShip(), mn, mx))
            o << "box (" << mn[0] << "," << mn[1] << "," << mn[2] << ")-(" << mx[0] << "," << mx[1] << "," << mx[2]
              << ") ";
        o << RoundStats() << "\nshield-dmg " << sdmg << " ";
        const int any = a.ok ? CountClass(a, kClassBeam) : 0; // diagnostics: whatever the system field
        o << "radius " << PlayerRadius() << " beams " << beams.size() << " (unfiltered " << any << "), incoming "
          << rs.size() << " (" << dmg << " hull dmg within " << horizon << " s)";
        for (size_t i = 0; i < rs.size() && i < 8; ++i)
            o << "\n  owner " << rs[i].owner << " dmg " << rs[i].hullDamage << " miss " << rs[i].miss << " m in "
              << rs[i].tHit << " s";
        *reply = o.str();
        return true;
    }
    if (cmd == "laneopen") // laneopen <ring> [toward]: the mission's lane permission (1 open, 0 closed, -1 unknown)
    {
        char* end = nullptr;
        const u32 ring = static_cast<u32>(strtoul(rest.c_str(), &end, 0));
        u32 toward = static_cast<u32>(strtoul(end, nullptr, 0));
        u32 next = 0, prev = 0;
        if (!toward && LaneLinks(ring, next, prev))
            toward = next ? next : prev;
        *reply = "laneopen " + std::to_string(ring) + " toward " + std::to_string(toward) + " = " +
                 std::to_string(LaneOpen(ring, toward));
        return true;
    }
    if (cmd == "lanelinks") // lanelinks <ring id>: its next / prev ring (GetTradelaneNextAndPrev)
    {
        u32 next = 0, prev = 0;
        const u32 ring = static_cast<u32>(strtoul(rest.c_str(), nullptr, 0));
        const bool ok = LaneLinks(ring, next, prev);
        *reply = "lanelinks " + std::to_string(ring) + (ok ? "" : " (none)") + " next=" + std::to_string(next) +
                 " prev=" + std::to_string(prev);
        return true;
    }
    if (cmd == "objobj") // objobj <ref>: the space object an objective's 'R' value names (rep instance or space id)
    {
        char b[96];
        u32 byId = 0, byRep = 0;
        const u32 id = ObjectiveObject(static_cast<u32>(strtoul(rest.c_str(), nullptr, 0)), &byId, &byRep);
        sprintf_s(b, "objobj %s id=%u by-id=%u by-rep=%u", rest.c_str(), id, byId, byRep);
        *reply = b;
        return true;
    }
    if (cmd == "sight") // sight <id>: what solid object (class) stands between the player ship and that object
    {
        const u32 id = static_cast<u32>(strtoul(rest.c_str(), nullptr, 0));
        float to[3];
        Pilot p;
        if (!id || !ObjectPos(id, to) || !ReadPilot(p))
        {
            *reply = "sight: unknown object or not in space";
            return true;
        }
        char b[96];
        sprintf_s(b, "sight %u blocker=0x%X", id, SightBlocker(p.pos, to, id));
        *reply = b;
        return true;
    }
    if (cmd == "routes") // research: the player's waypoint list and the contact list's type-3 list, raw
    {
        std::ostringstream o;
        o.setf(std::ios::fixed);
        o.precision(0);
        if (exe::Waypoints.ok() && exe::WaypointCount.ok())
        {
            const int n = Mem<int>(exe::WaypointCount);
            o << "waypoints " << n << "\n";
            for (int i = 0; i < n && i < 99; ++i)
            {
                const WaypointEntry* w = WaypointAt(i);
                o << " " << i << " pos " << w->pos[0] << " " << w->pos[1] << " " << w->pos[2] << " system " << w->system
                  << " object " << w->object << " seq " << w->seq << "\n";
            }
        }
        const uintptr_t head = exe::RouteList.ok() ? Mem<uintptr_t>(exe::RouteList) : 0;
        uintptr_t node = head ? Mem<uintptr_t>(head) : 0;
        for (int k = 0; node && node != head && k < 8; ++k, node = Mem<uintptr_t>(node))
        {
            o << "list node " << k << " raw";
            for (int w = 2; w < 16; ++w)
                o << " " << std::hex << Mem<u32>(node + 4 * w) << std::dec;
            o << "\n";
            const uintptr_t b = Mem<uintptr_t>(node + 0x14), e = Mem<uintptr_t>(node + 0x18);
            for (uintptr_t it = b; b && it + 0x1C <= e && it < b + 0x1C * 64; it += 0x1C)
            {
                o << "  entry";
                for (int w = 0; w < 7; ++w)
                {
                    const u32 v = Mem<u32>(it + 4 * w);
                    float f;
                    memcpy(&f, &v, 4);
                    if (std::fabs(f) > 1e-3f && std::fabs(f) < 1e7f)
                        o << " " << f << "f";
                    else
                        o << " " << v;
                }
                o << "\n";
            }
        }
        *reply = o.str();
        return true;
    }
    if (cmd == "settarget") // settarget <id>: select a space object as the target, natively
    {
        const u32 id = static_cast<u32>(strtoul(rest.c_str(), nullptr, 0));
        *reply = SetTarget(id) ? "target set " + std::to_string(id) : "target refused " + std::to_string(id);
        return true;
    }
    return false;
}
} // namespace space
