#pragma once
#include "SpectrumAnalyzer.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <deque>
#include <vector>

namespace spectrum {
enum class ScreenRole { Center, Left, Right, Waterfall };

struct ScreenWindow {
    ScreenRole role = ScreenRole::Center;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Rect bounds{};
};

class WallRenderer {
public:
    WallRenderer() = default;
    ~WallRenderer();
    WallRenderer(const WallRenderer&) = delete;
    WallRenderer& operator=(const WallRenderer&) = delete;
    bool initialize(bool enableFourth, bool windowed);
    void draw(const VisualState& state, int theme);
    std::size_t screenCount() const { return screens_.size(); }

private:
    void drawScreen(const ScreenWindow& screen, const VisualState& state, int theme);
    void drawPortrait(SDL_Renderer* r, float w, float h,
                      const VisualState& state, bool left, int theme);
    void drawCenter(SDL_Renderer* r, float w, float h,
                    const VisualState& state, int theme);
    void drawWaterfall(SDL_Renderer* r, float w, float h,
                       std::size_t bands, int theme);
    void drawSpectrum(SDL_Renderer* r, SDL_FRect rect,
                      const std::array<float, kMaxBands>& values,
                      const std::array<float, kMaxBands>& peaks,
                      std::size_t bands, int theme);
    void drawWave(SDL_Renderer* r, SDL_FRect rect,
                  const std::array<float, kWavePoints>& samples, SDL_Color color);
    void drawLevel(SDL_Renderer* r, SDL_FRect rect, float rms,
                   float peak, int theme);
    std::vector<ScreenWindow> screens_;
    std::deque<std::array<float, kMaxBands>> history_;
    std::size_t historyBandCount_ = 0;
};
} // namespace spectrum
