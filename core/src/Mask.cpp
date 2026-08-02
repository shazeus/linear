#include "linear/Mask.h"

#include <algorithm>
#include <stdexcept>

namespace linear {
namespace {

void validateRadius(int radius) {
    if (radius < 0) {
        throw std::invalid_argument("Mask radius must be non-negative");
    }
}

template <bool Maximum>
void neighborhoodInto(
    const Mask& input, Mask& horizontal, Mask& output, int radius) {
    const int width = input.width();
    const int height = input.height();
    const auto& source = input.values();
    auto& intermediate = horizontal.values();
    auto& destination = output.values();
    for (int y = 0; y < height; ++y) {
        const std::size_t row = static_cast<std::size_t>(y) * width;
        for (int x = 0; x < width; ++x) {
            float value = source[row + static_cast<std::size_t>(
                std::max(0, x - radius))];
            const int end = std::min(width - 1, x + radius);
            for (int xx = std::max(0, x - radius) + 1; xx <= end; ++xx) {
                if constexpr (Maximum) {
                    value = std::max(value, source[row + xx]);
                } else {
                    value = std::min(value, source[row + xx]);
                }
            }
            intermediate[row + static_cast<std::size_t>(x)] = value;
        }
    }
    for (int y = 0; y < height; ++y) {
        const int begin = std::max(0, y - radius);
        const int end = std::min(height - 1, y + radius);
        const std::size_t row = static_cast<std::size_t>(y) * width;
        for (int x = 0; x < width; ++x) {
            float value = intermediate[
                static_cast<std::size_t>(begin) * width + x];
            for (int yy = begin + 1; yy <= end; ++yy) {
                if constexpr (Maximum) {
                    value = std::max(
                        value, intermediate[static_cast<std::size_t>(yy) * width + x]);
                } else {
                    value = std::min(
                        value, intermediate[static_cast<std::size_t>(yy) * width + x]);
                }
            }
            destination[row + static_cast<std::size_t>(x)] = value;
        }
    }
}

template <bool Maximum>
Mask neighborhood(const Mask& input, int radius) {
    validateRadius(radius);
    if (radius == 0 || input.empty()) return input;
    Mask horizontal(input.width(), input.height());
    Mask output(input.width(), input.height());
    neighborhoodInto<Maximum>(input, horizontal, output, radius);
    return output;
}

} // namespace

Mask boxBlur(const Mask& input, int radius) {
    validateRadius(radius);
    if (radius == 0 || input.empty()) {
        return input;
    }
    Mask horizontal(input.width(), input.height());
    Mask output(input.width(), input.height());
    const int width = input.width();
    const int height = input.height();
    const auto& source = input.values();
    auto& intermediate = horizontal.values();
    auto& destination = output.values();
    for (int y = 0; y < input.height(); ++y) {
        const std::size_t row = static_cast<std::size_t>(y) * width;
        float sum = 0.0F;
        int count = 0;
        for (int x = -radius; x <= radius; ++x) {
            if (x >= 0 && x < width) {
                sum += source[row + static_cast<std::size_t>(x)];
                ++count;
            }
        }
        for (int x = 0; x < width; ++x) {
            intermediate[row + static_cast<std::size_t>(x)] =
                sum / static_cast<float>(count);
            const int remove = x - radius;
            const int add = x + radius + 1;
            if (remove >= 0) {
                sum -= source[row + static_cast<std::size_t>(remove)];
                --count;
            }
            if (add < width) {
                sum += source[row + static_cast<std::size_t>(add)];
                ++count;
            }
        }
    }
    for (int x = 0; x < width; ++x) {
        float sum = 0.0F;
        int count = 0;
        for (int y = -radius; y <= radius; ++y) {
            if (y >= 0 && y < height) {
                sum += intermediate[static_cast<std::size_t>(y) * width + x];
                ++count;
            }
        }
        for (int y = 0; y < height; ++y) {
            destination[static_cast<std::size_t>(y) * width + x] =
                sum / static_cast<float>(count);
            const int remove = y - radius;
            const int add = y + radius + 1;
            if (remove >= 0) {
                sum -= intermediate[
                    static_cast<std::size_t>(remove) * width + x];
                --count;
            }
            if (add < height) {
                sum += intermediate[static_cast<std::size_t>(add) * width + x];
                ++count;
            }
        }
    }
    return output;
}

Mask erode(const Mask& input, int radius) {
    return neighborhood<false>(input, radius);
}

Mask dilate(const Mask& input, int radius) {
    return neighborhood<true>(input, radius);
}

Mask morphologicalOpen(const Mask& input, int radius) {
    return dilate(erode(input, radius), radius);
}

Mask morphologicalClose(const Mask& input, int radius) {
    return erode(dilate(input, radius), radius);
}

Mask morphologicalOpenClose(const Mask& input, int radius) {
    validateRadius(radius);
    if (radius == 0 || input.empty()) return input;
    Mask horizontal(input.width(), input.height());
    Mask first(input.width(), input.height());
    Mask second(input.width(), input.height());
    neighborhoodInto<false>(input, horizontal, first, radius);
    neighborhoodInto<true>(first, horizontal, second, radius);
    neighborhoodInto<true>(second, horizontal, first, radius);
    neighborhoodInto<false>(first, horizontal, second, radius);
    return second;
}

Mask thresholdSoft(const Mask& input, float threshold, float softness) {
    Mask output(input.width(), input.height());
    const float safeSoftness = std::max(0.0F, softness);
    const float lower = threshold - safeSoftness * 0.5F;
    const float upper = threshold + safeSoftness * 0.5F;
    const auto& source = input.values();
    auto& destination = output.values();
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (safeSoftness <= 1.0e-7F) {
            destination[i] = source[i] >= threshold ? 1.0F : 0.0F;
        } else {
            destination[i] = clamp01((source[i] - lower) / (upper - lower));
        }
    }
    return output;
}

Mask resizeNearest(const Mask& input, int width, int height) {
    if (width < 0 || height < 0) {
        throw std::invalid_argument("Resize dimensions must be non-negative");
    }
    Mask output(width, height);
    if (input.empty() || output.empty()) {
        return output;
    }
    const auto& source = input.values();
    auto& destination = output.values();
    for (int y = 0; y < height; ++y) {
        const int sourceY = std::min(input.height() - 1, y * input.height() / height);
        const std::size_t sourceRow =
            static_cast<std::size_t>(sourceY) * input.width();
        const std::size_t destinationRow =
            static_cast<std::size_t>(y) * width;
        for (int x = 0; x < width; ++x) {
            const int sourceX = std::min(input.width() - 1, x * input.width() / width);
            destination[destinationRow + static_cast<std::size_t>(x)] =
                source[sourceRow + static_cast<std::size_t>(sourceX)];
        }
    }
    return output;
}

} // namespace linear
