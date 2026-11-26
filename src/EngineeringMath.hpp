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
inline constexpr float kLevelMeterFloorDb = -72.0f;
inline constexpr float kLevelMeterCeilingDb = 0.0f;

inline float levelMeterDbFraction(float db) {
    return std::clamp((db - kLevelMeterFloorDb) /
                      (kLevelMeterCeilingDb - kLevelMeterFloorDb), 0.0f, 1.0f);
}
inline float levelMeterDbAtFraction(float fraction) {
    return kLevelMeterFloorDb +
        std::clamp(fraction, 0.0f, 1.0f) *
        (kLevelMeterCeilingDb - kLevelMeterFloorDb);
}
inline float levelMeterSamplePeakFraction(float amplitude) {
    // Both VU and peak use the same logarithmic scale, unlike the old code
    // which mistakenly placed raw linear amplitude directly on a dB graph.
    return levelMeterDbFraction(
        20.0f * std::log10(std::max(amplitude, 1.0e-8f)));
}
inline float levelMeterSmoothedRmsFraction(float vuNormalized) {
    // VU follows the spectrum's -72..-2 normalized dB mapping. Translate to
    // the lower meter's -72..0 dBFS instead of treating it as linear amplitude.
    return levelMeterDbFraction(
        kSpectrumFloorDb + std::clamp(vuNormalized, 0.0f, 1.0f) *
        (kSpectrumCeilingDb - kSpectrumFloorDb));
}


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
