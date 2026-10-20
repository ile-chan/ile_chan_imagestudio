#include "wky_algorithm_support.h"
#include<nmmintrin.h>
#include"wky_ALG_AverageFilter.h"
//基于列积分图的均值滤波
void wkyAvrFilterBySumCol(BYTE* pGryImg, int width, int height, int M, int N, BYTE* pResImg) {
    // sumCol初始化
    // 滤波，外层循环自上而下
    // 每行sum初值
    // 求一行
    // 换行
    wkyBoxFilter(pGryImg, width, height, M, N, pResImg, -1);
}

//基于列积分的积分图实现
void wkyDoSumGryImg(BYTE* pGryImg, int width, int height, long long* pSumImg) {
    std::vector<long long> columns(width, 0);
    for (int y = 0; y < height; ++y) {
        long long row = 0;
        for (int x = 0; x < width; ++x) { columns[x] += pGryImg[y * width + x]; row += columns[x]; pSumImg[y * width + x] = row; }
    }
}

//基于SSE的积分图实现
void wkyDoSumGryImg_SSE_demo(BYTE* pGryImg, int width, int height, long long* pSumImg) {
    std::vector<long long> columns(width, 0);
    for (int y = 0; y < height; ++y) {
        int x = 0;
        for (; x + 1 < width; x += 2) {
            auto value = _mm_set_epi64x(pGryImg[y * width + x + 1], pGryImg[y * width + x]);
            auto sums = _mm_loadu_si128(reinterpret_cast<const __m128i*>(columns.data() + x));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(columns.data() + x), _mm_add_epi64(sums, value));
        }
        for (; x < width; ++x) columns[x] += pGryImg[y * width + x];
        long long row = 0;
        for (x = 0; x < width; ++x) { row += columns[x]; pSumImg[y * width + x] = row; }
    }
}

void wkyDoSumGryImg_SSE(BYTE* pGryImg, int width, int height, long long* pSumImg) {
    //约定图像宽度不大于4096,16字节对齐
    wkyDoSumGryImg_SSE_demo(pGryImg, width, height, pSumImg);
}

//基于积分图的快速均值滤波
void wkyAvrFilterBySumImg(long long* pSumImg, int width, int height, int M, int N, BYTE* pResImg) {
    M = M / 2 * 2 + 1; N = N / 2 * 2 + 1;
    const int hx = M / 2, hy = N / 2;
    auto at = [&](int x, int y) -> long long { return x < 0 || y < 0 ? 0 : pSumImg[y * width + x]; };
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        if (x < hx || y < hy || x + hx >= width || y + hy >= height)
            pResImg[y * width + x] = BYTE(at(x,y) - at(x-1,y) - at(x,y-1) + at(x-1,y-1));
        else {
            const auto sum = at(x+hx,y+hy) - at(x-hx-1,y+hy) - at(x+hx,y-hy-1) + at(x-hx-1,y-hy-1);
            pResImg[y * width + x] = wkyByte(double(sum) / (M * N));
        }
    }
}

