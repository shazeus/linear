#include "linear/Pixel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace linear {
namespace {

std::size_t bytesPerPixel(PixelFormat format) {
    switch (format) {
    case PixelFormat::Rgba8:
    case PixelFormat::Bgra8:
        return 4U;
    case PixelFormat::Rgba16:
        return 8U;
    case PixelFormat::Rgba32Float:
        return 16U;
    }
    throw std::invalid_argument("Unsupported pixel format");
}

void validate(const void* data, int width, int height, std::ptrdiff_t rowBytes,
              PixelFormat format) {
    if (width < 0 || height < 0) {
        throw std::invalid_argument("Pixel dimensions must be non-negative");
    }
    if (width > 0 && height > 0 && data == nullptr) {
        throw std::invalid_argument("Pixel data is null");
    }
    const auto minimum =
        static_cast<std::ptrdiff_t>(static_cast<std::size_t>(width) *
                                    bytesPerPixel(format));
    if (height > 0 && std::abs(rowBytes) < minimum) {
        throw std::invalid_argument("Pixel rowBytes is smaller than one row");
    }
}

const std::byte* rowAddress(
    const void* data, int height, std::ptrdiff_t rowBytes, int y) {
    const auto* bytes = static_cast<const std::byte*>(data);
    return rowBytes >= 0 ? bytes + static_cast<std::ptrdiff_t>(y) * rowBytes
                         : bytes + static_cast<std::ptrdiff_t>(height - 1 - y) *
                                       (-rowBytes);
}

std::byte* rowAddress(void* data, int height, std::ptrdiff_t rowBytes, int y) {
    auto* bytes = static_cast<std::byte*>(data);
    return rowBytes >= 0 ? bytes + static_cast<std::ptrdiff_t>(y) * rowBytes
                         : bytes + static_cast<std::ptrdiff_t>(height - 1 - y) *
                                       (-rowBytes);
}

Color unpremultiply(Color color, bool premultiplied) noexcept {
    if (premultiplied && color.a > 1.0e-7F && color.a < 1.0F) {
        color.r /= color.a;
        color.g /= color.a;
        color.b /= color.a;
    }
    return color;
}

Color premultiply(Color color, bool premultiplied) noexcept {
    if (premultiplied) {
        color.r *= color.a;
        color.g *= color.a;
        color.b *= color.a;
    }
    return color;
}

} // namespace

Image decodePixels(const ConstPixelBuffer& buffer) {
    validate(buffer.data, buffer.width, buffer.height, buffer.rowBytes, buffer.format);
    Image image(buffer.width, buffer.height);
    auto& outputPixels = image.pixels();
    for (int y = 0; y < buffer.height; ++y) {
        const std::byte* row =
            rowAddress(buffer.data, buffer.height, buffer.rowBytes, y);
        for (int x = 0; x < buffer.width; ++x) {
            Color color{};
            if (buffer.format == PixelFormat::Rgba8 ||
                buffer.format == PixelFormat::Bgra8) {
                const auto* pixel = reinterpret_cast<const std::uint8_t*>(
                    row + static_cast<std::size_t>(x) * 4U);
                const bool bgra = buffer.format == PixelFormat::Bgra8;
                color = {pixel[bgra ? 2 : 0] / 255.0F, pixel[1] / 255.0F,
                         pixel[bgra ? 0 : 2] / 255.0F, pixel[3] / 255.0F};
            } else if (buffer.format == PixelFormat::Rgba16) {
                std::uint16_t channels[4]{};
                std::memcpy(channels, row + static_cast<std::size_t>(x) * 8U, 8U);
                color = {channels[0] / 65535.0F, channels[1] / 65535.0F,
                         channels[2] / 65535.0F, channels[3] / 65535.0F};
            } else {
                float channels[4]{};
                std::memcpy(channels, row + static_cast<std::size_t>(x) * 16U, 16U);
                color = {channels[0], channels[1], channels[2], channels[3]};
            }
            outputPixels[static_cast<std::size_t>(y) * buffer.width + x] =
                unpremultiply(color, buffer.premultiplied);
        }
    }
    return image;
}

void encodePixels(const Image& image, const MutablePixelBuffer& buffer) {
    validate(buffer.data, buffer.width, buffer.height, buffer.rowBytes, buffer.format);
    if (image.width() != buffer.width || image.height() != buffer.height) {
        throw std::invalid_argument("Image and pixel buffer dimensions must match");
    }
    const auto& inputPixels = image.pixels();
    for (int y = 0; y < buffer.height; ++y) {
        std::byte* row = rowAddress(buffer.data, buffer.height, buffer.rowBytes, y);
        for (int x = 0; x < buffer.width; ++x) {
            const Color color = premultiply(
                inputPixels[static_cast<std::size_t>(y) * buffer.width + x],
                buffer.premultiplied);
            if (buffer.format == PixelFormat::Rgba8 ||
                buffer.format == PixelFormat::Bgra8) {
                auto* pixel = reinterpret_cast<std::uint8_t*>(
                    row + static_cast<std::size_t>(x) * 4U);
                const bool bgra = buffer.format == PixelFormat::Bgra8;
                pixel[bgra ? 2 : 0] = static_cast<std::uint8_t>(
                    clamp01(color.r) * 255.0F + 0.5F);
                pixel[1] = static_cast<std::uint8_t>(
                    clamp01(color.g) * 255.0F + 0.5F);
                pixel[bgra ? 0 : 2] = static_cast<std::uint8_t>(
                    clamp01(color.b) * 255.0F + 0.5F);
                pixel[3] = static_cast<std::uint8_t>(
                    clamp01(color.a) * 255.0F + 0.5F);
            } else if (buffer.format == PixelFormat::Rgba16) {
                const std::uint16_t channels[4] = {
                    static_cast<std::uint16_t>(
                        clamp01(color.r) * 65535.0F + 0.5F),
                    static_cast<std::uint16_t>(
                        clamp01(color.g) * 65535.0F + 0.5F),
                    static_cast<std::uint16_t>(
                        clamp01(color.b) * 65535.0F + 0.5F),
                    static_cast<std::uint16_t>(
                        clamp01(color.a) * 65535.0F + 0.5F)};
                std::memcpy(row + static_cast<std::size_t>(x) * 8U, channels, 8U);
            } else {
                const float channels[4] = {color.r, color.g, color.b, color.a};
                std::memcpy(row + static_cast<std::size_t>(x) * 16U, channels, 16U);
            }
        }
    }
}

Image resizeImageNearest(const Image& input, int width, int height) {
    if (width < 0 || height < 0) {
        throw std::invalid_argument("Resize dimensions must be non-negative");
    }
    Image output(width, height);
    if (input.empty() || width == 0 || height == 0) return output;
    const auto& source = input.pixels();
    auto& destination = output.pixels();
    for (int y = 0; y < height; ++y) {
        const int sourceY = std::min(
            input.height() - 1,
            static_cast<int>((static_cast<long long>(y) * input.height()) / height));
        for (int x = 0; x < width; ++x) {
            const int sourceX = std::min(
                input.width() - 1,
                static_cast<int>((static_cast<long long>(x) * input.width()) / width));
            destination[static_cast<std::size_t>(y) * width + x] =
                source[static_cast<std::size_t>(sourceY) * input.width() + sourceX];
        }
    }
    return output;
}

} // namespace linear
