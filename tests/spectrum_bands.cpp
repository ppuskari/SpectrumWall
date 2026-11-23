#include "SpectrumAnalyzer.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <numbers>

using namespace spectrum;

int main() {
    std::array<float, kFFTFrames * 2> stereo{};
    constexpr float pi = std::numbers::pi_v<float>;
    for (std::size_t i = 0; i < kFFTFrames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        stereo[i * 2] = 0.6f * std::sin(2.0f * pi * 110.0f * t);
        stereo[i * 2 + 1] = 0.4f * std::sin(2.0f * pi * 880.0f * t);
    }

    SpectrumAnalyzer analyzer;
    assert(analyzer.bandCount() == 128);

    for (const std::size_t count : { 64u, 80u, 96u, 128u, 192u, 224u, 256u }) {
        analyzer.setBandCount(count);
        assert(analyzer.bandCount() == count);
        // Out-of-range bars must remain inactive after changing the partition.
        const auto& blank = analyzer.state();
        for (std::size_t b = count; b < kMaxBands; ++b) {
            assert(blank.left[b] == 0.0f);
            assert(blank.right[b] == 0.0f);
        }

        analyzer.update(stereo, 1.0f / 60.0f);
        const auto& result = analyzer.state();
        assert(result.bandCount == count);
        assert(result.rmsLeft > 0.1f && result.rmsRight > 0.1f);
        assert(result.peakLeft > 0.5f && result.peakRight > 0.3f);
        bool leftActive = false, rightActive = false;
        for (std::size_t b = 0; b < count; ++b) {
            assert(result.left[b] >= 0.0f && result.left[b] <= 1.0f);
            assert(result.right[b] >= 0.0f && result.right[b] <= 1.0f);
            assert(result.leftPeaks[b] >= 0.0f && result.leftPeaks[b] <= 1.0f);
            assert(result.rightPeaks[b] >= 0.0f && result.rightPeaks[b] <= 1.0f);
            leftActive |= result.left[b] > 0.0f;
            rightActive |= result.right[b] > 0.0f;
        }
        assert(leftActive && rightActive);
    }

    analyzer.setBandCount(1);
    assert(analyzer.bandCount() == kMinBands);
    analyzer.setBandCount(500);
    assert(analyzer.bandCount() == kMaxBands);
    std::puts("Spectrum band switching and stereo FFT tests passed.");
    return 0;
}
