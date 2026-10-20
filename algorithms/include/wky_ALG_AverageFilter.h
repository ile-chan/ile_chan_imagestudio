#ifndef WKY_ALG_AVERAGE_FILTER_H
#define WKY_ALG_AVERAGE_FILTER_H
#include<windows.h>
//基于列积分的快速均值滤波
void wkyAvrFilterBySumCol(BYTE* pGryImg,  //原始灰度图像
	int width, int height,//图像的宽度和高度
	int M, int N, //滤波邻域：M列N行
	BYTE* pResImg //结果图像
);
//基于列积分的积分图实现
void wkyDoSumGryImg(BYTE* pGryImg,  //原始灰度图像
	int width, //图像的宽度 
	int height,//图像的高度
	long long* pSumImg //计算得到的积分图
);
//基于SSE的积分图实现
void wkyDoSumGryImg_SSE_demo(BYTE* pGryImg,  //原始灰度图像
	int width, //图像的宽度,必须是4的倍数
	int height,//图像的高度
	long long* pSumImg //计算得到的积分图
);
void wkyDoSumGryImg_SSE(BYTE* pGryImg,  //原始灰度图像
	int width, //图像的宽度,必须是4的倍数
	int height,//图像的高度
	long long* pSumImg //计算得到的积分图
);
//基于积分图的快速均值滤波
void wkyAvrFilterBySumImg(long long* pSumImg, //计算得到的积分图
	int width, int height,//图像的宽度和高度
	int M, int N, //滤波邻域：M列N行
	BYTE* pResImg //结果图像
);
#endif
