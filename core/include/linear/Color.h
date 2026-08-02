#pragma once

#include "linear/Types.h"

namespace linear {

enum class ColorSpace { Rgb, Hsv, Lab };

struct Hsv {
    float h{};
    float s{};
    float v{};
};

struct Lab {
    float l{};
    float a{};
    float b{};
};

[[nodiscard]] Hsv rgbToHsv(Color color) noexcept;
[[nodiscard]] Lab rgbToLab(Color color) noexcept;
[[nodiscard]] float rgbDistance(Color lhs, Color rhs) noexcept;
[[nodiscard]] float hsvDistance(Color lhs, Color rhs) noexcept;
[[nodiscard]] float labDistance(Color lhs, Color rhs) noexcept;
[[nodiscard]] float colorDistance(Color lhs, Color rhs, ColorSpace space) noexcept;
[[nodiscard]] float luminance(Color color) noexcept;

} // namespace linear
