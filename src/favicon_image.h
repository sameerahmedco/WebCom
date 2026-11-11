#pragma once

#include "win32_com_compat.h"
#include <gdiplus.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace webcom {

// WebView2 supplies PNG bytes; never fetch an icon URL separately. Bound the
// stream and PNG dimensions before asking GDI+ to decode, then retain only a
// 32x32 copy (4 KiB of pixels), not the stream or the original bitmap.
inline std::unique_ptr<Gdiplus::Bitmap> DecodeTabFavicon(IStream* stream) {
    if(!stream)return {};
    LARGE_INTEGER start{};
    if(FAILED(stream->Seek(start,STREAM_SEEK_SET,nullptr)))return {};
    constexpr ULONG limit=256*1024;
    std::vector<std::uint8_t> bytes(limit+1);
    ULONG total=0;
    while(total<=limit){
        ULONG received=0;
        const HRESULT result=stream->Read(bytes.data()+total,limit+1-total,&received);
        if(FAILED(result) || received>limit+1-total)return {};
        total+=received;
        if(result==S_FALSE || !received)break;
    }
    if(total<33 || total>limit)return {};
    constexpr std::array<std::uint8_t,8> png{137,80,78,71,13,10,26,10};
    if(!std::equal(png.begin(),png.end(),bytes.begin()) ||
        bytes[8]!=0 || bytes[9]!=0 || bytes[10]!=0 || bytes[11]!=13 ||
        bytes[12]!='I' || bytes[13]!='H' || bytes[14]!='D' || bytes[15]!='R')return {};
    const auto number=[&](std::size_t offset){return (static_cast<std::uint32_t>(bytes[offset])<<24)|
        (static_cast<std::uint32_t>(bytes[offset+1])<<16)|(static_cast<std::uint32_t>(bytes[offset+2])<<8)|bytes[offset+3];};
    const auto width = number(16);
    const auto height = number(20);
    if(!width || !height || width>512 || height>512)return {};
    Microsoft::WRL::ComPtr<IStream> memory;
    memory.Attach(SHCreateMemStream(bytes.data(),total));
    if(!memory)return {};
    Gdiplus::Bitmap original(memory.Get(),FALSE);
    if(original.GetLastStatus()!=Gdiplus::Ok || original.GetWidth()!=width || original.GetHeight()!=height)return {};
    auto scaled_bitmap=std::make_unique<Gdiplus::Bitmap>(32,32,PixelFormat32bppPARGB);
    if(scaled_bitmap->GetLastStatus()!=Gdiplus::Ok)return {};
    Gdiplus::Graphics graphics(scaled_bitmap.get());
    graphics.Clear(Gdiplus::Color(0,0,0,0));
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    const float scale=32.0f/static_cast<float>(std::max(width,height));
    const float w=static_cast<float>(width)*scale,h=static_cast<float>(height)*scale;
    if(graphics.DrawImage(&original,Gdiplus::RectF((32-w)/2,(32-h)/2,w,h))!=Gdiplus::Ok)return {};
    return scaled_bitmap;
}

} // namespace webcom
