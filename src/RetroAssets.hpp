#pragma once
#include "Retro3D.hpp"
#include <filesystem>
#include <vector>

namespace spectrum {
// Local-only mesh animation. OBJ files are sorted by filename. The first
// frame defines the normalization transform applied to all frames.
struct RetroAssetClip {
    std::vector<RetroMesh> frames;
    std::vector<std::filesystem::path> textures;
    float framesPerSecond=24.0f;
    bool empty() const { return frames.empty(); }
    const RetroMesh& frame(float seconds) const {
        const auto index=static_cast<std::size_t>(
            std::max(0.0f,seconds)*framesPerSecond)%frames.size();
        return frames[index];
    }
};
// Single static OBJ at root/can.obj, or animated OBJ frames at
// root/frames/*.obj. MTL Kd colors and map_Kd image references are supported.
// Empty clips gracefully keep the original procedural fallback.
RetroAssetClip loadRetroAssetClip(const std::filesystem::path& root,
                                  bool animated);
} // namespace spectrum
