#include "RetroTexture.hpp"
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>
#endif

namespace spectrum {
SDL_Texture* retroLoadTexture(SDL_Renderer* renderer,
                               const std::filesystem::path& filename) {
    if(!renderer)return nullptr;
#ifdef _WIN32
    const HRESULT init=CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool releaseCOM=SUCCEEDED(init);
    IWICImagingFactory* factory=nullptr;
    IWICBitmapDecoder* decoder=nullptr;
    IWICBitmapFrameDecode* frame=nullptr;
    IWICFormatConverter* converter=nullptr;
    SDL_Texture* texture=nullptr;
    const HRESULT create=CoCreateInstance(
        CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if(SUCCEEDED(create) &&
       SUCCEEDED(factory->CreateDecoderFromFilename(filename.c_str(),nullptr,
                        GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder)) &&
       SUCCEEDED(decoder->GetFrame(0,&frame)) &&
       SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
       SUCCEEDED(converter->Initialize(frame,GUID_WICPixelFormat32bppRGBA,
                    WICBitmapDitherTypeNone,nullptr,0.0,
                    WICBitmapPaletteTypeCustom))) {
        UINT width=0,height=0;
        if(SUCCEEDED(converter->GetSize(&width,&height)) &&
           width>0 && height>0 && width<=8192 && height<=8192) {
            SDL_Surface* image=SDL_CreateSurface(
                static_cast<int>(width),static_cast<int>(height),
                SDL_PIXELFORMAT_RGBA32);
            if(image) {
                const UINT stride=width*4;
                // SDL may pad surface rows: use its actual pitch.
                if(image->pitch>=static_cast<int>(stride) &&
                   SUCCEEDED(converter->CopyPixels(nullptr,
                       static_cast<UINT>(image->pitch),
                       static_cast<UINT>(image->pitch)*height,
                       static_cast<BYTE*>(image->pixels)))) {
                    texture=SDL_CreateTextureFromSurface(renderer,image);
                }
                SDL_DestroySurface(image);
            }
        }
    }
    if(converter)converter->Release();
    if(frame)frame->Release();
    if(decoder)decoder->Release();
    if(factory)factory->Release();
    if(releaseCOM)CoUninitialize();
    if(texture) {
        SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
        return texture;
    }
#endif
    // SDL3 ships with a BMP decoder even without SDL_image.
    SDL_Surface* image=SDL_LoadBMP(filename.string().c_str());
    if(!image)return nullptr;
    SDL_Texture* bitmapTexture=SDL_CreateTextureFromSurface(renderer,image);
    SDL_DestroySurface(image);
    if(bitmapTexture)SDL_SetTextureBlendMode(bitmapTexture,SDL_BLENDMODE_BLEND);
    return bitmapTexture;
}
} // namespace spectrum
