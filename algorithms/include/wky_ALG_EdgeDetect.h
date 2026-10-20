#ifndef WKY_ALG_EDGE_DETECT_H
#define WKY_ALG_EDGE_DETECT_H
#include<windows.h>
//Gradient算子
void wkyGradientGryImg(BYTE* pGryImg, int width, int height, BYTE* pGrdImg);
//Robert算子
void wkyRobertsGryImg(BYTE* pGryImg, int width, int height, BYTE* pRbtImg);
//Sobel算子
void wkySobelGryImg(BYTE* pGryImg, int width, int height, BYTE* pSbImg);
//Prewitt算子
void wkyPrewittGryImg(BYTE* pGryImg, int width, int height, BYTE* pPRTImg);
//沈俊算子
void wkyShenJunGryImg(BYTE* pGryImg, BYTE* pTmpImg, int width, int height, double a0, BYTE* pSJImg);
#endif
