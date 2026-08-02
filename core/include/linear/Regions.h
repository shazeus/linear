#pragma once

#include "linear/Types.h"

#include <cstdint>
#include <vector>

namespace linear {

struct Region {
    std::uint32_t componentId{};
    std::uint64_t trackId{};
    Rect bounds{};
    Point centroid{};
    Point weightedCentroid{};
    std::size_t pixelArea{};
    float fillRatio{};
    float perimeter{};
    Color averageColor{};
    float averageLuminance{};
    float confidence{};
    Point velocity{};
    int age{};
};

struct RegionFilter {
    std::size_t minimumArea{1};
    std::size_t maximumArea{};
    int minimumWidth{1};
    int minimumHeight{1};
    std::size_t maximumRegions{256};
    int edgeExclusion{};
    float threshold{0.5F};
};

[[nodiscard]] std::vector<Region> connectedComponents(
    const Mask& mask, const Image* source, const RegionFilter& filter);
[[nodiscard]] float intersectionOverUnion(const Rect& lhs, const Rect& rhs) noexcept;

} // namespace linear
