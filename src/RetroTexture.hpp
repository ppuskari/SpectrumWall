#pragma once
#include <SDL3/SDL.h>
#include <filesystem>

namespace spectrum {
// Decode PNG, JPEG or BMP with Windows Imaging Component (ships with
// Windows 11), then create one SDL3 texture per local material. No SDL_image
// DLL or external image dependencies required.
SDL_Texture* retroLoadTexture(SDL_Renderer* renderer,
                               const std::filesystem::path& filename);
}
