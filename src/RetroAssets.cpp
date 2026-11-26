#include "RetroAssets.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string_view>
#include <string>
#include <vector>

namespace spectrum {
namespace {
struct Material {
    RetroColor diffuse{218,206,192,255};
    int texture=-1;
};
struct ObjVertex {
    RetroVec3 pos{};
    RetroVec2 uv{};
};
int indexOf(std::string_view input,int count){
    int value=0;
    const auto [last,err]=std::from_chars(input.data(),
                                          input.data()+input.size(),value);
    if(err!=std::errc{} || last!=input.data()+input.size())return -1;
    int index=value>0?value-1:value<0?count+value:-1;
    return index>=0&&index<count?index:-1;
}
bool insideAssetRoot(const std::filesystem::path& root,
                     const std::filesystem::path& file) {
    const auto base=root.lexically_normal();
    const auto path=file.lexically_normal();
    auto a=base.begin(),b=path.begin();
    for(;a!=base.end();++a,++b)
        if(b==path.end() || *a!=*b)return false;
    return true;
}
void loadMtl(const std::filesystem::path& path,
             const std::filesystem::path& root,
             std::map<std::string,Material>& materials,
             std::vector<std::filesystem::path>& images) {
    std::ifstream stream(path);
    if(!stream) return;
    std::string name,line;
    while(std::getline(stream,line)) {
        std::istringstream in(line);
        std::string op;in>>op;
        if(op=="newmtl"){in>>name;materials.try_emplace(name);}
        if(name.empty())continue;
        if(op=="Kd") {
            float red=0.8f,green=0.8f,blue=0.8f;
            if(in>>red>>green>>blue) {
                materials[name].diffuse={
                    static_cast<unsigned char>(std::clamp(red*255.0f,0.0f,255.0f)),
                    static_cast<unsigned char>(std::clamp(green*255.0f,0.0f,255.0f)),
                    static_cast<unsigned char>(std::clamp(blue*255.0f,0.0f,255.0f)),255};
            }
        }else if(op=="map_Kd") {
            std::string filename;
            std::getline(in,filename);
            const auto start=filename.find_first_not_of(" \t");
            if(start==std::string::npos) continue;
            filename=filename.substr(start);
            // A local MTL is data, not permission to load external locations.
            const auto image=(path.parent_path()/filename).lexically_normal();
            if(!insideAssetRoot(root,image)) continue;
            const auto it=std::find(images.begin(),images.end(),image);
            if(it==images.end()) {
                if(images.size()>=64)continue;
                images.push_back(image);
                materials[name].texture=static_cast<int>(images.size()-1);
            }else materials[name].texture=static_cast<int>(
                 std::distance(images.begin(),it));
        }
    }
}
struct FaceRef {
    int vertex=-1,uv=-1;
};
FaceRef faceRef(const std::string& word,int vertices,int uvs) {
    const auto first=word.find('/');
    const auto second=word.find('/',first==std::string::npos?first:first+1);
    const std::string_view s(word);
    FaceRef value;
    value.vertex=indexOf(s.substr(0,first),vertices);
    if(first!=std::string::npos && second!=first+1)
        value.uv=indexOf(s.substr(first+1,
           second==std::string::npos?second:second-first-1),uvs);
    return value;
}
bool loadObj(const std::filesystem::path& file,
             const std::filesystem::path& root,
             RetroMesh& triangles,std::vector<std::filesystem::path>& images) {
    std::ifstream in(file);
    if(!in)return false;
    std::vector<RetroVec3> coords;
    std::vector<RetroVec2> uv;
    std::map<std::string,Material> materials;
    Material active{};
    std::string line;
    while(std::getline(in,line)){
        if(line.size()>4096)continue;
        std::istringstream parser(line);
        std::string op;parser>>op;
        if(op=="v"){
            RetroVec3 v;
            if(parser>>v.x>>v.y>>v.z && coords.size()<400000)
                coords.push_back(v);
        } else if(op=="vt"){
            RetroVec2 p;
            if(parser>>p.u>>p.v && uv.size()<400000)
                uv.push_back(p);
        } else if(op=="mtllib"){
            std::string mtl;std::getline(parser,mtl);
            const auto p=mtl.find_first_not_of(" \t");
            if(p==std::string::npos)continue;
            const auto path=(file.parent_path()/mtl.substr(p)).lexically_normal();
            if(insideAssetRoot(root,path))
                loadMtl(path,root,materials,images);
        } else if(op=="usemtl"){
            std::string name;parser>>name;
            const auto found=materials.find(name);
            active=found==materials.end()?Material{}:found->second;
        } else if(op=="f"){
            std::vector<FaceRef> face;
            std::string part;
            while(parser>>part && face.size()<256)
                face.push_back(faceRef(part,
                    static_cast<int>(coords.size()),static_cast<int>(uv.size())));
            if(face.size()<3 || triangles.size()>120000)continue;
            for(std::size_t t=1;t+1<face.size();++t){
                const FaceRef corners[3]={face[0],face[t],face[t+1]};
                if(corners[0].vertex<0||corners[1].vertex<0||
                   corners[2].vertex<0)continue;
                RetroTriangle tri{coords[corners[0].vertex],
                                  coords[corners[1].vertex],
                                  coords[corners[2].vertex],active.diffuse};
                for(int j=0;j<3;++j) {
                    if(corners[j].uv>=0) {
                        RetroVec2 p=uv[corners[j].uv];
                        p.v=1.0f-p.v; // OBJ UV origin is bottom-left
                        if(j==0)tri.uvA=p;
                        if(j==1)tri.uvB=p;
                        if(j==2)tri.uvC=p;
                    }
                }
                tri.textureId=active.texture;
                triangles.push_back(tri);
            }
        }
    }
    return !triangles.empty();
}
} // namespace

RetroAssetClip loadRetroAssetClip(const std::filesystem::path& root,
                                  bool animated) {
    RetroAssetClip clip;
    std::vector<std::filesystem::path> files;
    if(animated) {
        const auto folder=root/"frames";
        std::error_code ec;
        if(!std::filesystem::is_directory(folder,ec))return clip;
        for(auto it=std::filesystem::directory_iterator(folder,ec);
            !ec && it!=std::filesystem::directory_iterator();it.increment(ec)){
            if(it->is_regular_file(ec) && it->path().extension()==".obj")
                files.push_back(it->path());
        }
        std::sort(files.begin(),files.end());
        if(files.size()>96)files.resize(96);
    } else {
        files.push_back(root/"can.obj");
    }
    if(files.empty())return clip;
    for(const auto& file:files) {
        RetroMesh frame;
        if(!loadObj(file,root,frame,clip.textures))continue;
        clip.frames.push_back(std::move(frame));
    }
    if(clip.empty())return clip;
    // First frame determines a stable pivot across the animation, retaining
    // real frame-to-frame movement rather than recentering the dancer.
    const auto& first=clip.frames.front();
    RetroVec3 low{1e30f,1e30f,1e30f},high{-1e30f,-1e30f,-1e30f};
    for(const auto& tri:first){
        for(const auto& v:{tri.a,tri.b,tri.c}){
            low.x=std::min(low.x,v.x);low.y=std::min(low.y,v.y);
            low.z=std::min(low.z,v.z);
            high.x=std::max(high.x,v.x);high.y=std::max(high.y,v.y);
            high.z=std::max(high.z,v.z);
        }
    }
    const RetroVec3 center=(low+high)*0.5f;
    const RetroVec3 range=high-low;
    const float scale=3.0f/std::max({0.001f,range.x,range.y,range.z});
    for(auto& frame:clip.frames)
        for(auto& tri:frame){
            tri.a=(tri.a-center)*scale;
            tri.b=(tri.b-center)*scale;
            tri.c=(tri.c-center)*scale;
        }
    return clip;
}
} // namespace spectrum
