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
inline constexpr std::array<ShortcutHelpRow, 17> kVisualShortcuts {{
    {"1-8",   "Choose color palette"},
    {"C",     "Next palette"},
    {"V",     "LED / Lines / Hybrid / Fire"},
    {"S",     "Smoothing profile"},
    {"F",     "Frequency softening"},
    {"G",     "Glow off / soft / vivid"},
    {"T",     "Afterglow trails"},
    {"R",     "Reflections"},
    {"P",     "Bass-reactive particles"},
    {"X",     "Fire palette: 3 colors"},
    {"Y",     "Fire tips: Off/Flash/Hold"},
    {";",     "Fire ridge line on/off"},
    {"Z",     "Fire speed: 3 presets"},
    {"O",     "Fire quality: 3 presets"},
    {"W",     "Fourth monitor: Fire view"},
    {"[ / ]", "Band count -/+ 16"},
    {"B",     "Cycle band presets"}
}};
inline constexpr std::array<ShortcutHelpRow, 14> kControlShortcuts {{
    {"M",       "Center bass mirror"},
    {"H",       "Right portrait mirror"},
    {"E",       "Engineering axes/probe"},
    {"J",       "Stereo RMS balance"},
    {"N",       "Next audio source"},
    {"L",       "Capture / loopback"},
    {"D",       "Demo audio on/off"},
    {"K",       "3D: off / baby / can / both"},
    {"U",       "3D: beat response on/off"},
    {"I",       "3D: small / medium / large"},
    {"F1 / RMB","Show / hide help"},
    {"TAB",     "Help page (small view)"},
    {"ESC",     "Close help / exit"},
    {"Q",       "Quit and save"}
}};

struct ShortcutHelpLayout {
    bool compact = false;
    int columns = 2;
    int pages = 1;
    int page = 0;
    float panelWidth = 810.0f;
    float panelHeight = 525.0f;
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
    layout.panelHeight = 525.0f;
    const float widthRatio = std::max(0.1f,
        (outputWidth - 24.0f) / layout.panelWidth);
    const float heightRatio = std::max(0.1f,
        (outputHeight - 24.0f) / layout.panelHeight);
    layout.scale = std::clamp(std::min(widthRatio, heightRatio),
                              0.10f, 1.0f);
    return layout;
}
} // namespace spectrum
