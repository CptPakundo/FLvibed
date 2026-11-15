// The NPC icons' game-free half (GitHub 84 + 85; design audit 146_ "Ship icons v2", build 147_): a ship's top-down
// silhouette from its own model file (the UTF leaves of a .cmp / .3db: every part's Level0 VMeshRef placed by Cmpnd\Cons,
// its triangles projected on x / z, nose up), filled into a square mask, simplified as the v2 sheet did (slits between parts
// closed, a long hull's width raised to a floor), and baked to an alpha cell at the size it is drawn (thin parts kept about
// one pixel wide); and the rank tier of a pilot's name from the factions' rank words (faction_prop.ini rank_desig). Pure:
// tests\npcicons_test.cpp.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace npcicons
{
using byte = unsigned char;

// Freelancer's model CRC (VMeshRef's library id = this of the VMeshLibrary entry's lower-cased name; tools\mk3db.py fl_crc).
inline uint32_t ModelCrc(const std::string& name)
{
    static const uint32_t t[256] = {
        0x00000000, 0x09073096, 0x120E612C, 0x1B0951BA, 0xFF6DC419, 0xF66AF48F, 0xED63A535, 0xE46495A3,
        0xFEDB8832, 0xF7DCB8A4, 0xECD5E91E, 0xE5D2D988, 0x01B64C2B, 0x08B17CBD, 0x13B82D07, 0x1ABF1D91,
        0xFDB71064, 0xF4B020F2, 0xEFB97148, 0xE6BE41DE, 0x02DAD47D, 0x0BDDE4EB, 0x10D4B551, 0x19D385C7,
        0x036C9856, 0x0A6BA8C0, 0x1162F97A, 0x1865C9EC, 0xFC015C4F, 0xF5066CD9, 0xEE0F3D63, 0xE7080DF5,
        0xFB6E20C8, 0xF269105E, 0xE96041E4, 0xE0677172, 0x0403E4D1, 0x0D04D447, 0x160D85FD, 0x1F0AB56B,
        0x05B5A8FA, 0x0CB2986C, 0x17BBC9D6, 0x1EBCF940, 0xFAD86CE3, 0xF3DF5C75, 0xE8D60DCF, 0xE1D13D59,
        0x06D930AC, 0x0FDE003A, 0x14D75180, 0x1DD06116, 0xF9B4F4B5, 0xF0B3C423, 0xEBBA9599, 0xE2BDA50F,
        0xF802B89E, 0xF1058808, 0xEA0CD9B2, 0xE30BE924, 0x076F7C87, 0x0E684C11, 0x15611DAB, 0x1C662D3D,
        0xF6DC4190, 0xFFDB7106, 0xE4D220BC, 0xEDD5102A, 0x09B18589, 0x00B6B51F, 0x1BBFE4A5, 0x12B8D433,
        0x0807C9A2, 0x0100F934, 0x1A09A88E, 0x130E9818, 0xF76A0DBB, 0xFE6D3D2D, 0xE5646C97, 0xEC635C01,
        0x0B6B51F4, 0x026C6162, 0x196530D8, 0x1062004E, 0xF40695ED, 0xFD01A57B, 0xE608F4C1, 0xEF0FC457,
        0xF5B0D9C6, 0xFCB7E950, 0xE7BEB8EA, 0xEEB9887C, 0x0ADD1DDF, 0x03DA2D49, 0x18D37CF3, 0x11D44C65,
        0x0DB26158, 0x04B551CE, 0x1FBC0074, 0x16BB30E2, 0xF2DFA541, 0xFBD895D7, 0xE0D1C46D, 0xE9D6F4FB,
        0xF369E96A, 0xFA6ED9FC, 0xE1678846, 0xE860B8D0, 0x0C042D73, 0x05031DE5, 0x1E0A4C5F, 0x170D7CC9,
        0xF005713C, 0xF90241AA, 0xE20B1010, 0xEB0C2086, 0x0F68B525, 0x066F85B3, 0x1D66D409, 0x1461E49F,
        0x0EDEF90E, 0x07D9C998, 0x1CD09822, 0x15D7A8B4, 0xF1B33D17, 0xF8B40D81, 0xE3BD5C3B, 0xEABA6CAD,
        0xEDB88320, 0xE4BFB3B6, 0xFFB6E20C, 0xF6B1D29A, 0x12D54739, 0x1BD277AF, 0x00DB2615, 0x09DC1683,
        0x13630B12, 0x1A643B84, 0x016D6A3E, 0x086A5AA8, 0xEC0ECF0B, 0xE509FF9D, 0xFE00AE27, 0xF7079EB1,
        0x100F9344, 0x1908A3D2, 0x0201F268, 0x0B06C2FE, 0xEF62575D, 0xE66567CB, 0xFD6C3671, 0xF46B06E7,
        0xEED41B76, 0xE7D32BE0, 0xFCDA7A5A, 0xF5DD4ACC, 0x11B9DF6F, 0x18BEEFF9, 0x03B7BE43, 0x0AB08ED5,
        0x16D6A3E8, 0x1FD1937E, 0x04D8C2C4, 0x0DDFF252, 0xE9BB67F1, 0xE0BC5767, 0xFBB506DD, 0xF2B2364B,
        0xE80D2BDA, 0xE10A1B4C, 0xFA034AF6, 0xF3047A60, 0x1760EFC3, 0x1E67DF55, 0x056E8EEF, 0x0C69BE79,
        0xEB61B38C, 0xE266831A, 0xF96FD2A0, 0xF068E236, 0x140C7795, 0x1D0B4703, 0x060216B9, 0x0F05262F,
        0x15BA3BBE, 0x1CBD0B28, 0x07B45A92, 0x0EB36A04, 0xEAD7FFA7, 0xE3D0CF31, 0xF8D99E8B, 0xF1DEAE1D,
        0x1B64C2B0, 0x1263F226, 0x096AA39C, 0x006D930A, 0xE40906A9, 0xED0E363F, 0xF6076785, 0xFF005713,
        0xE5BF4A82, 0xECB87A14, 0xF7B12BAE, 0xFEB61B38, 0x1AD28E9B, 0x13D5BE0D, 0x08DCEFB7, 0x01DBDF21,
        0xE6D3D2D4, 0xEFD4E242, 0xF4DDB3F8, 0xFDDA836E, 0x19BE16CD, 0x10B9265B, 0x0BB077E1, 0x02B74777,
        0x18085AE6, 0x110F6A70, 0x0A063BCA, 0x03010B5C, 0xE7659EFF, 0xEE62AE69, 0xF56BFFD3, 0xFC6CCF45,
        0xE00AE278, 0xE90DD2EE, 0xF2048354, 0xFB03B3C2, 0x1F672661, 0x166016F7, 0x0D69474D, 0x046E77DB,
        0x1ED16A4A, 0x17D65ADC, 0x0CDF0B66, 0x05D83BF0, 0xE1BCAE53, 0xE8BB9EC5, 0xF3B2CF7F, 0xFAB5FFE9,
        0x1DBDF21C, 0x14BAC28A, 0x0FB39330, 0x06B4A3A6, 0xE2D03605, 0xEBD70693, 0xF0DE5729, 0xF9D967BF,
        0xE3667A2E, 0xEA614AB8, 0xF1681B02, 0xF86F2B94, 0x1C0BBE37, 0x150C8EA1, 0x0E05DF1B, 0x0702EF8D,
    };
    uint32_t c = 0xFFFFFFFFu;
    for (char ch : name)
    {
        const unsigned char b = static_cast<unsigned char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
        c = (c >> 8) ^ t[(c ^ b) & 0xFF];
    }
    return c ^ 0xFFFFFFFFu;
}

inline std::string Lower(std::string s)
{
    for (char& ch : s)
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    return s;
}
// "\a\b\c" -> {"a", "b", "c"}, lower case.
inline std::vector<std::string> PathParts(const std::string& path)
{
    std::vector<std::string> out;
    std::string cur;
    for (char ch : path)
        if (ch == '\\' || ch == '/')
        {
            if (!cur.empty())
                out.push_back(Lower(cur)), cur.clear();
        }
        else
            cur += ch;
    if (!cur.empty())
        out.push_back(Lower(cur));
    return out;
}
inline std::string CString(const std::vector<byte>& d, size_t o = 0, size_t max = 0x7FFFFFFF)
{
    std::string s;
    for (size_t i = o; i < d.size() && i - o < max && d[i]; ++i)
        s += static_cast<char>(d[i]);
    return Lower(s);
}
template <class T> inline bool ReadAt(const std::vector<byte>& d, size_t o, T& v)
{
    if (o + sizeof(T) > d.size())
        return false;
    memcpy(&v, d.data() + o, sizeof(T));
    return true;
}

struct Tri2
{
    float x[3], y[3]; // model x (right) and z (nose = -z: up on the screen)
};

// A pose: rotation (row-major, applied as m * v) and translation.
struct Pose
{
    float r[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    float t[3] = {0, 0, 0};
};
inline void Apply(const float m[9], const float v[3], float out[3])
{
    for (int i = 0; i < 3; ++i)
        out[i] = m[i * 3] * v[0] + m[i * 3 + 1] * v[1] + m[i * 3 + 2] * v[2];
}

// Every triangle of the model's Level0 meshes, top-down. `Leaf` has .path ("\Cmpnd\Root\File name") and .data (bytes):
// core\game\utf.h's utf::Leaf. Empty when nothing reads (no VMeshRef, no library, a bad offset: fails closed).
template <class Leaf> std::vector<Tri2> TopDown(const std::vector<Leaf>& leaves, int maxTris = 400000)
{
    std::vector<Tri2> out;
    std::map<uint32_t, const std::vector<byte>*> libs; // VMeshData by its library's CRC
    struct Con
    {
        std::string parent;
        float org[3], rot[9];
    };
    std::map<std::string, Con> cons;          // child part -> its joint
    std::map<std::string, std::string> objName, fileName; // Cmpnd\<part> -> object name, file name
    std::vector<std::pair<std::string, const std::vector<byte>*>> refs; // (top node, VMeshRef) in file order
    bool cmpnd = false;
    for (const Leaf& l : leaves)
    {
        const std::vector<std::string> p = PathParts(l.path);
        if (p.empty())
            continue;
        const std::string& last = p.back();
        if (last == "vmeshdata" && p.size() >= 2)
            libs[ModelCrc(p[p.size() - 2])] = &l.data;
        else if (p.size() == 3 && p[0] == "cmpnd" && p[1] == "cons")
        {
            const size_t size = last == "fix" ? 176 : (last == "rev" || last == "pris") ? 208 : last == "sph" ? 212 : 0;
            for (size_t o = 0; size && o + size <= l.data.size(); o += size)
            {
                Con c;
                c.parent = CString(l.data, o, 64);
                const std::string child = CString(l.data, o + 64, 64);
                memcpy(c.org, l.data.data() + o + 128, 12);
                memcpy(c.rot, l.data.data() + o + 140 + (last == "fix" ? 0 : 12), 36);
                cons[child] = c;
            }
        }
        else if (p.size() == 3 && p[0] == "cmpnd" && (last == "object name" || last == "file name"))
        {
            cmpnd = true;
            (last == "object name" ? objName : fileName)[p[1]] = CString(l.data);
        }
        else if (last == "vmeshref")
        {
            bool multi = false, level0 = false;
            for (const std::string& s : p)
                multi |= s.find("multilevel") != std::string::npos, level0 |= s.find("level0") != std::string::npos;
            if (level0 || !multi)
                refs.push_back({p[0], &l.data});
        }
    }
    // the parts: a .cmp's Cmpnd entries (object name -> its file node), a plain .3db one part at the origin
    std::vector<std::pair<std::string, std::string>> parts; // (object, top node)
    if (cmpnd)
    {
        for (const auto& [part, obj] : objName)
            if (fileName.count(part))
                parts.push_back({obj, fileName[part]});
    }
    else
        parts.push_back({"root", std::string()});
    auto world = [&](const std::string& obj) {
        Pose w;
        std::string cur = obj;
        for (int depth = 0; depth < 32; ++depth)
        {
            const auto it = cons.find(cur);
            if (it == cons.end())
                break;
            // w = joint * w (the parent's frame applied to what we have)
            Pose n;
            float m[9];
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    m[i * 3 + j] = it->second.rot[i * 3] * w.r[j] + it->second.rot[i * 3 + 1] * w.r[3 + j] +
                                   it->second.rot[i * 3 + 2] * w.r[6 + j];
            memcpy(n.r, m, sizeof m);
            Apply(it->second.rot, w.t, n.t);
            for (int i = 0; i < 3; ++i)
                n.t[i] += it->second.org[i];
            w = n;
            cur = it->second.parent;
        }
        return w;
    };
    for (const auto& [obj, node] : parts)
    {
        const std::vector<byte>* ref = nullptr;
        for (const auto& r : refs)
            if (node.empty() || r.first == node)
            {
                ref = r.second;
                break;
            }
        if (!ref)
            continue;
        uint32_t lib = 0;
        uint16_t sv = 0, vc = 0, si = 0, ic = 0, sm = 0, mc = 0;
        if (!ReadAt(*ref, 4, lib) || !ReadAt(*ref, 8, sv) || !ReadAt(*ref, 10, vc) || !ReadAt(*ref, 12, si) ||
            !ReadAt(*ref, 14, ic) || !ReadAt(*ref, 16, sm) || !ReadAt(*ref, 18, mc))
            continue;
        const auto li = libs.find(lib);
        if (li == libs.end())
            continue;
        const std::vector<byte>& d = *li->second;
        uint16_t mcount = 0, icount = 0, vcount = 0;
        if (!ReadAt(d, 8, mcount) || !ReadAt(d, 10, icount) || !ReadAt(d, 14, vcount) || !vcount)
            continue;
        const size_t io = 16 + 12 * size_t(mcount), vo = io + 2 * size_t(icount);
        if (vo > d.size())
            continue;
        const size_t stride = (d.size() - vo) / vcount;
        if (stride < 12)
            continue;
        const Pose w = world(obj);
        size_t pIdx = si;
        for (int m = sm; m < sm + mc && m < mcount; ++m)
        {
            uint16_t start = 0, nref = 0;
            ReadAt(d, 16 + 12 * size_t(m) + 4, start);
            ReadAt(d, 16 + 12 * size_t(m) + 8, nref);
            for (int t = 0; t + 2 < nref; t += 3)
            {
                Tri2 tri;
                bool ok = true;
                for (int k = 0; k < 3 && ok; ++k)
                {
                    uint16_t vi = 0;
                    ok = ReadAt(d, io + 2 * (pIdx + t + k), vi);
                    const size_t at = vo + stride * (size_t(sv) + start + vi);
                    float v[3], o3[3];
                    ok = ok && at + 12 <= d.size();
                    if (!ok)
                        break;
                    memcpy(v, d.data() + at, 12);
                    Apply(w.r, v, o3);
                    tri.x[k] = o3[0] + w.t[0], tri.y[k] = o3[2] + w.t[2];
                    ok = std::isfinite(tri.x[k]) && std::isfinite(tri.y[k]);
                }
                if (ok)
                    out.push_back(tri);
                if (static_cast<int>(out.size()) >= maxTris)
                    return out;
            }
            pIdx += nref;
        }
    }
    return out;
}

// The silhouette as a square mask (n x n, 0 / 255), simplified: slits between parts closed (a close of `closeR` pixels),
// cropped to the ship, the width raised to `widthFloor` of the length for a long hull (146_ v2: 0.5 for freighters,
// transports, capital ships; 0 for fighters), centred in the square. False: no triangle, or a degenerate span.
struct Mask
{
    int n = 0;
    std::vector<byte> px;
    float lengthM = 0, widthM = 0; // the model's own top-down size (metres)
};
inline bool Silhouette(const std::vector<Tri2>& tris, int n, float widthFloor, Mask& out, int closeR = 2)
{
    if (tris.empty() || n < 16)
        return false;
    float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
    for (const Tri2& t : tris)
        for (int k = 0; k < 3; ++k)
            x0 = std::min(x0, t.x[k]), x1 = std::max(x1, t.x[k]), y0 = std::min(y0, t.y[k]), y1 = std::max(y1, t.y[k]);
    const float span = std::max(x1 - x0, y1 - y0);
    if (!(span > 1e-3f))
        return false;
    // fill: a pixel is in when its centre lies inside a triangle or within half a pixel of its edges (the trace's polygon
    // fill draws the outline too: a vertical wall seen from above is a line, and a thin spar stays)
    const int pad = closeR + 1, inner = n - 2 * pad;
    const float sc = inner / span, cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    std::vector<byte> a(size_t(n) * n, 0);
    for (const Tri2& t : tris)
    {
        float X[3], Y[3];
        for (int k = 0; k < 3; ++k)
            X[k] = n / 2.0f + (t.x[k] - cx) * sc, Y[k] = n / 2.0f + (t.y[k] - cy) * sc;
        const int bx0 = std::max(0, int(std::floor(std::min({X[0], X[1], X[2]}) - 0.5f))),
                  bx1 = std::min(n - 1, int(std::ceil(std::max({X[0], X[1], X[2]}) + 0.5f))),
                  by0 = std::max(0, int(std::floor(std::min({Y[0], Y[1], Y[2]}) - 0.5f))),
                  by1 = std::min(n - 1, int(std::ceil(std::max({Y[0], Y[1], Y[2]}) + 0.5f)));
        const float area = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
        const float s = area < 0 ? -1.0f : 1.0f;
        float len[3];
        for (int e = 0; e < 3; ++e)
        {
            const int f = (e + 1) % 3;
            len[e] = std::sqrt((X[f] - X[e]) * (X[f] - X[e]) + (Y[f] - Y[e]) * (Y[f] - Y[e]));
        }
        for (int y = by0; y <= by1; ++y)
            for (int x = bx0; x <= bx1; ++x)
            {
                const float px = x + 0.5f, py = y + 0.5f;
                bool in = true;
                for (int e = 0; e < 3 && in; ++e)
                {
                    const int f = (e + 1) % 3;
                    const float ed = s * ((X[f] - X[e]) * (py - Y[e]) - (Y[f] - Y[e]) * (px - X[e]));
                    in = ed >= -0.5f * len[e]; // signed distance x length >= -half a pixel x length
                }
                if (in)
                    a[size_t(y) * n + x] = 255;
            }
    }
    // close: dilate then erode, square of radius closeR (separable)
    auto morph = [n](std::vector<byte>& m, int r, bool dilate) {
        std::vector<byte> tmp(m.size());
        for (int pass = 0; pass < 2; ++pass)
        {
            const std::vector<byte>& src = pass ? tmp : m;
            std::vector<byte>& dst = pass ? m : tmp;
            std::vector<byte> copy = src;
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x)
                {
                    byte v = dilate ? 0 : 255;
                    for (int d = -r; d <= r; ++d)
                    {
                        const int xx = pass ? x : x + d, yy = pass ? y + d : y;
                        const byte s = (xx < 0 || yy < 0 || xx >= n || yy >= n) ? 0 : copy[size_t(yy) * n + xx];
                        v = dilate ? std::max(v, s) : std::min(v, s);
                    }
                    dst[size_t(y) * n + x] = v;
                }
        }
    };
    if (closeR > 0)
        morph(a, closeR, true), morph(a, closeR, false);
    int mx0 = n, mx1 = -1, my0 = n, my1 = -1;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (a[size_t(y) * n + x])
                mx0 = std::min(mx0, x), mx1 = std::max(mx1, x), my0 = std::min(my0, y), my1 = std::max(my1, y);
    if (mx1 < 0)
        return false;
    const float w = float(mx1 - mx0 + 1), h = float(my1 - my0 + 1);
    float ww = w;
    if (h > w && w / h < widthFloor)
        ww = h * widthFloor; // a long hull's width raised to the floor (146_ v2)
    const float side = std::max(ww, h), ox = (side - ww) / 2, oy = (side - h) / 2;
    out.n = n;
    out.px.assign(size_t(n) * n, 0);
    out.widthM = x1 - x0, out.lengthM = y1 - y0;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            // the square's pixel -> the crop's (stretched across when floored)
            const float u = (x + 0.5f) * side / n - ox, v = (y + 0.5f) * side / n - oy;
            if (u < 0 || v < 0 || u >= ww || v >= h)
                continue;
            const int sx = mx0 + std::min(int(w) - 1, int(u * w / ww)), sy = my0 + std::min(int(h) - 1, int(v));
            out.px[size_t(y) * n + x] = a[size_t(sy) * n + sx];
        }
    return true;
}

// The mask baked to an alpha cell `size` px square (0..255): sampled at 4x, thin parts dilated by a quarter of a final pixel
// each side (146_ v2: a spar keeps about one pixel), then averaged down.
inline std::vector<byte> Bake(const Mask& m, int size)
{
    std::vector<byte> out;
    if (m.n <= 0 || size <= 0)
        return out;
    const int s = 4, g = size * s;
    std::vector<byte> hi(size_t(g) * g, 0), dil(size_t(g) * g, 0);
    for (int y = 0; y < g; ++y)
        for (int x = 0; x < g; ++x)
        {
            const int sx = std::min(m.n - 1, int((x + 0.5f) * m.n / g)), sy = std::min(m.n - 1, int((y + 0.5f) * m.n / g));
            hi[size_t(y) * g + x] = m.px[size_t(sy) * m.n + sx] ? 1 : 0;
        }
    for (int y = 0; y < g; ++y)
        for (int x = 0; x < g; ++x)
        {
            byte v = 0;
            for (int dy = -1; dy <= 1 && !v; ++dy)
                for (int dx = -1; dx <= 1 && !v; ++dx)
                {
                    const int xx = x + dx, yy = y + dy;
                    v = (xx >= 0 && yy >= 0 && xx < g && yy < g) ? hi[size_t(yy) * g + xx] : 0;
                }
            dil[size_t(y) * g + x] = v;
        }
    out.assign(size_t(size) * size, 0);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            int sum = 0;
            for (int dy = 0; dy < s; ++dy)
                for (int dx = 0; dx < s; ++dx)
                    sum += dil[size_t(y * s + dy) * g + x * s + dx];
            out[size_t(y) * size + x] = static_cast<byte>(sum * 255 / (s * s));
        }
    return out;
}

// The width floor for a ship type (CSimple::get_type bits, hudinfo::ClassName's order: fighter 16, freighter 17, gunboat 18,
// cruiser 19, transport 20, capital 21, mining 22): 146_ v2 floored freighters, transports, cruisers and capital ships.
inline float WidthFloor(uint32_t type)
{
    return (type & ((1u << 17) | (1u << 19) | (1u << 20) | (1u << 21))) ? 0.5f : 0.0f;
}

// ---- rank: the factions' rank words (faction_prop.ini rank_desig low, mid, high), the tier of a pilot's name
// A word -> its tier 1..3, or 0 when two factions put it at different tiers (no mark then: fails closed).
class RankWords
{
  public:
    void AddFaction(const std::wstring words[3])
    {
        for (int i = 0; i < 3; ++i)
        {
            if (words[i].empty())
                continue;
            const auto it = m_tier.find(words[i]);
            if (it == m_tier.end())
                m_tier[words[i]] = i + 1;
            else if (it->second != i + 1)
                it->second = 0;
        }
    }
    // The tier of `name`'s leading rank word (1..3), 0 none; `wordLen` the characters of the word and its space.
    int TierOf(const std::wstring& name, size_t* wordLen = nullptr) const
    {
        int best = 0;
        size_t bestLen = 0;
        for (const auto& [w, tier] : m_tier)
            if (w.size() > bestLen && name.size() > w.size() + 1 && name.compare(0, w.size(), w) == 0 &&
                name[w.size()] == L' ')
                best = tier, bestLen = w.size();
        if (wordLen)
            *wordLen = best ? bestLen + 1 : 0;
        return best;
    }
    size_t Size() const { return m_tier.size(); }
    size_t Ambiguous() const
    {
        size_t n = 0;
        for (const auto& kv : m_tier)
            n += kv.second == 0;
        return n;
    }

  private:
    std::map<std::wstring, int> m_tier;
};
} // namespace npcicons
