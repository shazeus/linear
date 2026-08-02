#pragma once

#include "linear/Types.h"

#include <string>

namespace linear {

struct BitmapTextStyle {
    int scale{2};
    int padding{3};
    int characterSpacing{1};
    // Empty selects Linear's built-in technical bitmap face. A non-empty
    // family name selects an installed Windows font through GDI.
    std::string fontFamily{};
    Color foreground{0.85F, 1.0F, 0.9F, 1.0F};
    Color background{0.02F, 0.08F, 0.07F, 0.72F};
    bool backgroundEnabled{true};
    float cornerRadius{};
    Color border{0.0F, 1.0F, 0.75F, 0.0F};
    int borderThickness{1};
    Color shadow{0.0F, 0.0F, 0.0F, 0.0F};
    Point shadowOffset{3.0F, 3.0F};
};

struct TextMetrics {
    int width{};
    int height{};
};

[[nodiscard]] TextMetrics measureBitmapText(
    const std::string& text, const BitmapTextStyle& style) noexcept;
void rasterizeBitmapText(
    Image& image, Point topLeft, const std::string& text,
    const BitmapTextStyle& style);

} // namespace linear
