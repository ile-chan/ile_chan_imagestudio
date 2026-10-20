#ifndef WKY_ALG_CLUSTER_H
#define WKY_ALG_CLUSTER_H
#include<windows.h>
//直方图
void wkyGetHistogram(BYTE* pGryImg, int width, int height, int* histogram);
// 聚类分割
bool wkyClusterHistogram(int* histogram, int nClass, double decision, double* center);
#endif
