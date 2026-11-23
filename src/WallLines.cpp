#include "WallRenderer.hpp"
#include "CurveSampling.hpp"
#include "SpectrumLayout.hpp"
#include "VisualStyle.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace spectrum {
namespace {
constexpr int kCurveSubsteps = 3; // 3 subpixels per FFT band, up to 766 curve points

SDL_FColor scaledColor(Rgb color, float alpha) {
    constexpr float scale = 1.0f / 255.0f;
    return {color.r * scale, color.g * scale, color.b * scale, alpha};
}

void stroke(SDL_Renderer* r, const std::vector<SDL_FPoint>& path,
            SDL_Color c, const std::initializer_list<float>& offsets) {
    if (path.size() < 2) return;
    std::vector<SDL_FPoint> shifted(path.size());
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (float offset : offsets) {
        for (std::size_t i = 0; i < path.size(); ++i)
            shifted[i] = {path[i].x, path[i].y + offset};
        SDL_RenderLines(r, shifted.data(), static_cast<int>(shifted.size()));
    }
}
} // namespace

void WallRenderer::drawCurveSpectrum(SDL_Renderer* r, SDL_FRect plot,
                                     const std::array<float, kMaxBands>& values,
                                     const std::array<float, kMaxBands>& peaks,
                                     const std::array<float, kMaxBands>& trails,
                                     std::size_t bands, int palette, bool right,
                                     bool mirror, bool fill) {
    if (bands < 2 || plot.w < 8 || plot.h < 8) return;
    const std::size_t points = (bands - 1) * kCurveSubsteps + 1;
    std::vector<SDL_FPoint> current, peakPath, trailPath;
    current.reserve(points); peakPath.reserve(points); trailPath.reserve(points);

    // Subtle graph divisions: retro oscilloscope meets Task Manager telemetry.
    SDL_SetRenderDrawColor(r, 52, 91, 110, 48);
    for (int i = 1; i < 5; ++i) {
        const float y = plot.y + plot.h * (i / 5.0f);
        SDL_RenderLine(r, plot.x, y, plot.x + plot.w, y);
    }

    std::vector<SDL_Vertex> fillVertices;
    std::vector<int> fillIndices;
    if (fill) {
        fillVertices.reserve(points * 2);
        fillIndices.reserve((points - 1) * 6);
    }
    const float baseline = plot.y + plot.h;
    for (std::size_t i = 0; i < points; ++i) {
        const float fraction = static_cast<float>(i) /
                               static_cast<float>(points - 1);
        const float band = fraction * static_cast<float>(bands - 1);
        const float v = sampleSpectrumCurve(values, bands, band, mirror);
        const float held = sampleSpectrumCurve(peaks, bands, band, mirror);
        const float ghost = sampleSpectrumCurve(trails, bands, band, mirror);
        const float x = plot.x + fraction * plot.w;
        const float y = plot.y + (1.0f - v) * plot.h;
        current.push_back({x, y});
        peakPath.push_back({x, plot.y + (1.0f - held) * plot.h});
        trailPath.push_back({x, plot.y + (1.0f - ghost) * plot.h});

        if (fill) {
            const float freq = mirror ? 1.0f - fraction : fraction;
            const Rgb rgb = paletteRgb(freq, v, v, palette, right);
            SDL_Vertex top{};
            top.position = {x, y};
            top.color = scaledColor(rgb, 0.12f + 0.11f * v);
            SDL_Vertex bottom{};
            bottom.position = {x, baseline};
            bottom.color = scaledColor(rgb, 0.008f);
            fillVertices.push_back(top);
            fillVertices.push_back(bottom);
            if (i > 0) {
                const int last = static_cast<int>(i * 2);
                fillIndices.insert(fillIndices.end(),
                    {last-2, last-1, last, last, last-1, last+1});
            }
        }
    }
    if (fill && !fillIndices.empty()) {
        SDL_RenderGeometry(r, nullptr, fillVertices.data(),
                           static_cast<int>(fillVertices.size()),
                           fillIndices.data(), static_cast<int>(fillIndices.size()));
    }

    // Long persistence envelope, clearly dimmer than both live and held peaks.
    if (trailsEnabled_) {
        stroke(r, trailPath, right ? SDL_Color{197, 89, 234, 35}
                                  : SDL_Color{42, 211, 206, 35}, {-2,0,2});
        stroke(r, trailPath, right ? SDL_Color{203, 116, 255, 90}
                                  : SDL_Color{55, 233, 239, 90}, {0});
    }

    // Bright, longer-lived peak-hold tracer. Not the same as the live signal.
    if (glowMode_ > 0)
        stroke(r, peakPath, {239, 232, 255, 26}, {-3,-2,2,3});
    stroke(r, peakPath, right ? SDL_Color{255, 185, 240, 176}
                              : SDL_Color{208, 237, 255, 176}, {0});

    // Neon current trace: a warm blurred outer fringe plus gradient colored core.
    if (glowMode_ > 0) {
        stroke(r, current, right ? SDL_Color{205, 72, 233, 22}
                                 : SDL_Color{44, 225, 227, 22},
               glowMode_ == 2 ? std::initializer_list<float>{-5,-3,3,5}
                              : std::initializer_list<float>{-2,2});
        stroke(r, current, right ? SDL_Color{218, 105, 250, 58}
                                 : SDL_Color{91, 231, 240, 58}, {-1.25f,1.25f});
    }

    // Frequency-scaled palette color follows original bin identity even when
    // the center-left or right portrait visualization is mirrored.
    for (std::size_t i = 1; i < points; ++i) {
        const float fraction = (static_cast<float>(i) - 0.5f) /
                               static_cast<float>(points - 1);
        const float freq = mirror ? 1.0f - fraction : fraction;
        const float activity = std::clamp(1.0f -
            (current[i - 1].y + current[i].y - 2.0f * plot.y) /
            (2.0f * plot.h), 0.0f, 1.0f);
        const Rgb color = paletteRgb(freq, activity, activity, palette, right);
        SDL_SetRenderDrawColor(r, color.r, color.g, color.b, 255);
        SDL_RenderLine(r, current[i-1].x, current[i-1].y,
                      current[i].x, current[i].y);
    }
}

void WallRenderer::drawCurveGlow(SDL_Renderer* r, SDL_FRect plot,
                                 const std::array<float, kMaxBands>& values,
                                 std::size_t bands, int palette, bool right,
                                 bool mirror) {
    if (bands < 2) return;
    const std::size_t points = (bands - 1) * kCurveSubsteps + 1;
    std::vector<SDL_FPoint> path;
    path.reserve(points);
    for (std::size_t i = 0; i < points; ++i) {
        const float fraction = static_cast<float>(i) /
                               static_cast<float>(points - 1);
        const float amplitude = sampleSpectrumCurve(
            values, bands, fraction * (bands - 1), mirror);
        path.push_back({plot.x + fraction * plot.w,
                        plot.y + (1.0f - amplitude) * plot.h});
    }
    const Rgb tint = paletteRgb(right ? 0.85f : 0.25f,
                                0.7f, 0.8f, palette, right);
    stroke(r, path, {tint.r, tint.g, tint.b,
                     static_cast<Uint8>(glowMode_ == 2 ? 180 : 105)},
           {-8,-5,-2,0,2,5,8});
}

void WallRenderer::drawSpectrumStyled(SDL_Renderer* r, SDL_FRect plot,
                                      const std::array<float, kMaxBands>& values,
                                      const std::array<float, kMaxBands>& peaks,
                                      const std::array<float, kMaxBands>& trails,
                                      std::size_t bands, int palette, bool right,
                                      bool mirror) {
    if (spectrumStyle_ != SpectrumStyle::Lines) {
        drawSpectrum(r, plot, values, peaks, trails, bands, palette,
                     right, mirror);
    }
    if (spectrumStyle_ != SpectrumStyle::Led) {
        drawCurveSpectrum(r, plot, values, peaks, trails, bands, palette,
                          right, mirror, spectrumStyle_ == SpectrumStyle::Lines);
    }
}
} // namespace spectrum
