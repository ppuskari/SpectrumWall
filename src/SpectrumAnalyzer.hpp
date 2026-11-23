#pragma once
#include "AudioCapture.hpp"
#include <array>
#include <complex>
#include <utility>

namespace spectrum {
inline constexpr std::size_t kMinBands = 64;
inline constexpr std::size_t kMaxBands = 256;
inline constexpr std::size_t kDefaultBands = 128;
inline constexpr std::size_t kBandStep = 16;
inline constexpr std::size_t kWavePoints = 512;

enum class SmoothingMode { Fast, Balanced, Liquid };
const char* smoothingModeName(SmoothingMode mode);

struct VisualState {
    std::size_t bandCount = kDefaultBands;
    std::array<float, kMaxBands> left{};
    std::array<float, kMaxBands> right{};
    std::array<float, kMaxBands> leftPeaks{};
    std::array<float, kMaxBands> rightPeaks{};
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
    explicit SpectrumAnalyzer(std::size_t bands = kDefaultBands);
    void setBandCount(std::size_t bands);
    std::size_t bandCount() const { return state_.bandCount; }
    void setSmoothingMode(SmoothingMode mode) { smoothingMode_ = mode; }
    SmoothingMode smoothingMode() const { return smoothingMode_; }
    void setSpectralSoftening(bool enabled) { spectralSoftening_ = enabled; }
    bool spectralSoftening() const { return spectralSoftening_; }
    void update(const std::array<float, kFFTFrames * 2>& input, float dt);
    const VisualState& state() const { return state_; }

private:
    void transform(std::size_t channel, const std::array<float, kFFTFrames * 2>& input,
                   std::array<float, kMaxBands>& bars);
    std::array<float, kFFTFrames> window_{};
    std::array<std::complex<float>, kFFTFrames> fft_{};
    std::array<float, kMaxBands> leftHold_{}, rightHold_{};
    std::array<std::pair<std::size_t, std::size_t>, kMaxBands> bandBins_{};
    std::size_t bassBandCount_ = 1;
    VisualState state_{};
    float bassBaseline_ = 0.0f;
    SmoothingMode smoothingMode_ = SmoothingMode::Balanced;
    bool spectralSoftening_ = true;
};

float toDb(float linear);
} // namespace spectrum
