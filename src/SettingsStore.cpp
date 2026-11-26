#include "SettingsStore.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <filesystem>
#endif

namespace spectrum {
namespace {
std::string_view strip(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

bool parseInt(std::string_view text, int& result) {
    text = strip(text);
    if (text.empty()) return false;
    int n = 0;
    const auto [ptr, ec] = std::from_chars(text.data(),
                                           text.data() + text.size(), n);
    if (ec != std::errc{} || ptr != text.data() + text.size()) return false;
    result = n;
    return true;
}

void setNumber(std::string_view raw, int& field, int minimum, int maximum) {
    int value = 0;
    if (parseInt(raw, value) && value >= minimum && value <= maximum)
        field = value;
}

void setBool(std::string_view raw, bool& field) {
    raw = strip(raw);
    if (raw == "1" || raw == "true" || raw == "on") field = true;
    else if (raw == "0" || raw == "false" || raw == "off") field = false;
}

std::string cleanSource(std::string_view raw) {
    raw = strip(raw);
    if (raw.size() > 255) raw = raw.substr(0, 255);
    std::string value(raw);
    value.erase(std::remove_if(value.begin(), value.end(),
                               [](char c) { return c == '\r' || c == '\n'; }),
                value.end());
    return value;
}
} // namespace

std::filesystem::path sessionSettingsPath(const char* argv0) {
#ifdef _WIN32
    // Supports Unicode paths and is independent of the calling shell's CWD.
    std::wstring filename(32768, L'\0');
    const DWORD count = GetModuleFileNameW(nullptr, filename.data(),
                                           static_cast<DWORD>(filename.size()));
    if (count != 0 && count < filename.size()) {
        filename.resize(count);
        return std::filesystem::path(filename).parent_path() /
               L"SpectrumWall.ini";
    }
#endif
    const auto executable = std::filesystem::absolute(
        argv0 != nullptr && *argv0 != '\0' ?
        std::filesystem::path(argv0) : std::filesystem::path("SpectrumWall"));
    return executable.parent_path() / "SpectrumWall.ini";
}

SessionSettings readSessionSettings(const std::filesystem::path& filename,
                                    bool* loaded) {
    if (loaded != nullptr) *loaded = false;
    SessionSettings result;
    std::ifstream input(filename, std::ios::binary);
    if (!input) return result;

    bool recognizedVersion = false;
    bool validVersion = true;
    std::string line;
    while (std::getline(input, line)) {
        // Avoid unbounded allocation from accidentally enormous config lines.
        if (line.size() > 2048) continue;
        const std::string_view raw = strip(line);
        if (raw.empty() || raw.front() == '#' || raw.front() == ';')
            continue;
        const auto sep = raw.find('=');
        if (sep == std::string_view::npos) continue;
        const auto key = strip(raw.substr(0, sep));
        const auto value = strip(raw.substr(sep + 1));
        if (key == "version") {
            int version = 0;
            recognizedVersion = true;
            validVersion = parseInt(value, version) && version == 1;
        } else if (key == "bands") {
            int bands = 0;
            if (parseInt(value, bands) && bands >= 64 && bands <= 256)
                result.bands = static_cast<std::size_t>(bands);
        } else if (key == "palette") {
            setNumber(value, result.palette, 1, 8);
        } else if (key == "visual") {
            setNumber(value, result.visual, 0, 2);
        } else if (key == "smoothing") {
            setNumber(value, result.smoothing, 0, 2);
        } else if (key == "glow") {
            setNumber(value, result.glow, 0, 2);
        } else if (key == "engineering") {
            setBool(value, result.engineering);
        } else if (key == "stereo_balance") {
            setBool(value, result.stereoBalance);
        } else if (key == "spectral_softening") {
            setBool(value, result.spectralSoftening);
        } else if (key == "trails") {
            setBool(value, result.trails);
        } else if (key == "reflections") {
            setBool(value, result.reflections);
        } else if (key == "particles") {
            setBool(value, result.particles);
        } else if (key == "center_mirror") {
            setBool(value, result.centerMirror);
        } else if (key == "right_portrait_mirror") {
            setBool(value, result.rightPortraitMirror);
        } else if (key == "retro3d") {
            setNumber(value, result.retro3d, 0, 3);
        } else if (key == "retro3d_beat") {
            setBool(value, result.retro3dBeat);
        } else if (key == "retro3d_size") {
            setNumber(value, result.retro3dSize, 0, 2);
        } else if (key == "fourth") {
            setBool(value, result.fourth);
        } else if (key == "windowed") {
            setBool(value, result.windowed);
        } else if (key == "demo") {
            setBool(value, result.demo);
        } else if (key == "loopback") {
            setBool(value, result.loopback);
        } else if (key == "source") {
            result.source = cleanSource(value);
        }
    }
    if (!validVersion || !recognizedVersion)
        return SessionSettings{};
    if (loaded != nullptr) *loaded = true;
    return result;
}

bool writeSessionSettings(const std::filesystem::path& filename,
                          const SessionSettings& settings) {
    std::filesystem::path temporary = filename;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output << "# SpectrumWall session preferences, version 1\n"
               << "# Updated once on a normal shutdown. Edit while closed.\n"
               << "# Command-line options take priority over this file.\n"
               << "version=1\n"
               << "bands=" << std::clamp(settings.bands,
                                        std::size_t{64}, std::size_t{256}) << '\n'
               << "palette=" << std::clamp(settings.palette, 1, 8) << '\n'
               << "visual=" << std::clamp(settings.visual, 0, 2) << '\n'
               << "smoothing=" << std::clamp(settings.smoothing, 0, 2) << '\n'
               << "engineering=" << (settings.engineering ? 1 : 0) << '\n'
               << "stereo_balance=" << (settings.stereoBalance ? 1 : 0) << '\n'
               << "spectral_softening=" << (settings.spectralSoftening ? 1 : 0) << '\n'
               << "glow=" << std::clamp(settings.glow, 0, 2) << '\n'
               << "trails=" << (settings.trails ? 1 : 0) << '\n'
               << "reflections=" << (settings.reflections ? 1 : 0) << '\n'
               << "particles=" << (settings.particles ? 1 : 0) << '\n'
               << "center_mirror=" << (settings.centerMirror ? 1 : 0) << '\n'
               << "right_portrait_mirror=" << (settings.rightPortraitMirror ? 1 : 0) << '\n'
               << "retro3d=" << std::clamp(settings.retro3d, 0, 3) << '\n'
               << "retro3d_beat=" << (settings.retro3dBeat ? 1 : 0) << '\n'
               << "retro3d_size=" << std::clamp(settings.retro3dSize, 0, 2) << '\n'
               << "fourth=" << (settings.fourth ? 1 : 0) << '\n'
               << "windowed=" << (settings.windowed ? 1 : 0) << '\n'
               << "demo=" << (settings.demo ? 1 : 0) << '\n'
               << "loopback=" << (settings.loopback ? 1 : 0) << '\n'
               << "source=" << cleanSource(settings.source) << '\n';
        output.flush();
        if (!output) return false;
        output.close();
        if (!output) return false;
    }
#ifdef _WIN32
    // Windows rename() won't replace an existing file. Atomic replacement
    // preserves the last known-good settings if a write gets interrupted.
    if (MoveFileExW(temporary.c_str(), filename.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0)
#else
    std::error_code ec;
    std::filesystem::rename(temporary, filename, ec);
    if (ec)
#endif
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}
} // namespace spectrum
