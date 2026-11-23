#include "WallRenderer.hpp"
#include "ShortcutHelpLayout.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace spectrum {
namespace {

void helpBox(SDL_Renderer* r, SDL_FRect area, SDL_Color color) {
    if (area.w <= 0 || area.h <= 0) return;
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(r, &area);
}

void helpStroke(SDL_Renderer* r, float x1, float y1, float x2, float y2,
                SDL_Color color) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_RenderLine(r, x1, y1, x2, y2);
}

// Menu rendering uses the application's existing tiny SDL3 bitmap font, but
// scales it for visibility. No new font files or dependencies are required.
void helpText(SDL_Renderer* r, float baseScale, float x, float y,
              const char* label, SDL_Color color, float textScale = 1.42f) {
    SDL_SetRenderScale(r, baseScale * textScale, baseScale * textScale);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_RenderDebugText(r, x / textScale, y / textScale, label);
    SDL_SetRenderScale(r, baseScale, baseScale);
}

void helpColumn(SDL_Renderer* r, float baseScale, float x, float top,
                const char* heading,
                const std::array<ShortcutHelpRow, 11>& entries) {
    helpText(r, baseScale, x, top, heading, {105, 215, 229, 255}, 1.29f);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const float y = top + 40.0f + 28.0f * static_cast<float>(i);
        if ((i & 1u) == 0u) {
            helpBox(r, {x - 6.0f, y - 5.0f, 351.0f, 26.0f},
                    {35, 55, 75, 62});
        }
        helpBox(r, {x, y - 3.0f, 92.0f, 23.0f},
                {42, 75, 96, 235});
        helpStroke(r, x, y + 19.0f, x + 91.0f, y + 19.0f,
                   {98, 173, 193, 180});
        helpText(r, baseScale, x + 7.0f, y + 1.0f, entries[i].key,
                 {240, 251, 255, 255});
        helpText(r, baseScale, x + 106.0f, y + 1.0f,
                 entries[i].description, {206, 225, 237, 255});
    }
}
} // namespace

void WallRenderer::toggleShortcutHelp(SDL_WindowID windowId,
                                       float windowX, float windowY) {
    if (windowId == 0) return;
    const auto window = SDL_GetWindowFromID(windowId);
    if (!window) return;
    const auto found = std::find_if(screens_.begin(), screens_.end(),
        [window](const ScreenWindow& screen) {
            return screen.window == window;
        });
    if (found == screens_.end()) return;

    if (helpWindowId_ == windowId) {
        dismissShortcutHelp();
        return;
    }
    helpWindowId_ = windowId;
    helpPage_ = 0;
    int w = 0, h = 0;
    if (windowX >= 0.0f && windowY >= 0.0f &&
        SDL_GetWindowSize(window, &w, &h) && w > 0 && h > 0) {
        helpAnchorX_ = std::clamp(windowX / static_cast<float>(w), 0.0f, 1.0f);
        helpAnchorY_ = std::clamp(windowY / static_cast<float>(h), 0.0f, 1.0f);
    } else {
        // Keyboard help starts centered rather than following the last mouse.
        helpAnchorX_ = -1.0f;
        helpAnchorY_ = -1.0f;
    }
}

void WallRenderer::advanceShortcutHelpPage(int direction) {
    if (helpWindowId_ == 0 || direction == 0) return;
    helpPage_ = (helpPage_ + (direction > 0 ? 1 : -1) + 2) % 2;
}

void WallRenderer::drawShortcutHelp(const ScreenWindow& screen,
                                     float width, float height) {
    if (helpWindowId_ == 0 ||
        SDL_GetWindowID(screen.window) != helpWindowId_) return;

    SDL_Renderer* r = screen.renderer;
    const auto layout = shortcutHelpLayout(width, height, helpPage_);
    const float scale = layout.scale;
    const float logicalW = width / scale;
    const float logicalH = height / scale;
    const float panelW = layout.panelWidth;
    const float panelH = layout.panelHeight;
    const float left = 12.0f, top = 12.0f;
    const float panelX = helpAnchorX_ < 0.0f
        ? (logicalW - panelW) * 0.5f
        : std::clamp(helpAnchorX_ * logicalW + 14.0f,
                     left, logicalW - panelW - left);
    const float panelY = helpAnchorY_ < 0.0f
        ? (logicalH - panelH) * 0.5f
        : std::clamp(helpAnchorY_ * logicalH + 14.0f,
                     top, logicalH - panelH - top);

    SDL_SetRenderScale(r, scale, scale);
    helpBox(r, {0.0f, 0.0f, logicalW, logicalH}, {0, 5, 12, 156});
    helpBox(r, {panelX + 8, panelY + 10, panelW, panelH},
            {0, 1, 6, 160});
    helpBox(r, {panelX, panelY, panelW, panelH},
            {10, 22, 34, 249});
    helpBox(r, {panelX, panelY, panelW, 5.0f},
            {60, 204, 222, 255});
    helpStroke(r, panelX + 19, panelY + 67,
               panelX + panelW - 19, panelY + 67,
               {67, 111, 135, 196});

    helpText(r, scale, panelX + 25, panelY + 24,
             "SPECTRUMWALL | SHORTCUTS",
             {244, 251, 255, 255}, 1.65f);
    helpText(r, scale, panelX + 27, panelY + 49,
             "LIVE CONTROL REFERENCE",
             {122, 171, 196, 255}, 1.12f);

    if (layout.compact) {
        const bool visuals = layout.page == 0;
        helpColumn(r, scale, panelX + 24, panelY + 85,
                   visuals ? "DISPLAY & VISUALS" : "LAYOUT / AUDIO / EXIT",
                   visuals ? kVisualShortcuts : kControlShortcuts);
    } else {
        helpStroke(r, panelX + 400, panelY + 81,
                   panelX + 400, panelY + 405,
                   {61, 98, 119, 150});
        helpColumn(r, scale, panelX + 23, panelY + 85,
                   "DISPLAY & VISUALS", kVisualShortcuts);
        helpColumn(r, scale, panelX + 418, panelY + 85,
                   "LAYOUT / AUDIO / EXIT", kControlShortcuts);
    }

    helpStroke(r, panelX + 18, panelY + 431,
               panelX + panelW - 18, panelY + 431,
               {65, 111, 135, 160});
    if (layout.compact) {
        helpText(r, scale, panelX + 22, panelY + 443,
                 "TAB / WHEEL: PAGE | ESC / CLICK: CLOSE",
                 {180, 207, 222, 255}, 1.15f);
        char page[24]{};
        std::snprintf(page, sizeof(page), "%d / %d",
                      layout.page + 1, layout.pages);
        helpText(r, scale, panelX + panelW - 64, panelY + 48,
                 page, {111, 223, 230, 255}, 1.12f);
    } else {
        helpText(r, scale, panelX + 24, panelY + 443,
                 "RIGHT CLICK / F1: TOGGLE   |   LEFT CLICK / ESC: CLOSE",
                 {180, 207, 222, 255}, 1.25f);
    }
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}
} // namespace spectrum
