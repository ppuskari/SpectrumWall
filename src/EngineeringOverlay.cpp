#include "WallRenderer.hpp"
#include "EngineeringMath.hpp"
#include "SpectrumLayout.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace spectrum {
namespace {
// SDL3's default 8px debug font was just below legibility on portrait
// displays. Scale only the text, leaving all tick positions and curves exact.
constexpr float kEngineeringTextScale = 1.30f;
void renderEngText(SDL_Renderer* r, float x, float y, const char* label) {
    SDL_SetRenderScale(r, kEngineeringTextScale, kEngineeringTextScale);
    SDL_RenderDebugText(r, x / kEngineeringTextScale,
                        y / kEngineeringTextScale, label);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}
template <typename... Args>
void renderEngFormat(SDL_Renderer* r, float x, float y,
                     const char* fmt, Args... args) {
    SDL_SetRenderScale(r, kEngineeringTextScale, kEngineeringTextScale);
    SDL_RenderDebugTextFormat(r, x / kEngineeringTextScale,
                              y / kEngineeringTextScale, fmt, args...);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

constexpr std::array<float, 10> kFrequencyTicks = {
    30, 60, 125, 250, 500, 1000, 2000, 4000, 8000, 16000
};
constexpr std::array<float, 7> kDecibelTicks = {
    -72, -60, -48, -36, -24, -12, -2
};

void engineeringLine(SDL_Renderer* r, float x1, float y1,
                     float x2, float y2, SDL_Color color) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_RenderLine(r, x1, y1, x2, y2);
}

void engineeringRect(SDL_Renderer* r, SDL_FRect rect, SDL_Color color) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(r, &rect);
}

void drawFrequencyAxis(SDL_Renderer* r, SDL_FRect plot, bool mirror) {
    const float baseY = plot.y + plot.h;
    engineeringLine(r, plot.x, baseY, plot.x + plot.w, baseY,
                    {160, 194, 214, 175});
    // Prevent overlapping tick captions on narrow portrait or windowed views.
    const float minimumTickPx = 75.0f;
    float lastLabelX = -10000.0f;
    for (float hz : kFrequencyTicks) {
        const float fraction = spectrumFrequencyFraction(hz);
        const float x = plot.x + (mirror ? 1.0f - fraction : fraction) * plot.w;
        engineeringLine(r, x, baseY, x, baseY + 6.0f,
                        {177, 218, 238, 195});
        engineeringLine(r, x, plot.y, x, baseY,
                        {65, 118, 139, 40});
        // The order is right-to-left on mirrored spectrums.
        if (std::abs(x - lastLabelX) < minimumTickPx) continue;
        // Restore bright text color after the faint frequency grid line.
        SDL_SetRenderDrawColor(r, 216, 234, 248, 245);
        // Local spacing: left-to-right entries can be reversed when mirrored.
        if (hz >= 1000.0f)
            renderEngFormat(r, x - 10.0f, baseY + 10.0f,
                                      "%gk", hz / 1000.0f);
        else
            renderEngFormat(r, x - 12.0f, baseY + 10.0f,
                                      "%g", hz);
        lastLabelX = x;
    }
}

void drawDecibelAxis(SDL_Renderer* r, SDL_FRect plot, float axisX, bool center) {
    engineeringLine(r, axisX, plot.y, axisX, plot.y + plot.h,
                    {195, 225, 242, 205});
    for (float db : kDecibelTicks) {
        const float y = plot.y + (1.0f - spectrumDbFraction(db)) * plot.h;
        engineeringLine(r, center ? axisX - 13.0f : axisX - 7.0f,
                        y, axisX + 7.0f, y, {200, 234, 245, 225});
        if (center) {
            engineeringRect(r, {axisX - 27, y - 8, 54, 16},
                            {4, 15, 24, 208});
            SDL_SetRenderDrawColor(r, 221, 236, 248, 255);
            renderEngFormat(r, axisX - 23, y - 6, "%g", db);
        } else {
            const float x = axisX + (axisX <= plot.x + 1 ? 5.0f : -38.0f);
            engineeringRect(r, {x - 2.0f, y - 8.0f, 46.0f, 16.0f},
                            {4, 15, 24, 195});
            SDL_SetRenderDrawColor(r, 221, 236, 248, 255);
            renderEngFormat(r, x, y - 6.0f, "%g", db);
        }
    }
}

bool mouseOverPlot(const ScreenWindow& screen, SDL_FRect plot,
                   float w, float h, float& x, float& y) {
    float globalX = 0, globalY = 0;
    SDL_GetGlobalMouseState(&globalX, &globalY);
    int wx = 0, wy = 0, ww = 0, wh = 0;
    if (!SDL_GetWindowPosition(screen.window, &wx, &wy) ||
        !SDL_GetWindowSize(screen.window, &ww, &wh) ||
        ww < 1 || wh < 1)
        return false;
    x = (globalX - static_cast<float>(wx)) * w / static_cast<float>(ww);
    y = (globalY - static_cast<float>(wy)) * h / static_cast<float>(wh);
    return x >= plot.x && x < plot.x + plot.w &&
           y >= plot.y && y <= plot.y + plot.h;
}

void drawProbe(SDL_Renderer* r, const ScreenWindow& screen,
               SDL_FRect plot, float w, float h, const VisualState& state,
               bool right, bool mirror, float captionY) {
    float mouseX = 0, mouseY = 0;
    if (!mouseOverPlot(screen, plot, w, h, mouseX, mouseY) ||
        state.bandCount < 1) return;

    const float fraction = std::clamp((mouseX - plot.x) / plot.w, 0.0f, 0.999999f);
    const auto visualBin = std::min(
        static_cast<std::size_t>(fraction * state.bandCount),
        state.bandCount - 1);
    const auto bin = visualBandIndex(visualBin, state.bandCount, mirror);
    const auto& raw = right ? state.rightRawDb : state.leftRawDb;
    const float db = raw[bin];
    const float floorVisible = std::max(kSpectrumFloorDb,
                                      std::min(kSpectrumCeilingDb, db));
    const float cx = plot.x + (visualBin + 0.5f) * plot.w / state.bandCount;
    const float cy = plot.y + (1.0f - spectrumDbFraction(floorVisible)) * plot.h;
    const float f0 = spectrumFrequencyAtFraction(
        static_cast<float>(bin) / state.bandCount);
    const float f1 = spectrumFrequencyAtFraction(
        static_cast<float>(bin + 1) / state.bandCount);

    engineeringLine(r, cx, plot.y, cx, plot.y + plot.h, {255, 249, 216, 120});
    engineeringLine(r, plot.x, cy, plot.x + plot.w, cy,
                    {255, 249, 216, 60});
    engineeringRect(r, {cx - 3, cy - 3, 6, 6}, {255, 249, 230, 255});
    const float labelX = std::clamp(cx - 150.0f, plot.x + 4.0f,
                                    plot.x + std::max(4.0f, plot.w - 325.0f));
    engineeringRect(r, {labelX - 4, captionY - 4, 325, 20},
                    {5, 15, 25, 228});
    SDL_SetRenderDrawColor(r, 230, 241, 255, 255);
    // Range is the actual logarithmic display bucket; dBFS is the raw
    // Hann-windowed FFT maximum before visual smoothing.
    renderEngFormat(r, labelX, captionY,
        "%c B%zu %.0f-%.0f Hz  %.1f dBFS",
        right ? 'R' : 'L', bin, f0, f1, db);
}

void drawLevelMeterEngineering(SDL_Renderer* renderer,
                               const ScreenWindow& screen,
                               SDL_FRect plot, float w, float h,
                               const VisualState& state,
                               bool right, bool mirror) {
    // The lower bars are full-band RMS, not per-frequency FFT buckets.
    // Their linear X span is a calibrated -72..0 dBFS level scale.
    constexpr std::array<int, 9> ticks = {
        -72, -60, -48, -36, -24, -12, -6, -2, 0
    };
    constexpr SDL_Color tickColor{188, 217, 233, 220};
    const float baseY = plot.y + plot.h;
    for (int tick : ticks) {
        const float fraction = levelMeterDbFraction(static_cast<float>(tick));
        const float x = plot.x +
            (mirror ? 1.0f - fraction : fraction) * plot.w;
        engineeringLine(renderer, x, baseY, x, baseY + 5.0f, tickColor);

        // Put a readable scale directly below both center-bottom meters.
        char label[16]{};
        std::snprintf(label, sizeof(label), "%d", tick);
        const float textWidth = std::strlen(label) *
                                8.0f * kEngineeringTextScale;
        const float labelX = std::clamp(x - 0.5f * textWidth,
            plot.x + 1.0f, plot.x + plot.w - textWidth - 1.0f);
        SDL_SetRenderDrawColor(renderer, 218, 236, 248, 245);
        renderEngText(renderer, labelX, baseY + 7.0f, label);
    }

    // Show the *instantaneous* block-RMS marker (cyan) in addition to the
    // existing smoothed RMS fill and white sample-peak marker. This is an
    // Audacity-style separation of current RMS vs peak measurements.
    const float actualRms = right ? state.rmsRight : state.rmsLeft;
    const float samplePeak = right ? state.peakRight : state.peakLeft;
    const float rmsPosition = levelMeterSamplePeakFraction(actualRms);
    const float rmsX = plot.x +
        (mirror ? 1.0f - rmsPosition : rmsPosition) * plot.w;
    engineeringLine(renderer, rmsX, plot.y - 4.0f,
                    rmsX, plot.y + plot.h + 3.0f,
                    {45, 246, 232, 240});

    // The pointer interrogates a dB threshold, not a frequency bucket.
    float mouseX = 0.0f, mouseY = 0.0f;
    const SDL_FRect hoverArea = {
        plot.x, plot.y - 4.0f, plot.w, plot.h + 13.0f
    };
    if (!mouseOverPlot(screen, hoverArea, w, h, mouseX, mouseY))
        return;

    const float fraction = std::clamp((mouseX - plot.x) / plot.w,
                                      0.0f, 1.0f);
    const float threshold = levelMeterDbAtFraction(
        mirror ? 1.0f - fraction : fraction);
    engineeringLine(renderer, mouseX, plot.y - 7.0f,
                    mouseX, plot.y + plot.h + 8.0f,
                    {255, 238, 160, 240});
    const float captionY = h * 0.975f;
    const float textX = plot.x + 6.0f;
    engineeringRect(renderer,
        {plot.x + 1.0f, captionY - 3.0f, plot.w - 2.0f, 19.0f},
        {5, 16, 28, 235});
    SDL_SetRenderDrawColor(renderer, 241, 245, 255, 255);
    renderEngFormat(renderer, textX, captionY,
        "%c THRESH %.1f  RMS %.1f  SAMPLE PEAK %.1f dBFS",
        right ? 'R' : 'L', threshold, toDb(actualRms), toDb(samplePeak));
}
} // namespace

void WallRenderer::drawEngineeringOverlay(const ScreenWindow& screen,
                                          const VisualState& state,
                                          float w, float h) {
    if (!engineeringEnabled_ || screen.role == ScreenRole::Waterfall) return;
    SDL_Renderer* r = screen.renderer;
    if (screen.role == ScreenRole::Center) {
        const float pad = std::max(20.0f, w * 0.026f);
        const float width = w - 2.0f * pad;
        const SDL_FRect left = {pad, h * 0.055f, width * 0.495f, h * 0.55f};
        const SDL_FRect right = {pad + width * 0.505f, h * 0.055f,
                                 width * 0.495f, h * 0.55f};
        drawFrequencyAxis(r, left, centerMirror_);
        drawFrequencyAxis(r, right, false);
        drawDecibelAxis(r, left, w * 0.5f, true);
        drawProbe(r, screen, left, w, h, state, false, centerMirror_,
                  h * 0.626f);
        drawProbe(r, screen, right, w, h, state, true, false,
                  h * 0.626f);
        // Independent level axes below both center VU meters, matching the
        // existing center-left mirror direction. No FFT frequency buckets here:
        // the horizontal coordinate is overall level in dBFS.
        const SDL_FRect meterLeft = {
            pad, h * 0.927f, width * 0.485f, h * 0.025f
        };
        const SDL_FRect meterRight = {
            pad + width * 0.515f, h * 0.927f,
            width * 0.485f, h * 0.025f
        };
        drawLevelMeterEngineering(r, screen, meterLeft, w, h,
                                  state, false, centerMirror_);
        drawLevelMeterEngineering(r, screen, meterRight, w, h,
                                  state, true, false);
        SDL_SetRenderDrawColor(r, 218, 235, 245, 210);
        renderEngText(r, w * 0.5f - 25.0f, h * 0.037f, "dBFS");
    } else {
        const float pad = std::max(12.0f, w * 0.027f);
        const SDL_FRect plot = {pad, h * 0.06f, w - 2 * pad, h * 0.75f};
        const bool right = screen.role == ScreenRole::Right;
        const bool mirror = portraitBandMirrored(!right, rightPortraitMirror_);
        drawFrequencyAxis(r, plot, mirror);
        drawDecibelAxis(r, plot, right ? plot.x : plot.x + plot.w, false);
        drawProbe(r, screen, plot, w, h, state, right, mirror, h * 0.818f);
        SDL_SetRenderDrawColor(r, 218, 235, 245, 220);
        renderEngText(r, right ? plot.x + 10 : plot.x + plot.w - 40,
                            h * 0.045f, "dBFS");
    }
}

void WallRenderer::drawStereoBalance(SDL_Renderer* r, float w, float h,
                                     const VisualState& state) {
    if (!stereoBalanceEnabled_) return;
    const float width = w * 0.54f;
    const float x = (w - width) * 0.5f;
    const float mid = x + width * 0.5f;
    const float y = h * 0.906f;
    const float barHeight = std::max(3.0f, h * 0.008f);
    const float position = stereoBalancePosition(state.balanceDbSmoothed);
    const float markerX = mid + position * width * 0.5f;
    // Keep this in the gap between the waveforms and the two VU bars.
    engineeringRect(r, {x - 5, h * 0.890f, width + 10, h * 0.036f},
                    {4, 13, 22, 220});
    SDL_SetRenderDrawColor(r, 230, 240, 247, 230);
    const char* leading = std::abs(state.balanceDb) < 0.25f ? "CENTER" :
                          state.balanceDb > 0.0f ? "RIGHT" : "LEFT";
    renderEngFormat(r, x + 4, h * 0.893f,
       "STEREO RMS BIAS: %s %+.1f dB  L %.1f / R %.1f dBFS",
       leading, state.balanceDb,
       20.0f * std::log10(std::max(state.rmsLeft, 1.0e-8f)),
       20.0f * std::log10(std::max(state.rmsRight, 1.0e-8f)));

    engineeringRect(r, {x, y, width * 0.5f, barHeight},
                    {21, 77, 104, 220});
    engineeringRect(r, {mid, y, width * 0.5f, barHeight},
                    {111, 40, 119, 220});
    engineeringLine(r, mid, y - 6, mid, y + barHeight + 6,
                    {246, 250, 255, 220});
    for (int db : {-18, -12, -6, 0, 6, 12, 18}) {
        const float tickX = mid + (static_cast<float>(db) /
                                   kBalanceMeterRangeDb) * width * 0.5f;
        engineeringLine(r, tickX, y + barHeight, tickX,
                        y + barHeight + (db == 0 ? 6.0f : 3.0f),
                        {230, 240, 246, 160});
        SDL_SetRenderDrawColor(r, 176, 205, 221, 230);
        if (db == 0)
            renderEngText(r, tickX - 3, y + barHeight + 6, "0");
        else
            renderEngFormat(r, tickX - 10, y + barHeight + 6,
                                       "%+d", db);
    }
    // The arrow is clamped to +/-18 dB; the text remains an actual RMS
    // difference in dB and can report values outside the visible scale.
    engineeringRect(r, {markerX - 3, y - 6, 6, barHeight + 9},
                    {251, 249, 213, 255});
    engineeringLine(r, markerX - 8, y - 1, markerX + 8, y - 1,
                    {255, 236, 192, 255});
}
} // namespace spectrum
