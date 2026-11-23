#pragma once
#include "AudioCapture.hpp"
#include <array>
#include <complex>

namespace spectrum {
inline constexpr std::size_t kBands = 96;
inline constexpr std::size_t kWavePoints = 512;

struct VisualState {
    std::array<float, kBands> left{};
    std::array<float, kBands> right{};
    std::array<float, kBands> leftPeaks{};
    std::array<float, kBands> rightPeaks{};
    std::array<float, kWavePoints> waveLeft{};
    std::array<float, kWavePoints> waveRight{};
    float rmsLeft = 0.0f;
    float rmsRight = 0.0f;
    float peakLeft = 0.0f;
    float peakRight = 0.0f;
    float vuLeft = 0.0f;
    float vuRight = 0.0f;
    float bassPulse = 0.0f;
};

class SpectrumAnalyzer {
public:
    SpectrumAnalyzer();
    void update(const std::array<float, kFFTFrames * 2>& input, float dt);
    const VisualState& state() const { return state_; }

private:
    void transform(std::size_t channel, const std::array<float, kFFTFrames * 2>& input,
                   std::array<float, kBands>& bars);
    std::array<float, kFFTFrames> window_{};
    std::array<std::complex<float>, kFFTFrames> fft_{};
    std::array<float, kBands> leftHold_{}, rightHold_{};
    VisualState state_{};
    float bassBaseline_ = 0.0f;
};

float toDb(float linear);
} // namespace spectrum
