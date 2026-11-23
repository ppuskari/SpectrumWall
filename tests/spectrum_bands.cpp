#include "SpectrumAnalyzer.hpp"
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
