#pragma once
#include <array>
#include <algorithm>
#include <cstddef>

namespace spectrum {

struct ShortcutHelpRow {
    const char* key;
    const char* description;
};

// One authoritative list for the renderer and compact-menu paging.
inline constexpr std::array<ShortcutHelpRow, 11> kVisualShortcuts {{
    {"1-8",   "Choose color palette"},
    {"C",     "Cycle palettes"},
    {"V",     "LED / Lines / Hybrid"},
    {"S",     "Fast / Balanced / Liquid"},
    {"F",     "Frequency softening"},
    {"G",     "Glow: off / soft / vivid"},
    {"T",     "Afterglow trails"},
    {"R",     "Reflections"},
    {"P",     "Bass-reactive particles"},
    {"[ / ]", "Decrease / increase bands"},
    {"B",     "Cycle 64-256 band presets"}
}};
inline constexpr std::array<ShortcutHelpRow, 11> kControlShortcuts {{
    {"M",       "Mirror center left spectrum"},
    {"H",       "Mirror right portrait"},
    {"E",       "Engineering axes / probes"},
    {"J",       "Stereo RMS balance meter"},
    {"N",       "Next audio input"},
    {"L",       "Capture / loopback"},
    {"D",       "Toggle demo audio"},
    {"F1 / RMB","Open / close help"},
    {"TAB",     "Next help page (small view)"},
    {"ESC",     "Close help; then exit"},
    {"Q",       "Quit and save preferences"}
}};

struct ShortcutHelpLayout {
    bool compact = false;
    int columns = 2;
    int pages = 1;
    int page = 0;
    float panelWidth = 810.0f;
    float panelHeight = 470.0f;
    float scale = 1.0f;
};

// SDL's fixed 8x8 debug font renders at 1.55x in the menu. At narrow window
// widths, switch to two pages rather than clipping half the commands.
inline ShortcutHelpLayout shortcutHelpLayout(float outputWidth,
                                              float outputHeight, int page) {
    ShortcutHelpLayout layout;
    layout.compact = outputWidth < 820.0f;
    layout.columns = layout.compact ? 1 : 2;
    layout.pages = layout.compact ? 2 : 1;
    layout.page = layout.compact ? ((page % 2 + 2) % 2) : 0;
    layout.panelWidth = layout.compact ? 420.0f : 810.0f;
    layout.panelHeight = 470.0f;
    const float widthRatio = std::max(0.1f,
        (outputWidth - 24.0f) / layout.panelWidth);
    const float heightRatio = std::max(0.1f,
        (outputHeight - 24.0f) / layout.panelHeight);
    layout.scale = std::clamp(std::min(widthRatio, heightRatio),
                              0.10f, 1.0f);
    return layout;
}
} // namespace spectrum
