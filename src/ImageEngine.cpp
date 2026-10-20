#include "ImageEngine.h"
#include "wky_algorithm_support.h"
#include "wky_ALG_Inverting.h"
#include "wky_ALG_Histogram.h"
#include "wky_ALG_LinearStretch.h"
#include "wky_ALG_AverageFilter.h"
#include "wky_ALG_MedianFilter.h"
#include "wky_ALG_BinImgFilter.h"
#include "wky_ALG_EdgeDetect.h"
#include "wky_ALG_EdgeSharpen.h"
#include "wky_ALG_Threshold.h"
#include "wky_ALG_Cluster.h"
#include "wky_ALG_2DHistogram.h"
#include "wky_ALG_ContourTrace.h"
#include "wky_ALG_ContourFill.h"
#include "wky_ALG_HoughLine.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace wky {
const std::vector<Operation>& Operations() {
    using P=Parameter;
    static const std::vector<Operation> items={
        {Op::Gray,0,L"转换为灰度",L"把彩色图像转换为 8 位灰度图。",{}},
        {Op::Invert,0,L"图像反色",L"调用课程反转算法，彩色图保留三个通道。",{}},
        {Op::InvertSSE,0,L"图像反色 · SSE",L"同一反色算法的 SSE 加速实现。",{}},
        {Op::Linear,0,L"线性灰度拉伸",L"按 g′ = k × g + b 映射灰度，并限制到 0–255。",{P{L"斜率 k",1.2,-10,10,false},P{L"偏移 b",0,-255,255,false}}},
        {Op::Log,0,L"对数变换",L"增强暗部灰度，适合观察较暗区域。",{}},
        {Op::Equalize,0,L"全局直方图均衡化",L"使用整幅图像的灰度累计分布增强对比度。",{}},
        {Op::LocalEqualize,0,L"局部直方图均衡化",L"在滑动邻域内均衡化；边缘保留原图。",{P{L"窗口宽（奇数）",15,1,127,true},P{L"窗口高（奇数）",15,1,127,true}}},
        {Op::MeanColumn,1,L"均值滤波 · 列累加",L"列累加与水平滑窗实现；边缘保留原图。",{P{L"窗口宽（奇数）",5,1,127,true},P{L"窗口高（奇数）",5,1,127,true}}},
        {Op::MeanIntegral,1,L"均值滤波 · 积分图",L"SSE2 构建 64 位积分图，快速计算窗口均值。",{P{L"窗口宽（奇数）",5,1,127,true},P{L"窗口高（奇数）",5,1,127,true}}},
        {Op::Median,1,L"中值滤波",L"滑动直方图中值滤波；可用于抑制椒盐噪声。",{P{L"窗口宽（奇数）",3,1,127,true},P{L"窗口高（奇数）",3,1,127,true}}},
        {Op::BinaryFilter,1,L"二值图像邻域滤波",L"先用 Otsu 二值化，再按邻域白色比例判定输出。",{P{L"窗口宽（奇数）",3,1,127,true},P{L"窗口高（奇数）",3,1,127,true},P{L"白色比例阈值",0.5,0,1,false}}},
        {Op::Gradient,2,L"Gradient 梯度",L"相邻行列差分，输出梯度幅值。",{}},
        {Op::Roberts,2,L"Roberts 边缘检测",L"2 × 2 对角差分，取最大绝对分量。",{}},
        {Op::Sobel,2,L"Sobel 边缘检测",L"3 × 3 加权差分，输出水平与垂直响应幅度之和。",{}},
        {Op::Prewitt,2,L"Prewitt 边缘检测",L"比较四个方向的差分响应，保留最强边缘。",{}},
        {Op::ShenJun,2,L"沈俊边缘检测",L"双向递归平滑，再提取符号变化的边界。",{P{L"平滑系数 α",0.3,0.01,0.99,false}}},
        {Op::Sharpen,2,L"拉普拉斯锐化",L"把八邻域拉普拉斯细节叠加到原灰度图。",{P{L"锐化强度",0.25,0,5,false}}},
        {Op::Threshold,3,L"固定阈值二值化",L"灰度不小于阈值时输出白色，其余输出黑色。",{P{L"阈值",128,0,255,true}}},
        {Op::SemiThreshold,3,L"半阈值处理",L"大于阈值的像素置白，其余保留灰度。",{P{L"阈值",128,0,255,true}}},
        {Op::Otsu,3,L"Otsu 自动阈值",L"根据直方图最大类间方差自动选择阈值。",{}},
        {Op::BlockThreshold,3,L"分块 Otsu 阈值",L"把图像分成多个区域，每块单独选择 Otsu 阈值。",{P{L"水平方块数",4,1,64,true},P{L"垂直方块数",4,1,64,true}}},
        {Op::PixelThreshold,3,L"局部均值自适应阈值",L"逐像素比较局部均值；边缘使用裁剪邻域。",{P{L"窗口宽（奇数）",15,1,127,true},P{L"窗口高（奇数）",15,1,127,true}}},
        {Op::Cluster,3,L"灰度直方图聚类",L"灰度频数加权聚类，按聚类中心生成分割图。",{P{L"类别数",3,2,16,true},P{L"收敛容差",0.1,0.0001,10,false}}},
        {Op::Contour,4,L"轮廓跟踪与绘制",L"白色为前景；对每个八连通区域跟踪外轮廓。",{P{L"阈值（−1 自动）",-1,-1,255,true},P{L"最小区域像素数",16,1,1000000,true}}},
        {Op::FillContour,4,L"轮廓区域填充",L"沿外轮廓填充内部，也可填充封闭轮廓的孔洞。",{P{L"阈值（−1 自动）",-1,-1,255,true},P{L"最小区域像素数",16,1,1000000,true}}},
        {Op::Measure,4,L"轮廓几何测量",L"叠加轮廓与外接矩形，列出面积、周长和区域像素数。",{P{L"阈值（−1 自动）",-1,-1,255,true},P{L"最小区域像素数",16,1,1000000,true}}},
        {Op::Hough,4,L"Hough 直线检测",L"先提取 Sobel 边缘，再投票检测最显著的一条直线。",{P{L"边缘幅值阈值",100,1,255,true}}},
        {Op::Histogram,5,L"绘制灰度直方图",L"把直方图绘制为图像；左侧原图仍保留。",{}},
        {Op::Histogram2D,5,L"绘制二维直方图",L"横轴为灰度、纵轴为邻域均值；亮度按对数频数显示。",{P{L"均值窗口宽",5,1,127,true},P{L"均值窗口高",5,1,127,true}}},
        {Op::Custom,6,L"自定义卷积核",L"输入奇数阶方阵；支持归一化、偏移、边界策略及彩色处理。",{}}
    };
    return items;
}
const Operation& Describe(Op id) {for(const auto& op:Operations())if(op.id==id)return op;return Operations().front();}
bool Image::gray() const {for(size_t i=0;i<bgr.size();i+=3)if(bgr[i]!=bgr[i+1]||bgr[i]!=bgr[i+2])return false;return true;}
std::vector<BYTE> ToGray(const Image& image) {
    std::vector<BYTE> result(size_t(image.width)*image.height);
    for(size_t i=0;i<result.size();++i)result[i]=BYTE((29*image.bgr[i*3]+150*image.bgr[i*3+1]+77*image.bgr[i*3+2]+128)>>8);
    return result;
}
Image FromGray(int width,int height,const std::vector<BYTE>& data) {
    Image image;image.width=width;image.height=height;image.bgr.resize(size_t(width)*height*3);
    for(size_t i=0;i<size_t(width)*height;++i)image.bgr[i*3]=image.bgr[i*3+1]=image.bgr[i*3+2]=data[i];return image;
}
Statistics Analyze(const Image& image) {
    Statistics stats;if(!image.valid())return stats;auto gray=ToGray(image);
    wkyGetHistogram(gray.data(),image.width,image.height,stats.histogram.data());
    wkyMinMaxGry(stats.histogram.data(),&stats.low,&stats.high);
    wkyBrightContrast(stats.histogram.data(),&stats.mean,&stats.contrast);return stats;
}
namespace {
void Cancelled(const std::atomic<bool>* flag){if(flag&&flag->load())throw std::runtime_error("处理已取消。");}
void Binary(std::vector<BYTE>& gray,int width,int height,int threshold) {
    bool binary=std::all_of(gray.begin(),gray.end(),[](BYTE g){return g==0||g==255;});
    if(threshold<0&&binary)return;
    if(threshold<0){int histogram[256];wkyGetHistogram(gray.data(),width,height,histogram);threshold=wkyGetOtsuThreshold(histogram,256);}
    wkyThreshold(gray.data(),width,height,threshold);
}
void ColorPixel(Image& image,int x,int y,BYTE b,BYTE g,BYTE r){if(x>=0&&y>=0&&x<image.width&&y<image.height){size_t i=(size_t(y)*image.width+x)*3;image.bgr[i]=b;image.bgr[i+1]=g;image.bgr[i+2]=r;}}
void Rectangle(Image& image,int x1,int y1,int x2,int y2) {
    for(int x=x1;x<=x2;++x){ColorPixel(image,x,y1,25,205,245);ColorPixel(image,x,y2,25,205,245);}
    for(int y=y1;y<=y2;++y){ColorPixel(image,x1,y,25,205,245);ColorPixel(image,x2,y,25,205,245);}
}
int Border(int value,int n,int mode) {
    if(value>=0&&value<n)return value;if(mode==2)return -1;if(mode==0)return std::clamp(value,0,n-1);
    if(n==1)return 0;int period=2*(n-1);value=(value%period+period)%period;return value<n?value:period-value;
}
Result Convolve(const Image& input,const Kernel& k,const std::atomic<bool>* cancel) {
    if(k.size<1||k.size>15||k.size%2!=1||k.values.size()!=size_t(k.size)*k.size)throw std::runtime_error("卷积核必须是 1–15 阶的奇数方阵。");
    if(k.border<0||k.border>2||!std::isfinite(k.divisor)||!std::isfinite(k.bias)||std::abs(k.bias)>65535)throw std::runtime_error("卷积参数无效。");
    double sum=0;for(auto v:k.values){if(!std::isfinite(v)||std::abs(v)>1000000)throw std::runtime_error("卷积系数须为有限数，绝对值不超过 1000000。");sum+=v;}
    double divisor=k.normalize?(std::abs(sum)>1e-12?sum:1):k.divisor;
    if(std::abs(divisor)<1e-12)throw std::runtime_error("卷积除数不能为零。可勾选按系数和归一化。");
    Image source=k.color?input:FromGray(input.width,input.height,ToGray(input));
    Result result;result.image=source;int half=k.size/2;
    for(int y=0;y<input.height;++y){Cancelled(cancel);for(int x=0;x<input.width;++x)for(int c=0;c<(k.color?3:1);++c){
        double response=0;
        for(int yy=0;yy<k.size;++yy)for(int xx=0;xx<k.size;++xx){
            int sy=Border(y+yy-half,input.height,k.border),sx=Border(x+xx-half,input.width,k.border);
            if(sx<0||sy<0)continue;
            int ki=k.flip?(k.size-1-yy)*k.size+k.size-1-xx:yy*k.size+xx;
            response+=k.values[ki]*source.bgr[(size_t(sy)*input.width+sx)*3+c];
        }
        size_t i=(size_t(y)*input.width+x)*3;result.image.bgr[i+c]=wkyByte(response/divisor+k.bias);
        if(!k.color)result.image.bgr[i+1]=result.image.bgr[i+2]=result.image.bgr[i];
    }}
    std::wostringstream info;info<<L"自定义 "<<k.size<<L" × "<<k.size<<L" 卷积核\r\n实际除数："<<divisor<<L"；偏移："<<k.bias<<L"\r\n"<<(k.flip?L"数学卷积（翻转卷积核）":L"相关运算（不翻转卷积核）")<<L"；"<<(k.color?L"逐通道彩色处理":L"灰度处理");result.details=info.str();return result;
}
Result Contours(const Image& input,std::vector<BYTE> gray,Op op,int threshold,int minimum,const std::atomic<bool>* cancel) {
    const int width=input.width,height=input.height;Binary(gray,width,height,threshold);
    std::vector<BYTE> visited(gray.size(),0),out(gray.size(),0);std::vector<int> queue;
    Result result;result.image=input;std::wostringstream info;int accepted=0,total=0;long long areaPixels=0;
    info<<L"白色前景 · 八连通区域 · 外轮廓\r\n";
    static int dx[8]={1,1,0,-1,-1,-1,0,1},dy[8]={0,-1,-1,-1,0,1,1,1};
    for(int start=0;start<width*height;++start){
        if(!gray[start]||visited[start])continue;Cancelled(cancel);queue.clear();queue.push_back(start);visited[start]=1;
        for(size_t at=0;at<queue.size();++at){int i=queue[at],x=i%width,y=i/width;for(int d=0;d<8;++d){int xx=x+dx[d],yy=y+dy[d];if(xx<0||yy<0||xx>=width||yy>=height)continue;int j=yy*width+xx;if(gray[j]&&!visited[j]){visited[j]=1;queue.push_back(j);}}}
        ++total;if(queue.size()<size_t(minimum))continue;
        if(++accepted>2048)throw std::runtime_error("前景区域超过 2048 个，请先滤波或增大最小区域像素数。");
        areaPixels+=queue.size();int x0=start%width,y0=start/width;
        std::vector<BYTE> chain(std::min<size_t>(size_t(width)*height*8+8,queue.size()*8+8));
        int n=wkyTraceContour(gray.data(),width,height,x0,y0,true,chain.data(),int(chain.size()));
        if(n<0)throw std::runtime_error("轮廓未能在有限步数内闭合，请先平滑二值图像。");
        if(n==0){out[start]=255;ColorPixel(result.image,x0,y0,80,220,50);continue;}
        if(op==Op::FillContour)wkyFillContour(out.data(),width,height,x0,y0,true,chain.data(),n,255,255,0);
        else wkyDrawContour(out.data(),width,x0,y0,chain.data(),n,255);
        if(op==Op::Measure){
            int x1,x2,y1,y2;wkyContourRect(x0,y0,chain.data(),n,&x1,&x2,&y1,&y2);Rectangle(result.image,x1,y1,x2,y2);
            int x=x0,y=y0;for(int i=0;i<n;++i){ColorPixel(result.image,x,y,80,220,50);x+=dx[chain[i]];y+=dy[chain[i]];}
            double perimeter=0,area=wkyContourPerimeterAndArea(chain.data(),n,&perimeter);
            if(accepted<=20)info<<L"区域 "<<accepted<<L"：像素 "<<queue.size()<<L"；几何面积 "<<std::fixed<<std::setprecision(1)<<area<<L"；周长 "<<perimeter<<L"；外接框 "<<x2-x1+1<<L" × "<<y2-y1+1<<L"\r\n";
        }
    }
    if(op!=Op::Measure)result.image=FromGray(width,height,out);
    info<<L"保留 "<<accepted<<L" / "<<total<<L" 个区域；前景像素合计 "<<areaPixels<<L"。";
    if(accepted>20&&op==Op::Measure)info<<L" 详细列表显示前 20 个区域。";
    if(!accepted)info<<L" 未找到满足条件的白色前景；可先反色或降低最小区域像素数。";
    result.details=info.str();return result;
}
}

Kernel ParseKernel(const std::wstring& text,int size) {
    Kernel k;k.size=size;k.values.clear();std::wstring normalized=text;
    for(auto& c:normalized)if(c==L','||c==L';'||c==L'，'||c==L'；'||c==L'['||c==L']')c=L' ';
    std::wistringstream in(normalized);double value;
    while(in>>value){if(!std::isfinite(value)||std::abs(value)>1000000)throw std::runtime_error("卷积系数必须是有限数，绝对值不超过 1000000。");k.values.push_back(value);}
    if(!in.eof())throw std::runtime_error("卷积核含无法识别的字符，请使用数字、空格或逗号。");
    if(k.values.size()!=size_t(size)*size)throw std::runtime_error("卷积系数数量与所选阶数不符。请每行输入一行系数。");return k;
}

Result Process(const Image& input,Op op,const std::vector<double>& params,const Kernel& kernel,const std::atomic<bool>* cancel) {
    Result result;
    try {
        if(!input.valid())throw std::runtime_error("请先打开一张图像。");
        const auto& description=Describe(op);std::vector<double> p;
        for(size_t i=0;i<description.parameters.size();++i){const auto& spec=description.parameters[i];double value=i<params.size()?params[i]:spec.initial;
            if(!std::isfinite(value)||value<spec.minimum||value>spec.maximum||(spec.integer&&std::floor(value)!=value))throw std::runtime_error("参数超出允许范围，或整数参数含有小数。");
            if(spec.name.find(L"窗口")!=std::wstring::npos){if(int(value)%2==0)throw std::runtime_error("窗口尺寸须为奇数。");bool horizontal=spec.name.find(L"宽")!=std::wstring::npos;if(value>(horizontal?input.width:input.height))throw std::runtime_error("窗口尺寸不能超过图像对应边长。");}
            p.push_back(value);
        }
        Cancelled(cancel);
        if(op==Op::Custom)return Convolve(input,kernel,cancel);
        if(op==Op::Invert||op==Op::InvertSSE){result.image=input;if(op==Op::Invert)wkyInvertRgbImg(result.image.bgr.data(),input.width,input.height);else wkyInvertSSE(result.image.bgr.data(),input.width,input.height,true);result.details=description.name+L"；保留彩色通道。";return result;}
        auto gray=ToGray(input),out=gray;const int width=input.width,height=input.height;size_t n=gray.size();
        std::wostringstream details;details<<description.name<<L"\r\n";
        switch(op){
        case Op::Gray: break;
        case Op::Linear:wkyLinearStretch(gray.data(),width,height,p[0],p[1],out.data());break;
        case Op::Log:wkyLogTransform(out.data(),width,height);break;
        case Op::Equalize:wkyHistogramEqualize(out.data(),width,height);break;
        case Op::LocalEqualize:wkyHistogramEqualizePixel(gray.data(),width,height,int(p[1]),int(p[0]),out.data());break;
        case Op::MeanColumn:wkyAvrFilterBySumCol(gray.data(),width,height,int(p[0]),int(p[1]),out.data());break;
        case Op::MeanIntegral:{std::vector<long long> integral(n);wkyDoSumGryImg_SSE(gray.data(),width,height,integral.data());wkyAvrFilterBySumImg(integral.data(),width,height,int(p[0]),int(p[1]),out.data());break;}
        case Op::Median:wkyMedianFilter(gray.data(),width,height,int(p[0]),int(p[1]),out.data());break;
        case Op::BinaryFilter:Binary(gray,width,height,-1);wkyBinImgFilter(gray.data(),width,height,int(p[0]),int(p[1]),p[2],out.data());break;
        case Op::Gradient:wkyGradientGryImg(gray.data(),width,height,out.data());break;
        case Op::Roberts:wkyRobertsGryImg(gray.data(),width,height,out.data());break;
        case Op::Sobel:wkySobelGryImg(gray.data(),width,height,out.data());break;
        case Op::Prewitt:wkyPrewittGryImg(gray.data(),width,height,out.data());break;
        case Op::ShenJun:{std::vector<BYTE> temp(n);wkyShenJunGryImg(gray.data(),temp.data(),width,height,p[0],out.data());break;}
        case Op::Sharpen:wkyEdgeSharpen(gray.data(),width,height,p[0],out.data());break;
        case Op::Threshold:wkyThreshold(gray.data(),width,height,int(p[0]),out.data());break;
        case Op::SemiThreshold:wkySemiThreshold(out.data(),width,height,int(p[0]));break;
        case Op::Otsu:{int hist[256];wkyGetHistogram(gray.data(),width,height,hist);int threshold=wkyGetOtsuThreshold(hist,256);wkyThreshold(gray.data(),width,height,threshold,out.data());details<<L"自动阈值："<<threshold<<L"\r\n";break;}
        case Op::BlockThreshold:wkyBlockThreshold(out.data(),width,height,int(p[0]),int(p[1]));break;
        case Op::PixelThreshold:wkyPixelThreshold(gray.data(),width,height,int(p[0]),int(p[1]),out.data());break;
        case Op::Cluster:{int hist[256];wkyGetHistogram(gray.data(),width,height,hist);int classes=int(p[0]);std::vector<double> centers(classes);if(!wkyClusterHistogram(hist,classes,p[1],centers.data()))throw std::runtime_error("无法从当前图像得到聚类中心。");
            BYTE lut[256];for(int g=0;g<256;++g){int best=0;for(int c=1;c<classes;++c)if(std::abs(g-centers[c])<std::abs(g-centers[best]))best=c;lut[g]=wkyByte(centers[best]);}for(size_t i=0;i<n;++i)out[i]=lut[gray[i]];
            details<<L"聚类中心：";for(auto c:centers)details<<std::fixed<<std::setprecision(1)<<c<<L"  ";details<<L"\r\n";break;}
        case Op::Histogram:{int hist[256];wkyGetHistogram(gray.data(),width,height,hist);out.resize(768*400);wkyDrawHistogram2Img(hist,256,1,out.data(),768,400);result.image=FromGray(768,400,out);details<<L"横轴：灰度 0–255；纵轴：相对频数。";result.details=details.str();return result;}
        case Op::Histogram2D:{std::vector<BYTE> average(n);wkyAvrFilterBySumCol(gray.data(),width,height,int(p[0]),int(p[1]),average.data());std::vector<int> hist(256*256);wky2DHistogram(gray.data(),average.data(),width,height,hist.data());int peak=*std::max_element(hist.begin(),hist.end());
            Image heat;heat.width=heat.height=512;heat.bgr.resize(512*512*3);for(int y=0;y<512;++y)for(int x=0;x<512;++x){double t=std::log1p(hist[(x/2)*256+(255-y/2)])/std::log1p(std::max(1,peak));size_t i=(size_t(y)*512+x)*3;heat.bgr[i]=wkyByte(22+110*t);heat.bgr[i+1]=wkyByte(25+225*t);heat.bgr[i+2]=wkyByte(28+210*t);}
            int selected[256];int count=wkyHistogramBy2D(gray.data(),average.data(),width,height,10,selected);details<<L"横轴：灰度 0–255；纵轴：邻域均值 0–255（向上增加）。\r\n亮度使用对数频数；|灰度 − 均值| ≤ 10 的像素数："<<count;result.image=std::move(heat);result.details=details.str();return result;}
        case Op::Contour:case Op::FillContour:case Op::Measure:return Contours(input,std::move(gray),op,int(p[0]),int(p[1]),cancel);
        case Op::Hough:{wkySobelGryImg(gray.data(),width,height,out.data());std::vector<int> xs,ys;size_t candidates=0;for(auto v:out)if(v>=p[0])++candidates;
            size_t step=std::max<size_t>(1,(candidates+49999)/50000),seen=0;for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(out[y*width+x]>=p[0]&&seen++%step==0){xs.push_back(x);ys.push_back(y);}
            if(xs.empty())throw std::runtime_error("未找到足够强的边缘，请降低边缘幅值阈值。");double a,b,c;int votes=wkyHoughLine(width,height,xs.data(),ys.data(),int(xs.size()),&a,&b,&c);result.image=input;
            for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(std::abs(a*x+b*y+c)<=0.85)ColorPixel(result.image,x,y,30,80,255);
            details<<std::fixed<<std::setprecision(4)<<L"直线："<<a<<L" x + "<<b<<L" y + "<<c<<L" = 0\r\n最高票数："<<votes<<L"；参与投票边缘点："<<xs.size()<<L"（最多 50000 点，均匀抽样）。";result.details=details.str();return result;}
        default:throw std::runtime_error("不支持的处理操作。");
        }
        Cancelled(cancel);result.image=FromGray(width,height,out);
        if(!p.empty()){details<<L"参数：";for(size_t i=0;i<p.size();++i)details<<description.parameters[i].name<<L" = "<<p[i]<<L"  ";details<<L"\r\n";}
        details<<L"灰度处理；结果可继续处理或另存为 BMP。";result.details=details.str();
    } catch(const std::exception& e){result.error=ErrorText(e);result.image={};}
    return result;
}

Image DemoImage() {
    Image image;image.width=800;image.height=560;image.bgr.resize(800*560*3);
    for(int y=0;y<560;++y)for(int x=0;x<800;++x){double base=24+105.0*x/799+24.0*y/559;BYTE b=wkyByte(base+28),g=wkyByte(base),r=wkyByte(base-12);
        if(x>90&&x<330&&y>100&&y<390){b=62;g=168;r=222;}
        if((x-540)*(x-540)+(y-265)*(y-265)<135*135){b=175;g=116;r=65;}
        if(x>415&&x<665&&y>370&&y<423){b=g=r=230;}
        if(x>130&&x<285&&y>158&&y<328){b=g=r=35;}
        if(std::abs(y-(0.38*x+35))<2){b=225;g=240;r=245;}
        size_t i=(size_t(y)*800+x)*3;image.bgr[i]=b;image.bgr[i+1]=g;image.bgr[i+2]=r;
    }
    return image;
}
}
