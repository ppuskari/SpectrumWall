#include "AudioCapture.hpp"
#include "SpectrumAnalyzer.hpp"
#include "WallRenderer.hpp"
#include "VisualStyle.hpp"
#include "SettingsStore.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <system_error>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <string_view>

using namespace spectrum;

namespace {
struct Options {
    bool demo = false;
    bool loopback = false;
    bool fourth = false;
    bool windowed = false;
    bool list = false;
    bool valid = true;
    std::size_t bands = kDefaultBands;
    int palette = 3; // Prism Rainbow: all frequencies visibly color-coded
    SmoothingMode smoothing = SmoothingMode::Balanced;
    SpectrumStyle visual = SpectrumStyle::Lines;
    bool engineering = false;
    bool balance = false;
    bool spectralSoftening = true;
    int glow = 2;
    bool trails = true;
    bool reflections = true;
    bool particles = true;
    bool centerMirror = true;
    bool rightPortraitMirror = true;
    Retro3DMode retro3d = Retro3DMode::Off;
    bool retro3dBeat = true;
    int retro3dSize = 1;
    bool noSave = false;
    std::string source;
};

Options parse(int argc, char** argv, Options options) {
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--demo") options.demo = true;
        else if (flag == "--no-demo") options.demo = false;
        else if (flag == "--defaults") { /* ignore stored settings: handled before parse */ }
        else if (flag == "--no-save") options.noSave = true;
        else if (flag == "--loopback") {
            options.loopback = true;
            options.demo = false;
            options.source.clear();
        }
        else if (flag == "--capture") {
            options.loopback = false;
            options.demo = false;
            options.source.clear();
        }
        else if (flag == "--fourth") options.fourth = true;
        else if (flag == "--no-fourth") options.fourth = false;
        else if (flag == "--engineering") options.engineering = true;
        else if (flag == "--no-engineering") options.engineering = false;
        else if (flag == "--stereo-balance") options.balance = true;
        else if (flag == "--no-stereo-balance") options.balance = false;
        else if (flag == "--retro3d-beat") options.retro3dBeat = true;
        else if (flag == "--no-retro3d-beat") options.retro3dBeat = false;
        else if (flag == "--retro3d" && i + 1 < argc) {
            const std::string mode = argv[++i];
            if (mode == "off") options.retro3d = Retro3DMode::Off;
            else if (mode == "dancer" || mode == "baby")
                options.retro3d = Retro3DMode::Dancer;
            else if (mode == "can") options.retro3d = Retro3DMode::Can;
            else if (mode == "both") options.retro3d = Retro3DMode::Both;
            else {
                std::fprintf(stderr, "Invalid --retro3d '%s' (off|baby|can|both).\n",
                             mode.c_str());
                options.valid = false;
            }
        }
        else if (flag == "--retro3d-size" && i + 1 < argc) {
            const std::string size = argv[++i];
            if (size == "small") options.retro3dSize = 0;
            else if (size == "medium") options.retro3dSize = 1;
            else if (size == "large") options.retro3dSize = 2;
            else {
                std::fprintf(stderr, "Invalid --retro3d-size '%s' (small|medium|large).\n",
                             size.c_str());
                options.valid = false;
            }
        }
        else if (flag == "--windowed") options.windowed = true;
        else if (flag == "--fullscreen") options.windowed = false;
        else if (flag == "--list-devices") options.list = true;
        else if (flag == "--source" && i + 1 < argc) {
            options.source = argv[++i];
            options.demo = false;
            options.loopback = false;
        }
        else if (flag == "--palette" && i + 1 < argc) {
            const std::string value = argv[++i];
            int n = 0;
            const auto [end, err] = std::from_chars(value.data(),
                                                    value.data() + value.size(), n);
            if (err != std::errc{} || end != value.data() + value.size() ||
                n < 1 || n > 8) {
                std::fprintf(stderr, "Invalid --palette value '%s' (1-8).\n",
                             value.c_str());
                options.valid = false;
            } else {
                options.palette = n - 1;
            }
        }
        else if (flag == "--visual" && i + 1 < argc) {
            const std::string mode = argv[++i];
            if (mode == "led") options.visual = SpectrumStyle::Led;
            else if (mode == "lines") options.visual = SpectrumStyle::Lines;
            else if (mode == "hybrid") options.visual = SpectrumStyle::Hybrid;
            else if (mode == "fire") options.visual = SpectrumStyle::Fire;
            else {
                std::fprintf(stderr, "Unknown --visual '%s' (led/lines/hybrid/fire).\n",
                             mode.c_str());
                options.valid = false;
            }
        }
        else if (flag == "--smoothing" && i + 1 < argc) {
            const std::string mode = argv[++i];
            if (mode == "fast") options.smoothing = SmoothingMode::Fast;
            else if (mode == "balanced") options.smoothing = SmoothingMode::Balanced;
            else if (mode == "liquid") options.smoothing = SmoothingMode::Liquid;
            else {
                std::fprintf(stderr, "Unknown --smoothing '%s' (fast/balanced/liquid).\n",
                             mode.c_str());
                options.valid = false;
            }
        }
        else if (flag == "--bands") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--bands requires a number between 64 and 256.\n");
                options.valid = false;
                continue;
            }
            const std::string value = argv[++i];
            int n = 0;
            const auto [end, err] = std::from_chars(value.data(),
                                                    value.data() + value.size(), n);
            if (err != std::errc{} || end != value.data() + value.size() ||
                n < static_cast<int>(kMinBands) || n > static_cast<int>(kMaxBands)) {
                std::fprintf(stderr, "Invalid --bands value '%s' (64-256).\n",
                             value.c_str());
                options.valid = false;
            } else {
                options.bands = static_cast<std::size_t>(n);
            }
        }
        else if (flag == "--help" || flag == "-h") {
            std::puts("SpectrumWall [--source TEXT] [--loopback] [--demo]"
                      " [--fourth] [--windowed] [--list-devices] [--bands 64..256]"
                      " [--palette 1..8] [--smoothing fast|balanced|liquid]"
                      " [--visual led|lines|hybrid|fire] [--engineering] [--stereo-balance]"
                      " [--retro3d off|baby|can|both] [--retro3d-size small|medium|large]"
                      " [--retro3d-beat] [--no-retro3d-beat]"
                      " [--defaults] [--no-save]");
            std::puts("Keys: Esc/Q quit; N next source; L toggle input/loopback;"
                      " D demo; 1..8/C palette; S smoothing; F frequency soften;"
                      " G glow; T trails; R reflections; P particles; V visual (incl Fire); M center mirror;"
                      " H right portrait mirror; E engineering axes; J stereo balance;"
                      " [/] bands; B presets; K 3D model; U beat response; I 3D size;"
                      " F1 / right click shortcut help; config auto-saves on exit");
            options.list = true;
        }
    }
    return options;
}

void syntheticStereo(std::array<float, kFFTFrames * 2>& samples, double endSeconds) {
    constexpr double tau = 2.0 * std::numbers::pi_v<double>;
    for (std::size_t i = 0; i < kFFTFrames; ++i) {
        const double t = endSeconds -
            static_cast<double>(kFFTFrames - 1 - i) / kSampleRate;
        const double kickPhase = t * 1.85 - std::floor(t * 1.85);
        const double kickEnvelope = std::pow(1.0 - kickPhase, 7.0);
        const double a = 0.55 * std::sin(tau * 110.0 * t) +
                         0.27 * std::sin(tau * 440.0 * t) +
                         0.16 * std::sin(tau * (880.0 + 6.0 * std::sin(t * 0.8)) * t) +
                         0.45 * kickEnvelope * std::sin(tau * 60.0 * t);
        const double b = 0.46 * std::sin(tau * 165.0 * t) +
                         0.3 * std::sin(tau * 660.0 * t) +
                         0.18 * std::sin(tau * 1760.0 * t) +
                         0.39 * kickEnvelope * std::sin(tau * 65.0 * t);
        samples[2 * i] = static_cast<float>(std::clamp(a, -1.0, 1.0));
        samples[2 * i + 1] = static_cast<float>(std::clamp(b, -1.0, 1.0));
    }
}
}

int main(int argc, char** argv) {
    // The INI lives next to SpectrumWall.exe even if launched from another
    // working directory. Explicit command-line settings always win.
    const auto settingsPath = sessionSettingsPath(
        argc > 0 ? argv[0] : nullptr);
    bool ignoreSaved = false;
    for (int i = 1; i < argc; ++i)
        if (std::string_view(argv[i]) == "--defaults") ignoreSaved = true;

    bool loaded = false;
    const SessionSettings saved =
        ignoreSaved ? SessionSettings{} :
                      readSessionSettings(settingsPath, &loaded);
    Options initial;
    if (loaded) {
        initial.bands = saved.bands;
        initial.palette = saved.palette - 1;
        initial.visual = static_cast<SpectrumStyle>(saved.visual);
        initial.smoothing = static_cast<SmoothingMode>(saved.smoothing);
        initial.engineering = saved.engineering;
        initial.balance = saved.stereoBalance;
        initial.spectralSoftening = saved.spectralSoftening;
        initial.glow = saved.glow;
        initial.trails = saved.trails;
        initial.reflections = saved.reflections;
        initial.particles = saved.particles;
        initial.centerMirror = saved.centerMirror;
        initial.rightPortraitMirror = saved.rightPortraitMirror;
        initial.retro3d = static_cast<Retro3DMode>(saved.retro3d);
        initial.retro3dBeat = saved.retro3dBeat;
        initial.retro3dSize = saved.retro3dSize;
        initial.fourth = saved.fourth;
        initial.windowed = saved.windowed;
        initial.demo = saved.demo;
        initial.loopback = saved.loopback;
        initial.source = saved.source;
    }
    const Options options = parse(argc, argv, initial);
    if (!options.valid) return 2;
    std::printf("SpectrumWall config: %s (%s)\n",
                settingsPath.string().c_str(),
                ignoreSaved ? "defaults requested" :
                loaded ? "restored" : "defaults, save on exit");
    AudioCapture audio;
    const bool audioAvailable = audio.initialize();
    if (audioAvailable) audio.printEndpoints();
    if (options.list) return 0;

    bool demo = options.demo;
    if (!demo && (!audioAvailable ||
        !audio.selectPreferred(options.source, options.loopback))) {
        std::puts("No usable recording source; using demo signal. Press D to switch.");
        demo = true;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL3 initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    int exitCode = 0;
    {
        WallRenderer wall;
        if (!wall.initialize(options.fourth, options.windowed)) {
            std::fprintf(stderr, "Could not create SpectrumWall render windows.\n");
            exitCode = 2;
        } else {
            std::printf("SpectrumWall: %zu active screens. "
                        "ESC/Q quit | N next audio | L loopback | D demo | 1-8 palettes |"
                        " S smoothing | F soften | G glow | T trails | R reflections |"
                        " P particles | V visual (+Fire) | M center mirror | H right mirror |"
                        " E engineering | J balance | K 3D | U beat | I size | F1 help | [/] bands | B presets\n",
                        wall.screenCount());
            wall.setSpectrumStyle(options.visual);
            wall.setEngineering(options.engineering);
            wall.setStereoBalance(options.balance);
            wall.setGlowMode(options.glow);
            wall.setTrails(options.trails);
            wall.setReflections(options.reflections);
            wall.setParticles(options.particles);
            wall.setCenterMirror(options.centerMirror);
            wall.setRightPortraitMirror(options.rightPortraitMirror);
            wall.setRetro3DMode(options.retro3d);
            wall.setRetro3DBeat(options.retro3dBeat);
            wall.setRetro3DSize(options.retro3dSize);
            wall.setRetroAssetsRoot(settingsPath.parent_path() /
                                    "assets" / "retro3d");
            SpectrumAnalyzer analyzer(options.bands);
            analyzer.setSmoothingMode(options.smoothing);
            analyzer.setSpectralSoftening(options.spectralSoftening);
            std::printf("Spectrum resolution: %zu active bands (4096-point FFT).\n",
                        analyzer.bandCount());
            std::array<float, kFFTFrames * 2> samples{};
            bool running = true;
            int theme = options.palette;
            Uint64 lastFrame = SDL_GetTicksNS();
            Uint64 lastStats = lastFrame;
            unsigned frameCounter = 0;
            std::printf("Visualization: %s | Palette: %s | Smoothing: %s.\n",
                        spectrumStyleName(wall.spectrumStyle()),
                        paletteName(theme), smoothingModeName(analyzer.smoothingMode()));

            while (running) {
                const Uint64 frameBegin = SDL_GetTicksNS();
                const float dt = static_cast<float>(
                    static_cast<double>(frameBegin - lastFrame) / 1.0e9);
                lastFrame = frameBegin;
                SDL_Event event{};
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_EVENT_QUIT ||
                        event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                        running = false;
                    }
                    // Per-window shortcut help: right-click opens on the
                    // clicked monitor, left-click outside dismisses.
                    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                        if (event.button.button == SDL_BUTTON_RIGHT) {
                            wall.toggleShortcutHelp(event.button.windowID,
                                                    event.button.x, event.button.y);
                            continue;
                        }
                        if (event.button.button == SDL_BUTTON_LEFT &&
                            wall.shortcutHelpOpen()) {
                            wall.dismissShortcutHelp();
                            continue;
                        }
                    }
                    if (event.type == SDL_EVENT_MOUSE_WHEEL &&
                        wall.shortcutHelpOpen()) {
                        wall.advanceShortcutHelpPage(event.wheel.y < 0.0f ? 1 :
                                                     event.wheel.y > 0.0f ? -1 : 0);
                        continue;
                    }
                    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) continue;
                    if (event.key.key == SDLK_F1) {
                        wall.toggleShortcutHelp(event.key.windowID);
                        continue;
                    }
                    if (wall.shortcutHelpOpen()) {
                        if (event.key.key == SDLK_ESCAPE) {
                            // First Escape closes help. Second Escape quits.
                            wall.dismissShortcutHelp();
                            continue;
                        }
                        if (event.key.key == SDLK_TAB) {
                            wall.advanceShortcutHelpPage(1);
                            continue;
                        }
                    }
                    switch (event.key.key) {
                    case SDLK_ESCAPE:
                    case SDLK_Q: running = false; break;
                    case SDLK_N:
                        if (audioAvailable && audio.next()) demo = false;
                        break;
                    case SDLK_L:
                        if (audioAvailable && audio.toggleCaptureLoopback()) demo = false;
                        break;
                    case SDLK_D:
                        demo = !demo;
                        std::printf("\nDemo mode: %s\n", demo ? "ON" : "OFF");
                        break;
                    case SDLK_1: theme = 0; break;
                    case SDLK_2: theme = 1; break;
                    case SDLK_3: theme = 2; break;
                    case SDLK_4: theme = 3; break;
                    case SDLK_5: theme = 4; break;
                    case SDLK_6: theme = 5; break;
                    case SDLK_7: theme = 6; break;
                    case SDLK_8: theme = 7; break;
                    case SDLK_C:
                        theme = (theme + 1) % kPaletteCount;
                        std::printf("\nPalette: %s\n", paletteName(theme));
                        break;
                    case SDLK_S: {
                        const auto current = analyzer.smoothingMode();
                        const auto next = current == SmoothingMode::Fast
                            ? SmoothingMode::Balanced :
                            current == SmoothingMode::Balanced
                            ? SmoothingMode::Liquid : SmoothingMode::Fast;
                        analyzer.setSmoothingMode(next);
                        std::printf("\nTemporal smoothing: %s\n", smoothingModeName(next));
                        break;
                    }
                    case SDLK_F:
                        analyzer.setSpectralSoftening(!analyzer.spectralSoftening());
                        std::printf("\nFrequency softening: %s\n",
                                    analyzer.spectralSoftening() ? "ON" : "OFF");
                        break;
                    case SDLK_G:
                        wall.cycleGlow();
                        std::printf("\nGPU glow: %d (0 off, 1 soft, 2 vivid)\n",
                                    wall.glowMode());
                        break;
                    case SDLK_T:
                        wall.toggleTrails();
                        std::printf("\nLED afterglow: %s\n",
                                    wall.trailsEnabled() ? "ON" : "OFF");
                        break;
                    case SDLK_R:
                        wall.toggleReflections();
                        std::printf("\nLED reflections: %s\n",
                                    wall.reflectionsEnabled() ? "ON" : "OFF");
                        break;
                    case SDLK_P:
                        wall.toggleParticles();
                        std::printf("\nReactive particles: %s\n",
                                    wall.particlesEnabled() ? "ON" : "OFF");
                        break;
                    case SDLK_V:
                        wall.cycleSpectrumStyle();
                        std::printf("\nSpectrum visualization: %s\n",
                                    spectrumStyleName(wall.spectrumStyle()));
                        break;
                    case SDLK_M:
                        wall.toggleCenterMirror();
                        std::printf("\nCenter stereo bass-inward mirror: %s\n",
                                    wall.centerMirrored() ? "ON" : "OFF");
                        break;
                    case SDLK_H:
                        wall.toggleRightPortraitMirror();
                        std::printf("\nRight portrait frequency mirror: %s\n",
                                    wall.rightPortraitMirrored() ? "ON" : "OFF");
                        break;
                    case SDLK_E:
                        wall.toggleEngineering();
                        std::printf("\nEngineering axes and live probe: %s\n",
                                    wall.engineeringEnabled() ? "ON" : "OFF");
                        break;
                    case SDLK_K:
                        wall.cycleRetro3DMode();
                        std::printf("\nRetro3D: %s\n",
                                    retro3dModeName(wall.retro3dMode()));
                        break;
                    case SDLK_U:
                        wall.toggleRetro3DBeat();
                        std::printf("\nRetro3D beat response: %s\n",
                                    wall.retro3dBeatReactive() ? "ON" : "OFF");
                        break;
                    case SDLK_I:
                        wall.cycleRetro3DSize();
                        std::printf("\nRetro3D size: %d (0 small, 1 medium, 2 large)\n",
                                    wall.retro3dSize());
                        break;
                    case SDLK_J:
                        wall.toggleStereoBalance();
                        std::printf("\nStereo RMS balance meter: %s\n",
                                    wall.stereoBalanceEnabled() ? "ON" : "OFF");
                        break;
                    case SDLK_LEFTBRACKET:
                        analyzer.setBandCount(std::max(kMinBands,
                            analyzer.bandCount() > kMinBands + kBandStep
                                ? analyzer.bandCount() - kBandStep : kMinBands));
                        std::printf("\nSpectrum: %zu bands\n", analyzer.bandCount());
                        break;
                    case SDLK_RIGHTBRACKET:
                        analyzer.setBandCount(std::min(kMaxBands,
                            analyzer.bandCount() + kBandStep));
                        std::printf("\nSpectrum: %zu bands\n", analyzer.bandCount());
                        break;
                    case SDLK_B: {
                        constexpr std::array<std::size_t, 5> presets = {
                            64, 96, 128, 192, 256
                        };
                        const auto it = std::upper_bound(presets.begin(),
                                                         presets.end(), analyzer.bandCount());
                        analyzer.setBandCount(it == presets.end() ? presets.front() : *it);
                        std::printf("\nSpectrum: %zu bands\n", analyzer.bandCount());
                        break;
                    }
                    default: break;
                    }
                }
                if (demo) {
                    syntheticStereo(samples, static_cast<double>(frameBegin) / 1.0e9);
                } else if (!audio.snapshot(samples)) {
                    samples.fill(0.0f);
                }
                analyzer.update(samples, dt);
                wall.draw(analyzer.state(), theme, dt, analyzer.smoothingMode());
                ++frameCounter;

                if (frameBegin - lastStats > 1000000000ULL) {
                    const auto& s = analyzer.state();
                    const float fps = frameCounter * (1.0e9f /
                        static_cast<float>(frameBegin - lastStats));
                    std::printf("\rL %6.1f dBFS R %6.1f dBFS | %zu bands | %.1f FPS | %s | %s      ",
                                toDb(s.peakLeft), toDb(s.peakRight),
                                s.bandCount, fps, demo ? "DEMO" : "LIVE",
                                audio.currentName().c_str());
                    frameCounter = 0;
                    std::fflush(stdout);
                    lastStats = frameBegin;
                }
                constexpr Uint64 framePeriod = 1000000000ULL / 60;
                const Uint64 elapsed = SDL_GetTicksNS() - frameBegin;
                if (elapsed < framePeriod) {
                    SDL_Delay(static_cast<Uint32>((framePeriod - elapsed) / 1000000ULL));
                }
            }
            // Only one small disk write per normal shutdown (Esc/Q or close).
            // Use a temporary file + atomic replacement to avoid corrupting
            // the previous good settings if the machine loses power.
            if (!options.noSave) {
                SessionSettings finalSettings;
                finalSettings.bands = analyzer.bandCount();
                finalSettings.palette = theme + 1;
                finalSettings.visual = static_cast<int>(wall.spectrumStyle());
                finalSettings.smoothing = static_cast<int>(analyzer.smoothingMode());
                finalSettings.engineering = wall.engineeringEnabled();
                finalSettings.stereoBalance = wall.stereoBalanceEnabled();
                finalSettings.spectralSoftening = analyzer.spectralSoftening();
                finalSettings.glow = wall.glowMode();
                finalSettings.trails = wall.trailsEnabled();
                finalSettings.reflections = wall.reflectionsEnabled();
                finalSettings.particles = wall.particlesEnabled();
                finalSettings.centerMirror = wall.centerMirrored();
                finalSettings.rightPortraitMirror = wall.rightPortraitMirrored();
                finalSettings.retro3d = static_cast<int>(wall.retro3dMode());
                finalSettings.retro3dBeat = wall.retro3dBeatReactive();
                finalSettings.retro3dSize = wall.retro3dSize();
                finalSettings.fourth = options.fourth;
                finalSettings.windowed = options.windowed;
                finalSettings.demo = demo;
                finalSettings.loopback = audio.active()
                    ? audio.currentIsLoopback() : options.loopback;
                finalSettings.source = audio.active()
                    ? audio.currentName() : options.source;
                if (writeSessionSettings(settingsPath, finalSettings))
                    std::printf("\nSaved SpectrumWall preferences: %s\n",
                                settingsPath.string().c_str());
                else
                    std::fprintf(stderr,
                        "\nCannot save SpectrumWall.ini beside the executable; "
                        "check folder permissions.\n");
            }
            std::puts("\nSpectrumWall shutdown.");
        }
    }

    SDL_Quit();
    return exitCode;
}
