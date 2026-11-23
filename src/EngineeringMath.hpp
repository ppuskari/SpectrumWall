#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace spectrum {
inline constexpr float kMinSpectrumHz = 30.0f;
inline constexpr float kMaxSpectrumHz = 18000.0f;
inline constexpr float kSpectrumFloorDb = -72.0f;
inline constexpr float kSpectrumCeilingDb = -2.0f;
inline constexpr float kBalanceMeterRangeDb = 18.0f;

inline float spectrumFrequencyFraction(float frequencyHz) {
    return std::clamp(
        std::log(std::clamp(frequencyHz, kMinSpectrumHz, kMaxSpectrumHz) /
                 kMinSpectrumHz) /
            std::log(kMaxSpectrumHz / kMinSpectrumHz),
        0.0f, 1.0f);
}

inline float spectrumFrequencyAtFraction(float fraction) {
    return kMinSpectrumHz * std::pow(
        kMaxSpectrumHz / kMinSpectrumHz,
        std::clamp(fraction, 0.0f, 1.0f));
}

inline float spectrumBandCenterHz(std::size_t band, std::size_t bands) {
    if (bands == 0) return kMinSpectrumHz;
    return spectrumFrequencyAtFraction(
        (static_cast<float>(band) + 0.5f) / static_cast<float>(bands));
}

inline float spectrumDbFraction(float db) {
    return std::clamp((db - kSpectrumFloorDb) /
                      (kSpectrumCeilingDb - kSpectrumFloorDb), 0.0f, 1.0f);
}

inline float spectrumDbAtFraction(float fraction) {
    return kSpectrumFloorDb +
        (kSpectrumCeilingDb - kSpectrumFloorDb) * fraction;
}

// Positive values indicate right-channel dominance. This compares overall
// full-band RMS amplitude, not a stereo phase or correlation measurement.
// A gate suppresses unstable ratios when the optical input is silent.
inline float stereoBalanceDb(float leftRms, float rightRms) {
    leftRms = std::max(0.0f, leftRms);
    rightRms = std::max(0.0f, rightRms);
    if (leftRms < 0.0001f && rightRms < 0.0001f) return 0.0f;
    constexpr float epsilon = 0.00000001f;
    const float db = 20.0f * std::log10(
        std::max(rightRms, epsilon) / std::max(leftRms, epsilon));
    return std::clamp(db, -90.0f, 90.0f);
}

inline float stereoBalancePosition(float balanceDb) {
    return std::clamp(balanceDb / kBalanceMeterRangeDb, -1.0f, 1.0f);
}

} // namespace spectrum
