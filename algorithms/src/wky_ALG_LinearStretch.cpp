#include "wky_algorithm_support.h"
#include"wky_ALG_LinearStretch.h"
void wkyLinearStretch(BYTE* pGryImg, int width, int height, double k, double b, BYTE* pResImg) {
    BYTE lut[256];
    for (int g = 0; g < 256; ++g) lut[g] = wkyByte(k * g + b);
    for (int i = 0; i < width * height; ++i) pResImg[i] = lut[pGryImg[i]];
}

