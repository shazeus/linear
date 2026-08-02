#include "linear/Regions.h"

#include "linear/Color.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace linear {
namespace {

struct Accumulator {
    std::size_t area{};
    int minX{std::numeric_limits<int>::max()};
    int minY{std::numeric_limits<int>::max()};
    int maxX{std::numeric_limits<int>::min()};
    int maxY{std::numeric_limits<int>::min()};
    double sumX{};
    double sumY{};
    double weightedX{};
    double weightedY{};
    double weight{};
    double confidence{};
    double perimeter{};
    double r{};
    double g{};
    double b{};
    double a{};
    double luma{};
};

} // namespace

std::vector<Region> connectedComponents(
    const Mask& mask, const Image* source, const RegionFilter& filter) {
    if (source != nullptr &&
        (source->width() != mask.width() || source->height() != mask.height())) {
        throw std::invalid_argument("Source and mask dimensions must match");
    }
    const int width = mask.width();
    const int height = mask.height();
    const std::size_t count = mask.values().size();
    if (count == 0U) return {};
    const auto& confidenceMask = mask.values();
    const auto* sourcePixels = source != nullptr
        ? source->pixels().data()
        : nullptr;
    std::vector<std::uint8_t> visited(count, 0U);
    struct PixelCoordinate { int x; int y; };
    std::vector<PixelCoordinate> stack;
    stack.reserve(std::min<std::size_t>(count, 65536U));
    const auto index = [width](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
               static_cast<std::size_t>(x);
    };
    std::vector<Region> regions;
    std::uint32_t componentId = 1U;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t seed = index(x, y);
            if (visited[seed] != 0U ||
                confidenceMask[seed] < filter.threshold) continue;
            Accumulator acc{};
            stack.clear();
            stack.push_back({x, y});
            visited[seed] = 1U;
            while (!stack.empty()) {
                const PixelCoordinate pixel = stack.back();
                stack.pop_back();
                const std::size_t pixelIndex = index(pixel.x, pixel.y);
                const float confidence = clamp01(confidenceMask[pixelIndex]);
                ++acc.area;
                acc.minX = std::min(acc.minX, pixel.x);
                acc.minY = std::min(acc.minY, pixel.y);
                acc.maxX = std::max(acc.maxX, pixel.x);
                acc.maxY = std::max(acc.maxY, pixel.y);
                acc.sumX += pixel.x;
                acc.sumY += pixel.y;
                acc.weightedX += static_cast<double>(pixel.x) * confidence;
                acc.weightedY += static_cast<double>(pixel.y) * confidence;
                acc.weight += confidence;
                acc.confidence += confidence;
                if (sourcePixels != nullptr) {
                    const Color sourcePixel = sourcePixels[pixelIndex];
                    acc.r += sourcePixel.r;
                    acc.g += sourcePixel.g;
                    acc.b += sourcePixel.b;
                    acc.a += sourcePixel.a;
                    acc.luma += 0.2126 * sourcePixel.r +
                                0.7152 * sourcePixel.g +
                                0.0722 * sourcePixel.b;
                }
                const int neighbors[4][2] = {
                    {pixel.x - 1, pixel.y}, {pixel.x + 1, pixel.y},
                    {pixel.x, pixel.y - 1}, {pixel.x, pixel.y + 1}};
                for (const auto& neighbor : neighbors) {
                    const int nx = neighbor[0];
                    const int ny = neighbor[1];
                    bool active = false;
                    std::size_t neighborIndex = 0U;
                    if (nx >= 0 && ny >= 0 && nx < width && ny < height) {
                        neighborIndex = index(nx, ny);
                        active = confidenceMask[neighborIndex] >= filter.threshold;
                    }
                    if (!active) {
                        acc.perimeter += 1.0;
                    } else if (visited[neighborIndex] == 0U) {
                        visited[neighborIndex] = 1U;
                        stack.push_back({nx, ny});
                    }
                }
            }

            const int regionWidth = acc.maxX - acc.minX + 1;
            const int regionHeight = acc.maxY - acc.minY + 1;
            const bool touchesExcludedEdge =
                filter.edgeExclusion > 0 &&
                (acc.minX < filter.edgeExclusion ||
                 acc.minY < filter.edgeExclusion ||
                 acc.maxX >= width - filter.edgeExclusion ||
                 acc.maxY >= height - filter.edgeExclusion);
            if (acc.area < filter.minimumArea ||
                (filter.maximumArea > 0U && acc.area > filter.maximumArea) ||
                regionWidth < filter.minimumWidth ||
                regionHeight < filter.minimumHeight || touchesExcludedEdge) {
                continue;
            }
            const double area = static_cast<double>(acc.area);
            const double safeWeight = acc.weight > 0.0 ? acc.weight : area;
            Region region{};
            region.componentId = componentId++;
            region.bounds = {
                static_cast<float>(acc.minX), static_cast<float>(acc.minY),
                static_cast<float>(regionWidth), static_cast<float>(regionHeight)};
            region.centroid = {
                static_cast<float>(acc.sumX / area),
                static_cast<float>(acc.sumY / area)};
            region.weightedCentroid = {
                static_cast<float>(
                    (acc.weight > 0.0 ? acc.weightedX : acc.sumX) / safeWeight),
                static_cast<float>(
                    (acc.weight > 0.0 ? acc.weightedY : acc.sumY) / safeWeight)};
            region.pixelArea = acc.area;
            region.fillRatio = static_cast<float>(area / region.bounds.area());
            region.perimeter = static_cast<float>(acc.perimeter);
            region.confidence = static_cast<float>(acc.confidence / area);
            if (sourcePixels != nullptr) {
                region.averageColor = {
                    static_cast<float>(acc.r / area),
                    static_cast<float>(acc.g / area),
                    static_cast<float>(acc.b / area),
                    static_cast<float>(acc.a / area)};
                region.averageLuminance = static_cast<float>(acc.luma / area);
            }
            regions.push_back(region);
        }
    }
    std::stable_sort(regions.begin(), regions.end(), [](const Region& lhs, const Region& rhs) {
        if (lhs.pixelArea != rhs.pixelArea) {
            return lhs.pixelArea > rhs.pixelArea;
        }
        if (lhs.bounds.y != rhs.bounds.y) {
            return lhs.bounds.y < rhs.bounds.y;
        }
        return lhs.bounds.x < rhs.bounds.x;
    });
    if (regions.size() > filter.maximumRegions) {
        regions.resize(filter.maximumRegions);
    }
    return regions;
}

float intersectionOverUnion(const Rect& lhs, const Rect& rhs) noexcept {
    const float left = std::max(lhs.x, rhs.x);
    const float top = std::max(lhs.y, rhs.y);
    const float right = std::min(lhs.x + lhs.width, rhs.x + rhs.width);
    const float bottom = std::min(lhs.y + lhs.height, rhs.y + rhs.height);
    const float intersection =
        std::max(0.0F, right - left) * std::max(0.0F, bottom - top);
    const float unionArea = lhs.area() + rhs.area() - intersection;
    return unionArea > 0.0F ? intersection / unionArea : 0.0F;
}

} // namespace linear
