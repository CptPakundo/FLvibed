// The model probe (modelprobe.h): the verb's squares asked of the model slot each frame, in the grid the experiment drew
// (four to a row, 8 pixels apart, centred on the viewport unless placed), turning at 0.8 radians a second, each half a radian
// ahead of the one before. The slot owns the holders, the camera, the lights and the draw.
#include "modelprobe.h"
#include "common.h"
#include "flexe.h"
#include "modelslot.h"

namespace modelprobe
{
namespace
{
constexpr int kMax = modelslot::kSlots;

bool g_on = false;
std::string g_file = "equipment\\models\\commodities\\nn_icons\\EQUIPICON_gun.3db";
int g_x = -1, g_y = -1, g_size = 200, g_count = 1;
modelslot::Model g_look = [] {
    modelslot::Model m;
    m.fill = 0.85f; // the experiment's default (a whole ship fits at 0.6: 48_ 4)
    m.spin = 0.8f;  // about 46 degrees a second
    return m;
}();
// One key per square: a new file reloads its holder, the same file keeps it (the slot's rule)
unsigned Key(int i) { return modelslot::KeyOf("modelprobe", i); }

void DropFrom(int first)
{
    for (int i = first; i < kMax; ++i)
        modelslot::Drop(Key(i));
}
} // namespace

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (!TestSwitch())
        return "modelprobe: only with the test switch";
    if (what == "on")
    {
        std::string file;
        if (in >> file && file != "-") // "-" keeps the file (to give a place only)
            g_file = file;
        int x = -1, y = -1, size = 0, count = 0;
        if (in >> x >> y)
            g_x = x, g_y = y;
        if (in >> size)
            g_size = size < 16 ? 16 : size;
        if (in >> count)
            g_count = count < 1 ? 1 : count > kMax ? kMax : count;
        DropFrom(g_count); // the squares past a smaller count
        g_on = true;
        modelslot::ResetCost();
    }
    else if (what == "off")
        g_on = false, DropFrom(0);
    else if (what == "set")
    {
        std::string k, v;
        in >> k >> v;
        const float f = static_cast<float>(atof(v.c_str()));
        if (k == "flags")
            g_look.flags = static_cast<unsigned>(strtoul(v.c_str(), nullptr, 0));
        else if (k == "fov")
            g_look.fov = std::min(89.0f, std::max(1.0f, f));
        else if (k == "fill")
            g_look.fill = std::min(2.0f, std::max(0.05f, f));
        else if (k == "ambient")
            g_look.ambient = atoi(v.c_str());
        else if (k == "pitch")
            g_look.pitch = f;
        else if (k == "spin")
            g_look.spin = f;
        else
            return "modelprobe set flags|fov|fill|ambient|pitch|spin <v>";
        modelslot::ResetCost();
    }
    const modelslot::Stats& s = modelslot::GetStats();
    int handle = -1, loaded = 0;
    double loadMs = 0;
    float a[4] = {}, b[4] = {}, dist = 0;
    modelslot::SlotInfo(Key(0), &handle, &loadMs, a, b, &dist);
    for (int i = 0; i < g_count; ++i)
    {
        const modelslot::Status st = modelslot::StatusOf(Key(i));
        loaded += st == modelslot::Status::Ready || st == modelslot::Status::Hidden;
    }
    std::ostringstream o;
    o << "modelprobe on=" << g_on << " symbols=" << s.symbols << " file=" << g_file << " count=" << g_count << " loaded=" << loaded
      << " drawn=" << s.drawn << " handle=" << handle << " load_ms=" << loadMs << " boundsA=(" << a[0] << "," << a[1] << "," << a[2]
      << "," << a[3] << ") boundsB=(" << b[0] << "," << b[1] << "," << b[2] << "," << b[3] << ") dist=" << dist << " square=" << g_x
      << "," << g_y << "," << g_size << " fov=" << g_look.fov << " flags=0x" << std::hex << g_look.flags << std::dec
      << " ambient=" << g_look.ambient << " fill=" << g_look.fill << " pitch=" << g_look.pitch << " spin=" << g_look.spin
      << " frames=" << s.frames << " draw_ms_avg=" << s.usAvg / 1000.0 << " draw_ms_max=" << s.usMax / 1000.0
      << " draw_ms_last=" << s.usLast / 1000.0 << " vp_before=" << s.vpBefore[0] << "," << s.vpBefore[1] << "," << s.vpBefore[2]
      << "," << s.vpBefore[3] << " vp_after=" << s.vpAfter[0] << "," << s.vpAfter[1] << "," << s.vpAfter[2] << "," << s.vpAfter[3]
      << " d3d_after=" << s.d3dAfter[0] << "," << s.d3dAfter[1] << "," << s.d3dAfter[2] << "," << s.d3dAfter[3]
      << " vp_same=" << s.vpSame << " faults=" << s.faults << " slots_used=" << s.used << " holders=" << s.held
      << " loads=" << s.loads << " frees=" << s.frees << " failures=" << s.failures << " refused=" << s.refused
      << " passes=" << s.passes << " releases=" << s.releases << " last_release=" << s.lastWhy
      << " why=" << (s.why.empty() ? "-" : s.why);
    return o.str();
}

void Ask()
{
    if (!g_on || !exe::RenderGetInt.ok())
        return;
    int vp[4] = {};
    hk::Call<void(__cdecl*)(int, int*)>(exe::RenderGetInt, 0xBA2, vp);
    if (vp[2] <= 0 || vp[3] <= 0)
        return;
    const int size = std::min(g_size, std::min(vp[2], vp[3]));
    const int x0 = g_x >= 0 ? g_x : vp[0] + (vp[2] - size) / 2, y0 = g_y >= 0 ? g_y : vp[1] + (vp[3] - size) / 2;
    modelslot::Model m = g_look;
    m.file = g_file.c_str();
    for (int i = 0; i < g_count; ++i)
    {
        m.rect = {x0 + (i % 4) * (size + 8), y0 + (i / 4) * (size + 8), size, size};
        m.yaw = i * 0.5f;
        modelslot::Show(Key(i), m);
    }
}
} // namespace modelprobe
