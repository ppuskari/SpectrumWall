#include "WallRenderer.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>

namespace spectrum {

void WallRenderer::drawFirePane(ScreenWindow& screen, FireLayer& layer,
                                SDL_FRect plot,
                                const std::array<float,kMaxBands>& values,
                                std::size_t bands,bool mirror,
                                float bassPulse,float dt) {
    if (plot.w<=2.0f || plot.h<=2.0f || bands==0) return;

    const int width=std::clamp(static_cast<int>(plot.w/4.0f),64,320);
    const int height=std::clamp(static_cast<int>(plot.h/4.0f),48,192);
    const bool resized=layer.cpu.width!=width || layer.cpu.height!=height;
    if(resized) {
        if(layer.texture) {
            SDL_DestroyTexture(layer.texture);
            layer.texture=nullptr;
        }
        layer.cpu.resize(width,height);
    }
    SDL_Renderer* renderer=screen.renderer;
    if(!layer.texture) {
        layer.texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       layer.cpu.width,layer.cpu.height);
        if(!layer.texture) {
            static bool reported=false;
            if(!reported) {
                std::fprintf(stderr,
                    "Fire texture creation failed: %s\n",SDL_GetError());
                reported=true;
            }
            return;
        }
        SDL_SetTextureBlendMode(layer.texture,SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(layer.texture,SDL_SCALEMODE_LINEAR);
    }
    // On first frame simulate at least once so the newly allocated texture
    // is never presented with undefined pixels. Thereafter upload at 30 Hz.
    const bool updated=layer.cpu.advance(values,bands,mirror,bassPulse,
                                         resized?std::max(dt,1.0f/30):dt);
    if(updated) {
        const auto& bytes=layer.cpu.rgba;
        SDL_UpdateTexture(layer.texture,nullptr,bytes.data(),layer.cpu.width*4);
    }
    // Soft perimeter glow using the same heat texture. It is outside the
    // source pane by just a few pixels; this does not add an LED halo.
    if(glowMode_!=0) {
        SDL_SetTextureAlphaMod(layer.texture,
                                glowMode_==2?70:35);
        const SDL_FRect halo={plot.x-7.0f,plot.y-4.0f,
                               plot.w+14.0f,plot.h+10.0f};
        SDL_RenderTexture(renderer,layer.texture,nullptr,&halo);
    }
    SDL_SetTextureAlphaMod(layer.texture,255);
    SDL_RenderTexture(renderer,layer.texture,nullptr,&plot);
}

} // namespace spectrum
