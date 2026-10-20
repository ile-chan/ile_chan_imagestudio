#ifndef WKY_ALG_CONTOURFILL_H
#define WKY_ALG_CONTOURFILL_H
#include<windows.h>
void wkyFillContour(BYTE* pGryImg, int width, int height, int x0, int y0, bool isOuter, BYTE* pCode, int N, int regionC, int contourC, int nonC);
#endif
