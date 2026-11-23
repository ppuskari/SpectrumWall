#define MINIAUDIO_IMPLEMENTATION
#include "AudioCapture.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace spectrum {
namespace {
std::string lower(std::string_view v) {
    std::string result(v);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

int preference(const AudioEndpoint& e) {
    if (e.type != ma_device_type_capture) return -1;
    const std::string name = lower(e.name);
    const bool realtek = name.find("realtek") != std::string::npos;
    const bool optical = name.find("s/pdif") != std::string::npos ||
                         name.find("spdif") != std::string::npos ||
                         name.find("optical") != std::string::npos;
    const bool digital = name.find("digital input") != std::string::npos ||
                         name.find("digital in") != std::string::npos;
    return (realtek && (optical || digital)) ? 100 :
           optical ? 75 : digital ? 70 : realtek ? 10 : 1;
}
}

AudioCapture::~AudioCapture() {
    disconnect();
    if (contextReady_) ma_context_uninit(&context_);
}

bool AudioCapture::initialize() {
#ifdef _WIN32
    const ma_backend backends[] = { ma_backend_wasapi };
    const ma_result result = ma_context_init(backends, 1, nullptr, &context_);
#else
    const ma_result result = ma_context_init(nullptr, 0, nullptr, &context_);
#endif
    if (result != MA_SUCCESS) {
        std::fprintf(stderr, "miniaudio context failed: %d\n", result);
        return false;
    }
    contextReady_ = true;
    ma_device_info* playback = nullptr;
    ma_device_info* capture = nullptr;
    ma_uint32 playbackCount = 0, captureCount = 0;
    if (ma_context_get_devices(&context_, &playback, &playbackCount,
                               &capture, &captureCount) != MA_SUCCESS) {
        std::fprintf(stderr, "Could not enumerate Windows audio endpoints.\n");
        return false;
    }

    // Capture endpoints are listed separately from playback endpoints.
    // In particular, never mistake "Realtek Digital Output" for SPDIF IN.
    for (ma_uint32 i = 0; i < captureCount; ++i) {
        endpoints_.push_back({capture[i].id, ma_device_type_capture, capture[i].name});
    }
#ifdef _WIN32
    // WASAPI loopback is only valid for playback endpoints, not capture endpoints.
    for (ma_uint32 i = 0; i < playbackCount; ++i) {
        endpoints_.push_back({playback[i].id, ma_device_type_loopback, playback[i].name});
    }
#endif
    return true;
}

void AudioCapture::printEndpoints() const {
    std::printf("Audio endpoints (48 kHz float stereo output from capture engine):\n");
    for (std::size_t i = 0; i < endpoints_.size(); ++i) {
        const auto& e = endpoints_[i];
        std::printf("  [%zu] %-8s %s\n", i,
                    e.type == ma_device_type_loopback ? "LOOPBACK" : "INPUT",
                    e.name.c_str());
    }
    if (endpoints_.empty()) std::puts("  No audio devices found. Try --demo.");
}

bool AudioCapture::selectPreferred(std::string_view search, bool loopback) {
    if (endpoints_.empty()) return false;
    if (!search.empty()) {
        const std::string needle = lower(search);
        for (std::size_t i = 0; i < endpoints_.size(); ++i) {
            if (lower(endpoints_[i].name).find(needle) != std::string::npos) {
                return select(i);
            }
        }
        std::fprintf(stderr, "Source '%s' not found; using automatic selection.\n",
                     std::string(search).c_str());
    }
    int bestScore = -2;
    std::size_t best = endpoints_.size();
    for (std::size_t i = 0; i < endpoints_.size(); ++i) {
        if (loopback) {
            if (endpoints_[i].type == ma_device_type_loopback) {
                best = i;
                break;
            }
        } else {
            const int score = preference(endpoints_[i]);
            if (score > bestScore) {
                best = i;
                bestScore = score;
            }
        }
    }
    if (best == endpoints_.size()) return false;
    return select(best);
}

bool AudioCapture::select(std::size_t index) {
    if (index >= endpoints_.size()) return false;
    disconnect();
    const AudioEndpoint& e = endpoints_[index];
    ma_device_config cfg = ma_device_config_init(e.type);
    cfg.capture.pDeviceID = &e.id;
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = 2;
    cfg.sampleRate = kSampleRate;
    cfg.dataCallback = onAudio;
    cfg.pUserData = this;

    const ma_result result = ma_device_init(&context_, &cfg, &device_);
    if (result != MA_SUCCESS) {
        std::fprintf(stderr, "Unable to open %s (error %d).\n", e.name.c_str(), result);
        return false;
    }
    deviceReady_ = true;
    if (ma_device_start(&device_) != MA_SUCCESS) {
        std::fprintf(stderr, "Unable to start %s.\n", e.name.c_str());
        disconnect();
        return false;
    }
    selected_ = index;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        head_ = 0;
        used_ = 0;
    }
    std::printf("Active source: [%zu] %s (%s)\n", index, e.name.c_str(),
                e.type == ma_device_type_loopback ? "WASAPI loopback" : "WASAPI recording");
    return true;
}

bool AudioCapture::next() {
    if (endpoints_.empty()) return false;
    const ma_device_type type = deviceReady_ ? endpoints_[selected_].type : ma_device_type_capture;
    for (std::size_t step = 1; step <= endpoints_.size(); ++step) {
        const std::size_t index = (selected_ + step) % endpoints_.size();
        if (endpoints_[index].type == type) return select(index);
    }
    return false;
}

bool AudioCapture::toggleCaptureLoopback() {
    const ma_device_type target =
        (deviceReady_ && endpoints_[selected_].type == ma_device_type_capture)
            ? ma_device_type_loopback : ma_device_type_capture;
    for (std::size_t i = 0; i < endpoints_.size(); ++i) {
        if (endpoints_[i].type == target) return select(i);
    }
    return false;
}

void AudioCapture::disconnect() {
    if (deviceReady_) {
        ma_device_uninit(&device_);
        deviceReady_ = false;
    }
}

std::string AudioCapture::currentName() const {
    return deviceReady_ ? endpoints_[selected_].name : "No active audio source";
}

void AudioCapture::onAudio(ma_device* device, void*, const void* input, ma_uint32 frameCount) {
    if (input == nullptr) return;
    auto* self = static_cast<AudioCapture*>(device->pUserData);
    if (self != nullptr) self->append(static_cast<const float*>(input), frameCount);
}

void AudioCapture::append(const float* frames, ma_uint32 count) {
    std::lock_guard<std::mutex> guard(mutex_);
    for (ma_uint32 i = 0; i < count; ++i) {
        ring_[head_ * 2] = frames[i * 2];
        ring_[head_ * 2 + 1] = frames[i * 2 + 1];
        head_ = (head_ + 1) % kRingFrames;
        if (used_ < kRingFrames) ++used_;
    }
}

bool AudioCapture::snapshot(std::array<float, kFFTFrames * 2>& out) {
    out.fill(0.0f);
    std::lock_guard<std::mutex> guard(mutex_);
    const std::size_t n = std::min(used_, kFFTFrames);
    for (std::size_t i = 0; i < n; ++i) {
        const auto pos = (head_ + kRingFrames - n + i) % kRingFrames;
        out[(kFFTFrames - n + i) * 2] = ring_[pos * 2];
        out[(kFFTFrames - n + i) * 2 + 1] = ring_[pos * 2 + 1];
    }
    return n != 0;
}
} // namespace spectrum
