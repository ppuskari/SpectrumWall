#pragma once

#include "miniaudio.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace spectrum {
inline constexpr std::size_t kFFTFrames = 4096;
inline constexpr std::size_t kRingFrames = 16384;
inline constexpr unsigned kSampleRate = 48000;

struct AudioEndpoint {
    ma_device_id id{};
    ma_device_type type = ma_device_type_capture;
    std::string name;
};

class AudioCapture {
public:
    AudioCapture() = default;
    ~AudioCapture();
    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    bool initialize();
    void printEndpoints() const;
    bool selectPreferred(std::string_view search = {}, bool loopback = false);
    bool select(std::size_t index);
    bool next();
    bool toggleCaptureLoopback();
    bool snapshot(std::array<float, kFFTFrames * 2>& out);
    bool active() const { return deviceReady_; }
    std::string currentName() const;
    const std::vector<AudioEndpoint>& endpoints() const { return endpoints_; }

private:
    static void onAudio(ma_device* device, void* output, const void* input,
                        ma_uint32 frameCount);
    void append(const float* frames, ma_uint32 count);
    void disconnect();

    ma_context context_{};
    ma_device device_{};
    bool contextReady_ = false;
    bool deviceReady_ = false;
    std::size_t selected_ = 0;
    std::vector<AudioEndpoint> endpoints_;
    std::array<float, kRingFrames * 2> ring_{};
    std::size_t head_ = 0;
    std::size_t used_ = 0;
    std::mutex mutex_;
};
} // namespace spectrum
