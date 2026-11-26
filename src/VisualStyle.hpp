#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spectrum {

// Colors are computed from both logarithmic frequency and vertical LED level.
// All palettes maintain luminance contrast from low-energy to high-energy bars.
struct Rgb { std::uint8_t r, g, b; };
inline constexpr int kPaletteCount = 8;

inline const char* paletteName(int palette) {
    switch (palette) {
    case 0: return "CLASSIC HIFI";
    case 1: return "NEON NIGHTS";
    case 2: return "GOLDEN AMBER";
    case 3: return "PRISM RAINBOW";
    case 4: return "AURORA";
    case 5: return "INFERNO";
    case 6: return "ICE BLUE";
    case 7: return "VINTAGE PHOSPHOR";
    default: return "CLASSIC HIFI";
    }
}

inline Rgb hsvToRgb(float hue, float saturation, float value) {
    hue = hue - std::floor(hue / 360.0f) * 360.0f;
    saturation = std::clamp(saturation, 0.0f, 1.0f);
    value = std::clamp(value, 0.0f, 1.0f);
    const float c = value * saturation;
    const float h = hue / 60.0f;
    const float x = c * (1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f));
    const float m = value - c;
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (h < 1)      { r = c; g = x; }
    else if (h < 2) { r = x; g = c; }
    else if (h < 3) { g = c; b = x; }
    else if (h < 4) { g = x; b = c; }
    else if (h < 5) { r = x; b = c; }
    else            { r = c; b = x; }
    const auto byte = [](float v) -> std::uint8_t {
        return static_cast<std::uint8_t>(
            std::clamp(static_cast<int>(std::lround(v * 255.0f)), 0, 255));
    };
    return {byte(r + m), byte(g + m), byte(b + m)};
}

inline Rgb paletteRgb(float frequency, float ledHeight, float energy, int palette,
                      bool rightChannel = false) {
    const float f = std::clamp(frequency, 0.0f, 1.0f);
    const float y = std::clamp(ledHeight, 0.0f, 1.0f);
    const float e = std::clamp(energy, 0.0f, 1.0f);

    float hue = 130.0f, saturation = 0.88f;
    switch (palette) {
    case 0: // Original green -> yellow -> red, with subtle frequency shading
        hue = y < 0.64f ? 144.0f - y * 125.0f
              : 64.0f - (y - 0.64f) * 160.0f;
        hue += 8.0f * f;
        break;
    case 1: // Electric cyan/violet/pink, stereo tint
        hue = 177.0f + f * 113.0f + y * 38.0f + (rightChannel ? 22.0f : 0.0f);
        break;
    case 2: // Gold to orange to vermilion
        hue = 53.0f - y * 45.0f - f * 7.0f;
        saturation = 0.98f;
        break;
    case 3: // Each frequency group receives its own rainbow hue
        hue = 300.0f * f + 30.0f * y;
        saturation = 0.92f;
        break;
    case 4: // Emerald -> teal -> deep blue
        hue = 135.0f + 98.0f * f + 19.0f * y;
        break;
    case 5: // Red / violet fire with glowing upper tips
        hue = 345.0f + 78.0f * f + 35.0f * y;
        saturation = 0.99f;
        break;
    case 6: // Cyan/blue/near-white
        hue = 184.0f + f * 56.0f;
        saturation = 0.90f - 0.68f * y * y;
        break;
    case 7: // Old-school amber CRT
        hue = 32.0f + 9.0f * f - 7.0f * y;
        saturation = 0.95f - 0.22f * y;
        break;
    default: break;
    }

    // Energy-aware brightness; avoid overly white/washed-out quiet signals.
    const float value = std::clamp(
        (0.47f + 0.32f * y + 0.21f * std::sqrt(e)), 0.0f, 1.0f);
    return hsvToRgb(hue, saturation, value);
}

} // namespace spectrum
