#pragma once
#include "SpectrumAnalyzer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace spectrum {

// 0 = no tips, 1 = short bright flare, 2 = slowly released held flare.
struct FirePeakTracker {
    std::array<float,kMaxBands> tip{};
    std::array<float,kMaxBands> age{};
    std::array<float,kMaxBands> previous{};
    std::size_t lastBands=0;
    void reset() {
        tip.fill(0.0f);
        age.fill(0.0f);
        previous.fill(0.0f);
        lastBands=0;
    }
    void advance(const std::array<float,kMaxBands>& values,
                 std::size_t bands,int mode,float dt) {
        bands=std::min(bands,kMaxBands);
        if(lastBands!=bands) {
            reset();
            lastBands=bands;
        }
        dt=std::clamp(dt,0.0f,0.10f);
        for(std::size_t i=0;i<bands;++i) {
            const float value=std::clamp(values[i],0.0f,1.0f);
            age[i]+=dt;
            if(mode==0) {
                tip[i]=0.0f;
            } else {
                const bool attack=value>0.13f &&
                    value>previous[i]+0.036f;
                if(attack) {
                    tip[i]=value;
                    age[i]=0.0f;
                } else if(mode==1) {
                    // Flutter and disappear quickly; no persistent hold.
                    tip[i]=std::max(0.0f,tip[i]-dt*2.4f);
                } else if(age[i]>0.70f) {
                    tip[i]=std::max(0.0f,tip[i]-dt*0.66f);
                }
                // Loud sustained tones may hold a recent spike but never
                // create a new one unless their amplitude rises.
            }
            previous[i]=value;
        }
    }
};
inline const char* firePeakModeName(int mode) {
    switch(mode) {
        case 0:return "OFF";
        case 1:return "FLASH";
        case 2:return "HOLD";
        default:return "OFF";
    }
}
inline const char* fireSpeedName(int speed) {
    switch(speed) {
        case 0:return "SLOW";
        case 1:return "NORMAL";
        case 2:return "FAST";
        default:return "NORMAL";
    }
}
inline float fireSpeedScale(int speed) {
    return speed==0?0.65f:speed==2?1.35f:1.0f;
}
inline const char* fireQualityName(int quality) {
    switch(quality) {
        case 0:return "ECO";
        case 1:return "BALANCED";
        case 2:return "HIGH";
        default:return "HIGH";
    }
}
} // namespace spectrum
