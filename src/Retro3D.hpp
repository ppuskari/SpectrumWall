#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace spectrum {

// Deliberately lightweight 3D software transform into SDL3's GPU-backed
// triangle renderer. No runtime GL context or dependencies beyond SDL3.
enum class Retro3DMode { Off = 0, Dancer = 1, Can = 2, Both = 3 };
inline const char* retro3dModeName(Retro3DMode mode) {
    switch (mode) {
    case Retro3DMode::Off: return "OFF";
    case Retro3DMode::Dancer: return "DANCING BABY HOMAGE";
    case Retro3DMode::Can: return "RETRO COLA CAN";
    case Retro3DMode::Both: return "BOTH";
    }
    return "OFF";
}
struct RetroVec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
inline RetroVec3 operator+(RetroVec3 a, RetroVec3 b) {
    return {a.x+b.x, a.y+b.y, a.z+b.z};
}
inline RetroVec3 operator-(RetroVec3 a, RetroVec3 b) {
    return {a.x-b.x, a.y-b.y, a.z-b.z};
}
inline RetroVec3 operator*(RetroVec3 a, float m) {
    return {a.x*m, a.y*m, a.z*m};
}
inline RetroVec3 retroCross(RetroVec3 a, RetroVec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
inline float retroDot(RetroVec3 a, RetroVec3 b) {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}
inline float retroLength(RetroVec3 a) {
    return std::sqrt(retroDot(a,a));
}
inline RetroVec3 retroNormalize(RetroVec3 a) {
    return a*(1.0f/std::max(1.0e-6f,retroLength(a)));
}
inline RetroVec3 retroRotateX(RetroVec3 a,float radians) {
    const float c=std::cos(radians),s=std::sin(radians);
    return {a.x,a.y*c-a.z*s,a.y*s+a.z*c};
}
inline RetroVec3 retroRotateY(RetroVec3 a,float radians) {
    const float c=std::cos(radians),s=std::sin(radians);
    return {a.x*c+a.z*s,a.y,-a.x*s+a.z*c};
}
inline RetroVec3 retroRotateZ(RetroVec3 a,float radians) {
    const float c=std::cos(radians),s=std::sin(radians);
    return {a.x*c-a.y*s,a.x*s+a.y*c,a.z};
}
inline float retroPerspective(float dimension,float depth) {
    return dimension*0.32f/std::max(1.1f,4.7f-depth);
}

struct RetroColor { unsigned char r=255,g=255,b=255,a=255; };
struct RetroVec2 {float u=0.0f,v=0.0f;};
struct RetroTriangle {
    RetroVec3 a,b,c;
    RetroColor color;
    RetroVec2 uvA{},uvB{},uvC{};
    int textureId=-1;
};
// Store triangles directly; at a few hundred faces per subject, triangle
// sorting is inexpensive compared to capture/FFT even on an older RX580.
using RetroMesh = std::vector<RetroTriangle>;

inline float retroBeatTarget(float bassPulse, bool reactive) {
    return reactive ? std::clamp(bassPulse,0.0f,1.0f) : 0.0f;
}
inline float retroBeatSlew(float previous, float target, float dt) {
    const float tau=target>previous?0.06f:0.25f;
    const float alpha=1.0f-std::exp(-std::clamp(dt,0.0f,0.2f)/tau);
    return previous+(target-previous)*alpha;
}
inline float retroSizeScale(int setting) {
    return setting==0?0.72f:setting==2?1.30f:1.0f;
}
} // namespace spectrum
