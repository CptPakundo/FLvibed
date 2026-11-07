// The nav map's zoom and pan: navview.h. The fields and calls: docs\FREELANCER_RE.md "The nav map's zoom and pan".
#include "game/navview.h"

#include "flexe.h"
#include "game/hudinfo.h"
#include "game/space.h"
#include "log.h"
#include "safemem.h"
#include "text.h"

#include <sstream>

namespace navview
{
constexpr u32 kZoom = 0x3F8, kBoxW = 0x418, kBoxH = 0x41C, kPortX = 0x424, kPortY = 0x428, kPortW = 0x430, kPortH = 0x434, kCentre = 0x46C, kAnimating = 0x478, kTargetView = 0x4A4,
              kView = 0x7D0, kGridLetters = 0xB08, kGridNumbers = 0xB28, kScaleX = 0x4A8, kScaleZ = 0x4AC, kWaypointRecords = 0x4B8; // (the last by the map's Clear Waypoint: 0x493680)
constexpr int kMaxWaypoints = 99;
constexpr u32 kMapScale = 0xF48, kBoxD = 0x420, kCamera = 0xC80;
constexpr float kFindZoom = 3.0f; // FIND ME from further out comes in to here

static u32 g_root = 0;
static View g_view;       // what stands in the map's fields
static Motion g_motion;
static bool g_movable = false, g_widest = true;
static int g_mapView = 1;
static float g_askZoom = 0, g_askU = 0, g_askT = 0; // the debug verb's one-shot
static float g_frameW = 0, g_frameH = 0; // SetFrame's
static bool g_askCentre = false;
static float g_centreX = 0, g_centreZ = 0;

static bool ReadF(u32 a, float& v) { return safemem::Read(a, &v, 4); }

static void WriteF(u32 a, float v)
{
    __try
    {
        *reinterpret_cast<volatile float*>(a) = v;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

static void WriteI(u32 a, u32 v)
{
    __try
    {
        *reinterpret_cast<volatile u32*>(a) = v;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

// The window's message call vt[0xA8] (ret 0xC): 1 hides or shows a control, 6 places it, as 0x49FBF0 does.
static void Message(u32 ctl, u32 msg, u32 a)
{
    __try
    {
        void** vt = *reinterpret_cast<void***>(ctl);
        reinterpret_cast<u32(__thiscall*)(u32, u32, u32, u32)>(vt[0xA8 / 4])(ctl, msg, a, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

static void PlaceGridItem(u32 ctl, bool shown, float x, float y)
{
    if (!ctl)
        return;
    Message(ctl, 1, shown ? 0 : 1);
    if (shown)
    {
        const float pos[3] = {x, y, 0};
        Message(ctl, 6, reinterpret_cast<u32>(pos));
    }
}

static void PlaceGrid(u32 root, const View& v)
{
    for (int i = 0; i < 8; ++i)
    {
        u32 letter = 0, number = 0;
        safemem::Read(root + kGridLetters + 4 * i, &letter, 4);
        safemem::Read(root + kGridNumbers + 4 * i, &number, 4);
        const float x = GridLetterX(i, v), y = GridNumberY(i, v);
        PlaceGridItem(letter, GridLetterShown(x), x, kGridLetterY);
        PlaceGridItem(number, GridNumberShown(y), kGridNumberX, y);
    }
}

// The game's own placement of the backgrounds and the records, as its drag and its zoom call them.
static bool Place(u32 root, float sectorAlpha)
{
    if (!exe::NavMapPlaceMeshes.ok() || !exe::NavMapPlaceObjects.ok())
        return false;
    __try
    {
        const auto meshes = reinterpret_cast<void(__thiscall*)(u32, int, float)>(exe::NavMapPlaceMeshes.get());
        meshes(root, 1, 1.0f);
        meshes(root, 2, sectorAlpha);
        reinterpret_cast<void(__thiscall*)(u32)>(exe::NavMapPlaceObjects.get())(root);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return true;
}

static void Store(u32 root, const View& v, int view)
{
    WriteF(root + kZoom, v.zoom);
    WriteF(root + kCentre, v.cx);
    WriteF(root + kCentre + 4, v.cy);
    // the map's camera has perspective, and 0x496600 brings the records nearer as Z grows (their depth is -0.55 + (C + D / 2) Z). Held at -0.55, the plane the game's hit test
    // unprojects to (0x492410), what the player sees is what a click finds, at every zoom
    float depth = 0;
    if (g_frameW > 0 && ReadF(root + kBoxD, depth))
        WriteF(root + kCentre + 8, -depth / 2);
    if (view != g_mapView)
        WriteI(root + kView, static_cast<u32>(view)), g_mapView = view;
    // (the draw shows the system's background under the sector's while this says 1 in view 2: 0x4926D0)
    WriteI(root + kTargetView, view == 2 && SectorAlpha(v.zoom) >= 1 ? 2 : 1);
}

bool Frame(u32 root, const Drive& in, float dt, float cursorX, float cursorY)
{
    g_movable = false, g_widest = false;
    u32 vt = 0, view = 0;
    u8 animating = 1;
    int layer = 0;
    float port[4] = {};
    Box box;
    View now;
    if (!root || !safemem::Read(root, &vt, 4) || !exe::VtNavMap.is(vt) || !safemem::Read(root + kView, &view, 4) || !safemem::Read(root + kAnimating, &animating, 1) ||
        !ReadF(root + kZoom, now.zoom) || !ReadF(root + kCentre, now.cx) || !ReadF(root + kCentre + 4, now.cy) || !ReadF(root + kBoxW, box.w) || !ReadF(root + kBoxH, box.h) ||
        !ReadF(root + kPortX, port[0]) || !ReadF(root + kPortY, port[1]) || !ReadF(root + kPortW, port[2]) || !ReadF(root + kPortH, port[3]))
        return false;
    if (exe::NavMapMode.ok())
        safemem::Read(static_cast<u32>(exe::NavMapMode.get()), &layer, 4);
    if (root != g_root)
        g_root = root, g_motion = Motion(), g_askZoom = 0, g_askU = g_askT = 0;
    if (g_frameW > 0)
        box.w = g_frameW, box.h = g_frameH;
    g_mapView = static_cast<int>(view);
    if (animating || (view != 1 && view != 2) || layer == 4 || layer == 6 || layer == 7 || !(box.w > 0.01f) || !(box.h > 0.01f) || !(port[2] > 8) || !(port[3] > 8) || !(now.zoom > 0))
    {
        g_motion = Motion();
        g_motion.target = std::log(std::max(now.zoom, kMinZoom));
        return false;
    }
    g_movable = true;
    if (view == 1)
        now.zoom = kMinZoom, now.cx = now.cy = 0;
    g_widest = now.zoom <= kBackToSystem;
    // the game moved it (its zoom buttons, its mouse drag): from where it stands
    // (the game's mouse drag counts in its own box: on the full-screen map its step is too long by the box against what the camera shows)
    float gameW = 0, gameH = 0;
    if (g_frameW > 0 && std::fabs(now.zoom - g_view.zoom) <= 1e-4f && ReadF(root + kBoxW, gameW) && ReadF(root + kBoxH, gameH) && gameW > 0 && gameH > 0)
    {
        now.cx = g_view.cx + (now.cx - g_view.cx) * g_frameW / gameW;
        now.cy = g_view.cy + (now.cy - g_view.cy) * g_frameH / gameH;
    }
    const bool gameMoved = std::fabs(now.zoom - g_view.zoom) > 1e-4f || std::fabs(now.cx - g_view.cx) > 1e-5f || std::fabs(now.cy - g_view.cy) > 1e-5f;
    if (std::fabs(now.zoom - g_view.zoom) > 1e-4f)
        g_motion.target = std::log(std::max(now.zoom, kMinZoom));
    g_view = now;
    Drive d = in;
    if (g_askZoom > 0)
    {
        g_motion.target = std::log(std::min(kMaxZoom, std::max(kMinZoom, g_askZoom)));
        g_askZoom = 0;
    }
    d.dragU += g_askU, d.dragT += g_askT;
    g_askU = g_askT = 0;
    bool centred = false;
    if (g_askCentre)
    {
        // the point to the viewport's middle: the camera says where it stands, C moves it there; from the widest zoom, some way in, or there is nothing to show
        g_askCentre = false, centred = true;
        float mapScale = 1, sx = 0, sy = 0;
        ReadF(root + kMapScale, mapScale); // (the records' coordinates are the world's by this)
        const float f = PixelsPerBox(root);
        if (g_view.zoom < kFindZoom)
        {
            g_view.zoom = kFindZoom, g_motion.target = std::log(kFindZoom);
            WriteF(root + kZoom, g_view.zoom);
        }
        if (f > 0 && Project(root, g_centreX * mapScale, g_centreZ * mapScale, sx, sy))
        {
            // (the projection above stands at the last frame's C and this Z: its offset from the viewport's middle is what C has to take away)
            g_view.cx += (port[0] + port[2] / 2 - sx) / (f * g_view.zoom);
            g_view.cy -= (port[1] + port[3] / 2 - sy) / (f * g_view.zoom);
        }
        Clamp(g_view, box);
    }
    float u = (cursorX - port[0]) / port[2], t = (cursorY - port[1]) / port[3];
    if (u < 0 || u > 1 || t < 0 || t > 1)
        u = t = 0.5f; // (the cursor on a panel: the map's centre)
    if (!Step(g_motion, g_view, box, d, u, t, dt) && !gameMoved && !centred)
        return false;
    const bool sector = g_frameW > 0 || g_view.zoom > kBackToSystem || g_motion.target > std::log(kBackToSystem);
    const int want = sector ? 2 : 1;
    if (g_view.zoom <= kBackToSystem && g_motion.target <= std::log(kBackToSystem))
        g_view = View(), g_motion = Motion();
    Store(root, g_view, want);
    Place(root, SectorAlpha(g_view.zoom));
    if (!(g_frameW > 0)) // (the full-screen map draws its own sectors)
        PlaceGrid(root, g_view);
    g_widest = g_view.zoom <= kBackToSystem;
    return true;
}

void SetFrame(float boxW, float boxH) { g_frameW = boxW, g_frameH = boxH; }

void Replace(u32 root)
{
    float z = 1;
    if (ReadF(root + kZoom, z))
        Place(root, SectorAlpha(z));
}

void Settle(u32 root)
{
    u32 view = 0;
    u8 animating = 1;
    int layer = 0;
    if (exe::NavMapMode.ok())
        safemem::Read(static_cast<u32>(exe::NavMapMode.get()), &layer, 4);
    if (!(g_frameW > 0) || !safemem::Read(root + kView, &view, 4) || !safemem::Read(root + kAnimating, &animating, 1) || view != 1 || animating || layer == 4 || layer == 6 || layer == 7)
        return;
    g_mapView = 1;
    Store(root, View(), 2);
    Place(root, 0);
}

bool OnMap(u32 root, float x, float y)
{
    float port[4] = {};
    if (!root || !ReadF(root + kPortX, port[0]) || !ReadF(root + kPortY, port[1]) || !ReadF(root + kPortW, port[2]) || !ReadF(root + kPortH, port[3]))
        return false;
    return x >= port[0] && y >= port[1] && x < port[0] + port[2] && y < port[1] + port[3];
}

int WaypointAt(u32 root, float x, float y, float reach)
{
    int best = -1;
    float bestD = reach * reach;
    for (int i = 0; i < kMaxWaypoints; ++i)
    {
        u32 rec = 0;
        int type = -1;
        float wx = 0, wz = 0, px = 0, py = 0;
        if (!safemem::Read(root + kWaypointRecords + 4 * i, &rec, 4) || !rec)
            break;
        if (!safemem::Read(rec, &type, 4) || type != 3 || !ReadF(rec + 0x30, wx) || !ReadF(rec + 0x38, wz) || !Project(root, wx, wz, px, py))
            continue;
        const float d = (px - x) * (px - x) + (py - y) * (py - y);
        if (d < bestD)
            bestD = d, best = i;
    }
    return best;
}

static bool CameraProject(u32 camera, const float* in, float* out)
{
    __try
    {
        void** vt = *reinterpret_cast<void***>(camera);
        reinterpret_cast<void(__stdcall*)(u32, float*, float*, float*, const float*)>(vt[0x34 / 4])(camera, &out[0], &out[1], &out[2], in);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return true;
}

float PixelsPerBox(u32 root)
{
    const float a[3] = {0, 0, -0.55f}, b[3] = {0.1f, 0, -0.55f};
    float pa[3] = {}, pb[3] = {};
    return root && CameraProject(root + kCamera, a, pa) && CameraProject(root + kCamera, b, pb) ? (pb[0] - pa[0]) / 0.1f : 0;
}

bool Project(u32 root, float wx, float wz, float& sx, float& sy)
{
    float z = 1, c[3] = {}, box[3] = {}, s = 0, sz = 0;
    if (!root || !ReadF(root + kZoom, z) || !safemem::Read(root + kCentre, c, sizeof(c)) || !safemem::Read(root + kBoxW, box, sizeof(box)) || !ReadF(root + kScaleX, s) || !ReadF(root + kScaleZ, sz) ||
        !(s > 0) || !(sz > 0))
        return false;
    // 0x496600's place of a record, in the box
    const float at[3] = {c[0] * z - box[0] / 2 - box[0] / 2 * (z - 1) + (wx + s) / (2 * s) * box[0] * z, c[1] * z - box[1] / 2 - box[1] / 2 * (z - 1) + (1 - (sz + wz) / (2 * sz)) * box[1] * z,
                         -0.55f + (c[2] * z - box[2] / 2 - box[2] / 2 * (z - 1) + box[2] * z)};
    float out[3] = {};
    if (!CameraProject(root + kCamera, at, out))
        return false;
    sx = out[0], sy = out[1];
    return true;
}

bool ProjectWorld(u32 root, float x, float z, float& sx, float& sy)
{
    float mapScale = 1;
    return ReadF(root + kMapScale, mapScale) && Project(root, x * mapScale, z * mapScale, sx, sy); // (the records' coordinates are the world's by this)
}

static u32 ShownSystem(u32 root)
{
    __try
    {
        const u32 shown = *reinterpret_cast<const u32*>(root + 0x7E0);
        return shown ? reinterpret_cast<u32(__thiscall*)(u32)>((*reinterpret_cast<void***>(shown))[0])(shown) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

bool ShowsShipSystem(u32 root)
{
    u32 view = 0;
    if (!root || !safemem::Read(root + kView, &view, 4) || view == 0 || !exe::PlayerSystem.ok())
        return false;
    const u32 shown = ShownSystem(root);
    return shown && shown == Mem<u32>(exe::PlayerSystem);
}

bool ObjectAt(u32 root, float x, float y, float reach, float* world, u32& id)
{
    u32 head = 0, node = 0;
    float mapScale = 1, bestD = reach * reach;
    bool found = false;
    if (!root || !safemem::Read(root + 0x400, &head, 4) || !head || !safemem::Read(head, &node, 4) || !ReadF(root + kMapScale, mapScale) || !(mapScale > 0))
        return false;
    for (int i = 0; node && node != head && i < 400; ++i)
    {
        u32 rec = 0, ids = 0;
        int type = -1;
        u8 visible = 0;
        float pos[3] = {}, px = 0, py = 0;
        if (!safemem::Read(node + 8, &rec, 4) || !safemem::Read(node, &node, 4))
            break;
        if (!safemem::Read(rec, &type, 4) || type != 0 || !safemem::Read(rec + 0x4C, &ids, 4) || !ids || !safemem::Read(rec + 0x48, &visible, 1) || !visible ||
            !safemem::Read(rec + 0x30, pos, sizeof(pos)) || !Project(root, pos[0], pos[2], px, py))
            continue;
        const float d = (px - x) * (px - x) + (py - y) * (py - y);
        if (d < bestD)
        {
            bestD = d, found = true;
            world[0] = pos[0] / mapScale, world[1] = pos[1] / mapScale, world[2] = pos[2] / mapScale;
            id = 0;
            safemem::Read(rec + 0x70, &id, 4);
        }
    }
    return found;
}

int Objects(u32 root, float* xy, int max)
{
    u32 head = 0, node = 0;
    float port[4] = {};
    if (!root || !safemem::Read(root + 0x400, &head, 4) || !head || !safemem::Read(head, &node, 4) || !ReadF(root + kPortX, port[0]) || !ReadF(root + kPortY, port[1]) || !ReadF(root + kPortW, port[2]) ||
        !ReadF(root + kPortH, port[3]))
        return 0;
    int n = 0;
    for (int i = 0; node && node != head && i < 400 && n < max; ++i)
    {
        u32 rec = 0, ids = 0, flags = 0;
        int type = -1;
        u8 visible = 0;
        float wx = 0, wz = 0, px = 0, py = 0;
        if (!safemem::Read(node + 8, &rec, 4) || !safemem::Read(node, &node, 4))
            break;
        // (what the game itself names: docs\FREELANCER_RE.md "The nav map's labels and records")
        if (!safemem::Read(rec, &type, 4) || type != 0 || !safemem::Read(rec + 4, &flags, 4) || !safemem::Read(rec + 0x4C, &ids, 4) || !ids || !safemem::Read(rec + 0x48, &visible, 1) || !visible ||
            !(flags & (2u | 0xC40u | 0x200101u)) || !ReadF(rec + 0x30, wx) || !ReadF(rec + 0x38, wz) || !Project(root, wx, wz, px, py))
            continue;
        if (px >= port[0] && py >= port[1] && px < port[0] + port[2] && py < port[1] + port[3])
            xy[2 * n] = px, xy[2 * n + 1] = py, ++n;
    }
    return n;
}

void CentreOn(float x, float z) { g_askCentre = true, g_centreX = x, g_centreZ = z; }

bool Movable() { return g_movable; }
bool AtWidest() { return g_widest; }

void Closed()
{
    g_root = 0, g_view = View(), g_motion = Motion(), g_movable = false, g_widest = true;
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "zoom")
        in >> g_askZoom;
    else if (what == "pan")
        in >> g_askU >> g_askT;
    std::ostringstream o;
    if (what == "rings") // every trade lane ring of the system: what the game knows of it, what FLvibed names it
    {
        int n = 0, unknown = 0;
        for (const space::Contact& c : space::Contacts())
            if (c.type & space::kTradelaneRing)
            {
                const bool u = hudinfo::Undiscovered(c.id);
                ++n, unknown += u ? 1 : 0;
                o << c.id << " unknown=" << u << " name='" << text::Utf8(hudinfo::DisplayName(c.id)) << "' route='" << text::Utf8(hudinfo::RouteName(c.id)) << "'\n";
            }
        o << "rings " << n << " unknown " << unknown << "\n";
    }
    if (what == "records" && g_root)
    {
        // each record the game names: its place, where the camera shows it, its radius
        u32 head = 0, node = 0;
        safemem::Read(g_root + 0x400, &head, 4);
        if (head)
            safemem::Read(head, &node, 4);
        for (int i = 0; node && node != head && i < 80; ++i)
        {
            u32 rec = 0, ids = 0, flags = 0;
            int type = -1;
            float wx = 0, wz = 0, r = 0, px = 0, py = 0;
            if (!safemem::Read(node + 8, &rec, 4) || !safemem::Read(node, &node, 4))
                break;
            u8 visible = 0;
            safemem::Read(rec, &type, 4), safemem::Read(rec + 4, &flags, 4), safemem::Read(rec + 0x4C, &ids, 4), safemem::Read(rec + 0x48, &visible, 1);
            ReadF(rec + 0x30, wx), ReadF(rec + 0x38, wz), ReadF(rec + 0x3C, r);
            Project(g_root, wx, wz, px, py);
            if (ids || type == 3)
                o << "type=" << type << " ids=" << ids << " vis=" << static_cast<int>(visible) << " flags=0x" << std::hex << flags << std::dec << " world=" << wx << "," << wz << " r=" << r << " camera=" << px << "," << py << "\n";
        }
    }
    o << "navview shipSystem=" << ShowsShipSystem(g_root) << " view=" << g_mapView << " zoom=" << g_view.zoom << " c=" << g_view.cx << "," << g_view.cy << " target=" << std::exp(g_motion.target) << " movable=" << g_movable << " widest=" << g_widest;
    return o.str();
}
} // namespace navview
