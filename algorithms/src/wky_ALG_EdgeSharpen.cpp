#include "wky_algorithm_support.h"
#include"wky_ALG_EdgeSharpen.h"
void wkyEdgeSharpen(BYTE* pGryImg, int width, int height, double sharpenFactor, BYTE* pResImg) {
    std::copy_n(pGryImg, size_t(width)*height, pResImg);
    for(int y=1;y+1<height;++y) for(int x=1;x+1<width;++x) {
        int i=y*width+x;
        int delta=8*pGryImg[i]-pGryImg[i-width-1]-pGryImg[i-width]-pGryImg[i-width+1]-pGryImg[i-1]-pGryImg[i+1]-pGryImg[i+width-1]-pGryImg[i+width]-pGryImg[i+width+1];
        pResImg[i]=wkyByte(pGryImg[i]+sharpenFactor*delta);
    }
}

