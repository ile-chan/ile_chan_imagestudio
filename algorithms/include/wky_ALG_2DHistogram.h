#ifndef WKY_ALG_2D_HISTOGRAM_H
#define WKY_ALG_2D_HISTOGRAM_H
#include<windows.h>
//二值化
void wkyThreshold(BYTE* pGryImg, int width, int height, int thre);
//Otsu阈值
int wkyGetOtsuThreshold(int* histogram, int nSize);
//画直方图
void wkyDrawHistogram2Img(int* histogram, int histSize, double maxScale, BYTE* pGryImg, int width, int height);
//二维直方图
void wky2DHistogram(BYTE* pGryImg, BYTE* pAvrImg, int width, int height, int* histogram2D);
int wkyHistogramBy2D(BYTE* pGryImg, BYTE* pAvrImg, int width, int height, int dist, int* histogram);
#endif
