#include "linear/TextRaster.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace linear {
namespace {

using Glyph = std::array<std::uint8_t, 7U>;

Glyph glyph(char value) noexcept {
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(value)))) {
    case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
    case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15}; case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14}; case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
    case '_': return {0,0,0,0,0,0,31}; case '-': return {0,0,0,31,0,0,0};
    case '.': return {0,0,0,0,0,12,12}; case '%': return {17,2,4,8,16,17,0};
    case ':': return {0,12,12,0,12,12,0}; case ' ': return {0,0,0,0,0,0,0};
    default: return {31,17,2,4,4,0,4};
    }
}

void composite(Image& image, int x, int y, Color source) noexcept {
    if (x < 0 || y < 0 || x >= image.width() || y >= image.height()) return;
    Color& destination = image.pixels()[
        static_cast<std::size_t>(y) * image.width() + x];
    const float alpha = clamp01(source.a);
    destination.r = source.r * alpha + destination.r * (1.0F - alpha);
    destination.g = source.g * alpha + destination.g * (1.0F - alpha);
    destination.b = source.b * alpha + destination.b * (1.0F - alpha);
    destination.a = alpha + destination.a * (1.0F - alpha);
}

bool insideRoundedRectangle(
    int x, int y, int width, int height, float radius) noexcept {
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    const float clampedRadius = std::clamp(
        radius, 0.0F,
        static_cast<float>(std::min(width, height)) * 0.5F);
    if (clampedRadius <= 0.0F) return true;
    const float px = static_cast<float>(x) + 0.5F;
    const float py = static_cast<float>(y) + 0.5F;
    const float nearestX = std::clamp(
        px, clampedRadius, static_cast<float>(width) - clampedRadius);
    const float nearestY = std::clamp(
        py, clampedRadius, static_cast<float>(height) - clampedRadius);
    const float dx = px - nearestX;
    const float dy = py - nearestY;
    return dx * dx + dy * dy <= clampedRadius * clampedRadius;
}

void rasterizePanel(
    Image& image, int originX, int originY, int width, int height,
    float radius, Color fill, Color border, int borderThickness) {
    const int thickness = std::clamp(
        borderThickness, 0, std::max(0, std::min(width, height) / 2));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!insideRoundedRectangle(x, y, width, height, radius)) continue;
            bool isBorder = false;
            if (border.a > 0.0F && thickness > 0) {
                const int innerWidth = width - thickness * 2;
                const int innerHeight = height - thickness * 2;
                isBorder = innerWidth <= 0 || innerHeight <= 0 ||
                    !insideRoundedRectangle(
                        x - thickness, y - thickness,
                        innerWidth, innerHeight,
                        std::max(0.0F, radius - thickness));
            }
            composite(
                image, originX + x, originY + y,
                isBorder ? border : fill);
        }
    }
}

#ifdef _WIN32

std::size_t utf8ToWide(
    const std::string& value, wchar_t* output,
    std::size_t outputCapacity) noexcept {
    if (output == nullptr || outputCapacity == 0U) return 0U;
    output[0] = L'\0';
    if (value.empty()) return 0U;
    const int capacity = static_cast<int>(std::min<std::size_t>(
        outputCapacity - 1U, static_cast<std::size_t>(INT_MAX)));
    const int converted = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(std::min<std::size_t>(
            value.size(), static_cast<std::size_t>(INT_MAX))),
        output, capacity);
    if (converted <= 0) return 0U;
    output[converted] = L'\0';
    return static_cast<std::size_t>(converted);
}

HFONT createSystemFont(const BitmapTextStyle& style) noexcept {
    std::array<wchar_t, LF_FACESIZE> family{};
    if (utf8ToWide(style.fontFamily, family.data(), family.size()) == 0U) {
        return nullptr;
    }
    const int pixelHeight = std::clamp(style.scale, 1, 8) * 8;
    return CreateFontW(
        -pixelHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family.data());
}

bool measureSystemText(
    const std::string& text, const BitmapTextStyle& style,
    TextMetrics& metrics) noexcept {
    HFONT font = createSystemFont(style);
    if (font == nullptr) return false;
    HDC device = CreateCompatibleDC(nullptr);
    if (device == nullptr) {
        DeleteObject(font);
        return false;
    }
    const HGDIOBJ previousFont = SelectObject(device, font);
    std::array<wchar_t, 512U> wideText{};
    const std::size_t length =
        utf8ToWide(text, wideText.data(), wideText.size());
    SIZE size{};
    const BOOL measured = GetTextExtentPoint32W(
        device, wideText.data(), static_cast<int>(length), &size);
    SelectObject(device, previousFont);
    DeleteDC(device);
    DeleteObject(font);
    if (measured == FALSE) return false;
    const int padding = std::max(0, style.padding);
    metrics = {
        static_cast<int>(size.cx) + padding * 2,
        static_cast<int>(size.cy) + padding * 2};
    return true;
}

bool rasterizeSystemText(
    Image& image, Point topLeft, const std::string& text,
    const BitmapTextStyle& style) noexcept {
    TextMetrics metrics{};
    if (!measureSystemText(text, style, metrics)) return false;
    const int originX = static_cast<int>(topLeft.x);
    const int originY = static_cast<int>(topLeft.y);
    if (style.shadow.a > 0.0F) {
        rasterizePanel(
            image,
            originX + static_cast<int>(std::lround(style.shadowOffset.x)),
            originY + static_cast<int>(std::lround(style.shadowOffset.y)),
            metrics.width, metrics.height, style.cornerRadius,
            style.shadow, {}, 0);
    }
    if (style.backgroundEnabled) {
        rasterizePanel(
            image, originX, originY, metrics.width, metrics.height,
            style.cornerRadius, style.background, style.border,
            style.borderThickness);
    }

    const int padding = std::max(0, style.padding);
    const int contentWidth = std::max(1, metrics.width - padding * 2);
    const int contentHeight = std::max(1, metrics.height - padding * 2);
    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = contentWidth;
    bitmapInfo.bmiHeader.biHeight = -contentHeight;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    void* bitmapPixels = nullptr;
    HDC device = CreateCompatibleDC(nullptr);
    if (device == nullptr) return false;
    HBITMAP bitmap = CreateDIBSection(
        device, &bitmapInfo, DIB_RGB_COLORS, &bitmapPixels, nullptr, 0);
    HFONT font = createSystemFont(style);
    if (bitmap == nullptr || bitmapPixels == nullptr || font == nullptr) {
        if (bitmap != nullptr) DeleteObject(bitmap);
        if (font != nullptr) DeleteObject(font);
        DeleteDC(device);
        return false;
    }
    std::memset(
        bitmapPixels, 0,
        static_cast<std::size_t>(contentWidth) * contentHeight * 4U);
    const HGDIOBJ previousBitmap = SelectObject(device, bitmap);
    const HGDIOBJ previousFont = SelectObject(device, font);
    SetBkMode(device, TRANSPARENT);
    SetTextColor(device, RGB(255, 255, 255));
    std::array<wchar_t, 512U> wideText{};
    const std::size_t length =
        utf8ToWide(text, wideText.data(), wideText.size());
    TextOutW(device, 0, 0, wideText.data(), static_cast<int>(length));

    const auto* coveragePixels = static_cast<const std::uint8_t*>(bitmapPixels);
    for (int y = 0; y < contentHeight; ++y) {
        for (int x = 0; x < contentWidth; ++x) {
            const std::size_t index =
                (static_cast<std::size_t>(y) * contentWidth + x) * 4U;
            const float coverage = static_cast<float>(std::max({
                coveragePixels[index], coveragePixels[index + 1U],
                coveragePixels[index + 2U]})) / 255.0F;
            if (coverage <= 0.0F) continue;
            Color foreground = style.foreground;
            foreground.a *= coverage;
            composite(
                image, originX + padding + x, originY + padding + y,
                foreground);
        }
    }
    SelectObject(device, previousFont);
    SelectObject(device, previousBitmap);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(device);
    return true;
}

#endif

} // namespace

TextMetrics measureBitmapText(
    const std::string& text, const BitmapTextStyle& style) noexcept {
#ifdef _WIN32
    if (!style.fontFamily.empty()) {
        TextMetrics systemMetrics{};
        if (measureSystemText(text, style, systemMetrics)) {
            return systemMetrics;
        }
    }
#endif
    const int scale = std::clamp(style.scale, 1, 8);
    const int spacing = std::max(0, style.characterSpacing) * scale;
    const int glyphWidth = 5 * scale;
    const int contentWidth = text.empty() ? 0 :
        static_cast<int>(text.size()) * glyphWidth +
        static_cast<int>(text.size() - 1U) * spacing;
    const int padding = std::max(0, style.padding);
    return {contentWidth + padding * 2, 7 * scale + padding * 2};
}

void rasterizeBitmapText(
    Image& image, Point topLeft, const std::string& text,
    const BitmapTextStyle& style) {
#ifdef _WIN32
    if (!style.fontFamily.empty() &&
        rasterizeSystemText(image, topLeft, text, style)) {
        return;
    }
#endif
    const TextMetrics metrics = measureBitmapText(text, style);
    const int originX = static_cast<int>(topLeft.x);
    const int originY = static_cast<int>(topLeft.y);
    if (style.shadow.a > 0.0F) {
        rasterizePanel(
            image,
            originX + static_cast<int>(std::lround(style.shadowOffset.x)),
            originY + static_cast<int>(std::lround(style.shadowOffset.y)),
            metrics.width, metrics.height, style.cornerRadius,
            style.shadow, {}, 0);
    }
    if (style.backgroundEnabled) {
        rasterizePanel(
            image, originX, originY, metrics.width, metrics.height,
            style.cornerRadius, style.background, style.border,
            style.borderThickness);
    }
    const int scale = std::clamp(style.scale, 1, 8);
    const int advance = 5 * scale + std::max(0, style.characterSpacing) * scale;
    const int padding = std::max(0, style.padding);
    for (std::size_t character = 0; character < text.size(); ++character) {
        const Glyph rows = glyph(text[character]);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[static_cast<std::size_t>(row)] & (1U << (4 - column))) == 0U) continue;
                for (int py = 0; py < scale; ++py) {
                    for (int px = 0; px < scale; ++px) {
                        composite(image,
                            originX + padding + static_cast<int>(character) * advance + column * scale + px,
                            originY + padding + row * scale + py, style.foreground);
                    }
                }
            }
        }
    }
}

} // namespace linear
