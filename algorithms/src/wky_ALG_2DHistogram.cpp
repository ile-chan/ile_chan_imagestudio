#include "wky_algorithm_support.h"
#include"wky_ALG_2DHistogram.h"
//画直方图
void wkyDrawHistogram2Img(int* histogram, int histSize, double maxScale, BYTE* pGryImg, int width, int height);
//二维直方图
void wky2DHistogram(BYTE* pGryImg, BYTE* pAvrImg, int width, int height, int* histogram2D) {
	int i;
	memset(histogram2D, 0, sizeof(int) * 256 * 256);
	for (i = 0; i < width * height; i++) {
		histogram2D[pGryImg[i] * 256 + pAvrImg[i]]++;
	}
	return;
}

int wkyHistogramBy2D(BYTE* pGryImg, BYTE* pAvrImg, int width, int height, int dist, int* histogram){
	int g, u;
	int num = 0;
	memset(histogram, 0, sizeof(int) * 256);
	for (int i = 0; i < width * height; i++){
		u = pAvrImg[i];
		g = pGryImg[i];
		if (abs(g - u) <= dist){
			histogram[g]++;
			num++;
		}
	}
	return num;
}

void wkyDrawHistogram2Img(int* histogram,int histSize,double maxScale,BYTE* pGryImg,int width,int height) {
    std::fill_n(pGryImg,size_t(width)*height,BYTE(255));
    int peak=0;for(int i=0;i<histSize;++i)peak=std::max(peak,histogram[i]);if(!peak)return;
    for(int x=0;x<width;++x) {int g=std::min(histSize-1,x*histSize/width);int bar=std::clamp(int(histogram[g]*double(height-1)/peak*maxScale),0,height-1);for(int y=height-1;y>=height-1-bar;--y)pGryImg[y*width+x]=32;}
}

