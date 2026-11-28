#pragma once
#include "SpectrumAnalyzer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace spectrum {

// Each pane gets its own low-resolution thermal simulation; no inter-channel
// mixing or dependency on the renderer's 60 Hz refresh cadence.
struct FireHeatField {
    int width = 0, height = 0;
    float accumulator = 0.0f;
    float time = 0.0f;
    std::vector<float> heat;
    std::vector<float> next;
    std::vector<std::uint8_t> rgba;

    void resize(int w, int h) {
        w = std::clamp(w, 32, 320);
        h = std::clamp(h, 32, 192);
        if (width == w && height == h) return;
        width = w;
        height = h;
        accumulator = 0.0f;
        time = 0.0f;
        heat.assign(static_cast<std::size_t>(w*h), 0.0f);
        next.assign(heat.size(), 0.0f);
        rgba.assign(heat.size()*4, 0);
    }

    // This uses only the instantaneous smoothed spectrum, not held peaks.
    // Keeping the physics independent of peak hold makes quiet passages
    // naturally cool and avoids permanent flames on an idle audio source.
    float envelope(const std::array<float, kMaxBands>& values,
                   std::size_t count, float fraction, bool mirror) const {
        if (count == 0) return 0.0f;
        fraction = std::clamp(mirror ? 1.0f-fraction : fraction, 0.0f, 1.0f);
        const float bin = fraction * static_cast<float>(count-1);
        const auto lo = static_cast<std::size_t>(bin);
        const auto hi = std::min(lo+1, count-1);
        const float mix = bin-static_cast<float>(lo);
        return std::clamp(values[lo]*(1.0f-mix)+values[hi]*mix, 0.0f, 1.0f);
    }

    static std::array<std::uint8_t,4> fireColor(float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        constexpr std::array<std::array<float,3>,6> knots {{
            {{0,0,0}}, {{78,7,2}}, {{183,19,1}},
            {{253,89,7}}, {{255,206,44}}, {{255,249,218}}
        }};
        const float segment = value*5.0f;
        const int lo = std::min(static_cast<int>(segment), 4);
        const float t = segment - static_cast<float>(lo);
        const auto& a = knots[lo];
        const auto& b = knots[lo+1];
        const auto r = static_cast<std::uint8_t>(a[0]+(b[0]-a[0])*t);
        const auto g = static_cast<std::uint8_t>(a[1]+(b[1]-a[1])*t);
        const auto bl = static_cast<std::uint8_t>(a[2]+(b[2]-a[2])*t);
        const auto alpha = static_cast<std::uint8_t>(
            std::clamp((value-0.014f)*440.0f,0.0f,255.0f));
        return {r,g,bl,alpha};
    }

    // Fixed 30 Hz update; at most two thermal steps per frame. Returns true
    // only when the RGBA texture needs uploading to the GPU.
    bool advance(const std::array<float,kMaxBands>& values,std::size_t count,
                 bool mirror,float bassPulse,float dt) {
        if (width == 0 || height == 0 || count == 0) return false;
        constexpr float step = 1.0f/30.0f;
        accumulator = std::min(2.0f*step,accumulator+
                      std::clamp(dt,0.0f,0.10f));
        bool changed = false;
        for (int iteration=0;iteration<2 && accumulator>=step; ++iteration) {
            accumulator -= step;
            time += step;
            simulate(values,count,mirror,bassPulse);
            changed = true;
        }
        if (changed) {
            for (std::size_t i=0;i<heat.size();++i) {
                const auto color=fireColor(heat[i]);
                rgba[i*4+0]=color[0];
                rgba[i*4+1]=color[1];
                rgba[i*4+2]=color[2];
                rgba[i*4+3]=color[3];
            }
        }
        return changed;
    }

private:
    void simulate(const std::array<float,kMaxBands>& values,std::size_t count,
                  bool mirror,float bassPulse) {
        const int w=width, h=height;
        const float bass=std::clamp(bassPulse,0.0f,1.0f);
        // Build a few tenths-of-a-band lateral blur to blend adjacent tongues
        // into a continuous fire bed rather than separate LED-height columns.
        for(int x=0;x<w;++x) {
            const float u=(static_cast<float>(x)+0.5f)/static_cast<float>(w);
            const float f0=envelope(values,count,u,mirror);
            const float fL=envelope(values,count,u-1.5f/w,mirror);
            const float fR=envelope(values,count,u+1.5f/w,mirror);
            const float fLL=envelope(values,count,u-4.0f/w,mirror);
            const float fRR=envelope(values,count,u+4.0f/w,mirror);
            const float energy=std::clamp(0.38f*f0+0.24f*(fL+fR)+
                                        0.07f*(fLL+fRR),0.0f,1.0f);
            const float bassWeight=1.0f-
                  (mirror?1.0f-u:u);
            const float boost=0.06f+bass*0.18f*bassWeight;
            const float heightTarget=energy<0.024f?0.0f:
                std::clamp(std::pow(energy,0.74f)*0.84f+boost,0.0f,0.96f);
            for(int y=0;y<h;++y) {
                const auto idx=static_cast<std::size_t>(y*w+x);
                const float elevation=static_cast<float>(h-1-y)/
                                      static_cast<float>(h-1);
                // Animated lateral noise cuts coherent horizontal edges;
                // turbulence is deterministic and stable in 30 Hz time.
                const float noise=0.047f*std::sin(x*0.173f+time*7.4f+
                                      elevation*12.1f) +
                                  0.033f*std::sin(x*0.37f-time*4.8f+
                                      elevation*20.3f) +
                                  0.019f*std::sin(x*0.071f+time*10.7f);
                const float edge=heightTarget+noise-elevation;
                float emission=0.0f;
                if(energy>=0.024f && edge>0.0f) {
                    // Hot root, cooler middle and bright transient tongues.
                    const float occupancy=std::clamp(edge*9.0f,0.0f,1.0f);
                    emission=(0.26f+0.64f*energy+
                              0.24f*bass*bassWeight) *
                              occupancy *
                              (0.85f+0.15f*std::sin(time*5.2f+x*0.24f));
                    emission*=std::clamp(1.08f-elevation*0.27f,0.0f,1.0f);
                }
                const int sway=static_cast<int>(
                    std::round(1.6f*std::sin(time*4.5f+x*0.09f+
                                             elevation*9.5f)));
                const int srcX=std::clamp(x+sway,0,w-1);
                const int srcY=std::min(h-1,y+2);
                const std::size_t old=static_cast<std::size_t>(srcY*w+srcX);
                const std::size_t left=static_cast<std::size_t>(
                    srcY*w+std::max(0,srcX-1));
                const std::size_t right=static_cast<std::size_t>(
                    srcY*w+std::min(w-1,srcX+1));
                const float advected=(0.66f*heat[old]+
                      0.17f*(heat[left]+heat[right])) *
                      (0.90f-0.07f*elevation);
                next[idx]=std::clamp(std::max(emission,advected),0.0f,1.0f);
            }
        }
        heat.swap(next);
    }
};

} // namespace spectrum
