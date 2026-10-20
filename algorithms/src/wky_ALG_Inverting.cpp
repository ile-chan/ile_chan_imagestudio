#include "wky_algorithm_support.h"
# include"wky_ALG_Inverting.h"
#include <nmmintrin.h>
void wkyInvertGryImg(BYTE* pGryImg, int width, int height) {
	BYTE* pCur = pGryImg, * pEnd = pGryImg + width * height;
	for (; pCur < pEnd; pCur++) {
		*pCur = 255 - *pCur;
	}
	return;
}

void wkyInvertRgbImg(BYTE* pRgbImg, int width, int height) {
	BYTE* pCur = pRgbImg, *pEnd = pRgbImg + width * height * 3;
	for (; pCur < pEnd; pCur++) {
		*pCur = 255 - *pCur;
	}
	return;
}

void wkyInvertSSE(BYTE* pImg, int width, int height, bool isRgb) {
    const size_t n = size_t(width) * height * (isRgb ? 3 : 1);
    const __m128i white = _mm_set1_epi8(char(255));
    size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        auto value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pImg + i));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(pImg + i), _mm_sub_epi8(white, value));
    }
    for (; i < n; ++i) pImg[i] = 255 - pImg[i];
}

