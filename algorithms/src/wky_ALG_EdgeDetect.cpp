#include "wky_algorithm_support.h"
#include"wky_ALG_EdgeDetect.h"
#include<math.h>
//Gradient算子
void wkyGradientGryImg(BYTE* pGryImg, int width, int height, BYTE* pGrdImg) {
    std::fill_n(pGrdImg, size_t(width) * height, BYTE(0));
    for (int y = 0; y + 1 < height; ++y) for (int x = 0; x + 1 < width; ++x) {
        const int i = y * width + x, dx = pGryImg[i+1] - pGryImg[i], dy = pGryImg[i+width] - pGryImg[i];
        pGrdImg[i] = wkyByte(std::hypot(double(dx), double(dy)));
    }
}

//Robert算子
void wkyRobertsGryImg(BYTE* pGryImg, int width, int height, BYTE* pRbtImg) {
    std::fill_n(pRbtImg, size_t(width) * height, BYTE(0));
    for (int y = 0; y + 1 < height; ++y) for (int x = 0; x + 1 < width; ++x) {
        const int i = y * width + x;
        pRbtImg[i] = BYTE(std::max(std::abs(pGryImg[i]-pGryImg[i+width+1]), std::abs(pGryImg[i+1]-pGryImg[i+width])));
    }
}

//Sobel算子
void wkySobelGryImg(BYTE* pGryImg, int width, int height, BYTE* pSbImg) {
    std::fill_n(pSbImg, size_t(width) * height, BYTE(0));
    for (int y = 1; y + 1 < height; ++y) for (int x = 1; x + 1 < width; ++x) {
        const int i = y * width + x;
        const int dx = -pGryImg[i-width-1]-2*pGryImg[i-1]-pGryImg[i+width-1]+pGryImg[i-width+1]+2*pGryImg[i+1]+pGryImg[i+width+1];
        const int dy = -pGryImg[i-width-1]-2*pGryImg[i-width]-pGryImg[i-width+1]+pGryImg[i+width-1]+2*pGryImg[i+width]+pGryImg[i+width+1];
        pSbImg[i] = wkyByte(std::abs(dx) + std::abs(dy));
    }
}

//Prewitt算子
void wkyPrewittGryImg(BYTE* pGryImg, int width, int height, BYTE* pPRTImg) {
    std::fill_n(pPRTImg, size_t(width) * height, BYTE(0));
    for (int y = 1; y + 1 < height; ++y) for (int x = 1; x + 1 < width; ++x) {
        const int i = y * width + x;
        int a=pGryImg[i-width-1], b=pGryImg[i-width], c=pGryImg[i-width+1], d=pGryImg[i-1], f=pGryImg[i+1], g=pGryImg[i+width-1], h=pGryImg[i+width], j=pGryImg[i+width+1];
        int dx=a+d+g-c-f-j, dy=a+b+c-g-h-j, d45=a+b+d-f-h-j, d135=b+c+f-d-g-h;
        pPRTImg[i] = wkyByte(std::max({std::abs(dx),std::abs(dy),std::abs(d45),std::abs(d135)}));
    }
}

//沈俊算子
void wkyShenJunGryImg(BYTE* pGryImg, BYTE* pTmpImg, int width, int height, double a0, BYTE* pSJImg) {
    a0 = std::clamp(a0, 0.01, 0.99);
    int lut[511]; int* alut = lut + 255;
    for (int d = -255; d <= 255; ++d) alut[d] = int(std::lround(d * a0));
    std::copy_n(pGryImg, size_t(width) * height, pTmpImg);
    for (int y = 0; y < height; ++y) {
        int pre = pTmpImg[y*width];
        for (int x=1;x<width;++x) { int i=y*width+x; pTmpImg[i]=BYTE(pre=pre+alut[pTmpImg[i]-pre]); }
        pre = pTmpImg[y*width+width-1];
        for (int x=width-2;x>=0;--x) { int i=y*width+x; pTmpImg[i]=BYTE(pre=pre+alut[pTmpImg[i]-pre]); }
    }
    for (int x = 0; x < width; ++x) {
        int pre=pTmpImg[x];
        for(int y=1;y<height;++y) {int i=y*width+x;pTmpImg[i]=BYTE(pre=pre+alut[pTmpImg[i]-pre]);}
        pre=pTmpImg[(height-1)*width+x];
        for(int y=height-2;y>=0;--y) {int i=y*width+x;pTmpImg[i]=BYTE(pre=pre+alut[pTmpImg[i]-pre]);}
    }
    for(int i=0;i<width*height;++i) pTmpImg[i]=BYTE(pTmpImg[i]>pGryImg[i]);
    std::fill_n(pSJImg, size_t(width)*height, BYTE(0));
    for(int y=1;y+1<height;++y) for(int x=1;x+1<width;++x) {
        int i=y*width+x;
        if(pTmpImg[i] && (!pTmpImg[i-1] || !pTmpImg[i+1] || !pTmpImg[i-width] || !pTmpImg[i+width])) pSJImg[i]=255;
    }
}

