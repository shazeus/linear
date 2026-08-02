#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace linear {

struct Color {
    float r{};
    float g{};
    float b{};
    float a{1.0F};
};

struct Point {
    float x{};
    float y{};
};

struct Rect {
    float x{};
    float y{};
    float width{};
    float height{};

    [[nodiscard]] float area() const noexcept {
        return std::max(0.0F, width) * std::max(0.0F, height);
    }
    [[nodiscard]] Point center() const noexcept {
        return {x + width * 0.5F, y + height * 0.5F};
    }
};

class Image {
public:
    Image() = default;
    Image(int width, int height, Color value = {})
        : width_(width), height_(height),
          pixels_(checkedSize(width, height), value) {}

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] bool empty() const noexcept { return pixels_.empty(); }

    Color& at(int x, int y) { return pixels_.at(index(x, y)); }
    const Color& at(int x, int y) const { return pixels_.at(index(x, y)); }
    [[nodiscard]] std::vector<Color>& pixels() noexcept { return pixels_; }
    [[nodiscard]] const std::vector<Color>& pixels() const noexcept { return pixels_; }

private:
    static std::size_t checkedSize(int width, int height) {
        if (width < 0 || height < 0) {
            throw std::invalid_argument("Image dimensions must be non-negative");
        }
        const auto w = static_cast<std::size_t>(width);
        const auto h = static_cast<std::size_t>(height);
        if (h != 0U && w > std::vector<Color>().max_size() / h) {
            throw std::length_error("Image dimensions are too large");
        }
        return w * h;
    }
    [[nodiscard]] std::size_t index(int x, int y) const {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) {
            throw std::out_of_range("Image coordinate out of range");
        }
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(x);
    }

    int width_{};
    int height_{};
    std::vector<Color> pixels_;
};

class Mask {
public:
    Mask() = default;
    Mask(int width, int height, float value = 0.0F)
        : width_(width), height_(height), values_(checkedSize(width, height), value) {}

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
    float& at(int x, int y) { return values_.at(index(x, y)); }
    float at(int x, int y) const { return values_.at(index(x, y)); }
    [[nodiscard]] std::vector<float>& values() noexcept { return values_; }
    [[nodiscard]] const std::vector<float>& values() const noexcept { return values_; }

private:
    static std::size_t checkedSize(int width, int height) {
        if (width < 0 || height < 0) {
            throw std::invalid_argument("Mask dimensions must be non-negative");
        }
        const auto w = static_cast<std::size_t>(width);
        const auto h = static_cast<std::size_t>(height);
        if (h != 0U && w > std::vector<float>().max_size() / h) {
            throw std::length_error("Mask dimensions are too large");
        }
        return w * h;
    }
    [[nodiscard]] std::size_t index(int x, int y) const {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) {
            throw std::out_of_range("Mask coordinate out of range");
        }
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(x);
    }

    int width_{};
    int height_{};
    std::vector<float> values_;
};

[[nodiscard]] inline float clamp01(float value) noexcept {
    return std::clamp(value, 0.0F, 1.0F);
}

} // namespace linear
