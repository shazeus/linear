#include "linear/PathGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace linear {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTwoPi = 2.0F * kPi;
constexpr float kEpsilon = 1.0e-5F;

struct IntegerPoint {
    int x{};
    int y{};

    bool operator==(const IntegerPoint& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

struct Edge {
    IntegerPoint first{};
    IntegerPoint second{};
    bool used{};
};

std::uint64_t pointKey(IntegerPoint point) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(point.x)) << 32U) |
           static_cast<std::uint32_t>(point.y);
}

float squaredDistance(Point first, Point second) noexcept {
    const float dx = first.x - second.x;
    const float dy = first.y - second.y;
    return dx * dx + dy * dy;
}

float pointLineDistance(Point point, Point first, Point second) noexcept {
    const float dx = second.x - first.x;
    const float dy = second.y - first.y;
    const float denominator = dx * dx + dy * dy;
    if (denominator <= kEpsilon) {
        return std::sqrt(squaredDistance(point, first));
    }
    const float amount = std::clamp(
        ((point.x - first.x) * dx + (point.y - first.y) * dy) / denominator,
        0.0F, 1.0F);
    const Point projection{first.x + dx * amount, first.y + dy * amount};
    return std::sqrt(squaredDistance(point, projection));
}

void simplifyClosed(std::vector<Point>& points, float tolerance) {
    if (points.size() <= 3U) {
        return;
    }
    const float effectiveTolerance = std::max(0.0F, tolerance);
    bool changed = true;
    while (changed && points.size() > 3U) {
        changed = false;
        for (std::size_t i = 0; i < points.size(); ++i) {
            const Point previous =
                points[(i + points.size() - 1U) % points.size()];
            const Point current = points[i];
            const Point next = points[(i + 1U) % points.size()];
            if (pointLineDistance(current, previous, next) <=
                effectiveTolerance + kEpsilon) {
                points.erase(points.begin() + static_cast<std::ptrdiff_t>(i));
                changed = true;
                break;
            }
        }
    }
}

void capPoints(std::vector<Point>& points, std::size_t maximum) {
    maximum = std::max<std::size_t>(maximum, 3U);
    if (points.size() <= maximum) {
        return;
    }
    std::vector<Point> limited;
    limited.reserve(maximum);
    for (std::size_t i = 0; i < maximum; ++i) {
        const std::size_t source =
            (i * points.size()) / maximum;
        limited.push_back(points[source]);
    }
    points = std::move(limited);
}

double signedArea(const std::vector<Point>& points) noexcept {
    double area = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Point first = points[i];
        const Point second = points[(i + 1U) % points.size()];
        area += static_cast<double>(first.x) * second.y -
                static_cast<double>(second.x) * first.y;
    }
    return area * 0.5;
}

std::vector<std::vector<Point>> traceLoops(std::vector<Edge>& edges) {
    std::unordered_multimap<std::uint64_t, std::size_t> outgoing;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        outgoing.emplace(pointKey(edges[i].first), i);
    }
    std::vector<std::vector<Point>> loops;
    for (std::size_t seed = 0; seed < edges.size(); ++seed) {
        if (edges[seed].used) {
            continue;
        }
        std::vector<Point> loop;
        std::size_t edgeIndex = seed;
        const IntegerPoint start = edges[seed].first;
        for (std::size_t guard = 0; guard <= edges.size(); ++guard) {
            Edge& edge = edges[edgeIndex];
            if (edge.used) {
                break;
            }
            edge.used = true;
            loop.push_back(
                {static_cast<float>(edge.first.x), static_cast<float>(edge.first.y)});
            if (edge.second == start) {
                break;
            }
            const auto range = outgoing.equal_range(pointKey(edge.second));
            std::size_t next = edges.size();
            for (auto candidate = range.first; candidate != range.second;
                 ++candidate) {
                if (!edges[candidate->second].used) {
                    next = std::min(next, candidate->second);
                }
            }
            if (next == edges.size()) {
                loop.clear();
                break;
            }
            edgeIndex = next;
        }
        if (loop.size() >= 3U) {
            loops.push_back(std::move(loop));
        }
    }
    return loops;
}

Point rotateAndTranslate(Point local, Point origin, float angle) noexcept {
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return {origin.x + local.x * cosine - local.y * sine,
            origin.y + local.x * sine + local.y * cosine};
}

struct Segment {
    Point first{};
    Point second{};
    float startDistance{};
    float length{};
};

std::vector<Segment> pathSegments(const Polyline& path, float& totalLength) {
    std::vector<Segment> segments;
    totalLength = 0.0F;
    if (path.points.size() < 2U) {
        return segments;
    }
    const std::size_t count =
        path.points.size() - 1U + (path.closed ? 1U : 0U);
    segments.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Point first = path.points[i];
        const Point second = path.points[(i + 1U) % path.points.size()];
        const float length = std::sqrt(squaredDistance(first, second));
        if (length <= kEpsilon) {
            continue;
        }
        segments.push_back({first, second, totalLength, length});
        totalLength += length;
    }
    return segments;
}

} // namespace

Polyline makeOrbitalPath(
    const Rect& bounds,
    std::uint64_t stableId,
    const OrbitSettings& settings) {
    const std::size_t maximum = std::max<std::size_t>(settings.maximumSegments, 3U);
    const std::size_t segmentCount =
        std::clamp<std::size_t>(settings.segments, 3U, maximum);
    const float radiusX = std::max(
        0.0F, bounds.width * 0.5F * std::max(0.0F, settings.radiusScaleX) +
                  settings.padding);
    const float radiusY = std::max(
        0.0F, bounds.height * 0.5F * std::max(0.0F, settings.radiusScaleY) +
                  settings.padding);
    const float sweep =
        std::clamp(settings.sweepAngleRadians, -kTwoPi, kTwoPi);
    const bool closed = std::fabs(std::fabs(sweep) - kTwoPi) <= 1.0e-4F;
    const std::size_t denominator =
        closed ? segmentCount : std::max<std::size_t>(segmentCount - 1U, 1U);
    Polyline path{};
    path.stableId = stableId;
    path.closed = closed;
    path.points.reserve(segmentCount);
    const Point center = bounds.center();
    for (std::size_t i = 0; i < segmentCount; ++i) {
        const float amount =
            static_cast<float>(i) / static_cast<float>(denominator);
        const float angle = settings.startAngleRadians + sweep * amount;
        const Point local{std::cos(angle) * radiusX,
                          std::sin(angle) * radiusY};
        path.points.push_back(
            rotateAndTranslate(local, center, settings.rotationRadians));
    }
    return path;
}

std::vector<Polyline> extractContourPaths(
    const Mask& mask,
    const ContourSettings& settings) {
    const int width = mask.width();
    const int height = mask.height();
    if (width <= 0 || height <= 0 || settings.maximumContours == 0U) {
        return {};
    }
    const auto index = [width](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
               static_cast<std::size_t>(x);
    };
    std::vector<std::uint8_t> visited(mask.values().size(), 0U);
    const auto& values = mask.values();
    const auto isActiveIndex = [&](std::size_t pixelIndex) {
        return values[pixelIndex] >= settings.threshold;
    };

    std::vector<Polyline> contours;
    std::uint64_t stableId = 1U;
    const int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int startY = 0; startY < height; ++startY) {
        for (int startX = 0; startX < width; ++startX) {
            const std::size_t startIndex = index(startX, startY);
            if (!isActiveIndex(startIndex) || visited[startIndex] != 0U) {
                continue;
            }
            std::vector<std::size_t> queue;
            std::vector<std::size_t> pixels;
            queue.push_back(startIndex);
            visited[startIndex] = 1U;
            while (!queue.empty()) {
                const std::size_t pixelIndex = queue.back();
                queue.pop_back();
                pixels.push_back(pixelIndex);
                const int pixelX = static_cast<int>(pixelIndex % width);
                const int pixelY = static_cast<int>(pixelIndex / width);
                for (const auto& offset : offsets) {
                    const int nx = pixelX + offset[0];
                    const int ny = pixelY + offset[1];
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                        continue;
                    }
                    const std::size_t neighbor = index(nx, ny);
                    if (isActiveIndex(neighbor) && visited[neighbor] == 0U) {
                        visited[neighbor] = 1U;
                        queue.push_back(neighbor);
                    }
                }
            }
            if (pixels.size() < settings.minimumArea) {
                ++stableId;
                continue;
            }

            std::vector<Edge> edges;
            edges.reserve(pixels.size() * 2U);
            const auto isActive = [&](int x, int y) {
                return x >= 0 && y >= 0 && x < width && y < height &&
                       isActiveIndex(index(x, y));
            };
            for (const std::size_t pixelIndex : pixels) {
                const int x = static_cast<int>(pixelIndex % width);
                const int y = static_cast<int>(pixelIndex / width);
                if (!isActive(x, y - 1)) {
                    edges.push_back({{x, y}, {x + 1, y}, false});
                }
                if (!isActive(x + 1, y)) {
                    edges.push_back({{x + 1, y}, {x + 1, y + 1}, false});
                }
                if (!isActive(x, y + 1)) {
                    edges.push_back({{x + 1, y + 1}, {x, y + 1}, false});
                }
                if (!isActive(x - 1, y)) {
                    edges.push_back({{x, y + 1}, {x, y}, false});
                }
            }
            auto loops = traceLoops(edges);
            if (loops.empty()) {
                ++stableId;
                continue;
            }
            auto outer = std::max_element(
                loops.begin(), loops.end(),
                [](const std::vector<Point>& first,
                   const std::vector<Point>& second) {
                    return std::fabs(signedArea(first)) <
                           std::fabs(signedArea(second));
                });
            simplifyClosed(*outer, settings.simplificationTolerance);
            capPoints(*outer, settings.maximumPointsPerContour);
            contours.push_back({stableId, true, std::move(*outer)});
            ++stableId;
            if (contours.size() >= settings.maximumContours) {
                return contours;
            }
        }
    }
    return contours;
}

std::vector<RepeaterInstance> repeatAlongPath(
    const Polyline& path,
    const PathRepeaterSettings& settings) {
    if (!std::isfinite(settings.spacing) || settings.spacing <= kEpsilon ||
        settings.maximumInstances == 0U) {
        return {};
    }
    float totalLength = 0.0F;
    const auto segments = pathSegments(path, totalLength);
    if (segments.empty()) {
        return {};
    }
    float distance = std::fmod(settings.phase, settings.spacing);
    if (distance < 0.0F) {
        distance += settings.spacing;
    }
    std::vector<RepeaterInstance> instances;
    instances.reserve(std::min<std::size_t>(
        settings.maximumInstances,
        static_cast<std::size_t>(totalLength / settings.spacing) + 1U));
    const float finalTolerance = path.closed ? -kEpsilon : kEpsilon;
    for (; distance <= totalLength + finalTolerance &&
           instances.size() < settings.maximumInstances;
         distance += settings.spacing) {
        auto segment = std::lower_bound(
            segments.begin(), segments.end(), distance,
            [](const Segment& value, float target) {
                return value.startDistance + value.length < target;
            });
        if (segment == segments.end()) {
            segment = std::prev(segments.end());
        }
        const float amount = std::clamp(
            (distance - segment->startDistance) / segment->length, 0.0F, 1.0F);
        const Point tangent{
            (segment->second.x - segment->first.x) / segment->length,
            (segment->second.y - segment->first.y) / segment->length};
        RepeaterInstance instance{};
        instance.stableId =
            path.stableId * 0x9E3779B185EBCA87ULL +
            static_cast<std::uint64_t>(instances.size() + 1U);
        instance.primitive = settings.primitive;
        instance.position = {
            segment->first.x +
                (segment->second.x - segment->first.x) * amount,
            segment->first.y +
                (segment->second.y - segment->first.y) * amount};
        instance.tangent = tangent;
        instance.rotationRadians =
            settings.alignToTangent ? std::atan2(tangent.y, tangent.x) : 0.0F;
        instance.scale = std::max(0.0F, settings.scale);
        instances.push_back(instance);
    }
    return instances;
}

std::vector<Polyline> buildRepeaterGlyph(
    const RepeaterInstance& instance,
    std::size_t ringSegments) {
    const float scale = std::max(0.0F, instance.scale);
    const auto transform = [&](Point point) {
        return rotateAndTranslate(
            {point.x * scale, point.y * scale},
            instance.position, instance.rotationRadians);
    };
    Polyline stroke{};
    stroke.stableId = instance.stableId;
    switch (instance.primitive) {
    case RepeaterPrimitive::Ring: {
        ringSegments = std::clamp<std::size_t>(ringSegments, 8U, 256U);
        stroke.closed = true;
        stroke.points.reserve(ringSegments);
        for (std::size_t i = 0; i < ringSegments; ++i) {
            const float angle =
                kTwoPi * static_cast<float>(i) /
                static_cast<float>(ringSegments);
            stroke.points.push_back(
                transform({std::cos(angle), std::sin(angle)}));
        }
        break;
    }
    case RepeaterPrimitive::Box:
        stroke.closed = true;
        stroke.points = {
            transform({-1.0F, -1.0F}), transform({1.0F, -1.0F}),
            transform({1.0F, 1.0F}), transform({-1.0F, 1.0F})};
        break;
    case RepeaterPrimitive::Arrow:
        stroke.points = {
            transform({-1.0F, 0.0F}), transform({1.0F, 0.0F}),
            transform({0.25F, -0.65F}), transform({1.0F, 0.0F}),
            transform({0.25F, 0.65F})};
        break;
    case RepeaterPrimitive::Tick:
        stroke.points = {
            transform({0.0F, -1.0F}), transform({0.0F, 1.0F})};
        break;
    }
    return {std::move(stroke)};
}

} // namespace linear
