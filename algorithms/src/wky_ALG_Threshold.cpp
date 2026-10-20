#include "wky_algorithm_support.h"
#include"wky_ALG_Threshold.h"
//直方图
void wkyGetHistogram(BYTE* pGryImg, int width, int height, int x1, int x2, int y1, int y2, int* histogram){
	BYTE* pCur;
	int x, y;
	memset(histogram, 0, sizeof(int) * 256);
	for (y = y1, pCur = pGryImg + y * width; y <= y2; y++, pCur += width){
		for (x = x1; x <= x2; x++){
			histogram[*(pCur + x)]++;
		}
	}
	return;
}

//二值化
void wkyThreshold(BYTE* pGryImg, int width, int height, int thre, BYTE* pBinImg) {
	BYTE *pGry, *pCur, * pEnd;
	for (pGry = pGryImg, pCur = pBinImg, pEnd = pBinImg + width * height; pCur < pEnd; pCur++, pGry++) {
		*pCur = (*pGry >= thre) * 255  ;
	}
	return;
}

void wkySemiThreshold(BYTE* pGryImg, int width, int height, int thre) {
	BYTE* pCur, * pEnd;
	for (pCur = pGryImg, pEnd = pGryImg + width * height; pCur < pEnd; pCur++) {
		if (*pCur > thre) {
			*pCur = 255;
		}
	}
	return;
}

void wkyThreshold(BYTE* pGryImg, int width, int height, int x1, int x2, int y1, int y2, int thre) {
    x1=std::clamp(x1,0,width-1);x2=std::clamp(x2,0,width-1);
    y1=std::clamp(y1,0,height-1);y2=std::clamp(y2,0,height-1);
    for(int y=y1;y<=y2;++y) for(int x=x1;x<=x2;++x) { int i=y*width+x;pGryImg[i]=pGryImg[i]>=thre?255:0; }
}

void wkyThreshold(BYTE* pGryImg, int width, int height, int thre) {
	BYTE* pCur, *pEnd;
	for (pCur = pGryImg, pEnd = pGryImg + width * height; pCur < pEnd; pCur++) {
		*pCur = (*pCur >= thre) * 255;
	}
	return;
}

//Otsu阈值
int wkyGetOtsuThreshold(int* histogram, int nSize) {
    long long total=0, sum=0, n1=0, s1=0;
    int low=-1,high=0;
    for(int g=0;g<nSize;++g) {total+=histogram[g];sum+=1LL*g*histogram[g];if(histogram[g]) {if(low<0)low=g;high=g;}}
    if(low<0)return 0;if(low==high)return low;
    double best=-1;int threshold=low+1;
    for(int g=low;g<high;++g) {
        n1+=histogram[g];s1+=1LL*g*histogram[g];long long n2=total-n1;
        if(!n1||!n2)continue;
        double d=double(s1)/n1-double(sum-s1)/n2;
        double score=double(n1)*n2*d*d;
        if(score>best){best=score;threshold=g+1;}
    }
    return threshold;
}

//分块阈值
void wkyBlockThreshold(BYTE* pGryImg, int width, int height, int nBlockX, int nBlockY) {
    nBlockX=std::clamp(nBlockX,1,width);nBlockY=std::clamp(nBlockY,1,height);
    for(int by=0;by<nBlockY;++by) for(int bx=0;bx<nBlockX;++bx) {
        int x1=bx*width/nBlockX,x2=(bx+1)*width/nBlockX-1,y1=by*height/nBlockY,y2=(by+1)*height/nBlockY-1;
        int histogram[256];wkyGetHistogram(pGryImg,width,height,x1,x2,y1,y2,histogram);
        int threshold=wkyGetOtsuThreshold(histogram,256);
        wkyThreshold(pGryImg,width,height,x1,x2,y1,y2,threshold);
    }
}

//逐点阈值
void wkyPixelThreshold(BYTE* pGryImg, int width, int height, int M, int N, BYTE* pResImg) {
    std::vector<long long> sum(size_t(width+1)*(height+1),0);
    for(int y=0;y<height;++y) {long long row=0;for(int x=0;x<width;++x) {row+=pGryImg[y*width+x];sum[size_t(y+1)*(width+1)+x+1]=sum[size_t(y)*(width+1)+x+1]+row;}}
    int hx=M/2,hy=N/2;
    for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
        int x1=std::max(0,x-hx),x2=std::min(width,x+hx+1),y1=std::max(0,y-hy),y2=std::min(height,y+hy+1);
        long long s=sum[size_t(y2)*(width+1)+x2]-sum[size_t(y1)*(width+1)+x2]-sum[size_t(y2)*(width+1)+x1]+sum[size_t(y1)*(width+1)+x1];
        pResImg[y*width+x]=1LL*pGryImg[y*width+x]*(x2-x1)*(y2-y1)>s?255:0;
    }
}

