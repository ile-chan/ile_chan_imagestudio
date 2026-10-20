#include "wky_algorithm_support.h"
#include"wky_ALG_ContourFill.h"
void wkyFillContour(BYTE* pGryImg, int width, int height, int x0, int y0, bool isOuter, BYTE* pCode, int N, int regionC, int contourC, int nonC) {
    //地址偏移量
    //轮廓起点,当前点,填充点
    // step.1-----初始化----------------------------------------//
    //轮廓起点的地址
    //不同链码对应的地址偏移量
    // step.2-----轮廓点的所有链码纵坐标变化量之和赋初值----------//
    //求sumC
    // step.3-----计算轮廓点的所有链码纵坐标变化量之和------------//
    //进入轮廓起点的链码
    //从该轮廓点出发的链码
    //像素的灰度值就是变化量之和 
    //当前轮廓点的出发码就是下一个轮廓点的进入码
    //指向下一个轮廓点的地址
    // step.4-----对填充起点和填充终点进行标记--------------------//
    //标记为填充起点
    //标记为填充终点
    // step.5-----按行在填充起点和填充终点之间进行填充------------//
    //外轮廓是从左向右,所以是+1;内轮廓反之
    //找到一个填充起点
    //一直填充到终点
    //该水平段已经填充过了,避免重复填充
    // step.6-----对轮廓点的颜色进行赋值-------------------------//
    // step.7-----结束-----------------------------------------//
    static int dx[8]={1,1,0,-1,-1,-1,0,1},dy[8]={0,-1,-1,-1,0,1,1,1};
    std::vector<POINT> vertices;vertices.push_back({x0,y0});
    int x=x0,y=y0,minY=y0,maxY=y0;
    for(int i=0;i<N;++i){x+=dx[pCode[i]&7];y+=dy[pCode[i]&7];vertices.push_back({x,y});minY=std::min(minY,y);maxY=std::max(maxY,y);}
    for(int yy=std::max(0,minY);yy<=std::min(height-1,maxY);++yy){
        std::vector<double> intersections;
        for(size_t i=1;i<vertices.size();++i){auto a=vertices[i-1],b=vertices[i];if((a.y>yy)!=(b.y>yy))intersections.push_back(a.x+double(yy-a.y)*(b.x-a.x)/(b.y-a.y));}
        std::sort(intersections.begin(),intersections.end());
        for(size_t i=0;i+1<intersections.size();i+=2)for(int xx=std::max(0,int(std::ceil(intersections[i])));xx<=std::min(width-1,int(std::floor(intersections[i+1])));++xx)pGryImg[yy*width+xx]=BYTE(regionC);
    }
    for(auto p:vertices)if(p.x>=0&&p.y>=0&&p.x<width&&p.y<height)pGryImg[p.y*width+p.x]=BYTE(contourC);
    (void)isOuter;(void)nonC;
}

