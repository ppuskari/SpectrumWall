#include "WallRenderer.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <vector>

namespace spectrum {

void WallRenderer::drawFirePane(ScreenWindow& screen, FireLayer& layer,
                                SDL_FRect plot,
                                const std::array<float,kMaxBands>& values,
                                std::size_t bands,bool mirror,
                                float bassPulse,float dt) {
    if (plot.w<=2.0f || plot.h<=2.0f || bands==0) return;

    // ECO/BALANCED/HIGH are resolution targets only. The existing HIGH
    // behavior matches our first Fire release exactly.
    constexpr int kDivisor[3]={8,6,4};
    constexpr int kMaxWidth[3]={144,224,320};
    constexpr int kMaxHeight[3]={96,144,192};
    const int q=std::clamp(fireQuality_,0,2);
    const int width=std::clamp(static_cast<int>(
        plot.w/static_cast<float>(kDivisor[q])),32,kMaxWidth[q]);
    const int height=std::clamp(static_cast<int>(
        plot.h/static_cast<float>(kDivisor[q])),32,kMaxHeight[q]);
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
    const float thermalDt=(resized?std::max(dt,1.0f/30.0f):dt)*
                          fireSpeedScale(fireSpeed_);
    const bool updated=layer.cpu.advance(values,bands,mirror,bassPulse,
                                         thermalDt,firePalette_);
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
    drawFireAccents(screen,layer,plot,values,bands,mirror,dt);
}

void WallRenderer::drawFireAccents(ScreenWindow& screen,FireLayer& layer,
                                  SDL_FRect plot,
                                  const std::array<float,kMaxBands>& values,
                                  std::size_t bands,bool mirror,float dt) {
    SDL_Renderer* renderer=screen.renderer;
    layer.accents.advance(values,bands,firePeaks_,dt);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);

    if(fireLines_ && bands>1) {
        // One softly colored, interpolated flame ridge. No LED bins, peak
        // horizontal lines or change to the underlying dBFS measurements.
        std::vector<SDL_FPoint> path;
        const std::size_t samples=std::min<std::size_t>(512,bands*2);
        path.reserve(samples);
        for(std::size_t i=0;i<samples;++i) {
            const float f=static_cast<float>(i)/
                          static_cast<float>(samples-1);
            const float value=layer.cpu.envelope(values,bands,f,mirror);
            const float flameHeight=value<0.024f?0.0f:
                std::clamp(std::pow(value,0.74f)*0.84f+0.055f,0.0f,0.97f);
            path.push_back({plot.x+f*plot.w,
                plot.y+(1.0f-flameHeight)*plot.h});
        }
        const auto c=FireHeatField::fireColor(0.81f,firePalette_);
        SDL_SetRenderDrawColor(renderer,c[0],c[1],c[2],160);
        SDL_RenderLines(renderer,path.data(),static_cast<int>(path.size()));
    }
    if(firePeaks_==0 || bands==0) return;

    // Peak flames are gradient triangles, rendered in one geometry batch.
    // A brief flare is not a conventional held peak line. Hold mode keeps
    // the same flame tips visible for a short plateau, then lets them fall.
    std::vector<SDL_Vertex> mesh;
    mesh.reserve(3*std::min<std::size_t>(bands,96));
    const std::size_t stride=std::max<std::size_t>(1,bands/96);
    for(std::size_t i=0;i<bands;i+=stride) {
        const float peak=layer.accents.tip[i];
        const float age=layer.accents.age[i];
        if(peak<0.16f)continue;
        const float alpha=firePeaks_==1
            ?std::clamp(1.0f-age/0.46f,0.0f,1.0f)
            :std::clamp(1.0f-std::max(0.0f,age-0.68f)/1.3f,0.0f,1.0f);
        if(alpha<=0.025f)continue;
        const float fraction=(static_cast<float>(i)+0.5f)/
                             static_cast<float>(bands);
        const float visual=mirror?1.0f-fraction:fraction;
        const float x=plot.x+visual*plot.w;
        const float target=std::clamp(std::pow(peak,0.74f)*0.84f+
                                      0.058f,0.0f,0.97f);
        const float y=plot.y+(1.0f-target)*plot.h;
        const float radius=std::clamp(plot.w/static_cast<float>(bands)*
                                     0.72f,3.0f,11.0f);
        const float height=std::clamp(plot.h*0.032f,8.0f,23.0f)*
                           (0.7f+0.4f*peak);
        const float jitter=std::sin(layer.cpu.time*12.0f+
                                     static_cast<float>(i)*2.17f)*
                           radius*0.30f;
        const auto hot=FireHeatField::fireColor(0.99f,firePalette_);
        const auto warm=FireHeatField::fireColor(0.72f,firePalette_);
        auto vertex=[](float px,float py,
                       const std::array<std::uint8_t,4>& c,float a)->SDL_Vertex {
            SDL_Vertex v{};
            v.position={px,py};
            v.color={c[0]/255.0f,c[1]/255.0f,c[2]/255.0f,
                     std::clamp(a,0.0f,1.0f)};
            v.tex_coord={0.0f,0.0f};
            return v;
        };
        mesh.push_back(vertex(x-radius,y+radius*0.8f,warm,alpha*0.14f));
        mesh.push_back(vertex(x+jitter,y-height,hot,alpha*0.82f));
        mesh.push_back(vertex(x+radius,y+radius*0.8f,warm,alpha*0.14f));
    }
    if(!mesh.empty()) SDL_RenderGeometry(renderer,nullptr,mesh.data(),
                           static_cast<int>(mesh.size()),nullptr,0);
}

void WallRenderer::drawFirePilotMeter(SDL_Renderer* renderer,SDL_FRect plot,
                                     float smoothedRms,bool mirror) {
    if(!renderer || plot.w<4.0f || plot.h<1.0f) return;
    // Decorative layer BEHIND the unmodified calibrated RMS/peak meter.
    // Its horizontal filling agrees with the exact dBFS meter transform.
    const float level=levelMeterSmoothedRmsFraction(smoothedRms);
    constexpr int n=96;
    std::vector<SDL_Vertex> points;
    std::vector<int> indices;
    points.reserve((n+1)*2);
    indices.reserve(n*6);
    const auto core=FireHeatField::fireColor(0.90f,firePalette_);
    const auto edge=FireHeatField::fireColor(0.59f,firePalette_);
    for(int i=0;i<=n;++i){
        const float fraction=static_cast<float>(i)/n;
        const float sample=mirror?1.0f-fraction:fraction;
        const float active=std::clamp((level-sample)*24.0f+0.25f,0.0f,1.0f);
        const float noise=0.52f+0.48f*std::sin(
            static_cast<float>(i)*0.51f+retro3dTime_*10.0f);
        const float height=(plot.h*0.80f+5.0f+8.0f*noise)*
                           active*(0.65f+0.35f*level);
        const float x=plot.x+plot.w*fraction;
        auto vertex=[&](float y,const std::array<std::uint8_t,4>& c,
                        float opacity)->SDL_Vertex {
            SDL_Vertex v{};
            v.position={x,y};
            v.color={c[0]/255.0f,c[1]/255.0f,c[2]/255.0f,
                     opacity*active};
            return v;
        };
        points.push_back(vertex(plot.y-height,core,0.12f));
        points.push_back(vertex(plot.y+plot.h,edge,0.58f));
        if(i>0){
            const int j=i*2;
            indices.insert(indices.end(),
                {j-2,j-1,j,j,j-1,j+1});
        }
    }
    SDL_RenderGeometry(renderer,nullptr,points.data(),
        static_cast<int>(points.size()),indices.data(),
        static_cast<int>(indices.size()));
}

} // namespace spectrum
