#pragma once

#include "linear/Types.h"

#include <cstddef>
#include <cstdint>

namespace linear {

enum class PixelFormat { Rgba8, Rgba16, Rgba32Float, Bgra8 };

struct ConstPixelBuffer {
    const void* data{};
    int width{};
    int height{};
    std::ptrdiff_t rowBytes{};
    PixelFormat format{PixelFormat::Rgba8};
    bool premultiplied{true};
};

struct MutablePixelBuffer {
    void* data{};
    int width{};
    int height{};
    std::ptrdiff_t rowBytes{};
    PixelFormat format{PixelFormat::Rgba8};
    bool premultiplied{true};
};

[[nodiscard]] Image decodePixels(const ConstPixelBuffer& buffer);
void encodePixels(const Image& image, const MutablePixelBuffer& buffer);
[[nodiscard]] Image resizeImageNearest(const Image& input, int width, int height);

} // namespace linear
