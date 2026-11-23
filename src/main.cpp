#include "AudioCapture.hpp"
#include "SpectrumAnalyzer.hpp"
#include "WallRenderer.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>

using namespace spectrum;

namespace {
struct Options {
    bool demo = false;
    bool loopback = false;
    bool fourth = false;
    bool windowed = false;
    bool list = false;
    std::string source;
};

Options parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--demo") options.demo = true;
        else if (flag == "--loopback") options.loopback = true;
        else if (flag == "--fourth") options.fourth = true;
        else if (flag == "--windowed") options.windowed = true;
        else if (flag == "--list-devices") options.list = true;
        else if (flag == "--source" && i + 1 < argc) options.source = argv[++i];
        else if (flag == "--help" || flag == "-h") {
            std::puts("SpectrumWall [--source TEXT] [--loopback] [--demo]"
                      " [--fourth] [--windowed] [--list-devices]");
            std::puts("Keys: Esc/Q quit; N next source; L toggle input/loopback;"
                      " D demo; 1/2/3 palettes");
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
    const Options options = parse(argc, argv);
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
                        "ESC/Q quit | N next audio | L loopback | D demo | 1-3 themes\n",
                        wall.screenCount());
            SpectrumAnalyzer analyzer;
            std::array<float, kFFTFrames * 2> samples{};
            bool running = true;
            int theme = 0;
            Uint64 lastFrame = SDL_GetTicksNS();
            Uint64 lastStats = lastFrame;

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
                    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) continue;
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
                    default: break;
                    }
                }
                if (demo) {
                    syntheticStereo(samples, static_cast<double>(frameBegin) / 1.0e9);
                } else if (!audio.snapshot(samples)) {
                    samples.fill(0.0f);
                }
                analyzer.update(samples, dt);
                wall.draw(analyzer.state(), theme);

                if (frameBegin - lastStats > 1000000000ULL) {
                    const auto& s = analyzer.state();
                    std::printf("\rL %6.1f dBFS  R %6.1f dBFS | %s | %s      ",
                                toDb(s.peakLeft), toDb(s.peakRight),
                                demo ? "DEMO" : "LIVE", audio.currentName().c_str());
                    std::fflush(stdout);
                    lastStats = frameBegin;
                }
                constexpr Uint64 framePeriod = 1000000000ULL / 60;
                const Uint64 elapsed = SDL_GetTicksNS() - frameBegin;
                if (elapsed < framePeriod) {
                    SDL_Delay(static_cast<Uint32>((framePeriod - elapsed) / 1000000ULL));
                }
            }
            std::puts("\nSpectrumWall shutdown.");
        }
    }

    SDL_Quit();
    return exitCode;
}
