#include "ImageEngine.h"
#include "bmpFile.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
namespace wky {
namespace {
std::string Utf8(const std::wstring& text) {
    int n = WideCharToMultiByte(CP_UTF8,0,text.c_str(),int(text.size()),nullptr,0,nullptr,nullptr);
    std::string result(n,'\0'); WideCharToMultiByte(CP_UTF8,0,text.c_str(),int(text.size()),result.data(),n,nullptr,nullptr); return result;
}
void Check(HRESULT hr, const char* message) { if(FAILED(hr)) throw std::runtime_error(message); }
struct ComApartment {
    HRESULT hr = CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ~ComApartment(){if(SUCCEEDED(hr))CoUninitialize();}
};
void ValidateDimensions(uint64_t width,uint64_t height) {
    if(!width||!height||width>32768||height>32768||width*height>MaxPixels)
        throw std::runtime_error("图像尺寸超出范围：最多 2000 万像素，单边不超过 32768。请先缩小图像。");
}
bool CourseBmpEligible(const std::wstring& path) {
    std::ifstream file{std::filesystem::path(path),std::ios::binary};
    BITMAPFILEHEADER f{}; BITMAPINFOHEADER b{};
    if(!file.read(reinterpret_cast<char*>(&f),sizeof f)||!file.read(reinterpret_cast<char*>(&b),sizeof b))return false;
    if(f.bfType!=0x4D42||b.biSize!=40||b.biPlanes!=1||b.biCompression!=BI_RGB||b.biWidth<=0||b.biHeight<=0||(b.biBitCount!=8&&b.biBitCount!=24))return false;
    ValidateDimensions(b.biWidth,b.biHeight);
    uint64_t stride=(uint64_t(b.biWidth)*b.biBitCount+31)/32*4;
    const auto length=std::filesystem::file_size(path);
    if(f.bfOffBits<54||(b.biBitCount==8&&f.bfOffBits<1078)||uint64_t(f.bfOffBits)+stride*b.biHeight>length)return false;
    if(b.biBitCount==8) {
        if(b.biClrUsed&&b.biClrUsed!=256)return false;
        for(int i=0;i<256;++i){RGBQUAD p{};if(!file.read(reinterpret_cast<char*>(&p),sizeof p)||p.rgbRed!=i||p.rgbGreen!=i||p.rgbBlue!=i)return false;}
    }
    return true;
}

bool TryIcon(const std::wstring& path,Image& output) {
    std::ifstream file{std::filesystem::path(path),std::ios::binary};
    WORD header[3]{};if(!file.read(reinterpret_cast<char*>(header),sizeof header)||header[0]!=0||header[1]!=1||header[2]==0||header[2]>256)return false;
    struct Entry {BYTE width,height,colors,reserved;WORD planes,bits;DWORD size,offset;};
    static_assert(sizeof(Entry)==16);Entry best{};int bestArea=0;
    for(int i=0;i<header[2];++i){Entry entry{};if(!file.read(reinterpret_cast<char*>(&entry),sizeof entry))return false;int area=(entry.width?entry.width:256)*(entry.height?entry.height:256);if(area>bestArea){best=entry;bestArea=area;}}
    auto length=std::filesystem::file_size(path);if(!best.size||best.size>32*1024*1024||uint64_t(best.offset)+best.size>length)return false;
    file.seekg(best.offset);std::vector<BYTE> data(best.size);if(!file.read(reinterpret_cast<char*>(data.data()),data.size()))return false;
    int width=best.width?best.width:256,height=best.height?best.height:256;
    HICON icon=CreateIconFromResourceEx(data.data(),DWORD(data.size()),TRUE,0x00030000,width,height,LR_DEFAULTCOLOR);if(!icon)return false;
    HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!bitmap){DestroyIcon(icon);DeleteDC(dc);return false;}
    std::fill_n(static_cast<BYTE*>(pixels),size_t(width)*height*4,BYTE(255));auto previous=SelectObject(dc,bitmap);
    BOOL ok=DrawIconEx(dc,0,0,icon,width,height,0,nullptr,DI_NORMAL);GdiFlush();
    if(ok){output.width=width;output.height=height;output.bgr.resize(size_t(width)*height*3);auto bytes=static_cast<BYTE*>(pixels);for(size_t i=0;i<size_t(width)*height;++i)for(int c=0;c<3;++c)output.bgr[i*3+c]=bytes[i*4+c];}
    SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);DestroyIcon(icon);return ok!=FALSE;
}
}

Image LoadImage(const std::wstring& path,std::wstring* route) {
    if(!std::filesystem::is_regular_file(path))throw std::runtime_error("找不到图像文件，请重新选择文件。");
    if(std::filesystem::file_size(path)>512ULL*1024*1024)throw std::runtime_error("文件超过 512 MB，请选择较小的图像。");
    if(CourseBmpEligible(path)) {
        int width=0,height=0;bool rgb=false;
        std::unique_ptr<BYTE[]> pixels(Rmw_ReadBmpFile2Img(Utf8(path).c_str(),&width,&height,&rgb));
        if(pixels) {
            Image image;image.width=width;image.height=height;
            if(rgb)image.bgr.assign(pixels.get(),pixels.get()+size_t(width)*height*3);
            else image=FromGray(width,height,std::vector<BYTE>(pixels.get(),pixels.get()+size_t(width)*height));
            if(route)*route=L"课程 BMP 读入函数";
            return image;
        }
    }
    Image icon;if(TryIcon(path,icon)){if(route)*route=L"Windows 图标解码器（最大分辨率条目）";return icon;}
    ComApartment apartment;
    ComPtr<IWICImagingFactory> factory;
    Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"无法创建 Windows 图像解码器。");
    ComPtr<IWICBitmapDecoder> decoder;
    Check(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder),"无法读取该格式或图像已损坏。请尝试 PNG、JPEG、BMP、TIFF、GIF 或 ICO。");
    ComPtr<IWICBitmapFrameDecode> frame;
    Check(decoder->GetFrame(0,&frame),"无法读取图像的第一帧。");
    UINT width=0,height=0;Check(frame->GetSize(&width,&height),"无法读取图像尺寸。");ValidateDimensions(width,height);
    ComPtr<IWICBitmapSource> source;Check(frame.As(&source),"无法读取图像数据。");
    ComPtr<IWICMetadataQueryReader> metadata;
    if(SUCCEEDED(frame->GetMetadataQueryReader(&metadata))) {
        PROPVARIANT orientation{};PropVariantInit(&orientation);
        HRESULT hr=metadata->GetMetadataByName(L"/app1/ifd/{ushort=274}",&orientation);
        if(FAILED(hr)){PropVariantClear(&orientation);PropVariantInit(&orientation);hr=metadata->GetMetadataByName(L"/ifd/{ushort=274}",&orientation);}
        unsigned value=SUCCEEDED(hr)&&orientation.vt==VT_UI2?orientation.uiVal:1;
        PropVariantClear(&orientation);
        WICBitmapTransformOptions transforms[]={WICBitmapTransformRotate0,WICBitmapTransformRotate0,WICBitmapTransformFlipHorizontal,WICBitmapTransformRotate180,WICBitmapTransformFlipVertical,
            WICBitmapTransformOptions(WICBitmapTransformRotate90|WICBitmapTransformFlipVertical),WICBitmapTransformRotate90,WICBitmapTransformOptions(WICBitmapTransformRotate90|WICBitmapTransformFlipHorizontal),WICBitmapTransformRotate270};
        if(value>=2&&value<=8){ComPtr<IWICBitmapFlipRotator> rotate;Check(factory->CreateBitmapFlipRotator(&rotate),"无法校正图像方向。");Check(rotate->Initialize(source.Get(),transforms[value]),"无法校正图像方向。");Check(rotate.As(&source),"无法校正图像方向。");source->GetSize(&width,&height);}
    }
    ComPtr<IWICFormatConverter> convert;Check(factory->CreateFormatConverter(&convert),"无法创建像素转换器。");
    Check(convert->Initialize(source.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"无法将图像转换为可处理的颜色格式。");
    std::vector<BYTE> bgra(size_t(width)*height*4);
    Check(convert->CopyPixels(nullptr,width*4,UINT(bgra.size()),bgra.data()),"图像像素读取失败，文件可能不完整。");
    Image image;image.width=int(width);image.height=int(height);image.bgr.resize(size_t(width)*height*3);
    for(size_t i=0;i<size_t(width)*height;++i)for(int c=0;c<3;++c)image.bgr[i*3+c]=BYTE(std::min(255,int(bgra[i*4+c])+255-bgra[i*4+3]));
    if(route)*route=L"Windows 图像解码器（第一帧，透明区域合成为白色）";
    return image;
}

void SaveBmp(const Image& image,const std::wstring& path) {
    if(!image.valid())throw std::runtime_error("没有可导出的图像，请先导入图像。");
    auto parent=std::filesystem::path(path).parent_path();if(parent.empty())parent=std::filesystem::current_path();
    wchar_t temp[MAX_PATH]{};
    if(!GetTempFileNameW(parent.c_str(),L"wky",0,temp))throw std::runtime_error("无法在目标文件夹创建文件，请检查路径和写入权限。");
    struct Temporary {std::wstring path;~Temporary(){DeleteFileW(path.c_str());}} temporary{temp};
    auto name=Utf8(temporary.path);bool gray=image.gray(),ok=false;
    if(gray){auto pixels=ToGray(image);ok=Rmw_Write8BitImg2BmpFile(pixels.data(),image.width,image.height,name.c_str());}
    else ok=Rmw_Write24BitImg2BmpFile(const_cast<BYTE*>(image.bgr.data()),image.width,image.height,name.c_str());
    uint64_t stride=(uint64_t(image.width)*(gray?1:3)+3)/4*4,expected=(gray?1078:54)+stride*image.height;
    if(!ok||std::filesystem::file_size(temporary.path)!=expected)throw std::runtime_error("BMP 写入不完整，原有目标文件保持不变。请检查磁盘空间。");
    std::ifstream verify{std::filesystem::path(temporary.path),std::ios::binary};BITMAPFILEHEADER f{};verify.read(reinterpret_cast<char*>(&f),sizeof f);verify.close();
    if(f.bfType!=0x4D42||f.bfSize!=expected)throw std::runtime_error("BMP 文件头校验失败，未替换目标文件。");
    if(!MoveFileExW(temporary.path.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("无法保存到目标位置。请关闭占用该文件的程序或选择其他文件名。");
}

std::wstring ErrorText(const std::exception& error) {
    const char* message=error.what();int n=MultiByteToWideChar(CP_UTF8,0,message,-1,nullptr,0);
    std::wstring result(n?size_t(n):0,L'\0');if(n){MultiByteToWideChar(CP_UTF8,0,message,-1,result.data(),n);result.pop_back();}
    return result.empty()?L"操作失败，请尝试其他参数或文件。":result;
}
}
