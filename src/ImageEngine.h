#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <atomic>
#include <exception>
#include <string>
#include <vector>

namespace wky {
constexpr size_t MaxPixels = 20000000;
struct Image {
    int width = 0, height = 0;
    std::vector<BYTE> bgr;
    bool valid() const { return width > 0 && height > 0 && bgr.size() == size_t(width) * height * 3; }
    bool gray() const;
};
enum class Op {
    Gray, Invert, InvertSSE, Linear, Log, Equalize, LocalEqualize,
    MeanColumn, MeanIntegral, Median, BinaryFilter,
    Gradient, Roberts, Sobel, Prewitt, ShenJun, Sharpen,
    Threshold, SemiThreshold, Otsu, BlockThreshold, PixelThreshold, Cluster,
    Histogram, Histogram2D, Contour, FillContour, Measure, Hough, Custom
};
struct Parameter { std::wstring name; double initial = 0, minimum = 0, maximum = 0; bool integer = false; };
struct Operation {
    Op id; int category; std::wstring name, explanation;
    std::vector<Parameter> parameters;
};
struct Kernel {
    int size = 3, border = 0;
    std::vector<double> values {0,0,0,0,1,0,0,0,0};
    double divisor = 1, bias = 0;
    bool normalize = true, flip = true, color = true;
};
struct Result { Image image; std::wstring details, error; };
struct Statistics { std::array<int,256> histogram{}; int low = 0, high = 0; double mean = 0, contrast = 0; };

const std::vector<Operation>& Operations();
const Operation& Describe(Op id);
Statistics Analyze(const Image& image);
std::vector<BYTE> ToGray(const Image& image);
Image FromGray(int width, int height, const std::vector<BYTE>& data);
Image LoadImage(const std::wstring& path, std::wstring* route = nullptr);
void SaveBmp(const Image& image, const std::wstring& path);
Result Process(const Image& input, Op op, const std::vector<double>& params = {}, const Kernel& kernel = {}, const std::atomic<bool>* cancel = nullptr);
Kernel ParseKernel(const std::wstring& text, int size);
std::wstring ErrorText(const std::exception& error);
Image DemoImage();
int RunSelfTests(const std::wstring& outputDirectory);
}
