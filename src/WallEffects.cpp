#include "WallRenderer.hpp"
#include "VisualStyle.hpp"
#include "SpectrumLayout.hpp"
#include "ParticleMath.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace spectrum {
namespace {
void fxRect(SDL_Renderer* renderer, SDL_FRect r, SDL_Color color) {
    if (r.w <= 0 || r.h <= 0) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &r);
}
} // namespace

void WallRenderer::drawGlowBars(SDL_Renderer* renderer, SDL_FRect plot,
                                const std::array<float, kMaxBands>& values,
                                std::size_t bands, int palette, bool right, bool mirror) {
    const float step = plot.w / static_cast<float>(bands);
    for (std::size_t b = 0; b < bands; ++b) {
        const float value = std::clamp(values[b], 0.0f, 1.0f);
        if (value < 0.05f) continue;
        const float freq = static_cast<float>(b) / static_cast<float>(bands - 1);
        const Rgb c = paletteRgb(freq, value, value, palette, right);
        fxRect(renderer,
            {plot.x + step * visualBandIndex(b, bands, mirror), plot.y + (1.0f - value) * plot.h,
             step * 1.25f, value * plot.h},
            {c.r, c.g, c.b, static_cast<Uint8>(glowMode_ == 2 ? 180 : 115)});
    }
}

void WallRenderer::drawReflections(SDL_Renderer* renderer, SDL_FRect plot,
                                   const std::array<float, kMaxBands>& values,
                                   std::size_t bands, int palette, bool right, bool mirror) {
    if (!reflectionsEnabled_) return;
    const float step = plot.w / static_cast<float>(bands);
    const float depth = plot.h * 0.04f;
    for (std::size_t b = 0; b < bands; ++b) {
        const float value = std::clamp(values[b], 0.0f, 1.0f);
        if (value < 0.05f) continue;
        const float freq = static_cast<float>(b) / static_cast<float>(bands - 1);
        const Rgb c = paletteRgb(freq, value, value, palette, right);
        for (int layer = 0; layer < 4; ++layer) {
            const auto alpha = static_cast<Uint8>(std::clamp(
                value * (4 - layer) * 15.0f, 0.0f, 100.0f));
            fxRect(renderer,
                {plot.x + visualBandIndex(b, bands, mirror) * step,
                 plot.y + plot.h + (layer * depth) / 4.0f,
                 step * 0.82f, depth / 4.0f + 1.0f},
                {c.r, c.g, c.b, alpha});
        }
    }
}

void WallRenderer::drawParticles(ScreenWindow& screen,
                                 const VisualState& state,float width,
                                 float height,float dt,int palette) {
    // Fire/Hybrid/LED/Lines share this independently swappable particle pass.
    // Render AFTER the spectrum, BEFORE engineering legends and help.
    if (!particlesEnabled_ || screen.role==ScreenRole::Waterfall) {
        screen.sparks.clear();
        screen.sparkBudget=0.0f;
        return;
    }
    SDL_Renderer* renderer=screen.renderer;
    const float elapsed=std::clamp(dt,0.0f,0.10f);
    auto& sparks=screen.sparks;
    sparks.erase(std::remove_if(sparks.begin(),sparks.end(),
        [elapsed](AudioSpark& p) {
            p.age+=elapsed;
            p.x+=p.vx*elapsed;
            p.y+=p.vy*elapsed;
            p.vx*=std::exp(-elapsed*0.3f);
            return p.age>=p.life || p.y < -0.08f || p.x < -0.1f || p.x>1.1f;
        }),sparks.end());

    const bool center=screen.role==ScreenRole::Center;
    const bool rightPortrait=screen.role==ScreenRole::Right;
    const float meanL=[&]() {
        float sum=0;
        for(std::size_t i=0;i<state.bandCount;++i)sum+=state.left[i];
        return sum/static_cast<float>(std::max(state.bandCount,std::size_t{1}));
    }();
    const float meanR=[&]() {
        float sum=0;
        for(std::size_t i=0;i<state.bandCount;++i)sum+=state.right[i];
        return sum/static_cast<float>(std::max(state.bandCount,std::size_t{1}));
    }();
    const float mean=center ? (meanL+meanR)*0.5f : 
        rightPortrait?meanR:meanL;
    screen.sparkBudget=std::min(7.0f,screen.sparkBudget+
                       sparkEmissionRate(mean,state.bassPulse)*elapsed);
    constexpr std::size_t kMaxSparks=160;
    const float portraitPad=std::max(12.0f,width*0.027f);
    const float centerPad=std::max(20.0f,width*0.026f);
    const float centerWidth=width-2*centerPad;
    for(int count=0;count<7 && screen.sparkBudget>=1.0f;++count) {
        screen.sparkBudget-=1.0f;
        if(sparks.size()>=kMaxSparks) break;

        const bool right=rightPortrait ||
            (center && sparkRandom(screen.sparkRandomState)>0.5f);
        const auto& bands=right?state.right:state.left;
        // Sample several potential launch points; prefer hot FFT bins.
        std::size_t index=0;
        float energy=0.0f;
        for(int tryIndex=0;tryIndex<4;++tryIndex){
            const auto candidate=std::min(
                static_cast<std::size_t>(sparkRandom(screen.sparkRandomState)*
                                        static_cast<float>(state.bandCount)),
                state.bandCount-1);
            if(bands[candidate]>energy){index=candidate;energy=bands[candidate];}
        }
        if(energy<0.075f)continue;

        const bool mirror=center?(right?false:centerMirror_):
                           rightPortrait?rightPortraitMirror_:false;
        const float bandX=sparkBandFraction(index,state.bandCount,mirror);
        const float plotX=center?
                   (right?centerPad+centerWidth*0.505f:centerPad):
                   portraitPad;
        const float plotW=center?centerWidth*0.495f:width-2*portraitPad;
        const float plotTop=center?height*0.055f:height*0.06f;
        const float plotHeight=center?height*0.55f:height*0.75f;
        const float jitter=sparkRandom(screen.sparkRandomState)-0.5f;
        AudioSpark p{};
        p.x=(plotX+plotW*bandX+jitter*std::min(plotW*0.022f,22.0f))/width;
        p.y=(plotTop+plotHeight*(1.0f-std::clamp(energy,0.0f,1.0f)))/height;
        const float randomDrift=sparkRandom(screen.sparkRandomState)-0.5f;
        p.vx=randomDrift*(0.016f+0.025f*state.bassPulse);
        p.vy=-(0.025f+0.095f*sparkRandom(screen.sparkRandomState)+
               0.04f*state.bassPulse);
        p.life=0.75f+1.15f*sparkRandom(screen.sparkRandomState);
        p.size=(1.7f+3.3f*sparkRandom(screen.sparkRandomState)+
                state.bassPulse*2.5f);
        p.heat=std::clamp(energy+state.bassPulse*0.4f,0.0f,1.0f);
        p.frequency=static_cast<float>(index)/
                 static_cast<float>(std::max(state.bandCount-1,std::size_t{1}));
        p.right=right;
        sparks.push_back(p);
    }

    for(const auto& p:sparks){
        const float alpha=sparkFade(p.age,p.life);
        if(alpha<0.012f)continue;
        const Rgb tint=paletteRgb(p.frequency,p.heat,
                                  std::min(1.0f,p.heat+0.25f),palette,p.right);
        const float x=p.x*width, y=p.y*height;
        const float size=p.size*(0.78f+0.30f*p.heat);
        // Soft halo, chromatic ring, then a clearly visible hot ember.
        fxRect(renderer,{x-size*1.8f,y-size*1.8f,size*3.6f,size*3.6f},
            {tint.r,tint.g,tint.b,
             static_cast<Uint8>(std::clamp(alpha*29.0f,0.0f,255.0f))});
        fxRect(renderer,{x-size*0.85f,y-size*0.85f,size*1.7f,size*1.7f},
            {tint.r,tint.g,tint.b,
             static_cast<Uint8>(std::clamp(alpha*125.0f,0.0f,255.0f))});
        fxRect(renderer,{x-size*0.28f,y-size*0.28f,size*0.56f,size*0.56f},
            {255,246,222,
             static_cast<Uint8>(std::clamp(alpha*230.0f,0.0f,255.0f))});
    }
}

void WallRenderer::drawBloom(ScreenWindow& screen, float width, float height,
                             const VisualState& state, int palette) {
    if (glowMode_ == 0 || screen.role == ScreenRole::Waterfall ||
        screen.glowUnsupported) return;

    SDL_Renderer* renderer = screen.renderer;
    const int tw = std::max(64, static_cast<int>(width / 6.0f));
    const int th = std::max(64, static_cast<int>(height / 6.0f));
    if (!screen.glowTarget || screen.glowWidth != tw || screen.glowHeight != th) {
        if (screen.glowTarget) SDL_DestroyTexture(screen.glowTarget);
        screen.glowTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_TARGET, tw, th);
        if (!screen.glowTarget) {
            std::fprintf(stderr, "Glow texture unavailable: %s\n", SDL_GetError());
            screen.glowUnsupported = true;
            return;
        }
        screen.glowWidth = tw;
        screen.glowHeight = th;
        SDL_SetTextureScaleMode(screen.glowTarget, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(screen.glowTarget, SDL_BLENDMODE_ADD);
    }
    if (!SDL_SetRenderTarget(renderer, screen.glowTarget)) {
        screen.glowUnsupported = true;
        return;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderScale(renderer, tw / width, th / height);

    const float padP = std::max(12.0f, width * 0.027f);
    const float padC = std::max(20.0f, width * 0.026f);
    const float centerWidth = width - 2 * padC;
    // Match the GPU bloom source to the active visual style; line mode must
    // not leave rectangular LED glows behind the new smooth curves.
    const auto glow = [&](SDL_FRect area,
                          const std::array<float, kMaxBands>& bands,
                          bool right, bool mirror = false) {
        if (spectrumStyle_ == SpectrumStyle::Led)
            drawGlowBars(renderer, area, bands, state.bandCount,
                         palette, right, mirror);
        else
            drawCurveGlow(renderer, area, bands, state.bandCount,
                          palette, right, mirror);
    };
    switch (screen.role) {
    case ScreenRole::Left:
        glow({padP, height * 0.06f, width - 2 * padP, height * 0.75f},
             state.left, false);
        break;
    case ScreenRole::Right:
        glow({padP, height * 0.06f, width - 2 * padP, height * 0.75f},
             state.right, true, rightPortraitMirror_);
        break;
    case ScreenRole::Center:
        glow({padC, height * 0.055f, centerWidth * 0.495f, height * 0.55f},
             state.left, false, centerMirror_);
        glow({padC + centerWidth * 0.505f, height * 0.055f,
              centerWidth * 0.495f, height * 0.55f}, state.right, true);
        break;
    case ScreenRole::Waterfall: break;
    }
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_SetRenderTarget(renderer, nullptr);

    SDL_SetTextureAlphaMod(screen.glowTarget,
                           static_cast<Uint8>(glowMode_ == 2 ? 155 : 85));
    const SDL_FRect spread = {-13.0f, -13.0f, width + 26.0f, height + 26.0f};
    SDL_RenderTexture(renderer, screen.glowTarget, nullptr, &spread);
    SDL_SetTextureAlphaMod(screen.glowTarget,
                           static_cast<Uint8>(glowMode_ == 2 ? 115 : 65));
    const SDL_FRect exact = {0.0f, 0.0f, width, height};
    SDL_RenderTexture(renderer, screen.glowTarget, nullptr, &exact);
}
} // namespace spectrum
