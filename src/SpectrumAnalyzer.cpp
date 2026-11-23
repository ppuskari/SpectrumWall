#include "SpectrumAnalyzer.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace spectrum {
namespace {
constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kFloorDb = -72.0f;
constexpr float kTopDb = -2.0f;

float normalized(float linear) {
    return std::clamp((toDb(linear) - kFloorDb) / (kTopDb - kFloorDb), 0.0f, 1.0f);
}
}

const char* smoothingModeName(SmoothingMode mode) {
    switch (mode) {
    case SmoothingMode::Fast: return "FAST";
    case SmoothingMode::Balanced: return "BALANCED";
    case SmoothingMode::Liquid: return "LIQUID";
    }
    return "BALANCED";
}

float toDb(float linear) {
    return 20.0f * std::log10(std::max(0.00000001f, linear));
}

SpectrumAnalyzer::SpectrumAnalyzer(std::size_t bands) {
    for (std::size_t i = 0; i < kFFTFrames; ++i) {
        window_[i] = 0.5f * (1.0f -
            std::cos((2.0f * kPi * static_cast<float>(i)) /
                     static_cast<float>(kFFTFrames - 1)));
    }
    setBandCount(bands);
}

void SpectrumAnalyzer::setBandCount(std::size_t bands) {
    bands = std::clamp(bands, kMinBands, kMaxBands);
    if (bands != state_.bandCount) {
        // Old bar envelopes and peak holds are not meaningful after rebucketing.
        state_ = VisualState{};
        leftHold_.fill(0.0f);
        rightHold_.fill(0.0f);
        bassBaseline_ = 0.0f;
    }
    state_.bandCount = bands;

    constexpr float minFrequency = 30.0f;
    constexpr float maxFrequency = 18000.0f;
    for (std::size_t band = 0; band < bands; ++band) {
        const float f0 = minFrequency * std::pow(maxFrequency / minFrequency,
                                    static_cast<float>(band) / static_cast<float>(bands));
        const float f1 = minFrequency * std::pow(maxFrequency / minFrequency,
                                    static_cast<float>(band + 1) / static_cast<float>(bands));
        const std::size_t b0 = std::clamp(
            static_cast<std::size_t>(std::floor(f0 * kFFTFrames / kSampleRate)),
            static_cast<std::size_t>(1), kFFTFrames / 2 - 1);
        const std::size_t b1 = std::clamp(
            static_cast<std::size_t>(std::ceil(f1 * kFFTFrames / kSampleRate)),
            b0 + 1, kFFTFrames / 2);
        bandBins_[band] = { b0, b1 };
    }
    // Bass response remains tied to ~30-180 Hz, not to an arbitrary band index.
    bassBandCount_ = std::clamp(
        static_cast<std::size_t>(std::ceil(static_cast<float>(bands) *
            std::log(180.0f / minFrequency) /
            std::log(maxFrequency / minFrequency))),
        static_cast<std::size_t>(1), bands);
}

void SpectrumAnalyzer::transform(std::size_t channel,
                                 const std::array<float, kFFTFrames * 2>& input,
                                 std::array<float, kMaxBands>& bars) {
    for (std::size_t i = 0; i < kFFTFrames; ++i) {
        fft_[i] = { input[2 * i + channel] * window_[i], 0.0f };
    }

    // Iterative radix-2 FFT; the initial implementation keeps analysis dependency-free.
    for (std::size_t i = 1, j = 0; i < kFFTFrames; ++i) {
        std::size_t bit = kFFTFrames >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(fft_[i], fft_[j]);
    }
    for (std::size_t size = 2; size <= kFFTFrames; size <<= 1) {
        const std::complex<float> phaseStep =
            std::polar(1.0f, -2.0f * kPi / static_cast<float>(size));
        for (std::size_t offset = 0; offset < kFFTFrames; offset += size) {
            std::complex<float> phase = { 1.0f, 0.0f };
            for (std::size_t i = 0; i < size / 2; ++i) {
                const auto even = fft_[offset + i];
                const auto odd = fft_[offset + i + size / 2] * phase;
                fft_[offset + i] = even + odd;
                fft_[offset + i + size / 2] = even - odd;
                phase *= phaseStep;
            }
        }
    }

    const float scale = 4.0f / static_cast<float>(kFFTFrames - 1);
    for (std::size_t band = 0; band < state_.bandCount; ++band) {
        const auto [b0, b1] = bandBins_[band];
        float magnitude = 0.0f;
        for (std::size_t bin = b0; bin < b1; ++bin) {
            magnitude = std::max(magnitude, std::abs(fft_[bin]) * scale);
        }
        bars[band] = normalized(magnitude);
    }
}

void SpectrumAnalyzer::update(const std::array<float, kFFTFrames * 2>& input, float dt) {
    dt = std::clamp(dt, 0.001f, 0.1f);
    std::array<float, kMaxBands> newLeft{}, newRight{};
    transform(0, input, newLeft);
    transform(1, input, newRight);

    // Display-domain frequency smoothing. This does not invent FFT resolution:
    // it simply avoids jagged adjacent bar heights while retaining channel separation.
    if (spectralSoftening_) {
        auto soften = [this](std::array<float, kMaxBands>& values) {
            const auto original = values;
            for (std::size_t i = 0; i < state_.bandCount; ++i) {
                const auto lo = i > 0 ? i - 1 : i;
                const auto hi = std::min(i + 1, state_.bandCount - 1);
                values[i] = original[i] * 0.70f +
                            (original[lo] + original[hi]) * 0.15f;
            }
        };
        soften(newLeft);
        soften(newRight);
    }

    float sumLeft = 0.0f, sumRight = 0.0f;
    float pkLeft = 0.0f, pkRight = 0.0f;
    for (std::size_t i = 0; i < kFFTFrames; ++i) {
        const float l = input[2 * i], r = input[2 * i + 1];
        sumLeft += l * l;
        sumRight += r * r;
        pkLeft = std::max(pkLeft, std::abs(l));
        pkRight = std::max(pkRight, std::abs(r));
    }
    state_.rmsLeft = std::sqrt(sumLeft / static_cast<float>(kFFTFrames));
    state_.rmsRight = std::sqrt(sumRight / static_cast<float>(kFFTFrames));
    state_.peakLeft = pkLeft;
    state_.peakRight = pkRight;

    // Frame-rate independent time constants, not arbitrary per-frame smoothing.
    float attackSeconds = 0.028f, releaseSeconds = 0.24f;
    float peakHoldSeconds = 0.38f, vuSeconds = 0.30f;
    switch (smoothingMode_) {
    case SmoothingMode::Fast:
        attackSeconds = 0.010f; releaseSeconds = 0.105f;
        peakHoldSeconds = 0.22f; vuSeconds = 0.20f; break;
    case SmoothingMode::Liquid:
        attackSeconds = 0.060f; releaseSeconds = 0.50f;
        peakHoldSeconds = 0.62f; vuSeconds = 0.42f; break;
    case SmoothingMode::Balanced: break;
    }
    const float vuAlpha = 1.0f - std::exp(-dt / vuSeconds);
    state_.vuLeft += (normalized(state_.rmsLeft) - state_.vuLeft) * vuAlpha;
    state_.vuRight += (normalized(state_.rmsRight) - state_.vuRight) * vuAlpha;

    const float attack = 1.0f - std::exp(-dt / attackSeconds);
    const float release = 1.0f - std::exp(-dt / releaseSeconds);
    for (std::size_t i = 0; i < state_.bandCount; ++i) {
        auto smooth = [attack, release](float old, float current) {
            return old + (current - old) * (current > old ? attack : release);
        };
        state_.left[i] = smooth(state_.left[i], newLeft[i]);
        state_.right[i] = smooth(state_.right[i], newRight[i]);

        auto peak = [dt, peakHoldSeconds](float current, float& held, float& remaining) {
            if (current >= held) {
                held = current;
                remaining = peakHoldSeconds;
            } else if (remaining > 0.0f) {
                remaining = std::max(0.0f, remaining - dt);
            } else {
                held = std::max(current, held - dt * 0.45f);
            }
            return held;
        };
        state_.leftPeaks[i] = peak(state_.left[i], state_.leftPeaks[i], leftHold_[i]);
        state_.rightPeaks[i] = peak(state_.right[i], state_.rightPeaks[i], rightHold_[i]);
    }

    float bass = 0.0f;
    for (std::size_t i = 0; i < bassBandCount_; ++i) {
        bass += 0.5f * (state_.left[i] + state_.right[i]);
    }
    bass /= static_cast<float>(bassBandCount_);
    bassBaseline_ += (bass - bassBaseline_) * (1.0f - std::exp(-dt / 1.0f));
    const float pulse = std::clamp((bass - bassBaseline_) * 2.4f, 0.0f, 1.0f);
    state_.bassPulse = std::max(pulse, state_.bassPulse * std::exp(-dt * 11.0f));

    for (std::size_t i = 0; i < kWavePoints; ++i) {
        const std::size_t src = i * (kFFTFrames - 1) / (kWavePoints - 1);
        state_.waveLeft[i] = input[src * 2];
        state_.waveRight[i] = input[src * 2 + 1];
    }
}
} // namespace spectrum
