#pragma once
#include "SpectrumAnalyzer.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <deque>
#include <vector>

namespace spectrum {
enum class ScreenRole { Center, Left, Right, Waterfall };
enum class SpectrumStyle { Led = 0, Lines = 1, Hybrid = 2 };
inline const char* spectrumStyleName(SpectrumStyle style) {
    switch (style) {
    case SpectrumStyle::Led: return "LED";
    case SpectrumStyle::Lines: return "FLUID LINES";
    case SpectrumStyle::Hybrid: return "HYBRID";
    }
    return "LED";
}

struct ScreenWindow {
    ScreenRole role = ScreenRole::Center;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Rect bounds{};
    SDL_Texture* glowTarget = nullptr;
    int glowWidth = 0;
    int glowHeight = 0;
    bool glowUnsupported = false;
};

class WallRenderer {
public:
    WallRenderer() = default;
    ~WallRenderer();
    WallRenderer(const WallRenderer&) = delete;
    WallRenderer& operator=(const WallRenderer&) = delete;
    bool initialize(bool enableFourth, bool windowed);
    void draw(const VisualState& state, int palette, float dt,
              SmoothingMode smoothing = SmoothingMode::Balanced);
    void cycleGlow() { glowMode_ = (glowMode_ + 1) % 3; }
    int glowMode() const { return glowMode_; }
    void toggleTrails() { trailsEnabled_ = !trailsEnabled_; }
    bool trailsEnabled() const { return trailsEnabled_; }
    void toggleReflections() { reflectionsEnabled_ = !reflectionsEnabled_; }
    bool reflectionsEnabled() const { return reflectionsEnabled_; }
    void toggleParticles() { particlesEnabled_ = !particlesEnabled_; }
    void cycleSpectrumStyle() {
        spectrumStyle_ = static_cast<SpectrumStyle>(
            (static_cast<int>(spectrumStyle_) + 1) % 3);
    }
    void setSpectrumStyle(SpectrumStyle style) { spectrumStyle_ = style; }
    SpectrumStyle spectrumStyle() const { return spectrumStyle_; }
    void toggleCenterMirror() { centerMirror_ = !centerMirror_; }
    bool centerMirrored() const { return centerMirror_; }
    void toggleRightPortraitMirror() { rightPortraitMirror_ = !rightPortraitMirror_; }
    bool rightPortraitMirrored() const { return rightPortraitMirror_; }
    bool particlesEnabled() const { return particlesEnabled_; }
    std::size_t screenCount() const { return screens_.size(); }

private:
    void drawScreen(ScreenWindow& screen, const VisualState& state, int palette,
                    SmoothingMode smoothing);
    void drawBloom(ScreenWindow& screen, float w, float h, const VisualState& state,
                   int palette);
    void drawGlowBars(SDL_Renderer* r, SDL_FRect plot,
                      const std::array<float, kMaxBands>& values,
                      std::size_t bands, int palette, bool right, bool mirror = false);
    void drawReflections(SDL_Renderer* r, SDL_FRect plot,
                         const std::array<float, kMaxBands>& values,
                         std::size_t bands, int palette, bool right, bool mirror = false);
    void drawParticles(SDL_Renderer* r, float w, float h, float pulse, int palette);
    void drawCurveSpectrum(SDL_Renderer* r, SDL_FRect plot,
                           const std::array<float, kMaxBands>& values,
                           const std::array<float, kMaxBands>& peaks,
                           const std::array<float, kMaxBands>& trails,
                           std::size_t bands, int palette, bool right,
                           bool mirror, bool fill);
    void drawCurveGlow(SDL_Renderer* r, SDL_FRect plot,
                       const std::array<float, kMaxBands>& values,
                       std::size_t bands, int palette, bool right,
                       bool mirror);
    void drawSpectrumStyled(SDL_Renderer* r, SDL_FRect plot,
                            const std::array<float, kMaxBands>& values,
                            const std::array<float, kMaxBands>& peaks,
                            const std::array<float, kMaxBands>& trails,
                            std::size_t bands, int palette, bool right,
                            bool mirror = false);
    void drawPortrait(SDL_Renderer* r, float w, float h,
                      const VisualState& state, bool left, int theme);
    void drawCenter(SDL_Renderer* r, float w, float h,
                    const VisualState& state, int theme);
    void drawWaterfall(SDL_Renderer* r, float w, float h,
                       std::size_t bands, int theme);
    void drawSpectrum(SDL_Renderer* r, SDL_FRect rect,
                      const std::array<float, kMaxBands>& values,
                      const std::array<float, kMaxBands>& peaks,
                      const std::array<float, kMaxBands>& trails,
                      std::size_t bands, int palette, bool right, bool mirror = false);
    void drawWave(SDL_Renderer* r, SDL_FRect rect,
                  const std::array<float, kWavePoints>& samples, SDL_Color color);
    void drawLevel(SDL_Renderer* r, SDL_FRect rect, float rms,
                   float peak, int theme);
    std::vector<ScreenWindow> screens_;
    std::deque<std::array<float, kMaxBands>> history_;
    std::size_t historyBandCount_ = 0;
    std::array<float, kMaxBands> afterglowLeft_{};
    std::array<float, kMaxBands> afterglowRight_{};
    float latestPulse_ = 0.0f;
    int glowMode_ = 2; // 0 off, 1 soft, 2 vivid
    bool trailsEnabled_ = true;
    bool reflectionsEnabled_ = true;
    bool particlesEnabled_ = true;
    bool centerMirror_ = true;
    bool rightPortraitMirror_ = true;
    SpectrumStyle spectrumStyle_ = SpectrumStyle::Lines;
};
} // namespace spectrum
