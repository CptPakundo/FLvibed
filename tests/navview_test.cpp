// core\game\navview_rules.h (pure): the zoom holds the cursor's point, the map's edge stays on the viewport's, a gesture never jumps.
#include "test.h"

#include "../core/game/navview_rules.h"

using namespace navview;

namespace
{
bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }
} // namespace

void Test_navview()
{
    const Box box;
    // at Z = 1 the map fills the viewport exactly, both ways round
    {
        View v;
        CHECK(Near(PortU(v, box, 0), 0) && Near(PortU(v, box, 1), 1));
        CHECK(Near(PortV(v, box, 0), 0) && Near(PortV(v, box, 1), 1));
        CHECK(Near(MapU(v, box, 0.25f), 0.25f) && Near(MapV(v, box, 0.75f), 0.75f));
    }
    // the game's drag: a mouse moved dx right moves C by 2 dx / portW x W / 2Z: a viewport's fraction dx / portW on screen
    {
        View v;
        v.zoom = 4;
        const float before = PortU(v, box, 0.5f);
        v.cx += 0.1f * box.w / v.zoom;
        CHECK(Near(PortU(v, box, 0.5f) - before, 0.1f));
    }
    // the zoom keeps the point under the cursor, wherever it stands, while the clamp allows
    for (float u : {0.5f, 0.3f, 0.62f})
        for (float t : {0.5f, 0.4f, 0.7f})
        {
            View v;
            ZoomAbout(v, box, 0.5f, 0.5f, 2);
            const float mu = MapU(v, box, u), mv = MapV(v, box, t);
            ZoomAbout(v, box, u, t, 3);
            CHECK(Near(PortU(v, box, mu), u) && Near(PortV(v, box, mv), t));
        }
    // zooming at a corner from the whole map: the corner stays a corner (the map's edge is on the viewport's edge)
    {
        View v;
        ZoomAbout(v, box, 0, 0, 5);
        CHECK(Near(PortU(v, box, 0), 0) && Near(PortV(v, box, 0), 0));
        ZoomAbout(v, box, 1, 1, 8.6f);
        CHECK(v.zoom == kMaxZoom);
        CHECK(PortU(v, box, 0) <= 0.001f && PortU(v, box, 1) >= 0.999f && PortV(v, box, 0) <= 0.001f && PortV(v, box, 1) >= 0.999f);
    }
    // the clamp: no pan shows past the map's edge, at any zoom; at Z = 1 no pan at all
    {
        View v;
        PanBy(v, box, 0.5f, 0.5f);
        CHECK(v.cx == 0 && v.cy == 0);
        v.zoom = 2;
        PanBy(v, box, 10, -10);
        CHECK(Near(PortU(v, box, 0), 0));
        CHECK(Near(PortV(v, box, 1), 1) || Near(PortV(v, box, 0), 0));
    }
    // a full pull covers the whole range in about kFullZoomSeconds, at 60 and at 25 frames a second; the release stays where it is
    for (float dt : {1 / 60.0f, 1 / 25.0f})
    {
        View v;
        Motion m;
        Drive d;
        d.zoomRate = 1;
        float time = 0;
        while (v.zoom < kMaxZoom - 0.01f && time < 5)
        {
            Step(m, v, box, d, 0.5f, 0.5f, dt);
            time += dt;
        }
        CHECK(time > kFullZoomSeconds - 0.1f && time < kFullZoomSeconds + 0.5f);
        d.zoomRate = -1;
        for (float s = 0; s < 3; s += dt)
            Step(m, v, box, d, 0.5f, 0.5f, dt);
        CHECK(Near(v.zoom, kMinZoom, 0.01f));
    }
    // the stick: full tilt right moves the view right, so the map left, at the same screen speed at every zoom
    for (float z : {2.0f, 6.0f})
    {
        View v;
        v.zoom = z;
        Motion m;
        m.target = std::log(z);
        Drive d;
        d.panX = 1;
        const float before = PortU(v, box, 0.5f);
        Step(m, v, box, d, 0.5f, 0.5f, 0.1f);
        CHECK(Near(PortU(v, box, 0.5f) - before, -kPanRate * 0.1f));
    }
    // no input, no move
    {
        View v;
        v.zoom = 3;
        Motion m;
        m.target = std::log(3.0f);
        CHECK(!Step(m, v, box, Drive(), 0.5f, 0.5f, 0.016f));
    }
    // a swipe glides on after the release, and stops
    {
        View v;
        v.zoom = 4;
        Motion m;
        m.target = std::log(4.0f);
        Drive d;
        d.touching = true, d.dragU = 0.01f;
        for (int i = 0; i < 10; ++i)
            Step(m, v, box, d, 0.5f, 0.5f, 0.016f);
        const float atRelease = v.cx;
        d = Drive();
        CHECK(Step(m, v, box, d, 0.5f, 0.5f, 0.016f));
        CHECK(v.cx > atRelease);
        for (int i = 0; i < 120; ++i)
            Step(m, v, box, d, 0.5f, 0.5f, 0.016f);
        CHECK(m.glideU == 0);
        CHECK(!Step(m, v, box, d, 0.5f, 0.5f, 0.016f));
    }
    // gestures: a touch without movement does nothing; a drag past the dead zone follows the finger with no jump
    {
        Gesture g;
        Finger f[kFingers];
        f[0] = {true, 0.5f, 0.5f};
        GestureOut o = Read(g, f, false);
        CHECK(o.dragU == 0 && !o.touching);
        f[0].x = 0.505f;
        o = Read(g, f, false);
        CHECK(o.dragU == 0 && !o.touching);
        f[0].x = 0.54f;
        o = Read(g, f, false);
        CHECK(o.touching);
        f[0].x = 0.56f;
        o = Read(g, f, false);
        CHECK(Near(o.dragU, 0.02f));
        // a second finger: a new gesture, nothing jumps
        f[1] = {true, 0.2f, 0.6f};
        o = Read(g, f, false);
        CHECK(o.dragU == 0 && o.zoomStep == 0 && !o.touching);
        // both up the pad: zoom in, no pan
        f[0].y -= 0.05f, f[1].y -= 0.05f;
        o = Read(g, f, false);
        CHECK(o.zoomStep > 0 && o.dragU == 0);
        f[0].y += 0.1f, f[1].y += 0.1f;
        o = Read(g, f, false);
        CHECK(o.zoomStep < 0);
        // one finger lifts: no jump
        f[1].down = false;
        o = Read(g, f, false);
        CHECK(o.dragU == 0 && o.zoomStep == 0);
    }
    // a click is View: the finger on it moves nothing until it lifts
    {
        Gesture g;
        Finger f[kFingers];
        f[0] = {true, 0.5f, 0.5f};
        Read(g, f, true);
        f[0].x = 0.8f;
        GestureOut o = Read(g, f, false);
        CHECK(o.dragU == 0 && !o.touching);
        f[0].down = false;
        Read(g, f, false);
        f[0] = {true, 0.5f, 0.5f};
        Read(g, f, false);
        f[0].x = 0.6f;
        CHECK(Read(g, f, false).touching);
    }
    // a lane's name and what its ends name
    {
        std::wstring a, b;
        CHECK(SplitLane(L"Fort Bush->Norfolk Shipyard", a, b) && a == L"Fort Bush" && b == L"Norfolk Shipyard");
        CHECK(!SplitLane(L"Trade Lane Ring", a, b) && !SplitLane(L"->Fort Bush", a, b) && !SplitLane(L"Fort Bush->", a, b));
        CHECK(EndNames(L"Fort Bush", L"Fort Bush") && EndNames(L"West Point", L"West Point Military Academy"));
        CHECK(!EndNames(L"West", L"Western Station") && !EndNames(L"", L"Fort Bush") && !EndNames(L"Fort Bush Station", L"Fort Bush"));
    }
    // the grid: the game's own numbers at its two ends
    {
        View v;
        CHECK(Near(GridLetterX(0, v), -0.279f) && Near(GridNumberY(0, v), 0.252f));
        v.zoom = kMaxZoom;
        CHECK(Near(GridLetterX(1, v), (0.027f + 0.07f - 0.279f) * 8.6f));
        CHECK(Near(GridNumberY(1, v), (-0.0115f + 0.252f - 0.07f) * 8.6f));
    }
}
