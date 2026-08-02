#pragma once

#include "linear/Types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace linear {

struct Polyline {
    std::uint64_t stableId{};
    bool closed{};
    std::vector<Point> points;
};

struct OrbitSettings {
    float radiusScaleX{1.0F};
    float radiusScaleY{1.0F};
    float padding{};
    float rotationRadians{};
    float startAngleRadians{};
    float sweepAngleRadians{6.28318530717958647692F};
    std::size_t segments{64U};
    std::size_t maximumSegments{4096U};
};

[[nodiscard]] Polyline makeOrbitalPath(
    const Rect& bounds,
    std::uint64_t stableId,
    const OrbitSettings& settings = {});

struct ContourSettings {
    float threshold{0.5F};
    std::size_t minimumArea{1U};
    std::size_t maximumContours{256U};
    std::size_t maximumPointsPerContour{4096U};
    float simplificationTolerance{};
};

// Produces one clockwise outer cell-edge contour per four-connected mask
// component. Holes are deliberately excluded from this first contour source.
[[nodiscard]] std::vector<Polyline> extractContourPaths(
    const Mask& mask,
    const ContourSettings& settings = {});

enum class RepeaterPrimitive {
    Ring,
    Box,
    Arrow,
    Tick
};

struct PathRepeaterSettings {
    RepeaterPrimitive primitive{RepeaterPrimitive::Ring};
    float spacing{16.0F};
    float phase{};
    float scale{1.0F};
    bool alignToTangent{true};
    std::size_t maximumInstances{256U};
};

struct RepeaterInstance {
    std::uint64_t stableId{};
    RepeaterPrimitive primitive{RepeaterPrimitive::Ring};
    Point position{};
    Point tangent{1.0F, 0.0F};
    float rotationRadians{};
    float scale{1.0F};
};

[[nodiscard]] std::vector<RepeaterInstance> repeatAlongPath(
    const Polyline& path,
    const PathRepeaterSettings& settings = {});

// Converts a repeater instance into actual local primitive strokes in image
// coordinates. Rings and boxes are closed; arrows and ticks are open.
[[nodiscard]] std::vector<Polyline> buildRepeaterGlyph(
    const RepeaterInstance& instance,
    std::size_t ringSegments = 16U);

} // namespace linear
