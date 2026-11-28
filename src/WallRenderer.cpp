#include "WallRenderer.hpp"
#include "VisualStyle.hpp"
#include "SpectrumLayout.hpp"
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
    for(auto* texture:retroDancerTextures_)if(texture)SDL_DestroyTexture(texture);
    for(auto* texture:retroCanTextures_)if(texture)SDL_DestroyTexture(texture);
    for (auto& s : screens_) {
        if (s.leftFire.texture) SDL_DestroyTexture(s.leftFire.texture);
        if (s.rightFire.texture) SDL_DestroyTexture(s.rightFire.texture);
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
                                std::size_t bands, int theme, bool right, bool mirror) {
    constexpr int segments = 36;
    const float step = plot.w / static_cast<float>(bands);
    const float segmentH = plot.h / static_cast<float>(segments);
    const float ledWidth = std::max(0.4f, step * 0.84f);
    const float ledHeight = std::max(1.0f, segmentH * 0.82f);
    const Uint64 clock = SDL_GetTicks();
    for (std::size_t b = 0; b < bands; ++b) {
        const float x = plot.x + step * static_cast<float>(visualBandIndex(b, bands, mirror));
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
        if (glowMode_) {
            SDL_Color halo = color; halo.a = glowMode_ == 2 ? 45 : 22;
            for (float d : {-4.0f, -2.0f, 2.0f, 4.0f})
                line(r, x0, y0 + d, x1, y1 + d, halo);
        }
        line(r, x0, y0, x1, y1, color);
    }
}

void WallRenderer::drawLevel(SDL_Renderer* r, SDL_FRect plot, float rms,
                             float peak, int theme, bool mirror) {
    // VU arrives as normalized smoothed RMS (-72..-2 dBFS), while peak is
    // instantaneous linear sample amplitude. Convert BOTH to the same true
    // -72..0 dBFS bottom meter scale before drawing.
    const float level = levelMeterSmoothedRmsFraction(rms);
    const float peakPos = levelMeterSamplePeakFraction(peak);
    constexpr int segments = 80;
    const float step = plot.w / segments;
    for (int i = 0; i < segments; ++i) {
        const float f = static_cast<float>(i + 1) / segments;
        // Mirror the lower-left center VU bar toward the outside, matching
        // the inward-facing bass mapping of the spectrum above it.
        const float x = plot.x + (mirror ? (segments - 1 - i) : i) * step;
        rect(r, {x, plot.y, step * 0.86f, plot.h},
             colorFor(f, f, level, f <= level, theme));
    }
    rect(r, { plot.x + (mirror ? 1.0f - peakPos : peakPos) * plot.w - 2.0f,
              plot.y - 2.0f, 4.0f, plot.h + 4.0f },
         peak > 0.99f ? SDL_Color{255, 40, 44, 255}
                      : SDL_Color{235, 241, 255, 255});
    // A faint reference line marks the project's -2 dBFS headroom target.
    const float target = levelMeterDbFraction(-2.0f);
    const float targetX = plot.x + (mirror ? 1.0f - target : target) * plot.w;
    line(r, targetX, plot.y - 3.0f,
         targetX, plot.y + plot.h + 3.0f,
         { 240, 240, 240, 150 });
}

void WallRenderer::drawPortrait(ScreenWindow& screen, float w, float h,
                                const VisualState& state, bool left, int theme,
                                float dt) {
    SDL_Renderer* r=screen.renderer;
    const float pad = std::max(12.0f, w * 0.027f);
    const auto& values = left ? state.left : state.right;
    const auto& peaks = left ? state.leftPeaks : state.rightPeaks;
    // Mirror only the right portrait's frequency axis; data remains unchanged.
    const bool mirror = portraitBandMirrored(left, rightPortraitMirror_);
    const SDL_FRect plot={pad, h*0.06f, w-2*pad, h*0.75f};
    if(spectrumStyle_==SpectrumStyle::Fire) {
        drawFirePane(screen,screen.leftFire,plot,values,
                     state.bandCount,mirror,state.bassPulse,dt);
    } else {
        drawSpectrumStyled(r,plot,values,peaks,
                           left?afterglowLeft_:afterglowRight_,
                           state.bandCount,theme,!left,mirror);
        if(spectrumStyle_!=SpectrumStyle::Lines)
            drawReflections(r,plot,values,state.bandCount,theme,
                            !left,mirror);
    }
    drawWave(r, {pad, h * 0.84f, w - 2 * pad, h * 0.07f},
             left ? state.waveLeft : state.waveRight,
             left ? SDL_Color{ 63, 226, 232, 255 } : SDL_Color{ 228, 112, 249, 255 });
    if(spectrumStyle_==SpectrumStyle::Fire && fireMeters_)
        drawFirePilotMeter(r,{pad,h*0.94f,w-2*pad,h*0.015f},
                           left?state.vuLeft:state.vuRight,false);
    drawLevel(r, {pad, h * 0.94f, w - 2 * pad, h * 0.015f},
              left ? state.vuLeft : state.vuRight,
              left ? state.peakLeft : state.peakRight, theme);
}

void WallRenderer::drawCenter(ScreenWindow& screen, float w, float h,
                              const VisualState& state, int theme,float dt) {
    SDL_Renderer* r=screen.renderer;
    const float pad = std::max(20.0f, w * 0.026f);
    const float width = w - 2 * pad;
    const float half = width * 0.495f;
    // Mirrored left half: treble at outer edge, bass at center.
    // The right half remains in normal bass-to-treble order.
    const SDL_FRect leftPlot={pad,h*0.055f,half,h*0.55f};
    const SDL_FRect rightPlot={pad+width*0.505f,h*0.055f,half,h*0.55f};
    if(spectrumStyle_==SpectrumStyle::Fire) {
        drawFirePane(screen,screen.leftFire,leftPlot,state.left,
                     state.bandCount,centerMirror_,state.bassPulse,dt);
        drawFirePane(screen,screen.rightFire,rightPlot,state.right,
                     state.bandCount,false,state.bassPulse,dt);
    } else {
        drawSpectrumStyled(r,leftPlot,state.left,state.leftPeaks,
                           afterglowLeft_,state.bandCount,theme,false,centerMirror_);
        if(spectrumStyle_!=SpectrumStyle::Lines)
            drawReflections(r,leftPlot,state.left,state.bandCount,
                            theme,false,centerMirror_);
        drawSpectrumStyled(r,rightPlot,state.right,state.rightPeaks,
                           afterglowRight_,state.bandCount,theme,true);
        if(spectrumStyle_!=SpectrumStyle::Lines)
            drawReflections(r,rightPlot,state.right,state.bandCount,theme,true);
    }
    line(r, w * 0.5f, h * 0.045f, w * 0.5f, h * 0.61f,
         { 40, 70, 88, 200 });
    drawWave(r, {pad, h * 0.65f, width, h * 0.115f}, state.waveLeft,
             { 60, 228, 245, 255 });
    drawWave(r, {pad, h * 0.77f, width, h * 0.115f}, state.waveRight,
             { 208, 113, 255, 255 });
    if(spectrumStyle_==SpectrumStyle::Fire && fireMeters_) {
        drawFirePilotMeter(r,{pad,h*0.927f,width*0.485f,h*0.025f},
                           state.vuLeft,centerMirror_);
        drawFirePilotMeter(r,{pad+width*0.515f,h*0.927f,
                              width*0.485f,h*0.025f},state.vuRight,false);
    }
    drawLevel(r, {pad, h * 0.927f, width * 0.485f, h * 0.025f},
              state.vuLeft, state.peakLeft, theme, centerMirror_);
    drawLevel(r, {pad + width * 0.515f, h * 0.927f, width * 0.485f, h * 0.025f},
              state.vuRight, state.peakRight, theme);
}

void WallRenderer::drawWaterfall(SDL_Renderer* r, float w, float h,
                                 std::size_t bands, int theme) {
    const float pad = std::max(18.0f, w * 0.016f);
    const float width = w - 2 * pad, height = h - 2 * pad;
    if (history_.empty()) return;
    const float cellW = width / static_cast<float>(bands);
    const float cellH = height / static_cast<float>(history_.size());
    std::size_t row = 0;
    for (const auto& frame : history_) {
        for (std::size_t band = 0; band < bands; ++band) {
            const float strength = std::clamp(frame[band], 0.0f, 1.0f);
            const float freq = static_cast<float>(band) /
                               static_cast<float>(bands - 1);
            const Rgb tint = paletteRgb(freq, strength, strength, theme);
            const float light = 0.13f + strength * 0.87f;
            rect(r, {pad + band * cellW, pad + row * cellH, cellW + 0.25f,
                     cellH + 0.25f},
                 { static_cast<Uint8>(tint.r * light),
                   static_cast<Uint8>(tint.g * light),
                   static_cast<Uint8>(tint.b * light), 255 });
        }
        ++row;
    }
}

void WallRenderer::drawScreen(ScreenWindow& screen, const VisualState& state,
                              int theme, SmoothingMode smoothing,float dt) {
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
        line(r, 0, y, w, y, { 27, 42, 55, 55 });
    }
    for (int i = 1; i <= 18; ++i) {
        const float x = i * w / 19.0f;
        line(r, x, 0, x, h, { 20, 36, 49, 28 });
    }
    drawBloom(screen, w, h, state, theme);
    switch (screen.role) {
    case ScreenRole::Left: drawPortrait(screen,w,h,state,true,theme,dt); break;
    case ScreenRole::Right: drawPortrait(screen,w,h,state,false,theme,dt); break;
    case ScreenRole::Center: drawCenter(screen,w,h,state,theme,dt); break;
    case ScreenRole::Waterfall:
        if(spectrumStyle_==SpectrumStyle::Fire && fireFourth_)
            drawCenter(screen,w,h,state,theme,dt);
        else
            drawWaterfall(r,w,h,state.bandCount,theme);
        break;
    }
    // Foreground sparks remain visible over full-height LED/Hybrid bars.
    // Keep the measurement and keyboard layers above them.
    drawParticles(screen,state,w,h,dt,theme);
    if (screen.role == ScreenRole::Center)
        drawRetro3D(r, w, h);
    if (engineeringEnabled_)
        drawEngineeringOverlay(screen, state, w, h);
    if ((screen.role == ScreenRole::Center ||
        (screen.role == ScreenRole::Waterfall &&
         spectrumStyle_ == SpectrumStyle::Fire && fireFourth_)) &&
        stereoBalanceEnabled_)
        drawStereoBalance(r, w, h, state);

    // Minimal hardware-accelerated OSD confirms live band changes in fullscreen.
    SDL_SetRenderDrawColor(r, 192, 224, 232, 235);
    SDL_SetRenderScale(r, 2.0f, 2.0f);
    SDL_RenderDebugTextFormat(r, 8.0f, 8.0f,
                              "%zu BANDS | %s | %s | %s | GLOW %d | TRAILS %s",
                              state.bandCount, spectrumStyleName(spectrumStyle_),
                              paletteName(theme), smoothingModeName(smoothing),
                              glowMode_, trailsEnabled_ ? "ON" : "OFF");
    if (screen.role == ScreenRole::Right)
        SDL_RenderDebugTextFormat(r, 8.0f, 18.0f, "RIGHT PORTRAIT MIRROR: %s",
                                  rightPortraitMirror_ ? "ON" : "OFF");
    if (engineeringEnabled_)
        SDL_RenderDebugTextFormat(r, 8.0f, 29.0f, "ENGINEERING: dBFS / LOG Hz");
    if (screen.role == ScreenRole::Center && stereoBalanceEnabled_)
        SDL_RenderDebugText(r, 8.0f, 40.0f, "STEREO RMS BALANCE: ON");
    SDL_RenderDebugTextFormat(r,8.0f,54.0f,"PARTICLES: %s  SPARKS: %zu",
                              particlesEnabled_?"ON":"OFF",
                              screen.sparks.size());
    if (spectrumStyle_ == SpectrumStyle::Fire) {
        SDL_RenderDebugTextFormat(r,8.0f,65.0f,
            "FIRE %s | TIPS %s | RIDGE %s",
            firePaletteName(firePalette_),firePeakModeName(firePeaks_),
            fireLines_?"ON":"OFF");
        SDL_RenderDebugTextFormat(r,8.0f,76.0f,
            "SPEED %s | QUALITY %s | 4TH FIRE %s",
            fireSpeedName(fireSpeed_),fireQualityName(fireQuality_),
            fireFourth_?"ON":"OFF");
        SDL_RenderDebugTextFormat(r,8.0f,87.0f,
            "PILOT METERS %s",fireMeters_?"ON":"OFF");
    }
    SDL_SetRenderScale(r, 1.0f, 1.0f);
    // Context-help is intentionally the final UI layer, above engineering
    // rulers, stereo RMS gauges, and the persistent shortcut status line.
    drawShortcutHelp(screen, w, h);
    SDL_RenderPresent(r);
}

void WallRenderer::draw(const VisualState& state, int theme, float dt,
                        SmoothingMode smoothing) {
    if (historyBandCount_ != state.bandCount) {
        history_.clear();
        afterglowLeft_.fill(0.0f);
        afterglowRight_.fill(0.0f);
        historyBandCount_ = state.bandCount;
    }
    const float decay = std::exp(-std::clamp(dt, 0.0f, 0.2f) / 0.62f);
    for (std::size_t i = 0; i < state.bandCount; ++i) {
        afterglowLeft_[i] = trailsEnabled_
            ? std::max(state.left[i], afterglowLeft_[i] * decay) : state.left[i];
        afterglowRight_[i] = trailsEnabled_
            ? std::max(state.right[i], afterglowRight_[i] * decay) : state.right[i];
    }
    std::array<float, kMaxBands> combined{};
    for (std::size_t i = 0; i < state.bandCount; ++i)
        combined[i] = std::max(state.left[i], state.right[i]);
    history_.push_back(combined);
    if (history_.size() > 180) history_.pop_front();
    // One animation clock for all monitors, independent of rendering speed.
    // Rising bass transients kick the dance pose; smoother bass energy also
    // expands motion and accelerates the can temporarily.
    const float elapsed=std::clamp(dt,0.0f,0.10f);
    retro3dTime_ += elapsed;
    retro3dBeatCooldown_ = std::max(0.0f,retro3dBeatCooldown_-elapsed);
    const float target=retroBeatTarget(state.bassPulse,retro3dBeatReactive_);
    retro3dEnergy_=retroBeatSlew(retro3dEnergy_,target,elapsed);
    const bool beat=retro3dBeatReactive_ &&
        retro3dBeatCooldown_<=0.0f &&
        target>0.33f && target>retro3dLastPulse_+0.11f;
    if(beat) {
        retro3dDancePhase_ += 0.42f;
        retro3dBeatCooldown_ = 0.24f;
    }
    retro3dDancePhase_ += elapsed*(4.9f+retro3dEnergy_*2.3f);
    retro3dLastPulse_=target;
    for (auto& s : screens_) drawScreen(s, state, theme, smoothing, elapsed);
}
} // namespace spectrum
