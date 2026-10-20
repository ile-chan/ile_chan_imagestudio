#include "wky_algorithm_support.h"
#include"wky_ALG_ContourTrace.h"
#include<math.h>
//画轮廓
void wkyDrawContour(BYTE* pGryImg, int width, int x, int y, BYTE* pCode, int N, BYTE color) {
	static int dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
	static int dy[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
	int i, pAdd[8];
	BYTE* pCur;
	for (i = 0; i < 8; i++) {
		pAdd[i] = dy[i] * width + dx[i];
	}
	*(pGryImg + width * y + x) = color;
	for (pCur = pGryImg + y * width + x, i = 0; i < N; i++) {
		*pCur = color;
		pCur += pAdd[pCode[i]];
	}
	return;
}

//外接矩形
void wkyContourRect(int x0, int y0, BYTE* pCode, int N, int* x1, int* x2, int* y1, int* y2) {
	static int dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
	static int dy[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
	int i;
	for (i = 0, *x1 = x0, *x2 = x0, *y1 = y0, *y2 = y0; i < N; i++) {
		x0 += dx[pCode[i]];
		y0 += dy[pCode[i]];
		if (x0 > *x2) {
			*x2 = x0;
		}
		else if (x0 < *x1) {
			*x1 = x0;
		}
		if (y0 > *y2) {
			*y2 = y0;
		}
		else if (y0 < *y1) {
			*y1 = y0;
		}
	}
	return;
}

//周长
double wkyContourPerimeter(BYTE* pChainCode, int n) {
	int i, ne, no;
	for (i = 0, no = 0, ne = 0; i < n; i++) {
		if (pChainCode[i] % 2) {
			no++;
		}
		else {
			ne++;
		}
	}
	return no * sqrt(2.0) + ne;
}

//面积
double wkyContourArea(BYTE* pChainCode, int n) {
	static int dx[8] = { 1, 1, 0,-1,-1,-1, 0, 1 };
	static int dy[8] = { 0,-1,-1,-1, 0, 1, 1, 1 };
	int i, yi, ci;
	double S;
	for (i = 0, S = 0, yi = 0; i < n; i++)
	{
		ci = pChainCode[i];
		S += (yi + dy[ci] / 2.0) * dx[ci];
		yi += dy[ci];
	}
	return fabs(S);
}

double wkyContourPerimeterAndArea(BYTE* pChainCode, int n, double* Perimeter) {
    *Perimeter=wkyContourPerimeter(pChainCode,n);
    return wkyContourArea(pChainCode,n);
}

//轮廓包围的像素个数
int wkyContourPixels(BYTE* pChainCode, int n) {
    if(n<=0)return n==0?1:0;
    return int(std::lround(wkyContourArea(pChainCode,n)+n/2.0+1));
}

//像素是否被轮廓包围
bool wkyIsPixelInContour(int x0, int y0, BYTE* pChainCode, int n, int x, int y) {
    static int dx[8]={1,1,0,-1,-1,-1,0,1},dy[8]={0,-1,-1,-1,0,1,1,1};
    bool inside=false;int ax=x0,ay=y0;
    for(int i=0;i<n;++i) {int d=pChainCode[i]&7,bx=ax+dx[d],by=ay+dy[d];
        long long cross=1LL*(x-ax)*(by-ay)-1LL*(y-ay)*(bx-ax);
        if(cross==0&&x>=std::min(ax,bx)&&x<=std::max(ax,bx)&&y>=std::min(ay,by)&&y<=std::max(ay,by))return true;
        if((ay>y)!=(by>y)) {double edge=ax+double(y-ay)*(bx-ax)/(by-ay);if(x<edge)inside=!inside;}
        ax=bx;ay=by;
    }
    return n==0?(x==x0&&y==y0):inside;
}

//图像边框清零
void wkySetImageBoundary(BYTE* pBinImg, int width, int height, BYTE color) {
    if(width<=0||height<=0)return;
    std::fill_n(pBinImg,width,color);std::fill_n(pBinImg+size_t(height-1)*width,width,color);
    for(int y=0;y<height;++y){pBinImg[y*width]=color;pBinImg[y*width+width-1]=color;}
}

//轮廓跟踪算法
int wkyTraceContour(BYTE* pBinImg, int width, int height, int x0, int y0,
	bool isOuter, BYTE* pChainCode, int maxChainCodeNum) {
    static int dx[8]={1,1,0,-1,-1,-1,0,1},dy[8]={0,-1,-1,-1,0,1,1,1};
    if(x0<0||y0<0||x0>=width||y0>=height||!pBinImg[y0*width+x0]||maxChainCodeNum<=0)return 0;
    int x=x0,y=y0,bx=x0+(isOuter?-1:1),by=y0,firstX=0,firstY=0,n=0;
    for(;;) {
        int back=4;for(int k=0;k<8;++k)if(x+dx[k]==bx&&y+dy[k]==by){back=k;break;}
        int chosen=-1;
        for(int k=1;k<=8;++k){int d=(back+k)&7,xx=x+dx[d],yy=y+dy[d];if(xx>=0&&yy>=0&&xx<width&&yy<height&&pBinImg[yy*width+xx]){chosen=d;break;}}
        if(chosen<0)return 0;
        int nx=x+dx[chosen],ny=y+dy[chosen];
        if(n>0&&x==x0&&y==y0&&nx==firstX&&ny==firstY)return n;
        if(n==maxChainCodeNum)return -1;
        if(n==0){firstX=nx;firstY=ny;}
        pChainCode[n++]=BYTE(chosen);
        int before=(chosen+7)&7;bx=x+dx[before];by=y+dy[before];x=nx;y=ny;
    }
}

