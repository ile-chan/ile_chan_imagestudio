#include "wky_algorithm_support.h"
#include"wky_ALG_Histogram.h"
#include<math.h>
//直方图及其计算
void wkyGetHistogram(BYTE* pGryImg, int width, int height, int* histogram) {
	BYTE* pCur = pGryImg, * pEnd = pGryImg + width * height;
	memset(histogram, 0, sizeof(int) * 256);
	for (; pCur < pEnd; pCur++) {
		histogram[*pCur]++;
	}
	return;
}

void wkyMinMaxGry(int* histogram, int* minGry, int* maxGry) {
    *minGry = *maxGry = 0;
    for (int g = 0; g < 256; ++g) if (histogram[g]) { *minGry = g; break; }
    for (int g = 255; g >= 0; --g) if (histogram[g]) { *maxGry = g; break; }
}

void wkyBrightContrast(int* histogram, double* bright, double* contrast) {
    double count = 0, sum = 0, variance = 0;
    for (int g = 0; g < 256; ++g) { count += histogram[g]; sum += double(g) * histogram[g]; }
    *bright = count ? sum / count : 0;
    for (int g = 0; g < 256; ++g) variance += histogram[g] * (g - *bright) * (g - *bright);
    *contrast = count ? std::sqrt(variance / count) : 0;
}

//直方图均衡化
void wkyHistogramEqualize(BYTE* pGryImg, int width, int height) {
    int histogram[256]; wkyGetHistogram(pGryImg, width, height, histogram);
    const int count = width * height;
    int first = 0; while (first < 256 && !histogram[first]) ++first;
    if (first == 256 || histogram[first] == count) return;
    int lut[256] = {}, cumulative = 0, base = histogram[first];
    for (int g = 0; g < 256; ++g) {
        cumulative += histogram[g];
        lut[g] = wkyByte(255.0 * (cumulative - base) / (count - base));
    }
    for (int i = 0; i < count; ++i) pGryImg[i] = BYTE(lut[pGryImg[i]]);
}

//对数变换
void wkyLogTransform(BYTE* pGryImg, int width, int height) {
    int histogram[256], low, high; wkyGetHistogram(pGryImg, width, height, histogram);
    wkyMinMaxGry(histogram, &low, &high);
    if (high == 0) return;
    BYTE lut[256]; const double scale = 255.0 / std::log(1.0 + high);
    for (int g = 0; g < 256; ++g) lut[g] = wkyByte(scale * std::log(1.0 + g));
    for (int i = 0; i < width * height; ++i) pGryImg[i] = lut[pGryImg[i]];
}

//逐点直方图均衡化
void wkyHistogramEqualizePixel(BYTE* pGryImg, int width, int height, int N, int M, BYTE* pResImg);
//
//14Bit直方图均衡化
void wkyHistogramEqualize(short int* pGry14BitImg, int width, int height, BYTE* pResImg) {
    constexpr int levels = 1 << 14;
    std::vector<int> histogram(levels, 0), lut(levels, 0);
    const int count = width * height;
    for (int i = 0; i < count; ++i) ++histogram[std::clamp<int>(pGry14BitImg[i], 0, levels - 1)];
    int first = 0; while (first < levels && !histogram[first]) ++first;
    int cumulative = 0, base = first < levels ? histogram[first] : 0;
    for (int g = 0; g < levels; ++g) {
        cumulative += histogram[g];
        lut[g] = count == base ? wkyByte(g * 255.0 / (levels - 1)) : wkyByte(255.0 * (cumulative - base) / (count - base));
    }
    for (int i = 0; i < count; ++i) pResImg[i] = BYTE(lut[std::clamp<int>(pGry14BitImg[i], 0, levels - 1)]);
}

void wkyHistogramEqualizePixel(BYTE* pGryImg, int width, int height, int N, int M, BYTE* pResImg) {
    std::copy_n(pGryImg, size_t(width) * height, pResImg);
    M = M / 2 * 2 + 1; N = N / 2 * 2 + 1;
    const int hx = M / 2, hy = N / 2, total = M * N;
    for (int y = hy; y < height - hy; ++y) {
        int hist[256] = {};
        for (int yy = y - hy; yy <= y + hy; ++yy)
            for (int xx = 0; xx < M && xx < width; ++xx) ++hist[pGryImg[yy * width + xx]];
        for (int x = hx; x < width - hx; ++x) {
            int first = 0, cumulative = 0;
            while (first < 256 && !hist[first]) ++first;
            for (int g = 0; g <= pGryImg[y * width + x]; ++g) cumulative += hist[g];
            if (first < 256 && total != hist[first])
                pResImg[y * width + x] = wkyByte(255.0 * (cumulative - hist[first]) / (total - hist[first]));
            if (x + hx + 1 < width)
                for (int yy = y - hy; yy <= y + hy; ++yy) { --hist[pGryImg[yy * width + x - hx]]; ++hist[pGryImg[yy * width + x + hx + 1]]; }
        }
    }
}

