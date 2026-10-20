#ifndef WKY_ALG_CONTOUR_TRACE_H
#define WKY_ALG_CONTOUR_TRACE_H
#include<windows.h>
//画轮廓
void wkyDrawContour(BYTE* pGryImg, int width, int x, int y, BYTE* pCode, int N, BYTE color);
//外接矩形
void wkyContourRect(int x0, int y0, BYTE* pCode, int N, int* x1, int* x2, int* y1, int* y2);
//周长
double wkyContourPerimeter(BYTE* pChainCode, int n);
//面积
double wkyContourArea(BYTE* pChainCode, int n);
double wkyContourPerimeterAndArea(BYTE* pChainCode, int n, double* Perimeter);
//轮廓包围的像素个数
int wkyContourPixels(BYTE* pChainCode, int n);
//像素是否被轮廓包围
bool wkyIsPixelInContour(int x0, int y0, BYTE* pChainCode, int n, int x, int y);
//图像边框清零
void wkySetImageBoundary(BYTE* pBinImg, int width, int height, BYTE color);
//轮廓跟踪算法
int wkyTraceContour(BYTE* pBinImg, int width, int height, int x0, int y0, bool isOuter, BYTE* pChainCode, int maxChainCodeNum);
#endif
