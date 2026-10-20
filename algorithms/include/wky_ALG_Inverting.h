# ifndef WKY_ALG_INVERTING_H
# define WKY_ALG_INVERTING_H
#include <windows.h>
void wkyInvertGryImg(BYTE* pGryImg, int width, int height);
void wkyInvertRgbImg(BYTE* pRgbImg, int width, int height);
void wkyInvertSSE(BYTE* pImg, int width, int height, bool isRgb = false);
# endif
