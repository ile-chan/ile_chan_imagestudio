#include "ImageEngine.h"
#include "wky_algorithm_support.h"
#include "wky_ALG_Inverting.h"
#include "wky_ALG_Histogram.h"
#include "wky_ALG_AverageFilter.h"
#include "wky_ALG_MedianFilter.h"
#include "wky_ALG_Threshold.h"
#include "wky_ALG_ContourTrace.h"
#include "wky_ALG_ContourFill.h"
#include "wky_ALG_HoughLine.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <stdexcept>
#include <wincodec.h>
#include <wrl/client.h>

namespace wky {
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void EncodeWic(const Image& image,const std::wstring& path,REFGUID container) {
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> f;Require(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f))),"WIC factory");
    ComPtr<IWICStream> stream;f->CreateStream(&stream);Require(SUCCEEDED(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE)),"WIC stream");
    ComPtr<IWICBitmapEncoder> encoder;Require(SUCCEEDED(f->CreateEncoder(container,nullptr,&encoder)),"WIC encoder");encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache);
    ComPtr<IWICBitmapFrameEncode> frame;frame=nullptr;encoder->CreateNewFrame(&frame,nullptr);frame->Initialize(nullptr);frame->SetSize(image.width,image.height);
    WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR;frame->SetPixelFormat(&format);
    ComPtr<IWICBitmap> bitmap;f->CreateBitmapFromMemory(image.width,image.height,GUID_WICPixelFormat24bppBGR,image.width*3,UINT(image.bgr.size()),const_cast<BYTE*>(image.bgr.data()),&bitmap);
    Require(SUCCEEDED(frame->WriteSource(bitmap.Get(),nullptr)),"WIC write source");Require(SUCCEEDED(frame->Commit()),"WIC frame commit");Require(SUCCEEDED(encoder->Commit()),"WIC encoder commit");
}
}
int RunSelfTests(const std::wstring& directory) {
    std::filesystem::create_directories(directory);
    std::ofstream log(std::filesystem::path(directory)/L"self-test.txt",std::ios::binary);int passed=0,failed=0;
    auto test=[&](const std::string& name,const std::function<void()>& body){try{body();++passed;log<<"PASS "<<name<<"\n";}catch(const std::exception& e){++failed;log<<"FAIL "<<name<<": "<<e.what()<<"\n";}log.flush();};
    std::mt19937 rng(4312);
    test("brightness and population standard deviation",[]{int h[256]{};h[0]=h[100]=1;double mean,contrast;wkyBrightContrast(h,&mean,&contrast);Require(mean==50&&contrast==50,"statistics mismatch");});
    test("empty histogram safe bounds",[]{int h[256]{},low=-1,high=-1;wkyMinMaxGry(h,&low,&high);Require(low==0&&high==0,"empty histogram");Require(wkyGetOtsuThreshold(h,256)==0,"empty Otsu");});
    test("Otsu includes high-intensity final bin",[]{int h[256]{};h[0]=50;h[255]=50;int t=wkyGetOtsuThreshold(h,256);Require(t>0&&t<=255,"invalid split");});
    test("SSE inversion unaligned short buffers and tails",[&]{for(int n=1;n<128;++n){std::vector<BYTE> b(n+18,77),expected=b;for(int i=0;i<n;++i)b[i+1]=expected[i+1]=BYTE(rng()%256);for(int i=0;i<n;++i)expected[i+1]=255-expected[i+1];wkyInvertSSE(b.data()+1,n,1,false);Require(b==expected,"SSE buffer mismatch");}});
    test("box filters vs independent reference, odd widths",[&]{for(int width:{3,5,8,17,67})for(int height:{3,7,12}){
        std::vector<BYTE> src(width*height),a(src.size()),b(src.size()),reference;for(auto& v:src)v=BYTE(rng()%256);reference=src;
        for(int y=1;y<height-1;++y)for(int x=1;x<width-1;++x){int sum=0;for(int yy=y-1;yy<=y+1;++yy)for(int xx=x-1;xx<=x+1;++xx)sum+=src[yy*width+xx];reference[y*width+x]=BYTE(std::lround(sum/9.0));}
        std::vector<long long> integral(src.size()),simd(src.size());wkyDoSumGryImg(src.data(),width,height,integral.data());wkyDoSumGryImg_SSE(src.data(),width,height,simd.data());Require(integral==simd,"scalar vs SIMD integral");
        wkyAvrFilterBySumCol(src.data(),width,height,3,3,a.data());wkyAvrFilterBySumImg(integral.data(),width,height,3,3,b.data());Require(a==reference&&b==reference,"box reference mismatch");}});
    test("integral cumulative sum exceeds signed 32-bit",[]{int width=3001,height=3001;std::vector<BYTE> src(size_t(width)*height,255);std::vector<long long> sum(src.size());wkyDoSumGryImg_SSE(src.data(),width,height,sum.data());Require(sum.back()==1LL*width*height*255&&sum.back()>INT_MAX,"integral overflow");});
    test("median vs independently sorted neighborhoods",[&]{int w=17,h=13;std::vector<BYTE> src(w*h),out(w*h),expected;for(auto&v:src)v=BYTE(rng()%256);expected=src;for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x){std::vector<BYTE> v;for(int yy=y-1;yy<=y+1;++yy)for(int xx=x-1;xx<=x+1;++xx)v.push_back(src[yy*w+xx]);std::sort(v.begin(),v.end());expected[y*w+x]=v[4];}wkyMedianFilter(src.data(),w,h,3,3,out.data());Require(out==expected,"median mismatch");});
    test("regional threshold starts at requested row",[]{std::vector<BYTE> data(30,150);wkyThreshold(data.data(),6,5,2,4,2,3,128);for(int y=0;y<5;++y)for(int x=0;x<6;++x)Require(data[y*6+x]==(y>=2&&y<=3&&x>=2&&x<=4?255:150),"regional bounds");});
    test("block threshold covers uneven and tiny partitions",[]{std::vector<BYTE> data(7*5);for(int i=0;i<35;++i)data[i]=BYTE(i*7);wkyBlockThreshold(data.data(),7,5,3,2);Require(std::all_of(data.begin(),data.end(),[](BYTE v){return v==0||v==255;}),"unprocessed pixels");});
    test("equalization and log constant images",[]{for(BYTE value:{BYTE(0),BYTE(42),BYTE(255)}){std::vector<BYTE> data(63,value);wkyHistogramEqualize(data.data(),9,7);Require(std::all_of(data.begin(),data.end(),[&](BYTE v){return v==value;}),"constant equalization");wkyLogTransform(data.data(),9,7);Require(data[0]==(value?255:0),"constant log");}});
    test("14-bit equalization range and endpoints",[]{short values[]={0,8192,16383};BYTE out[3]{};wkyHistogramEqualize(values,3,1,out);Require(out[0]==0&&out[2]==255&&out[1]>=127&&out[1]<=128,"14-bit mismatch");});
    test("chain code rectangle measurements and membership",[]{BYTE chain[]={0,0,6,6,4,4,2,2};double p=0,a=wkyContourPerimeterAndArea(chain,8,&p);Require(a==4&&p==8,"rectangle measures");Require(wkyContourPixels(chain,8)==9,"lattice pixels");Require(wkyIsPixelInContour(2,3,chain,8,3,4),"inside test");Require(!wkyIsPixelInContour(2,3,chain,8,0,0),"outside test");});
    test("trace and fill with a border-touching object",[]{int w=9,h=8;std::vector<BYTE> mask(w*h,0),filled(w*h,0),chain(1024);for(int y=0;y<=4;++y)for(int x=0;x<=5;++x)mask[y*w+x]=255;int n=wkyTraceContour(mask.data(),w,h,0,0,true,chain.data(),int(chain.size()));Require(n==18,"trace perimeter");wkyFillContour(filled.data(),w,h,0,0,true,chain.data(),n,255,255,0);Require(filled==mask,"fill mismatch");});
    test("isolated foreground and bounded trace buffer",[]{std::vector<BYTE> mask(25,0);mask[12]=255;BYTE code[2]{};Require(wkyTraceContour(mask.data(),5,5,2,2,true,code,2)==0,"isolated trace");std::fill(mask.begin(),mask.end(),255);Require(wkyTraceContour(mask.data(),5,5,0,0,true,code,2)==-1,"trace capacity");});
    test("Hough vertical, horizontal and diagonal lines",[]{for(int kind=0;kind<3;++kind){int x[40],y[40];for(int i=0;i<40;++i){x[i]=kind==0?12:i;y[i]=kind==1?17:i;}double a,b,c;int votes=wkyHoughLine(64,64,x,y,40,&a,&b,&c);Require(votes>=39,"Hough votes");for(int i=0;i<40;++i)Require(std::abs(a*x[i]+b*y[i]+c)<1,"line residual");}});
    Image source;source.width=67;source.height=49;source.bgr.resize(67*49*3);for(auto&v:source.bgr)v=BYTE(rng()%256);
    test("all 30 GUI operations produce valid results",[&]{for(const auto& op:Operations()){auto result=Process(source,op.id);Require(result.error.empty(),"operation returned error");Require(result.image.valid(),"operation output invalid");}});
    test("Sobel response on linear horizontal ramp",[]{Image ramp;ramp.width=ramp.height=7;ramp.bgr.resize(7*7*3);for(int y=0;y<7;++y)for(int x=0;x<7;++x)for(int c=0;c<3;++c)ramp.bgr[(y*7+x)*3+c]=BYTE(10*x);auto result=Process(ramp,Op::Sobel);Require(result.image.bgr[(3*7+3)*3]==80,"Sobel coefficient mismatch");});
    test("constant-image derivatives and sharpening",[]{auto image=FromGray(17,13,std::vector<BYTE>(17*13,95));for(auto op:{Op::Gradient,Op::Roberts,Op::Sobel,Op::Prewitt,Op::ShenJun}){auto result=Process(image,op);Require(result.error.empty()&&std::all_of(result.image.bgr.begin(),result.image.bgr.end(),[](BYTE v){return v==0;}),"constant derivative");}auto result=Process(image,Op::Sharpen);Require(result.image.bgr==image.bgr,"constant sharpen");});
    test("custom identity and independently computed flipped convolution",[&]{auto result=Process(source,Op::Custom);Require(result.image.bgr==source.bgr,"identity mismatch");Kernel k;k.values={1,0,0,0,0,0,0,0,0};k.normalize=false;k.flip=true;k.border=0;auto r=Process(source,Op::Custom,{},k);for(int y=0;y<49;++y)for(int x=0;x<67;++x)for(int c=0;c<3;++c)Require(r.image.bgr[(y*67+x)*3+c]==source.bgr[(std::min(48,y+1)*67+std::min(66,x+1))*3+c],"flipped convolution mismatch");});
    test("custom zero sum, mirror, zero padding, grayscale",[&]{for(int mode=0;mode<3;++mode){Kernel k;k.values={0,-1,0,-1,4,-1,0,-1,0};k.border=mode;k.color=false;auto r=Process(source,Op::Custom,{},k);Require(r.error.empty()&&r.image.gray(),"kernel mode");}auto tiny=FromGray(1,1,std::vector<BYTE>{40});Kernel k;k.values={1,1,1,1,1,1,1,1,1};k.border=1;Require(Process(tiny,Op::Custom,{},k).image.bgr==tiny.bgr,"one-pixel mirror");});
    test("invalid parameters and kernels preserve input",[&]{auto before=source.bgr;Require(!Process(source,Op::Median,{4,3}).error.empty(),"even window accepted");Kernel k;k.normalize=false;k.divisor=0;Require(!Process(source,Op::Custom,{},k).error.empty(),"zero divisor accepted");bool rejected=false;try{ParseKernel(L"1 nope 0",3);}catch(...){rejected=true;}Require(rejected&&source.bgr==before,"invalid input handling");});
    test("BMP color and gray, all padding residues, Unicode paths",[&]{for(int width=1;width<=9;++width)for(bool gray:{false,true}){
        Image image;image.width=width;image.height=7;image.bgr.resize(width*7*3);for(size_t i=0;i<image.bgr.size()/3;++i){image.bgr[3*i]=BYTE(i*31);image.bgr[3*i+1]=gray?image.bgr[3*i]:BYTE(i*11);image.bgr[3*i+2]=gray?image.bgr[3*i]:BYTE(i*19);}
        auto path=(std::filesystem::path(directory)/(L"中文路径_"+std::to_wstring(width)+(gray?L"_灰度.bmp":L"_彩色.bmp"))).wstring();SaveBmp(image,path);std::wstring route;auto decoded=LoadImage(path,&route);Require(image.bgr==decoded.bgr&&image.width==decoded.width&&image.height==decoded.height,"BMP roundtrip");Require(route.find(L"课程")!=std::wstring::npos,"course BMP reader not used");
    }});
    test("PNG and TIFF lossless import, JPEG and GIF import",[&]{struct Format{const wchar_t* ext;const GUID* container;bool lossless;};Format formats[]={{L"png",&GUID_ContainerFormatPng,true},{L"tiff",&GUID_ContainerFormatTiff,true},{L"jpg",&GUID_ContainerFormatJpeg,false},{L"gif",&GUID_ContainerFormatGif,false}};
        for(const auto& fmt:formats){auto path=(std::filesystem::path(directory)/(L"导入格式测试."+std::wstring(fmt.ext))).wstring();EncodeWic(source,path,*fmt.container);auto decoded=LoadImage(path);Require(decoded.width==source.width&&decoded.height==source.height,"format dimensions");if(fmt.lossless)Require(decoded.bgr==source.bgr,"lossless format pixels");}
    });
    test("malformed image rejection",[&]{auto path=std::filesystem::path(directory)/L"损坏图像.bmp";{std::ofstream f(path,std::ios::binary);f<<"BMbroken";}bool rejected=false;try{LoadImage(path.wstring());}catch(...){rejected=true;}Require(rejected,"malformed image accepted");});
    auto fixtureRoot=std::filesystem::path(directory).parent_path()/L"fixtures";
    std::ifstream manifest(fixtureRoot/L"manifest.tsv");std::string fixtureLine;
    while(std::getline(manifest,fixtureLine)){
        auto first=fixtureLine.find('\t'),second=fixtureLine.find('\t',first+1);if(first==std::string::npos||second==std::string::npos)continue;
        auto filename=fixtureLine.substr(0,first),reference=fixtureLine.substr(first+1,second-first-1);int tolerance=std::stoi(fixtureLine.substr(second+1));
        test("independent import fixture: "+filename,[&,filename,reference,tolerance]{auto got=LoadImage((fixtureRoot/filename).wstring()),expected=LoadImage((fixtureRoot/reference).wstring());Require(got.width==expected.width&&got.height==expected.height,"fixture dimensions / orientation");for(size_t i=0;i<got.bgr.size();++i)Require(std::abs(int(got.bgr[i])-expected.bgr[i])<=tolerance,"fixture pixel / orientation / palette / alpha mismatch");});
    }
    test("cancellation and empty image rejection",[&]{std::atomic<bool> cancel{true};Require(!Process(source,Op::Custom,{},Kernel{},&cancel).error.empty(),"cancel ignored");Require(!Process({},Op::Gray).error.empty(),"empty image accepted");});
    SaveBmp(DemoImage(),(std::filesystem::path(directory)/L"演示图.bmp").wstring());
    log<<"\nTOTAL "<<passed+failed<<"  PASSED "<<passed<<"  FAILED "<<failed<<"\n";return failed?1:0;
}
}
