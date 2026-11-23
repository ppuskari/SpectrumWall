#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace spectrum {

// Human-editable SpectrumWall.ini values. No audio hardware or SDL dependency,
// so these settings can be round-trip tested in the CLI regression executable.
struct SessionSettings {
    int version = 1;
    std::size_t bands = 128;
    int palette = 4;         // 1..8, human-facing numbers
    int visual = 1;          // 0 LED / 1 lines / 2 hybrid
    int smoothing = 1;       // 0 fast / 1 balanced / 2 liquid
    bool engineering = false;
    bool stereoBalance = false;
    bool spectralSoftening = true;
    int glow = 2;            // off / soft / vivid
    bool trails = true;
    bool reflections = true;
    bool particles = true;
    bool centerMirror = true;
    bool rightPortraitMirror = true;
    bool fourth = false;
    bool windowed = false;
    bool demo = false;
    bool loopback = false;
    std::string source;
};

// Always locate the INI in the executable folder, not PowerShell's current
// directory. On Windows this uses GetModuleFileNameW for Unicode/long paths.
std::filesystem::path sessionSettingsPath(const char* argv0);
SessionSettings readSessionSettings(const std::filesystem::path& filename,
                                    bool* loaded = nullptr);
// Writes to .tmp, flushes, then replaces the INI. One write at clean exit.
bool writeSessionSettings(const std::filesystem::path& filename,
                          const SessionSettings& settings);

} // namespace spectrum
