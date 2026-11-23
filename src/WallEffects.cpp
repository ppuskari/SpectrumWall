#include "WallRenderer.hpp"
#include "VisualStyle.hpp"
#include "SpectrumLayout.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace spectrum {
namespace {
void fxRect(SDL_Renderer* renderer, SDL_FRect r, SDL_Color color) {
    if (r.w <= 0 || r.h <= 0) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &r);
}
} // namespace

void WallRenderer::drawGlowBars(SDL_Renderer* renderer, SDL_FRect plot,
                                const std::array<float, kMaxBands>& values,
                                std::size_t bands, int palette, bool right, bool mirror) {
    const float step = plot.w / static_cast<float>(bands);
    for (std::size_t b = 0; b < bands; ++b) {
        const float value = std::clamp(values[b], 0.0f, 1.0f);
        if (value < 0.05f) continue;
        const float freq = static_cast<float>(b) / static_cast<float>(bands - 1);
        const Rgb c = paletteRgb(freq, value, value, palette, right);
        fxRect(renderer,
            {plot.x + step * visualBandIndex(b, bands, mirror), plot.y + (1.0f - value) * plot.h,
             step * 1.25f, value * plot.h},
            {c.r, c.g, c.b, static_cast<Uint8>(glowMode_ == 2 ? 180 : 115)});
    }
}

void WallRenderer::drawReflections(SDL_Renderer* renderer, SDL_FRect plot,
                                   const std::array<float, kMaxBands>& values,
                                   std::size_t bands, int palette, bool right, bool mirror) {
    if (!reflectionsEnabled_) return;
    const float step = plot.w / static_cast<float>(bands);
    const float depth = plot.h * 0.04f;
    for (std::size_t b = 0; b < bands; ++b) {
        const float value = std::clamp(values[b], 0.0f, 1.0f);
        if (value < 0.05f) continue;
        const float freq = static_cast<float>(b) / static_cast<float>(bands - 1);
        const Rgb c = paletteRgb(freq, value, value, palette, right);
        for (int layer = 0; layer < 4; ++layer) {
            const auto alpha = static_cast<Uint8>(std::clamp(
                value * (4 - layer) * 15.0f, 0.0f, 100.0f));
            fxRect(renderer,
                {plot.x + visualBandIndex(b, bands, mirror) * step,
                 plot.y + plot.h + (layer * depth) / 4.0f,
                 step * 0.82f, depth / 4.0f + 1.0f},
                {c.r, c.g, c.b, alpha});
        }
    }
}

void WallRenderer::drawParticles(SDL_Renderer* renderer, float width, float height,
                                 float pulse, int palette) {
    if (!particlesEnabled_) return;
    const float time = static_cast<float>(SDL_GetTicks()) * 0.001f;
    for (int i = 0; i < 40; ++i) {
        const float phase = static_cast<float>(i) * 12.9898f;
        const float x = (0.5f + 0.49f *
            std::sin(phase + time * (0.05f + (i % 4) * 0.018f))) * width;
        const float y = (0.5f + 0.47f *
            std::cos(phase * 0.73f + time * 0.055f)) * height;
        const float level = std::clamp(pulse * 0.65f +
            0.26f * std::abs(std::sin(phase + time)), 0.0f, 1.0f);
        const Rgb c = paletteRgb(static_cast<float>(i) / 40.0f,
                                 0.65f, level, palette);
        fxRect(renderer, {x, y, 1.4f + level * 2.2f, 1.4f + level * 2.2f},
               {c.r, c.g, c.b, static_cast<Uint8>(25 + level * 75)});
    }
}

void WallRenderer::drawBloom(ScreenWindow& screen, float width, float height,
                             const VisualState& state, int palette) {
    if (glowMode_ == 0 || screen.role == ScreenRole::Waterfall ||
        screen.glowUnsupported) return;

    SDL_Renderer* renderer = screen.renderer;
    const int tw = std::max(64, static_cast<int>(width / 6.0f));
    const int th = std::max(64, static_cast<int>(height / 6.0f));
    if (!screen.glowTarget || screen.glowWidth != tw || screen.glowHeight != th) {
        if (screen.glowTarget) SDL_DestroyTexture(screen.glowTarget);
        screen.glowTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_TARGET, tw, th);
        if (!screen.glowTarget) {
            std::fprintf(stderr, "Glow texture unavailable: %s\n", SDL_GetError());
            screen.glowUnsupported = true;
            return;
        }
        screen.glowWidth = tw;
        screen.glowHeight = th;
        SDL_SetTextureScaleMode(screen.glowTarget, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(screen.glowTarget, SDL_BLENDMODE_ADD);
    }
    if (!SDL_SetRenderTarget(renderer, screen.glowTarget)) {
        screen.glowUnsupported = true;
        return;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderScale(renderer, tw / width, th / height);

    const float padP = std::max(12.0f, width * 0.027f);
    const float padC = std::max(20.0f, width * 0.026f);
    const float centerWidth = width - 2 * padC;
    switch (screen.role) {
    case ScreenRole::Left:
        drawGlowBars(renderer, {padP, height * 0.06f, width - 2 * padP,
                                height * 0.75f}, state.left, state.bandCount,
                     palette, false);
        break;
    case ScreenRole::Right:
        drawGlowBars(renderer, {padP, height * 0.06f, width - 2 * padP,
                                height * 0.75f}, state.right, state.bandCount,
                     palette, true);
        break;
    case ScreenRole::Center:
        drawGlowBars(renderer, {padC, height * 0.055f, centerWidth * 0.495f,
                                height * 0.55f}, state.left, state.bandCount,
                     palette, false, centerMirror_);
        drawGlowBars(renderer, {padC + centerWidth * 0.505f,
                                height * 0.055f, centerWidth * 0.495f,
                                height * 0.55f}, state.right, state.bandCount,
                     palette, true);
        break;
    case ScreenRole::Waterfall: break;
    }
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_SetRenderTarget(renderer, nullptr);

    SDL_SetTextureAlphaMod(screen.glowTarget,
                           static_cast<Uint8>(glowMode_ == 2 ? 155 : 85));
    const SDL_FRect spread = {-13.0f, -13.0f, width + 26.0f, height + 26.0f};
    SDL_RenderTexture(renderer, screen.glowTarget, nullptr, &spread);
    SDL_SetTextureAlphaMod(screen.glowTarget,
                           static_cast<Uint8>(glowMode_ == 2 ? 115 : 65));
    const SDL_FRect exact = {0.0f, 0.0f, width, height};
    SDL_RenderTexture(renderer, screen.glowTarget, nullptr, &exact);
}
} // namespace spectrum
