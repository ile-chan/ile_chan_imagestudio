#ifndef WKY_ALG_HISTOGRAM_H
#define WKY_ALG_HISTOGRAM_H
#include<windows.h>
//直方图及其计算
void wkyGetHistogram(BYTE* pGryImg, int width, int height, int* histogram);
void wkyMinMaxGry(int* histogram, int* minGry, int* maxGry);
void wkyBrightContrast(int* histogram, double* bright, double* contrast);
//直方图均衡化
void wkyHistogramEqualize(BYTE* pGryImg, int width, int height);
//对数变换
void wkyLogTransform(BYTE* pGryImg, int width, int height);
//逐点直方图均衡化
void wkyHistogramEqualizePixel(BYTE* pGryImg, int width, int height, int N, int M, BYTE* pResImg);
//
//14Bit直方图均衡化
void wkyHistogramEqualize(short int* pGry14BitImg, int width, int height, BYTE* pResImg);
#endif
