// FLvibed - 3-component vector arithmetic on plain float[3] arrays (positions, velocities, directions as the game
// and fl::Kinematics hold them). Header-only, shared by every module (core\game), compiled into each.
// Written 24 Oct 2026 to replace the autopilot's hand-rolled idioms (checkup audit 3.5): each function keeps the
// arithmetic order of the idiom it replaced, so float results are bit-identical to the code before it. Every function
// takes pointers to three components; `out` must not alias an input (Cross, Sub, Add, Scale).
#pragma once
#include "common.h"

namespace geom
{
inline void Sub(float* out, const float* a, const float* b)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}
inline void Add(float* out, const float* a, const float* b)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}
inline void Scale(float* out, const float* a, float s)
{
    out[0] = a[0] * s;
    out[1] = a[1] * s;
    out[2] = a[2] * s;
}
inline float Dot(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline float Len(const float* a) { return sqrtf(Dot(a, a)); }
inline float Dist(const float* a, const float* b)
{
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return Len(d);
}
// Normalise in place: each component divided by the length (a division, never a reciprocal multiply: the idiom's
// result bit for bit). A zero vector is left as it is (the callers guard the length first; the old loops produced NaN).
inline void Norm(float* v)
{
    const float len = Len(v);
    if (len == 0)
        return;
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}
inline void Cross(float* out, const float* a, const float* b)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}
// The nose from a fl::Kinematics rotation (row-major 3x3, columns = the ship's right / up / back axes): forward is the
// negated third column (TurnTowards steers -z).
inline void Forward(float* out, const float* rot)
{
    out[0] = -rot[2];
    out[1] = -rot[5];
    out[2] = -rot[8];
}
} // namespace geom
