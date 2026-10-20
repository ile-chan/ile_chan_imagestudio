#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
inline BYTE wkyByte(double value) {
    if (!std::isfinite(value)) return 0;
    return BYTE(std::clamp(std::lround(value), 0L, 255L));
}

inline FILE* wkyFopenUtf8(const char* path, const char* mode) {
    auto wide = [](const char* s) {
        int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0);
        if (!n) return std::wstring();
        std::wstring result(n, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, result.data(), n);
        result.pop_back(); return result;
    };
    auto p = wide(path), m = wide(mode);
    return p.empty() || m.empty() ? nullptr : _wfopen(p.c_str(), m.c_str());
}

inline void wkyBoxFilter(BYTE* src, int width, int height, int M, int N, BYTE* out, double binaryRatio) {
    std::copy_n(src, size_t(width) * height, out);
    M = M / 2 * 2 + 1; N = N / 2 * 2 + 1;
    if (M > width || N > height || M <= 0 || N <= 0) return;
    int hx = M / 2, hy = N / 2;
    std::vector<long long> columns(width, 0);
    for (int y = 0; y < N; ++y) for (int x = 0; x < width; ++x) columns[x] += src[y * width + x];
    for (int y = hy; y < height - hy; ++y) {
        long long sum = 0;
        for (int x = 0; x < M; ++x) sum += columns[x];
        for (int x = hx; x < width - hx; ++x) {
            out[y * width + x] = binaryRatio < 0 ? wkyByte(double(sum) / (M * N)) : BYTE(sum >= binaryRatio * 255 * M * N ? 255 : 0);
            if (x + hx + 1 < width) sum += columns[x + hx + 1] - columns[x - hx];
        }
        if (y + hy + 1 < height) for (int x = 0; x < width; ++x) columns[x] += src[(y + hy + 1) * width + x] - src[(y - hy) * width + x];
    }
}

