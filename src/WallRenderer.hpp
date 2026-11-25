#pragma once
#include "SpectrumAnalyzer.hpp"
#include "Retro3D.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
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
    void setGlowMode(int mode) { glowMode_ = std::clamp(mode, 0, 2); }
    void toggleTrails() { trailsEnabled_ = !trailsEnabled_; }
    bool trailsEnabled() const { return trailsEnabled_; }
    void setTrails(bool enabled) { trailsEnabled_ = enabled; }
    void toggleReflections() { reflectionsEnabled_ = !reflectionsEnabled_; }
    bool reflectionsEnabled() const { return reflectionsEnabled_; }
    void setReflections(bool enabled) { reflectionsEnabled_ = enabled; }
    void toggleParticles() { particlesEnabled_ = !particlesEnabled_; }
    void cycleSpectrumStyle() {
        spectrumStyle_ = static_cast<SpectrumStyle>(
            (static_cast<int>(spectrumStyle_) + 1) % 3);
    }
    void setSpectrumStyle(SpectrumStyle style) { spectrumStyle_ = style; }
    SpectrumStyle spectrumStyle() const { return spectrumStyle_; }
    void toggleCenterMirror() { centerMirror_ = !centerMirror_; }
    bool centerMirrored() const { return centerMirror_; }
    void setCenterMirror(bool enabled) { centerMirror_ = enabled; }
    void toggleRightPortraitMirror() { rightPortraitMirror_ = !rightPortraitMirror_; }
    bool rightPortraitMirrored() const { return rightPortraitMirror_; }
    void setRightPortraitMirror(bool enabled) { rightPortraitMirror_ = enabled; }
    bool particlesEnabled() const { return particlesEnabled_; }
    void setParticles(bool enabled) { particlesEnabled_ = enabled; }
    void toggleEngineering() { engineeringEnabled_ = !engineeringEnabled_; }
    bool engineeringEnabled() const { return engineeringEnabled_; }
    void setEngineering(bool enabled) { engineeringEnabled_ = enabled; }
    void toggleStereoBalance() { stereoBalanceEnabled_ = !stereoBalanceEnabled_; }
    bool stereoBalanceEnabled() const { return stereoBalanceEnabled_; }
    void setStereoBalance(bool enabled) { stereoBalanceEnabled_ = enabled; }
    std::size_t screenCount() const { return screens_.size(); }
    // The context-help popup is transient UI, never written to the INI.
    void toggleShortcutHelp(SDL_WindowID windowId, float windowX = -1.0f,
                            float windowY = -1.0f);
    void dismissShortcutHelp() { helpWindowId_ = 0; helpPage_ = 0; }
    bool shortcutHelpOpen() const { return helpWindowId_ != 0; }
    void advanceShortcutHelpPage(int direction);
    void cycleRetro3DMode() {
        retro3dMode_=static_cast<Retro3DMode>(
            (static_cast<int>(retro3dMode_)+1)%4);
    }
    void setRetro3DMode(Retro3DMode mode) { retro3dMode_=mode; }
    Retro3DMode retro3dMode() const { return retro3dMode_; }
    void toggleRetro3DBeat() { retro3dBeatReactive_=!retro3dBeatReactive_; }
    bool retro3dBeatReactive() const { return retro3dBeatReactive_; }
    void setRetro3DBeat(bool enabled) { retro3dBeatReactive_=enabled; }
    void cycleRetro3DSize() { retro3dSize_=(retro3dSize_+1)%3; }
    void setRetro3DSize(int size) {retro3dSize_=std::clamp(size,0,2);}
    int retro3dSize() const {return retro3dSize_;}


private:
    void drawScreen(ScreenWindow& screen, const VisualState& state, int palette,
                    SmoothingMode smoothing);
    void drawShortcutHelp(const ScreenWindow& screen, float width, float height);
    void drawRetro3D(SDL_Renderer* r,float width,float height);
    void drawEngineeringOverlay(const ScreenWindow& screen,
                                const VisualState& state, float w, float h);
    void drawStereoBalance(SDL_Renderer* renderer, float w, float h,
                           const VisualState& state);
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
                   float peak, int theme, bool mirror = false);
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
    bool engineeringEnabled_ = false;
    bool stereoBalanceEnabled_ = false;
    SDL_WindowID helpWindowId_ = 0;
    int helpPage_ = 0;
    float helpAnchorX_ = 0.5f;
    float helpAnchorY_ = 0.35f;
    Retro3DMode retro3dMode_ = Retro3DMode::Off; // opt in
    bool retro3dBeatReactive_ = true;
    int retro3dSize_ = 1;
    float retro3dTime_ = 0.0f;
    float retro3dDancePhase_ = 0.0f;
    float retro3dEnergy_ = 0.0f;
    float retro3dLastPulse_ = 0.0f;
    float retro3dBeatCooldown_ = 0.0f;
};
} // namespace spectrum
