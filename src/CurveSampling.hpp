#pragma once
#include "SpectrumLayout.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace spectrum {

// Interpolates a visual envelope in *display* order, preserving the source
// frequency axis (including portrait and center mirroring) and sample endpoints.
// Hermite tangents are clamped so peaks never overshoot the actual FFT values.
template<std::size_t Capacity>
inline float sampleSpectrumCurve(const std::array<float, Capacity>& values,
                                 std::size_t bands, float displayIndex,
                                 bool mirror) {
    if (bands == 0) return 0.0f;
    if (bands == 1) return std::clamp(values[0], 0.0f, 1.0f);
    const float x = std::clamp(displayIndex, 0.0f, static_cast<float>(bands - 1));
    const std::size_t i = static_cast<std::size_t>(std::floor(x));
    if (i >= bands - 1)
        return std::clamp(values[visualBandIndex(bands - 1, bands, mirror)],
                          0.0f, 1.0f);

    const auto value = [&](std::size_t at) {
        return std::clamp(values[visualBandIndex(at, bands, mirror)], 0.0f, 1.0f);
    };
    const float a = value(i);
    const float b = value(i + 1);
    const float before = value(i == 0 ? 0 : i - 1);
    const float after = value(std::min(i + 2, bands - 1));
    const float t = x - static_cast<float>(i);
    const float t2 = t * t, t3 = t2 * t;
    const float m0 = 0.5f * (b - before);
    const float m1 = 0.5f * (after - a);
    const float interpolated =
        (2.0f * t3 - 3.0f * t2 + 1.0f) * a +
        (t3 - 2.0f * t2 + t) * m0 +
        (-2.0f * t3 + 3.0f * t2) * b +
        (t3 - t2) * m1;
    // Avoid ringing/overshoot above or below actual neighboring amplitudes.
    return std::clamp(interpolated, std::min(a, b), std::max(a, b));
}
} // namespace spectrum
