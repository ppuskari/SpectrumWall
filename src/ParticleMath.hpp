#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace spectrum {
// Reusable normalization for particle emissions and future Fire tongues.
// The coordinate is based on visual position, not raw FFT frequency.
inline float sparkBandFraction(std::size_t index,std::size_t bands,bool mirror) {
    if(bands==0) return 0.5f;
    const float f=(static_cast<float>(std::min(index,bands-1))+0.5f) /
                   static_cast<float>(bands);
    return mirror?1.0f-f:f;
}
inline float sparkFade(float age,float lifetime) {
    if(lifetime<=0.0f)return 0.0f;
    const float t=std::clamp(age/lifetime,0.0f,1.0f);
    return (1.0f-t)*(1.0f-t);
}
inline float sparkEmissionRate(float averageEnergy,float bassPulse) {
    const float e=std::clamp(averageEnergy,0.0f,1.0f);
    const float beat=std::clamp(bassPulse,0.0f,1.0f);
    // Silence yields zero particles; typical music generates 15-60/sec.
    return e<0.012f && beat<0.01f?0.0f:
           8.0f+65.0f*e+40.0f*beat;
}
inline float sparkRandom(std::uint32_t& seed) {
    seed=seed*1664525u+1013904223u;
    return static_cast<float>(seed>>8)*(1.0f/16777216.0f);
}
} // namespace spectrum
