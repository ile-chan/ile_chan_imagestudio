#include "wky_algorithm_support.h"
#include"wky_ALG_BinImgFilter.h"
void wkyBinImgFilter(BYTE* pBinImg, int width, int height, int M, int N, double threshold, BYTE* pResImg) {
    wkyBoxFilter(pBinImg, width, height, M, N, pResImg, std::clamp(threshold, 0.0, 1.0));
}

