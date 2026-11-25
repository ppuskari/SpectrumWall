#include "WallRenderer.hpp"
#include "Retro3D.hpp"
#include "RetroAssets.hpp"
#include "RetroTexture.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <numbers>
#include <vector>

namespace spectrum {
namespace {
constexpr float kPi=std::numbers::pi_v<float>;
constexpr float kTau=2.0f*kPi;

void triangle(RetroMesh& dst,RetroVec3 a,RetroVec3 b,RetroVec3 c,
              RetroColor color) {dst.push_back({a,b,c,color});}

void ellipsoid(RetroMesh& dst,RetroVec3 center,RetroVec3 radii,
               RetroColor color,int slices=12,int stacks=8) {
    for(int i=0;i<stacks;++i) {
        const float a0=-kPi*0.5f+kPi*i/stacks;
        const float a1=-kPi*0.5f+kPi*(i+1)/stacks;
        for(int j=0;j<slices;++j) {
            const float b0=kTau*j/slices,b1=kTau*(j+1)/slices;
            auto point=[&](float a,float b)->RetroVec3 {
                return {center.x+radii.x*std::cos(a)*std::cos(b),
                        center.y+radii.y*std::sin(a),
                        center.z+radii.z*std::cos(a)*std::sin(b)};
            };
            auto p0=point(a0,b0),p1=point(a1,b0),
                 p2=point(a1,b1),p3=point(a0,b1);
            triangle(dst,p0,p1,p2,color);
            triangle(dst,p0,p2,p3,color);
        }
    }
}

void segment(RetroMesh& dst,RetroVec3 a,RetroVec3 b,
             float radius,RetroColor color) {
    const RetroVec3 axis=retroNormalize(b-a);
    const RetroVec3 nearVertical={1.0f,0.0f,0.0f};
    const RetroVec3 side=retroNormalize(retroCross(axis,nearVertical));
    const RetroVec3 forward=retroNormalize(retroCross(axis,side));
    for(int i=0;i<10;++i) {
        const float t0=kTau*i/10,t1=kTau*(i+1)/10;
        const RetroVec3 d0=(side*std::cos(t0)+forward*std::sin(t0))*radius;
        const RetroVec3 d1=(side*std::cos(t1)+forward*std::sin(t1))*radius;
        triangle(dst,a+d0,b+d0,b+d1,color);
        triangle(dst,a+d0,b+d1,a+d1,color);
    }
    ellipsoid(dst,a,{radius,radius,radius},color,9,5);
    ellipsoid(dst,b,{radius,radius,radius},color,9,5);
}

void dancer(RetroMesh& dst,float phase,float pulse) {
    const RetroColor skin{230,175,148,245};
    const RetroColor lightSkin{250,207,182,255};
    const RetroColor shadowSkin{194,134,117,255};
    const RetroColor diaper{236,241,232,255};
    const RetroColor diaperEdge{160,185,195,255};
    const RetroColor eyes{37,43,49,255};
    const float sway=std::sin(phase)*0.16f;
    const float dip=std::max(0.0f,std::sin(phase*0.5f))*0.14f+pulse*0.18f;
    // Stylized, original low-poly retro CGI toddler rig. It is not the
    // copyrighted Character Studio / Viewpoint #5653 Dancing Baby mesh.
    ellipsoid(dst,{sway,0.30f-dip,0.0f},{0.52f,0.66f,0.31f},skin,14,10);
    ellipsoid(dst,{sway,0.14f-dip,0.28f},{0.37f,0.38f,0.16f},
              lightSkin,14,8);
    ellipsoid(dst,{sway,-0.30f-dip,0.0f},{0.54f,0.31f,0.36f},
              diaper,14,8);
    segment(dst,{sway-0.43f,-0.27f-dip,0.08f},
            {sway+0.41f,-0.27f-dip,0.08f},0.075f,diaperEdge);

    // Cheeky oversized head, ears, visible eyes, small nose.
    const float headX=sway+0.04f*std::sin(phase*0.66f);
    const float headY=1.17f-dip+0.08f*std::sin(phase*0.7f);
    ellipsoid(dst,{headX,headY,0.01f},{0.51f,0.56f,0.47f},
              lightSkin,18,12);
    ellipsoid(dst,{headX-0.48f,headY-0.04f,0.02f},{0.13f,0.17f,0.09f},
              skin,10,7);
    ellipsoid(dst,{headX+0.48f,headY-0.04f,0.02f},{0.13f,0.17f,0.09f},
              skin,10,7);
    ellipsoid(dst,{headX-0.18f,headY+0.04f,0.439f},
              {0.047f,0.057f,0.026f},eyes,8,7);
    ellipsoid(dst,{headX+0.18f,headY+0.04f,0.439f},
              {0.047f,0.057f,0.026f},eyes,8,7);
    ellipsoid(dst,{headX,headY-0.16f,0.46f},
              {0.09f,0.09f,0.10f},skin,10,7);
    segment(dst,{headX-0.11f,headY-0.29f,0.42f},
            {headX+0.11f,headY-0.29f,0.42f},0.027f,shadowSkin);

    // Independent articulated arm and leg movement; phase shifts with kicks.
    for(int side : {-1,1}) {
        const float sign=static_cast<float>(side);
        const float armSwing=std::sin(phase+sign*0.95f);
        const RetroVec3 shoulder={sway+sign*0.52f,0.57f-dip,0.0f};
        const RetroVec3 elbow={shoulder.x+sign*(0.29f+0.17f*armSwing),
                                shoulder.y-0.31f+0.18f*armSwing,0.17f};
        const RetroVec3 hand={elbow.x+sign*(0.22f+0.15f*armSwing),
                               elbow.y-0.16f+0.22f*armSwing,0.23f};
        segment(dst,shoulder,elbow,0.17f,skin);
        segment(dst,elbow,hand,0.135f,skin);
        ellipsoid(dst,hand,{0.16f,0.16f,0.12f},lightSkin,10,7);

        const float step=std::sin(phase+sign*kPi*0.5f);
        const RetroVec3 hip={sway+sign*0.31f,-0.50f-dip,0.04f};
        const RetroVec3 knee={hip.x+sign*(0.13f+0.07f*step),
                              hip.y-0.45f+std::max(0.0f,step)*0.21f,
                              0.15f+0.17f*step};
        const RetroVec3 foot={knee.x+sign*0.04f,
                              -1.46f-dip+std::max(0.0f,step)*0.37f,
                              0.26f+0.23f*step};
        segment(dst,hip,knee,0.25f,skin);
        segment(dst,knee,foot,0.17f,skin);
        ellipsoid(dst,foot+RetroVec3{0,0,0.12f},
                  {0.21f,0.11f,0.29f},lightSkin,12,8);
    }
}

void canMesh(RetroMesh& dst,float pulse) {
    // Authentic metal geometry with rounded lip, shoulder and bottom bevel.
    // Artwork is deliberately generic RED COLA; the classic Coca-Cola
    // trademark/90s demo assets are not redistributed by this project.
    constexpr int slices=32;
    const RetroColor red{194,15,32,255};
    const RetroColor brightRed{241,39,54,255};
    const RetroColor darkRed{136,13,26,255};
    const RetroColor silver{195,206,215,255};
    const RetroColor topSilver{125,147,160,255};
    const RetroColor white{252,248,232,255};
    struct Ring {float y,r;};
    constexpr std::array<Ring,7> rings {{
        {-1.10f,0.43f},{-1.035f,0.49f},{-0.91f,0.53f},
        {0.91f,0.53f},{1.035f,0.49f},{1.10f,0.44f},
        {1.125f,0.43f}
    }};
    for(int b=0;b<6;++b) {
        for(int i=0;i<slices;++i) {
            const float a0=kTau*i/slices, a1=kTau*(i+1)/slices;
            auto p=[&](int ring,float a)->RetroVec3 {
                return {rings[ring].r*std::cos(a),
                        rings[ring].y,rings[ring].r*std::sin(a)};
            };
            const bool metal=b==0 || b>=4;
            const bool gleam=(i%32)>=9 && (i%32)<=12;
            const bool ribbon=b==2 && ((i+3)%32<=3 || (i+3)%32>=28);
            const RetroColor paint=metal?(b==5?topSilver:silver):
                    ribbon?white:gleam?brightRed:(i%8<4?red:darkRed);
            const auto p0=p(b,a0),p1=p(b+1,a0),p2=p(b+1,a1),p3=p(b,a1);
            triangle(dst,p0,p1,p2,paint);triangle(dst,p0,p2,p3,paint);
        }
    }
    // Body wave, glossy contrasting diagonal swoosh, all conforming to the
    // cylinder rather than a flat 2D sprite.
    for(int row=0;row<16;++row){
        const float y0=-0.82f+row*0.104f,y1=y0+0.104f;
        const float a0=0.23f+row*0.18f+0.05f*pulse;
        const float a1=0.23f+(row+1)*0.18f+0.05f*pulse;
        for(float offset : {-kTau,0.0f,kTau}){
            const float lo0=a0+offset,hi0=lo0+0.36f;
            const float lo1=a1+offset,hi1=lo1+0.36f;
            auto q=[](float y,float a)->RetroVec3 {
                return {0.539f*std::cos(a),y,0.539f*std::sin(a)};
            };
            const auto x0=q(y0,lo0),x1=q(y1,lo1),
                       x2=q(y1,hi1),x3=q(y0,hi0);
            triangle(dst,x0,x1,x2,white);triangle(dst,x0,x2,x3,white);
        }
    }
    ellipsoid(dst,{0,1.10f,0},{0.43f,0.026f,0.43f},silver,28,4);
    ellipsoid(dst,{0,1.13f,0},{0.25f,0.012f,0.18f},topSilver,20,4);
    ellipsoid(dst,{0,-1.09f,0},{0.43f,0.025f,0.43f},topSilver,28,4);
}

struct SortFace {
    std::array<SDL_Vertex,3> verts;
    float depth=0.0f;
    int textureId=-1;
};
SDL_Vertex project(RetroVec3 v,float originX,float originY,float base,
                   RetroColor tint,float shade,RetroVec2 uv) {
    const float k=base/std::max(1.1f,4.7f-v.z);
    SDL_Vertex out{};
    out.position={originX+v.x*k,originY-v.y*k};
    out.color={std::clamp(tint.r*shade/255.0f,0.0f,1.0f),
               std::clamp(tint.g*shade/255.0f,0.0f,1.0f),
               std::clamp(tint.b*shade/255.0f,0.0f,1.0f),
               tint.a/255.0f};
    out.tex_coord={uv.u,uv.v};
    return out;
}

void renderObject(SDL_Renderer* r,const RetroMesh& mesh,
                  const std::vector<SDL_Texture*>& textures,
                  float originX,float originY,float base,
                  float yaw,float pitch,float roll,float bounce) {
    std::vector<SortFace> faces;
    faces.reserve(mesh.size());
    for(const auto& t:mesh) {
        auto transform=[&](RetroVec3 p)->RetroVec3 {
            p=retroRotateZ(p,roll);
            p=retroRotateY(p,yaw);
            p=retroRotateX(p,pitch);
            p.y+=bounce;
            return p;
        };
        const auto a=transform(t.a),b=transform(t.b),c=transform(t.c);
        const auto n=retroNormalize(retroCross(b-a,c-a));
        const float light=std::max(0.0f,retroDot(n,
                              retroNormalize({0.25f,0.62f,0.78f})));
        const float shade=0.48f+0.50f*light;
        SortFace face;
        face.depth=(a.z+b.z+c.z)/3.0f;
        const bool textured=t.textureId>=0 &&
            static_cast<std::size_t>(t.textureId)<textures.size() &&
            textures[t.textureId]!=nullptr;
        face.textureId=textured?t.textureId:-1;
        // Textured faces use white vertex tint so the original material
        // artwork remains recognizable, with a subtle Lambert light.
        const RetroColor tint=textured?RetroColor{255,255,255,t.color.a}:t.color;
        face.verts={project(a,originX,originY,base,tint,shade,t.uvA),
                    project(b,originX,originY,base,tint,shade,t.uvB),
                    project(c,originX,originY,base,tint,shade,t.uvC)};
        faces.push_back(face);
    }
    // Back-to-front sorted painter pass; depth occlusion is good enough for
    // two separately spaced lightweight retro objects and works with SDL3.
    std::sort(faces.begin(),faces.end(),
        [](const SortFace& a,const SortFace& b){return a.depth<b.depth;});
    // Preserve global painter-order, even if triangles use different source
    // textures. Flush only when material switches to avoid reordering faces.
    std::vector<SDL_Vertex> geometry;
    geometry.reserve(std::min(faces.size()*3,std::size_t{24000}));
    int active=-2;
    auto flush=[&] {
        if(geometry.empty()) return;
        SDL_Texture* texture=active>=0 &&
              static_cast<std::size_t>(active)<textures.size()
                  ? textures[active] : nullptr;
        SDL_RenderGeometry(r,texture,geometry.data(),
                           static_cast<int>(geometry.size()),nullptr,0);
        geometry.clear();
    };
    for(const auto& f:faces) {
        if(f.textureId!=active){flush();active=f.textureId;}
        geometry.insert(geometry.end(),f.verts.begin(),f.verts.end());
        if(geometry.size()>=24000)flush();
    }
    flush();
}
} // namespace

void WallRenderer::setRetroAssetsRoot(const std::filesystem::path& root) {
    // Asset paths are intentionally local-only and live next to the EXE.
    // No recursive directory search and no content is uploaded to GitHub.
    retroDancerClip_=loadRetroAssetClip(root/"dancer",true);
    retroCanClip_=loadRetroAssetClip(root/"can",false);
    const auto it=std::find_if(screens_.begin(),screens_.end(),
            [](const ScreenWindow& screen){
                return screen.role==ScreenRole::Center;
            });
    if(it==screens_.end())return;
    SDL_Renderer* renderer=it->renderer;
    auto setup=[&](const RetroAssetClip& clip,
                    std::vector<SDL_Texture*>& output) {
        for(auto* tex:output)if(tex)SDL_DestroyTexture(tex);
        output.clear();
        for(const auto& texture:clip.textures)
            output.push_back(retroLoadTexture(renderer,texture));
    };
    setup(retroDancerClip_,retroDancerTextures_);
    setup(retroCanClip_,retroCanTextures_);
    std::printf("Retro3D local assets: dancer=%zu frames (%zu textures), "
                "can=%zu frames (%zu textures)\\n",
                retroDancerClip_.frames.size(),retroDancerTextures_.size(),
                retroCanClip_.frames.size(),retroCanTextures_.size());
}

void WallRenderer::drawRetro3D(SDL_Renderer* r,float w,float h) {
    if(retro3dMode_==Retro3DMode::Off)return;
    const float base=std::min(w,h)*0.80f*retroSizeScale(retro3dSize_);
    const bool both=retro3dMode_==Retro3DMode::Both;
    const float pulse=retro3dEnergy_;
    if(retro3dMode_==Retro3DMode::Dancer || both) {
        RetroMesh generated;
        if(retroDancerClip_.empty())dancer(generated,retro3dDancePhase_,pulse);
        const RetroMesh& mesh=retroDancerClip_.empty()
            ? generated : retroDancerClip_.frame(retro3dDancePhase_ /
                                               kTau * 2.8f);
        const float x=w*(both?0.30f:0.50f);
        const float y=h*(0.37f+0.015f*std::sin(retro3dTime_*0.83f));
        renderObject(r,mesh,retroDancerTextures_,x,y,base,
            std::sin(retro3dTime_*0.34f)*0.60f+0.16f*retro3dTime_,
            0.08f*std::sin(retro3dTime_*0.55f),
            0.06f*std::sin(retro3dDancePhase_),
            0.07f*std::sin(retro3dDancePhase_)+pulse*0.11f);
    }
    if(retro3dMode_==Retro3DMode::Can || both) {
        RetroMesh generated;
        if(retroCanClip_.empty())canMesh(generated,pulse);
        const RetroMesh& mesh=retroCanClip_.empty()
            ?generated:retroCanClip_.frames.front();
        const float x=w*(both?0.71f:0.50f);
        const float y=h*(0.37f+0.02f*std::sin(retro3dTime_*0.76f+1.0f));
        renderObject(r,mesh,retroCanTextures_,x,y,base,
            retro3dTime_*(0.62f+0.48f*pulse),
            0.18f+0.11f*std::sin(retro3dTime_*0.57f),
            0.18f*std::sin(retro3dTime_*0.49f),
            pulse*0.13f);
    }
}
} // namespace spectrum
