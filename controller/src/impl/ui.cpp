#include "ui.h"

#include "../fl.h"
#include "debug.h"
#include "log.h"

namespace ui
{
// UI camera exe::UiCamera (see Freelancer.exe 0x4BDD40): pixel = center + scale * (pos.xy / -pos.z)
static float UiCam(uintptr_t offset) { return Mem<float>(exe::UiCamera, offset); }

// FlControl layout (shared by the 46-entry-vtable control family)
constexpr u32 OFS_PARENT = 0x04;
constexpr u32 OFS_SIBLING = 0x08;
constexpr u32 OFS_CHILD = 0x0C;
constexpr u32 OFS_NAME = 0x18;
constexpr u32 OFS_STATE = 0x6C;
constexpr u32 OFS_POS = 0xA8;
constexpr u32 OFS_BOX = 0xB4;
bool Project(float x, float y, float z, float& sx, float& sy)
{
    if (z > -0.01f || !exe::UiCamera.ok())
        return false;
    const float cx = UiCam(0x68), cy = UiCam(0x6C);
    const float kx = UiCam(0x78), ky = UiCam(0x7C);
    sx = cx + kx * (x / -z);
    sy = cy + ky * (y / -z);
    return true;
}

static bool Read(uintptr_t a, void* out, size_t n) { return debug::SafeRead(a, out, n); }

// Control classes (by vtable) that are hit-testable but never clickable.
static bool IsPassiveClass(u32 vt)
{
    return exe::VtFrontLabel.is(vt) || exe::VtText.is(vt) || exe::VtTitleText.is(vt) || exe::VtImage.is(vt);
}

static void Walk(uintptr_t ctl, uintptr_t root, float ox, float oy, float oz, int depth, std::vector<Item>& out,
                 int& budget)
{
    // Children are a sibling chain; this walks `ctl` and its siblings.
    uintptr_t prev = 0;
    int guard = 0;
    while (ctl && ctl != prev && budget-- > 0 && guard++ < 400)
    {
        u32 state = 0, child = 0, sibling = 0;
        float pos[3] = {}, box[3] = {};
        if (!Read(ctl + OFS_STATE, &state, 4) || !Read(ctl + OFS_CHILD, &child, 4) ||
            !Read(ctl + OFS_SIBLING, &sibling, 4))
            return;
        Read(ctl + OFS_POS, pos, 12);
        Read(ctl + OFS_BOX, box, 12);

        const bool visible = (state & 0x03) == 0x03;
        u32 vtable = 0;
        Read(ctl, &vtable, 4);
        if (visible && exe::VtFrontButton.is(vtable)) // main menu buttons (different class layout)
        {
            // Main-menu style buttons keep a 4x3 transform (translation at +0x60) and a hit
            // rectangle (width, height) at +0xB4 that extends right and down from the translation.
            float t[3] = {}, b[2] = {};
            Read(ctl + 0x60, t, 12);
            Read(ctl + OFS_BOX, b, 8);
            float sx, sy;
            if (b[0] > 0 && Project(t[0] + b[0] * 0.5f, t[1] - b[1] * 0.5f, t[2], sx, sy))
            {
                Item it;
                it.ctl = ctl;
                it.root = root;
                it.x = sx;
                it.y = sy;
                const float kx = fabsf(UiCam(0x78)), ky = fabsf(UiCam(0x7C));
                it.hw = kx * b[0] * 0.5f / -t[2];
                it.hh = ky * b[1] * 0.5f / -t[2];
                it.depth = -t[2];
                strcpy_s(it.name, "FrontButton");
                out.push_back(it);
            }
        }
        else if (visible)
        {
            // The control's world position: its parent frame's transform (the 4x3 at +0x3C: a 3x3 at +0x3C and the
            // translation at +0x60 - the window classes NN_Dealer / NN_Inventory / NN_MissionVendor ... place themselves
            // absolutely and stamp that frame into every child; their own +0xA8 stays 0 and their parent is an unrelated
            // button) applied to its own +0xA8, when it has one; else the parents' +0xA8 chain (the HUD, the NeuroNet
            // buttons: their frames are 0). Verified 25 Oct 2026 against the game's hit test: the dealer's and the
            // player's rows, the filters, the close and buy buttons all land where the game hits them (the chain alone
            // put the player's rows at the screen centre and the dealer's off screen; the probe had to fill them in).
            float frame[12] = {};
            float ax = ox + pos[0], ay = oy + pos[1], az = oz + pos[2];
            if (Read(ctl + 0x3C, frame, sizeof(frame)) && (frame[9] != 0 || frame[10] != 0 || frame[11] != 0))
            {
                ax = frame[9] + frame[0] * pos[0] + frame[1] * pos[1] + frame[2] * pos[2];
                ay = frame[10] + frame[3] * pos[0] + frame[4] * pos[1] + frame[5] * pos[2];
                az = frame[11] + frame[6] * pos[0] + frame[7] * pos[1] + frame[8] * pos[2];
            }
            const bool enabled = (state & 0x08) != 0;
            // (passive image / text classes are hit-testable but never clickable: skipped here as CollectAll skips them
            // among the probe's results - a frame around a window otherwise counts as its item)
            if (enabled && box[0] > 0.0001f && box[1] > 0.0001f && !IsPassiveClass(vtable))
            {
                float sx, sy;
                if (Project(ax, ay, az, sx, sy))
                {
                    Item it;
                    it.ctl = ctl;
                    it.root = root;
                    it.x = sx;
                    it.y = sy;
                    const float kx = fabsf(UiCam(0x78)), ky = fabsf(UiCam(0x7C));
                    it.hw = kx * box[0] / -az;
                    it.hh = ky * box[1] / -az;
                    it.depth = -az;
                    Read(ctl + OFS_NAME, it.name, 32);
                    it.name[32] = 0;
                    const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());
                    if (it.x >= 0 && it.y >= 0 && it.x <= w && it.y <= h)
                        out.push_back(it);
                }
            }
            if (child && depth < 40)
                Walk(child, root, ax, ay, az, depth + 1, out, budget);
        }
        prev = ctl;
        ctl = sibling;
    }
}

std::vector<Item> Collect()
{
    std::vector<Item> out;
    int budget = 6000;
    u32 node = Mem<u32>(exe::UiControls);
    int seen = 0;
    while (node && seen++ < 20000)
    {
        u32 next = 0, ctl = 0, parent = 1;
        if (!Read(node + 4, &next, 4) || !Read(node + 8, &ctl, 4))
            break;
        if (ctl && Read(ctl + OFS_PARENT, &parent, 4) && parent == 0)
        {
            // Walk only this root (not its siblings, which are other roots or junk).
            u32 state = 0, child = 0;
            float pos[3] = {};
            Read(ctl + OFS_STATE, &state, 4);
            Read(ctl + OFS_CHILD, &child, 4);
            Read(ctl + OFS_POS, pos, 12);
            if ((state & 0x03) == 0x03 && child)
                Walk(child, ctl, pos[0], pos[1], pos[2], 1, out, budget);
        }
        node = next;
    }
    return out;
}

uintptr_t Hovered() { return Mem<uintptr_t>(exe::HoveredControl); }

uintptr_t ModalRoot(const std::vector<Item>& items)
{
    // Freelancer's keyboard focus handler (exe::KeyFocus) is a sub-object embedded in the window
    // that grabbed focus; the owning window is the closest root object below that pointer.
    const uintptr_t focus = Mem<uintptr_t>(exe::KeyFocus);
    if (!focus)
        return 0;
    uintptr_t best = 0;
    for (const Item& it : items)
    {
        if (it.root <= focus && focus - it.root < 0x4000 && it.root > best)
            best = it.root;
    }
    return best;
}

int FindAt(const std::vector<Item>& items, float x, float y)
{
    int best = -1;
    float bestDepth = 1e9f;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const Item& it = items[i];
        if (fabsf(x - it.x) <= it.hw && fabsf(y - it.y) <= it.hh && it.depth < bestDepth)
        {
            best = static_cast<int>(i);
            bestDepth = it.depth;
        }
    }
    return best;
}

int FindInDirection(const std::vector<Item>& items, float fx, float fy, float dx, float dy, uintptr_t exclude)
{
    // Spatial navigation on rectangles: measure to the nearest point of each control (large
    // controls such as lists are reachable even when their center is far away), prefer
    // controls ahead in the requested direction and penalize sideways offset.
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-4f)
        return -1;
    dx /= len;
    dy /= len;
    int best = -1;
    float bestScore = 1e30f;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const Item& it = items[i];
        if (it.ctl == exclude)
            continue;
        if ((it.x - fx) * dx + (it.y - fy) * dy < 4.0f)
            continue; // center not ahead
        const float nx = Clampf(fx, it.x - it.hw, it.x + it.hw), ny = Clampf(fy, it.y - it.hh, it.y + it.hh);
        const float vx = nx - fx, vy = ny - fy;
        const float forward = vx * dx + vy * dy;
        if (forward < 4.0f)
            continue; // the cursor is already level with this control in that direction
        const float side = fabsf(vx * -dy + vy * dx);
        const float score = forward + side * 2.5f;
        if (score < bestScore)
        {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

uintptr_t HitTest(float x, float y)
{
    // Same ray Freelancer builds for its cursor (0x4BDD40 + camera rotation + normalize).
    if (!exe::UiCamera.ok())
        return 0;
    const float cx = UiCam(0x68), cy = UiCam(0x6C);
    const float sx = UiCam(0x78), sy = UiCam(0x7C);
    const float n = UiCam(0xA4);
    const float d[3] = {(x - cx) * n / sx, (y - cy) * n / sy, -n};
    const float* m = reinterpret_cast<const float*>(exe::UiCamera.get() + 4); // 3x3 rotation
    float r[3];
    for (int i = 0; i < 3; ++i)
        r[i] = m[i * 3 + 0] * d[0] + m[i * 3 + 1] * d[1] + m[i * 3 + 2] * d[2];
    const float len = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    if (len > 0)
        for (float& v : r)
            v /= len;
    typedef uintptr_t(__cdecl * HitTestFn)(const float* ray);
    return hk::Call<HitTestFn>(exe::HitTest, static_cast<const float*>(r));
}

std::vector<Item> Probe(float step)
{
    perf::Scope ps("ui.Probe");
    struct Box
    {
        float x0, y0, x1, y1;
    };
    std::vector<std::pair<uintptr_t, Box>> found;
    const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());
    for (float y = step * 0.5f; y < h; y += step)
    {
        for (float x = step * 0.5f; x < w; x += step)
        {
            const uintptr_t c = HitTest(x, y);
            if (!c)
                continue;
            auto it = std::find_if(found.begin(), found.end(), [c](const auto& p) { return p.first == c; });
            if (it == found.end())
                found.push_back({c, {x, y, x, y}});
            else
            {
                it->second.x0 = std::min(it->second.x0, x);
                it->second.y0 = std::min(it->second.y0, y);
                it->second.x1 = std::max(it->second.x1, x);
                it->second.y1 = std::max(it->second.y1, y);
            }
        }
    }
    std::vector<Item> out;
    for (const auto& f : found)
    {
        Item it;
        it.ctl = f.first;
        it.x = (f.second.x0 + f.second.x1) * 0.5f;
        it.y = (f.second.y0 + f.second.y1) * 0.5f;
        it.hw = (f.second.x1 - f.second.x0) * 0.5f + step * 0.5f;
        it.hh = (f.second.y1 - f.second.y0) * 0.5f + step * 0.5f;
        Read(f.first + OFS_NAME, it.name, 32);
        it.name[32] = 0;
        // Find the root window of this control.
        uintptr_t r = f.first, parent = 0;
        for (int guard = 0; guard < 64 && Read(r + OFS_PARENT, &parent, 4) && parent; ++guard)
            r = parent;
        it.root = r;
        out.push_back(it);
    }
    return out;
}

bool IsList(uintptr_t ctl)
{
    float rowH, firstRow;
    return ListRows(ctl, rowH, firstRow);
}

bool ListRows(uintptr_t ctl, float& rowH, float& firstRowY)
{
    // Freelancer's UI scales with the screen height around the screen center, so row metrics are
    // fractions of the height relative to the center (measured at 3840x2160, HDE 0.7.1). The row pitch is 0.029 H on
    // both lists (25 Oct 2026: the 0.0318 H the front list had was a hit-cell value of another layout and drifted a
    // row after a dozen rows; ListHoveredRow re-anchors). Measured through the lists' own hovered-row field with the
    // cursor swept down the list: the front list's hit rows start 0.2639 H above the middle and are 0.0293 H tall
    // (row 0 = 0.2639 .. 0.2343, centre 0.2491 = the drawn text's centre); the in-game list's start 0.2593 H above
    // (row 0 centre 0.2447; its text centre 0.2246 lies 0.7 row below the hit cell's).
    u32 vt = 0;
    Read(ctl, &vt, 4);
    float row, first;
    if (exe::VtFrontList.is(vt)) // front end (load game, new character)
    {
        row = 0.0290f;
        first = -0.2491f;
    }
    else if (exe::VtGameList.is(vt)) // in-game load/save
    {
        row = 0.0290f;
        first = -0.2447f;
    }
    else
        return false;
    const float h = static_cast<float>(fl::ScreenHeight());
    rowH = row * h;
    firstRowY = h * 0.5f + first * h;
    return true;
}

int ListHoveredRow(uintptr_t ctl)
{
    u32 vt = 0, hovered = 0xFFFFFFFFu;
    if (!Read(ctl, &vt, 4) || !(exe::VtFrontList.is(vt) || exe::VtGameList.is(vt)) || !Read(ctl + 0x7C, &hovered, 4))
        return -1;
    return static_cast<int>(hovered);
}

static std::vector<Item> CollectAllFrom(std::vector<Item> items)
{
    perf::Scope ps("ui.CollectAll");
    // A tree item another control covers at its centre (the game's own hit test names that other control there) is not
    // reachable by the cursor: dropped, so the probe's result for that spot stands (25 Oct 2026: the dealers' invisible
    // owner button NNInventoryButton, 40 px, lies under the player's second row).
    items.erase(std::remove_if(items.begin(), items.end(),
                               [](const Item& it)
                               {
                                   const uintptr_t hit = HitTest(it.x, it.y);
                                   return hit && hit != it.ctl;
                               }),
                items.end());
    const float step = std::max(24.0f, fl::ScreenHeight() / 40.0f);
    for (const Item& p : Probe(step))
    {
        u32 vt = 0;
        Read(p.ctl, &vt, 4);
        if (IsPassiveClass(vt))
            continue;
        if (std::find_if(items.begin(), items.end(), [&](const Item& i) { return i.ctl == p.ctl; }) != items.end())
            continue;
        items.push_back(p);
    }

    // Window bodies and panels are hit-testable too, but clicking them does nothing: drop
    // any item whose rectangle encloses a much smaller item of the same window.
    std::vector<Item> out;
    out.reserve(items.size());
    for (const Item& a : items)
    {
        bool container = false;
        for (const Item& b : items)
        {
            if (&a == &b || b.root != a.root || IsList(a.ctl))
                continue;
            if (fabsf(b.x - a.x) < a.hw && fabsf(b.y - a.y) < a.hh && b.hw * b.hh < a.hw * a.hh * 0.5f)
            {
                container = true;
                break;
            }
        }
        if (!container)
            out.push_back(a);
    }
    return out;
}

std::vector<Item> CollectAll() { return CollectAllFrom(Collect()); }

// The tree walk's clickable set as one number: every item's control, root and rectangle (whole pixels), and the screen.
static uint64_t TreeSignature(const std::vector<Item>& tree)
{
    uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](uint64_t v)
    {
        h ^= v;
        h *= 1099511628211ull;
    };
    mix(static_cast<uint64_t>(fl::ScreenWidth()) << 32 | static_cast<u32>(fl::ScreenHeight()));
    for (const Item& it : tree)
    {
        mix(it.ctl);
        mix(it.root);
        mix(static_cast<uint64_t>(static_cast<u32>(lroundf(it.x))) << 32 | static_cast<u32>(lroundf(it.y)));
        mix(static_cast<uint64_t>(static_cast<u32>(lroundf(it.hw))) << 32 | static_cast<u32>(lroundf(it.hh)));
    }
    return h;
}

const std::vector<Item>& CollectAllCached()
{
    static std::vector<Item> cached;
    static uint64_t sig = 0;
    static ULONGLONG at = 0;
    static bool have = false;
    perf::Scope ps("ui.CollectAllCached");
    const ULONGLONG now = GetTickCount64();
    std::vector<Item> tree = Collect();
    const uint64_t h = TreeSignature(tree);
    if (!have || now - at >= 2000 || (h != sig && now - at >= 50))
    {
        cached = CollectAllFrom(std::move(tree));
        sig = h, at = now, have = true;
    }
    return cached;
}

const char* ControlName(uintptr_t ctl)
{
    static char name[33];
    name[0] = 0;
    if (ctl)
        Read(ctl + OFS_NAME, name, 32);
    name[32] = 0;
    return name;
}

uintptr_t FindControl(const char* name) { return hk::Call<uintptr_t(__cdecl*)(const char*)>(exe::FindControl, name); }
} // namespace ui

namespace ui
{
bool ControlRect(uintptr_t ctl, Item& out)
{
    if (!ctl)
        return false;
    // the chain from the root down (Walk's base for each level: the parents' positions added, a frame replacing them)
    std::vector<uintptr_t> chain;
    for (uintptr_t c = ctl; c && chain.size() < 64; )
    {
        chain.push_back(c);
        u32 parent = 0;
        if (!Read(c + OFS_PARENT, &parent, 4))
            return false;
        c = parent;
    }
    float ax = 0, ay = 0, az = 0;
    for (size_t i = chain.size(); i-- > 0;)
    {
        const uintptr_t c = chain[i];
        float pos[3] = {}, frame[12] = {};
        if (!Read(c + OFS_POS, pos, 12))
            return false;
        if (Read(c + 0x3C, frame, sizeof(frame)) && (frame[9] != 0 || frame[10] != 0 || frame[11] != 0))
        {
            ax = frame[9] + frame[0] * pos[0] + frame[1] * pos[1] + frame[2] * pos[2];
            ay = frame[10] + frame[3] * pos[0] + frame[4] * pos[1] + frame[5] * pos[2];
            az = frame[11] + frame[6] * pos[0] + frame[7] * pos[1] + frame[8] * pos[2];
        }
        else
            ax += pos[0], ay += pos[1], az += pos[2];
    }
    float box[3] = {};
    if (!Read(ctl + OFS_BOX, box, 12))
        return false;
    float sx, sy;
    if (!Project(ax, ay, az, sx, sy))
        return false;
    out = Item();
    out.ctl = ctl;
    out.root = chain.back();
    out.x = sx;
    out.y = sy;
    const float kx = fabsf(UiCam(0x78)), ky = fabsf(UiCam(0x7C));
    out.hw = kx * box[0] / -az;
    out.hh = ky * box[1] / -az;
    out.depth = -az;
    Read(ctl + OFS_NAME, out.name, 32);
    out.name[32] = 0;
    return true;
}

std::vector<uintptr_t> VisibleRoots()
{
    std::vector<uintptr_t> out;
    u32 node = Mem<u32>(exe::UiControls);
    for (int seen = 0; node && seen < 20000; ++seen)
    {
        u32 next = 0, ctl = 0, parent = 1, state = 0;
        if (!Read(node + 4, &next, 4) || !Read(node + 8, &ctl, 4))
            break;
        if (ctl && Read(ctl + OFS_PARENT, &parent, 4) && parent == 0 && Read(ctl + OFS_STATE, &state, 4) &&
            (state & 0x03) == 0x03)
            out.push_back(ctl);
        node = next;
    }
    return out;
}
} // namespace ui
