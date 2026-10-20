#ifndef WKY_ALG_THRESHOLD_H
#define WKY_ALG_THRESHOLD_H
#include<windows.h>
//直方图
void wkyGetHistogram(BYTE* pGryImg, int width, int height, int x1, int x2, int y1, int y2, int* histogram);
//二值化
void wkyThreshold(BYTE* pGryImg, int width, int height, int thre);
void wkyThreshold(BYTE* pGryImg, int width, int height, int thre, BYTE* pBinImg);
void wkySemiThreshold(BYTE* pGryImg, int width, int height, int thre);
void wkyThreshold(BYTE* pGryImg, int width, int height, int x1, int x2, int y1, int y2, int thre);
//Otsu阈值
int wkyGetOtsuThreshold(int* histogram, int nSize);
//分块阈值
void wkyBlockThreshold(BYTE* pGryImg, int width, int height, int nBlockX, int nBlockY);
//逐点阈值
void wkyPixelThreshold(BYTE* pGryImg, int width, int height, int M, int N, BYTE* pResImg);
#endif
