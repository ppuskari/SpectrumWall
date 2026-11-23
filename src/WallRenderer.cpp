#include "WallRenderer.hpp"
#include "VisualStyle.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>

namespace spectrum {
namespace {
SDL_Color colorFor(float frequency, float height, float energy,
                   bool illuminated, int theme, bool right = false,
                   Uint8 alpha = 255) {
    if (!illuminated) return { 11, 20, 29, 255 };
    const Rgb rgb = paletteRgb(frequency, height, energy, theme, right);
    return { rgb.r, rgb.g, rgb.b, alpha };
}

void setColor(SDL_Renderer* r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void rect(SDL_Renderer* r, SDL_FRect position, SDL_Color color) {
    if (position.w <= 0.0f || position.h <= 0.0f) return;
    setColor(r, color);
    SDL_RenderFillRect(r, &position);
}

void line(SDL_Renderer* r, float x1, float y1, float x2, float y2, SDL_Color color) {
    setColor(r, color);
    SDL_RenderLine(r, x1, y1, x2, y2);
}

const char* roleName(ScreenRole r) {
    switch (r) {
    case ScreenRole::Center: return "STEREO MASTER";
    case ScreenRole::Left: return "LEFT CHANNEL";
    case ScreenRole::Right: return "RIGHT CHANNEL";
    case ScreenRole::Waterfall: return "SPECTRAL WATERFALL";
    }
    return "SPECTRUM";
}
}

WallRenderer::~WallRenderer() {
    for (auto& s : screens_) {
        if (s.glowTarget) SDL_DestroyTexture(s.glowTarget);
        if (s.renderer) SDL_DestroyRenderer(s.renderer);
        if (s.window) SDL_DestroyWindow(s.window);
    }
}

bool WallRenderer::initialize(bool enableFourth, bool windowed) {
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (ids == nullptr || count < 1) {
        std::fprintf(stderr, "No displays: %s\n", SDL_GetError());
        if (ids) SDL_free(ids);
        return false;
    }
    std::vector<SDL_Rect> bounds;
    for (int i = 0; i < count; ++i) {
        SDL_Rect b{};
        if (SDL_GetDisplayBounds(ids[i], &b)) bounds.push_back(b);
    }
    SDL_free(ids);
    if (bounds.empty()) return false;

    auto area = [&bounds](std::size_t i) {
        return static_cast<long long>(bounds[i].w) * bounds[i].h;
    };
    std::size_t center = 0;
    for (std::size_t i = 0; i < bounds.size(); ++i) {
        const bool landscape = bounds[i].w >= bounds[i].h;
        const bool centerLandscape = bounds[center].w >= bounds[center].h;
        if ((landscape && !centerLandscape) ||
            (landscape == centerLandscape &&
             (area(i) > area(center) ||
              (area(i) == area(center) && bounds[i].y > bounds[center].y)))) {
            center = i;
        }
    }
    struct Assignment { std::size_t index; ScreenRole role; };
    std::vector<Assignment> assigned = { {center, ScreenRole::Center} };
    const float centerX = bounds[center].x + bounds[center].w * 0.5f;
    std::optional<std::size_t> left, right;

    for (std::size_t i = 0; i < bounds.size(); ++i) {
        if (i == center || bounds[i].h <= bounds[i].w) continue;
        const float x = bounds[i].x + bounds[i].w * 0.5f;
        if (x < centerX && (!left || bounds[i].x > bounds[*left].x)) left = i;
        if (x >= centerX && (!right || bounds[i].x < bounds[*right].x)) right = i;
    }
    if (left) assigned.push_back({*left, ScreenRole::Left});
    if (right) assigned.push_back({*right, ScreenRole::Right});

    if (enableFourth) {
        for (std::size_t i = 0; i < bounds.size(); ++i) {
            if (i == center || bounds[i].w <= bounds[i].h) continue;
            assigned.push_back({i, ScreenRole::Waterfall});
            break;
        }
    }

    for (const auto& entry : assigned) {
        SDL_Rect b = bounds[entry.index];
        const char* name = roleName(entry.role);
        if (windowed) {
            b.w = static_cast<int>(b.w * 0.68f);
            b.h = static_cast<int>(b.h * 0.68f);
        }
        const SDL_WindowFlags flags = windowed
            ? (SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)
            : (SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        SDL_Window* window = SDL_CreateWindow(name, b.w, b.h, flags);
        if (!window) {
            std::fprintf(stderr, "Window failed: %s\n", SDL_GetError());
            return false;
        }
        SDL_SetWindowPosition(window, b.x, b.y);
        SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer) {
            std::fprintf(stderr, "Renderer failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            return false;
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderVSync(renderer, 0); // one shared render loop governs all screens
        screens_.push_back({entry.role, window, renderer, b});
        std::printf("Display %s: %dx%d at (%d,%d), backend=%s\n", name,
                    b.w, b.h, b.x, b.y, SDL_GetRendererName(renderer));
    }
    return !screens_.empty();
}

void WallRenderer::drawSpectrum(SDL_Renderer* r, SDL_FRect plot,
                                const std::array<float, kMaxBands>& values,
                                const std::array<float, kMaxBands>& peaks,
                                const std::array<float, kMaxBands>& trails,
                                std::size_t bands, int theme, bool right) {
    constexpr int segments = 36;
    const float step = plot.w / static_cast<float>(bands);
    const float segmentH = plot.h / static_cast<float>(segments);
    const float ledWidth = std::max(0.4f, step * 0.84f);
    const float ledHeight = std::max(1.0f, segmentH * 0.82f);
    const Uint64 clock = SDL_GetTicks();
    for (std::size_t b = 0; b < bands; ++b) {
        const float x = plot.x + step * static_cast<float>(b);
        const float freq = static_cast<float>(b) / static_cast<float>(bands - 1);
        for (int s = 0; s < segments; ++s) {
            const float level = static_cast<float>(s + 1) / segments;
            const float y = plot.y + plot.h - (s + 1) * segmentH;
            const bool on = values[b] >= level;
            const bool ghost = trailsEnabled_ && !on && trails[b] >= level;
            SDL_Color color;
            if (on) {
                color = colorFor(freq, level, values[b], true, theme, right);
                if (glowMode_ && segmentH > 4.0f && step > 2.0f && level > 0.4f) {
                    SDL_Color halo = color; halo.a = 38;
                    rect(r, {x - 1.5f, y - 1.5f, ledWidth + 3.0f,
                             ledHeight + 3.0f}, halo);
                }
            } else if (ghost) {
                color = colorFor(freq, level, trails[b], true, theme, right,
                                 static_cast<Uint8>(45 + 65 * level));
            } else {
                const Rgb dim = paletteRgb(freq, level, 0.12f, theme, right);
                color = { static_cast<Uint8>(dim.r * 0.07f + 6),
                          static_cast<Uint8>(dim.g * 0.07f + 8),
                          static_cast<Uint8>(dim.b * 0.07f + 11), 255 };
            }
            rect(r, {x, y, ledWidth, ledHeight}, color);
        }
        const float peak = std::clamp(peaks[b], 0.0f, 1.0f);
        if (peak < 0.02f) continue;
        const float py = plot.y + (1.0f - peak) * plot.h;
        const Rgb tint = paletteRgb(freq, peak, peak, theme, right);
        rect(r, {x, py, ledWidth, std::max(1.4f, segmentH * 0.28f)},
             {static_cast<Uint8>(std::min(255, tint.r + 40)),
              static_cast<Uint8>(std::min(255, tint.g + 40)),
              static_cast<Uint8>(std::min(255, tint.b + 40)), 255});
        if (glowMode_ && peak > 0.5f && ((b * 37u + clock / 100u) % 19u == 0u)) {
            const float cx = x + ledWidth * 0.5f;
            line(r, cx - 4, py, cx + 4, py, {240, 249, 255, 210});
            line(r, cx, py - 4, cx, py + 4, {240, 249, 255, 210});
        }
    }
}

void WallRenderer::drawWave(SDL_Renderer* r, SDL_FRect plot,
                            const std::array<float, kWavePoints>& samples, SDL_Color color) {
    const float middle = plot.y + plot.h / 2.0f;
    line(r, plot.x, middle, plot.x + plot.w, middle, { 35, 65, 78, 200 });
    for (std::size_t i = 1; i < kWavePoints; ++i) {
        const float x0 = plot.x + (i - 1) * plot.w / (kWavePoints - 1);
        const float x1 = plot.x + i * plot.w / (kWavePoints - 1);
        const float y0 = middle - std::clamp(samples[i - 1], -1.0f, 1.0f) * plot.h * 0.43f;
        const float y1 = middle - std::clamp(samples[i], -1.0f, 1.0f) * plot.h * 0.43f;
        SDL_Color halo = color; halo.a = 50;
        line(r, x0, y0 - 2, x1, y1 - 2, halo);
        line(r, x0, y0 + 2, x1, y1 + 2, halo);
        line(r, x0, y0, x1, y1, color);
    }
}

void WallRenderer::drawLevel(SDL_Renderer* r, SDL_FRect plot, float rms,
                             float peak, int theme) {
    const float level = std::clamp(rms, 0.0f, 1.0f);
    const float peakPos = std::clamp(peak, 0.0f, 1.0f);
    constexpr int segments = 80;
    const float step = plot.w / segments;
    for (int i = 0; i < segments; ++i) {
        const float f = static_cast<float>(i + 1) / segments;
        rect(r, {plot.x + i * step, plot.y, step * 0.86f, plot.h},
             colorFor(f, f <= level, theme));
    }
    rect(r, { plot.x + peakPos * plot.w - 2.0f, plot.y - 2.0f, 4.0f, plot.h + 4.0f },
         peak > 0.99f ? SDL_Color{255, 40, 44, 255}
                      : SDL_Color{235, 241, 255, 255});
    // A faint reference line marks the project's -2 dBFS headroom target.
    const float target = std::pow(10.0f, -2.0f / 20.0f);
    line(r, plot.x + target * plot.w, plot.y - 3.0f,
         plot.x + target * plot.w, plot.y + plot.h + 3.0f,
         { 240, 240, 240, 150 });
}

void WallRenderer::drawPortrait(SDL_Renderer* r, float w, float h,
                                const VisualState& state, bool left, int theme) {
    const float pad = std::max(12.0f, w * 0.027f);
    const auto& values = left ? state.left : state.right;
    const auto& peaks = left ? state.leftPeaks : state.rightPeaks;
    drawSpectrum(r, {pad, h * 0.06f, w - 2 * pad, h * 0.75f},
                 values, peaks, state.bandCount, theme);
    drawWave(r, {pad, h * 0.84f, w - 2 * pad, h * 0.07f},
             left ? state.waveLeft : state.waveRight,
             left ? SDL_Color{ 63, 226, 232, 255 } : SDL_Color{ 228, 112, 249, 255 });
    drawLevel(r, {pad, h * 0.94f, w - 2 * pad, h * 0.015f},
              left ? state.vuLeft : state.vuRight,
              left ? state.peakLeft : state.peakRight, theme);
}

void WallRenderer::drawCenter(SDL_Renderer* r, float w, float h,
                              const VisualState& state, int theme) {
    const float pad = std::max(20.0f, w * 0.026f);
    const float width = w - 2 * pad;
    const float half = width * 0.495f;
    drawSpectrum(r, {pad, h * 0.055f, half, h * 0.55f},
                 state.left, state.leftPeaks, state.bandCount, theme);
    drawSpectrum(r, {pad + width * 0.505f, h * 0.055f, half, h * 0.55f},
                 state.right, state.rightPeaks, state.bandCount, theme);
    line(r, w * 0.5f, h * 0.045f, w * 0.5f, h * 0.61f,
         { 40, 70, 88, 200 });
    drawWave(r, {pad, h * 0.65f, width, h * 0.115f}, state.waveLeft,
             { 60, 228, 245, 255 });
    drawWave(r, {pad, h * 0.77f, width, h * 0.115f}, state.waveRight,
             { 208, 113, 255, 255 });
    drawLevel(r, {pad, h * 0.927f, width * 0.485f, h * 0.025f},
              state.vuLeft, state.peakLeft, theme);
    drawLevel(r, {pad + width * 0.515f, h * 0.927f, width * 0.485f, h * 0.025f},
              state.vuRight, state.peakRight, theme);
}

void WallRenderer::drawWaterfall(SDL_Renderer* r, float w, float h,
                                 std::size_t bands, int) {
    const float pad = std::max(18.0f, w * 0.016f);
    const float width = w - 2 * pad, height = h - 2 * pad;
    if (history_.empty()) return;
    const float cellW = width / static_cast<float>(bands);
    const float cellH = height / static_cast<float>(history_.size());
    std::size_t row = 0;
    for (const auto& frame : history_) {
        for (std::size_t band = 0; band < bands; ++band) {
            const float strength = std::clamp(frame[band], 0.0f, 1.0f);
            const auto v = static_cast<unsigned char>(strength * 255.0f);
            rect(r, {pad + band * cellW, pad + row * cellH, cellW + 0.25f,
                     cellH + 0.25f},
                 { static_cast<Uint8>(std::min(255, static_cast<int>(v) * 2)),
                   static_cast<Uint8>(strength > 0.5f ? (strength - 0.5f) * 510 : 0),
                   static_cast<Uint8>(std::min(255, 32 + static_cast<int>(v))),
                   255 });
        }
        ++row;
    }
}

void WallRenderer::drawScreen(const ScreenWindow& screen,
                              const VisualState& state, int theme) {
    SDL_Renderer* r = screen.renderer;
    int width = 0, height = 0;
    if (!SDL_GetRenderOutputSize(r, &width, &height)) return;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const auto pulse = static_cast<Uint8>(state.bassPulse * 20.0f);
    SDL_SetRenderDrawColor(r, static_cast<Uint8>(6 + pulse / 3),
                          static_cast<Uint8>(10 + pulse / 2),
                          static_cast<Uint8>(18 + pulse), 255);
    SDL_RenderClear(r);
    for (int i = 1; i <= 12; ++i) {
        const float y = i * h / 13.0f;
        line(r, 0, y, w, y, { 27, 42, 55, 60 });
    }
    switch (screen.role) {
    case ScreenRole::Left: drawPortrait(r, w, h, state, true, theme); break;
    case ScreenRole::Right: drawPortrait(r, w, h, state, false, theme); break;
    case ScreenRole::Center: drawCenter(r, w, h, state, theme); break;
    case ScreenRole::Waterfall: drawWaterfall(r, w, h, state.bandCount, theme); break;
    }
    // Minimal hardware-accelerated OSD confirms live band changes in fullscreen.
    SDL_SetRenderDrawColor(r, 192, 224, 232, 235);
    SDL_SetRenderScale(r, 2.0f, 2.0f);
    SDL_RenderDebugTextFormat(r, 8.0f, 8.0f, "%zu BANDS", state.bandCount);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
    SDL_RenderPresent(r);
}

void WallRenderer::draw(const VisualState& state, int theme) {
    if (historyBandCount_ != state.bandCount) {
        history_.clear();
        historyBandCount_ = state.bandCount;
    }
    std::array<float, kMaxBands> combined{};
    for (std::size_t i = 0; i < state.bandCount; ++i)
        combined[i] = std::max(state.left[i], state.right[i]);
    history_.push_back(combined);
    if (history_.size() > 180) history_.pop_front();
    for (const auto& s : screens_) drawScreen(s, state, theme);
}
} // namespace spectrum
