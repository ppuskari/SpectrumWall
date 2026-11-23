#include "SpectrumAnalyzer.hpp"
#include "EngineeringMath.hpp"
#include "CurveSampling.hpp"
#include "SpectrumLayout.hpp"
#include "VisualStyle.hpp"
#include <array>

#include <cmath>
#include <cstdio>
#include <numbers>

using namespace spectrum;

// Explicit checks remain active in Release builds (unlike assert under NDEBUG).
#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "CHECK failed: %s (line %d)\\n", #expr, __LINE__); \
    return 1; \
} } while (false)

int main() {
    std::array<float, kFFTFrames * 2> stereo{};
    constexpr float pi = std::numbers::pi_v<float>;
    for (std::size_t i = 0; i < kFFTFrames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        stereo[i * 2] = 0.6f * std::sin(2.0f * pi * 110.0f * t);
        stereo[i * 2 + 1] = 0.4f * std::sin(2.0f * pi * 880.0f * t);
    }

    // Bass in the middle on the center stereo panel:
    // left half maps low bin -> right edge, right half stays low -> left edge.
    for (const std::size_t count : {64u, 96u, 128u, 256u}) {
        for (std::size_t b = 0; b < count; ++b) {
            CHECK(visualBandIndex(b, count, false) == b);
            CHECK(visualBandIndex(b, count, true) == count - 1 - b);
        }
        CHECK(visualBandIndex(0, count, true) == count - 1);
        CHECK(visualBandIndex(0, count, false) == 0);
        CHECK(visualBandIndex(count - 1, count, true) == 0);
        // Left portrait remains low-to-high; right portrait mirrors by default.
        CHECK(!portraitBandMirrored(true));
        CHECK(portraitBandMirrored(false));
        CHECK(!portraitBandMirrored(false, false));
        CHECK(visualBandIndex(0, count, portraitBandMirrored(true)) == 0);
        CHECK(visualBandIndex(0, count, portraitBandMirrored(false)) == count - 1);
        CHECK(visualBandIndex(count - 1, count, portraitBandMirrored(false)) == 0);
    }

    // Curves must interpolate the actual FFT values without overshoot and
    // preserve both channel orientation and left/center/right mirror behavior.
    for (const std::size_t bands : {64u, 96u, 128u, 192u, 256u}) {
        std::array<float, kMaxBands> ramp{};
        for (std::size_t i = 0; i < bands; ++i)
            ramp[i] = static_cast<float>(i) /
                      static_cast<float>(bands - 1);
        for (bool mirror : {false, true}) {
            const float firstExpected = mirror ? 1.0f : 0.0f;
            const float lastExpected = mirror ? 0.0f : 1.0f;
            CHECK(std::abs(sampleSpectrumCurve(ramp, bands, 0.0f, mirror)
                           - firstExpected) < 1.0e-6f);
            CHECK(std::abs(sampleSpectrumCurve(ramp, bands,
                           static_cast<float>(bands - 1), mirror)
                           - lastExpected) < 1.0e-6f);
            for (std::size_t i = 0; i < (bands - 1) * 3 + 1; ++i) {
                const float pos = static_cast<float>(i) / 3.0f;
                const float v = sampleSpectrumCurve(ramp, bands, pos, mirror);
                CHECK(std::isfinite(v));
                CHECK(v >= 0.0f && v <= 1.0f);
                const auto bin = static_cast<std::size_t>(pos);
                const float a = ramp[visualBandIndex(bin, bands, mirror)];
                const float b = ramp[visualBandIndex(
                    std::min(bin + 1, bands - 1), bands, mirror)];
                CHECK(v >= std::min(a,b)-1e-6f);
                CHECK(v <= std::max(a,b)+1e-6f);
            }
        }
    }

    // Log frequency axis endpoints, bucket center and dBFS mapping.
    CHECK(std::abs(spectrumFrequencyFraction(30.0f)) < 1.0e-6f);
    CHECK(std::abs(spectrumFrequencyFraction(18000.0f) - 1.0f) < 1.0e-6f);
    CHECK(std::abs(spectrumFrequencyAtFraction(0.0f) - 30.0f) < 0.01f);
    CHECK(std::abs(spectrumFrequencyAtFraction(1.0f) - 18000.0f) < 0.1f);
    CHECK(std::abs(spectrumDbAtFraction(0.0f) + 72.0f) < 0.001f);
    CHECK(std::abs(spectrumDbAtFraction(1.0f) + 2.0f) < 0.001f);
    CHECK(std::abs(spectrumDbFraction(-37.0f) - 0.5f) < 0.001f);
    CHECK(spectrumBandCenterHz(0, 128) >= 30.0f);
    CHECK(spectrumBandCenterHz(127, 128) <= 18000.0f);

    // Silence is centered; equal RMS is balanced; positive bias is R.
    CHECK(stereoBalanceDb(0.0f, 0.0f) == 0.0f);
    CHECK(stereoBalanceDb(0.00001f, 0.00002f) == 0.0f);
    CHECK(std::abs(stereoBalanceDb(0.5f, 0.5f)) < 0.001f);
    CHECK(std::abs(stereoBalanceDb(0.5f, 1.0f) - 6.0206f) < 0.002f);
    CHECK(std::abs(stereoBalanceDb(1.0f, 0.5f) + 6.0206f) < 0.002f);
    CHECK(stereoBalancePosition(100.0f) == 1.0f);
    CHECK(stereoBalancePosition(-100.0f) == -1.0f);
    CHECK(stereoBalancePosition(0.0f) == 0.0f);

    SpectrumAnalyzer analyzer;
    CHECK(analyzer.bandCount() == 128);

    for (const std::size_t count : { 64u, 80u, 96u, 128u, 192u, 224u, 256u }) {
        analyzer.setBandCount(count);
        CHECK(analyzer.bandCount() == count);
        // Out-of-range bars must remain inactive after changing the partition.
        const auto& blank = analyzer.state();
        for (std::size_t b = count; b < kMaxBands; ++b) {
            CHECK(blank.left[b] == 0.0f);
            CHECK(blank.right[b] == 0.0f);
        }

        analyzer.update(stereo, 1.0f / 60.0f);
        const auto& result = analyzer.state();
        CHECK(result.bandCount == count);
        CHECK(result.rmsLeft > 0.1f && result.rmsRight > 0.1f);
        CHECK(result.peakLeft > 0.5f && result.peakRight > 0.3f);
        CHECK(std::isfinite(result.balanceDb));
        CHECK(std::isfinite(result.balanceDbSmoothed));
        CHECK(result.balanceDb < 0.0f); // left 0.6, right 0.4
        bool rawLeftDetected = false;
        for (std::size_t b = 0; b < count; ++b)
            rawLeftDetected |= result.leftRawDb[b] > -40.0f;
        CHECK(rawLeftDetected);
        bool leftActive = false, rightActive = false;
        for (std::size_t b = 0; b < count; ++b) {
            CHECK(result.left[b] >= 0.0f && result.left[b] <= 1.0f);
            CHECK(result.right[b] >= 0.0f && result.right[b] <= 1.0f);
            CHECK(result.leftPeaks[b] >= 0.0f && result.leftPeaks[b] <= 1.0f);
            CHECK(result.rightPeaks[b] >= 0.0f && result.rightPeaks[b] <= 1.0f);
            leftActive |= result.left[b] > 0.0f;
            rightActive |= result.right[b] > 0.0f;
        }
        CHECK(leftActive && rightActive);
    }

    // Verify temporal smoothing modes and their runtime selection.
    SpectrumAnalyzer quick(128), fluid(128);
    quick.setSmoothingMode(SmoothingMode::Fast);
    fluid.setSmoothingMode(SmoothingMode::Liquid);
    quick.setSpectralSoftening(false);
    fluid.setSpectralSoftening(false);
    CHECK(!quick.spectralSoftening());
    quick.update(stereo, 1.0f / 60.0f);
    fluid.update(stereo, 1.0f / 60.0f);
    bool foundFasterAttack = false;
    for (std::size_t b = 0; b < 128; ++b)
        if (quick.state().left[b] > fluid.state().left[b] + 0.01f)
            foundFasterAttack = true;
    CHECK(foundFasterAttack);
    CHECK(quick.smoothingMode() == SmoothingMode::Fast);
    CHECK(fluid.smoothingMode() == SmoothingMode::Liquid);
    CHECK(smoothingModeName(SmoothingMode::Liquid)[0] == 'L');

    for (int palette = 0; palette < kPaletteCount; ++palette) {
        const Rgb bass = paletteRgb(0.05f, 0.2f, 0.9f, palette, false);
        const Rgb treble = paletteRgb(0.95f, 0.8f, 0.9f, palette, true);
        CHECK(bass.r != treble.r || bass.g != treble.g ||
              bass.b != treble.b);
        CHECK(paletteName(palette)[0] != '\0');
    }
    CHECK(hsvToRgb(0.0f, 1.0f, 1.0f).r == 255);
    CHECK(hsvToRgb(120.0f, 1.0f, 1.0f).g == 255);
    CHECK(hsvToRgb(240.0f, 1.0f, 1.0f).b == 255);

    analyzer.setBandCount(1);
    CHECK(analyzer.bandCount() == kMinBands);
    analyzer.setBandCount(500);
    CHECK(analyzer.bandCount() == kMaxBands);
    std::puts("Spectrum band switching and stereo FFT tests passed.");
    return 0;
}
