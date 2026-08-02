#include "linear/Color.h"

#include <algorithm>
#include <cmath>

namespace linear {
namespace {

float srgbToLinear(float value) noexcept {
    value = std::max(0.0F, value);
    return value <= 0.04045F ? value / 12.92F
                             : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

float labPivot(float value) noexcept {
    constexpr float epsilon = 216.0F / 24389.0F;
    constexpr float kappa = 24389.0F / 27.0F;
    return value > epsilon ? std::cbrt(value) : (kappa * value + 16.0F) / 116.0F;
}

} // namespace

Hsv rgbToHsv(Color color) noexcept {
    const float maximum = std::max({color.r, color.g, color.b});
    const float minimum = std::min({color.r, color.g, color.b});
    const float delta = maximum - minimum;
    Hsv result{};
    result.v = maximum;
    result.s = maximum <= 0.0F ? 0.0F : delta / maximum;
    if (delta <= 1.0e-7F) {
        return result;
    }
    if (maximum == color.r) {
        result.h = std::fmod((color.g - color.b) / delta, 6.0F);
    } else if (maximum == color.g) {
        result.h = ((color.b - color.r) / delta) + 2.0F;
    } else {
        result.h = ((color.r - color.g) / delta) + 4.0F;
    }
    result.h /= 6.0F;
    if (result.h < 0.0F) {
        result.h += 1.0F;
    }
    return result;
}

Lab rgbToLab(Color color) noexcept {
    const float r = srgbToLinear(color.r);
    const float g = srgbToLinear(color.g);
    const float b = srgbToLinear(color.b);
    const float x = (0.4124564F * r + 0.3575761F * g + 0.1804375F * b) / 0.95047F;
    const float y = (0.2126729F * r + 0.7151522F * g + 0.0721750F * b);
    const float z = (0.0193339F * r + 0.1191920F * g + 0.9503041F * b) / 1.08883F;
    const float fx = labPivot(x);
    const float fy = labPivot(y);
    const float fz = labPivot(z);
    return {116.0F * fy - 16.0F, 500.0F * (fx - fy), 200.0F * (fy - fz)};
}

float rgbDistance(Color lhs, Color rhs) noexcept {
    const float dr = lhs.r - rhs.r;
    const float dg = lhs.g - rhs.g;
    const float db = lhs.b - rhs.b;
    return std::sqrt(dr * dr + dg * dg + db * db) / std::sqrt(3.0F);
}

float hsvDistance(Color lhs, Color rhs) noexcept {
    const Hsv a = rgbToHsv(lhs);
    const Hsv b = rgbToHsv(rhs);
    const float rawHue = std::fabs(a.h - b.h);
    const float hue = std::min(rawHue, 1.0F - rawHue);
    const float ds = a.s - b.s;
    const float dv = a.v - b.v;
    return std::sqrt(4.0F * hue * hue + ds * ds + dv * dv) / std::sqrt(3.0F);
}

float labDistance(Color lhs, Color rhs) noexcept {
    const Lab a = rgbToLab(lhs);
    const Lab b = rgbToLab(rhs);
    const float dl = a.l - b.l;
    const float da = a.a - b.a;
    const float db = a.b - b.b;
    // CIE76 normalized by the approximate maximum useful sRGB delta.
    return std::min(1.0F, std::sqrt(dl * dl + da * da + db * db) / 258.0F);
}

float colorDistance(Color lhs, Color rhs, ColorSpace space) noexcept {
    switch (space) {
    case ColorSpace::Rgb:
        return rgbDistance(lhs, rhs);
    case ColorSpace::Hsv:
        return hsvDistance(lhs, rhs);
    case ColorSpace::Lab:
        return labDistance(lhs, rhs);
    }
    return rgbDistance(lhs, rhs);
}

float luminance(Color color) noexcept {
    return 0.2126F * color.r + 0.7152F * color.g + 0.0722F * color.b;
}

} // namespace linear
