#include "wky_algorithm_support.h"
#include"wky_ALG_MedianFilter.h"
void wkyMedianFilter(BYTE* pGryImg, int width, int height, int M, int N, BYTE* pResImg) {
    // 初始化直方图
    // 第一个中值
    std::copy_n(pGryImg, size_t(width) * height, pResImg);
    M = M / 2 * 2 + 1; N = N / 2 * 2 + 1;
    const int hx = M / 2, hy = N / 2, target = M * N / 2;
    if (M > width || N > height) return;
    for (int y = hy; y < height - hy; ++y) {
        int hist[256] = {};
        for (int yy = y - hy; yy <= y + hy; ++yy)
            for (int xx = 0; xx < M; ++xx) ++hist[pGryImg[yy * width + xx]];
        for (int x = hx; x < width - hx; ++x) {
            int cumulative = 0, median = 0;
            for (; median < 255; ++median) { cumulative += hist[median]; if (cumulative > target) break; }
            pResImg[y * width + x] = BYTE(median);
            if (x + hx + 1 < width)
                for (int yy = y - hy; yy <= y + hy; ++yy) { --hist[pGryImg[yy * width + x - hx]]; ++hist[pGryImg[yy * width + x + hx + 1]]; }
        }
    }
}

