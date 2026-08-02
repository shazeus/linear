#include "linear/Graphics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace linear {
namespace {

float pointDistanceSquared(Point lhs, Point rhs) noexcept {
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    return dx * dx + dy * dy;
}

void composite(Image& image, int x, int y, Color color, float coverage = 1.0F) {
    if (coverage <= 0.0F || color.a <= 0.0F) return;
    if (x < 0 || y < 0 || x >= image.width() || y >= image.height()) {
        return;
    }
    Color& destination = image.pixels()[
        static_cast<std::size_t>(y) * image.width() + x];
    const float alpha = clamp01(color.a * coverage);
    destination.r = color.r * alpha + destination.r * (1.0F - alpha);
    destination.g = color.g * alpha + destination.g * (1.0F - alpha);
    destination.b = color.b * alpha + destination.b * (1.0F - alpha);
    destination.a = alpha + destination.a * (1.0F - alpha);
}

void drawDisc(Image& image, Point center, float radius, Color color) {
    const int minimumX = static_cast<int>(std::floor(center.x - radius - 1.0F));
    const int maximumX = static_cast<int>(std::ceil(center.x + radius + 1.0F));
    const int minimumY = static_cast<int>(std::floor(center.y - radius - 1.0F));
    const int maximumY = static_cast<int>(std::ceil(center.y + radius + 1.0F));
    for (int y = minimumY; y <= maximumY; ++y) {
        for (int x = minimumX; x <= maximumX; ++x) {
            const float dx = (static_cast<float>(x) + 0.5F) - center.x;
            const float dy = (static_cast<float>(y) + 0.5F) - center.y;
            const float coverage = clamp01(radius + 0.5F - std::sqrt(dx * dx + dy * dy));
            composite(image, x, y, color, coverage);
        }
    }
}

void drawLine(Image& image, Point start, Point end, float width, Color color) {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 1.0e-8F) {
        drawDisc(image, start, std::max(0.5F, width * 0.5F), color);
        return;
    }
    const float length = std::sqrt(lengthSquared);
    const float inverseLength = 1.0F / length;
    const float normalX = -dy * inverseLength;
    const float normalY = dx * inverseLength;
    const int samples = std::clamp(
        static_cast<int>(std::ceil(std::max(1.0F, width))), 1, 20);
    const float sampleOpacity = std::min(1.0F, width / samples);
    const int steps = std::max(
        1, static_cast<int>(std::ceil(std::max(std::fabs(dx), std::fabs(dy)))));
    for (int sample = 0; sample < samples; ++sample) {
        const float offset = static_cast<float>(sample) + 0.5F -
                             static_cast<float>(samples) * 0.5F;
        const Point shiftedStart{
            start.x + normalX * offset, start.y + normalY * offset};
        Color sampleColor = color;
        sampleColor.a *= sampleOpacity;
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float x = shiftedStart.x + dx * t;
            const float y = shiftedStart.y + dy * t;
            const int baseX = static_cast<int>(std::floor(x));
            const int baseY = static_cast<int>(std::floor(y));
            const float fractionX = x - static_cast<float>(baseX);
            const float fractionY = y - static_cast<float>(baseY);
            composite(image, baseX, baseY, sampleColor,
                      (1.0F - fractionX) * (1.0F - fractionY));
            composite(image, baseX + 1, baseY, sampleColor,
                      fractionX * (1.0F - fractionY));
            composite(image, baseX, baseY + 1, sampleColor,
                      (1.0F - fractionX) * fractionY);
            composite(image, baseX + 1, baseY + 1, sampleColor,
                      fractionX * fractionY);
        }
    }
}

void drawPatternedLine(
    Image& image, Point start, Point end, float width, Color color,
    bool dashed, float dashLength, float gapLength, float& phase) {
    if (!dashed) {
        drawLine(image, start, end, width, color);
        return;
    }
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 1.0e-6F) return;
    const float dash = std::max(1.0F, dashLength);
    const float gap = std::max(1.0F, gapLength);
    const float period = dash + gap;
    float position = 0.0F;
    while (position < length) {
        float inPeriod = std::fmod(phase, period);
        if (inPeriod < 0.0F) inPeriod += period;
        const bool drawing = inPeriod < dash;
        const float boundary = drawing ? dash : period;
        const float advance = std::min(length - position, boundary - inPeriod);
        if (drawing && advance > 0.0F) {
            const float first = position / length;
            const float second = (position + advance) / length;
            drawLine(
                image, {start.x + dx * first, start.y + dy * first},
                {start.x + dx * second, start.y + dy * second}, width, color);
        }
        position += std::max(advance, 1.0e-4F);
        phase = std::fmod(phase + std::max(advance, 1.0e-4F), period);
    }
}

void drawConnection(
    Image& image, Point start, Point end, const OverlayStyle& style,
    Color color, Color arrowColor, std::size_t stableSeed) {
    float dashPhase = 0.0F;
    auto routeSegment = [&](Point first, Point second) {
        drawPatternedLine(
            image, first, second, style.connectionWidth, color,
            style.connectionDashEnabled, style.connectionDashLength,
            style.connectionGapLength, dashPhase);
    };
    const auto finishArrow = [&] {
        if (!style.connectionArrowsEnabled) return;
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 1.0F) return;
        const float ux = dx / length;
        const float uy = dy / length;
        const float size = std::clamp(style.connectionArrowSize, 2.0F, 100.0F);
        const Point base{end.x - ux * size, end.y - uy * size};
        const float wing = size * 0.45F;
        drawLine(image, end, {base.x - uy * wing, base.y + ux * wing},
                 style.connectionWidth, arrowColor);
        drawLine(image, end, {base.x + uy * wing, base.y - ux * wing},
                 style.connectionWidth, arrowColor);
    };

    if (style.connectionStyle == ConnectionStyle::Straight ||
        style.connectionCurvature <= 0.0F) {
        routeSegment(start, end);
        finishArrow();
        return;
    }

    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 1.0F) {
        routeSegment(start, end);
        finishArrow();
        return;
    }

    if (style.connectionStyle == ConnectionStyle::StepBend) {
        const float midpointX = start.x + dx *
            std::clamp(style.connectionCurvature, 0.05F, 0.95F);
        const Point first{midpointX, start.y};
        const Point second{midpointX, end.y};
        routeSegment(start, first);
        routeSegment(first, second);
        routeSegment(second, end);
        finishArrow();
        return;
    }

    if (style.connectionStyle == ConnectionStyle::PcbTrace) {
        const float lead = std::min(
            length * std::clamp(style.connectionCurvature, 0.1F, 0.45F), 160.0F);
        const float signX = dx >= 0.0F ? 1.0F : -1.0F;
        const Point first{start.x + signX * lead, start.y};
        const Point second{end.x - signX * lead, end.y};
        routeSegment(start, first);
        routeSegment(first, second);
        routeSegment(second, end);
        finishArrow();
        return;
    }

    const float direction = (stableSeed & 1U) == 0U ? 1.0F : -1.0F;
    const float bow = std::min(length * 0.45F, 220.0F) *
                      std::clamp(style.connectionCurvature, 0.0F, 1.0F) *
                      direction;
    const Point control{
        (start.x + end.x) * 0.5F - (dy / length) * bow,
        (start.y + end.y) * 0.5F + (dx / length) * bow};
    const int segments = std::clamp(
        static_cast<int>(std::ceil(length / 8.0F)), 8, 96);
    if (style.connectionStyle == ConnectionStyle::SmoothBend) {
        const Point firstControl{
            start.x + dx * 0.33F - (dy / length) * bow,
            start.y + dy * 0.33F + (dx / length) * bow};
        const Point secondControl{
            start.x + dx * 0.67F + (dy / length) * bow,
            start.y + dy * 0.67F - (dx / length) * bow};
        Point previous = start;
        for (int segment = 1; segment <= segments; ++segment) {
            const float t = static_cast<float>(segment) /
                            static_cast<float>(segments);
            const float inverse = 1.0F - t;
            const Point next{
                inverse * inverse * inverse * start.x +
                    3.0F * inverse * inverse * t * firstControl.x +
                    3.0F * inverse * t * t * secondControl.x +
                    t * t * t * end.x,
                inverse * inverse * inverse * start.y +
                    3.0F * inverse * inverse * t * firstControl.y +
                    3.0F * inverse * t * t * secondControl.y +
                    t * t * t * end.y};
            routeSegment(previous, next);
            previous = next;
        }
        finishArrow();
        return;
    }
    Point previous = start;
    for (int segment = 1; segment <= segments; ++segment) {
        const float t = static_cast<float>(segment) /
                        static_cast<float>(segments);
        const float inverse = 1.0F - t;
        const Point next{
            inverse * inverse * start.x + 2.0F * inverse * t * control.x +
                t * t * end.x,
            inverse * inverse * start.y + 2.0F * inverse * t * control.y +
                t * t * end.y};
        routeSegment(previous, next);
        previous = next;
    }
    finishArrow();
}

void drawRectangle(Image& image, Rect bounds, float width, Color color,
                   bool brackets, float cornerLengthFraction) {
    const Point topLeft{bounds.x, bounds.y};
    const Point topRight{bounds.x + bounds.width, bounds.y};
    const Point bottomLeft{bounds.x, bounds.y + bounds.height};
    const Point bottomRight{bounds.x + bounds.width, bounds.y + bounds.height};
    if (!brackets) {
        drawLine(image, topLeft, topRight, width, color);
        drawLine(image, topRight, bottomRight, width, color);
        drawLine(image, bottomRight, bottomLeft, width, color);
        drawLine(image, bottomLeft, topLeft, width, color);
        return;
    }
    const float length = std::max(
        2.0F, std::min(bounds.width, bounds.height) *
                  std::clamp(cornerLengthFraction, 0.05F, 0.5F));
    drawLine(image, topLeft, {topLeft.x + length, topLeft.y}, width, color);
    drawLine(image, topLeft, {topLeft.x, topLeft.y + length}, width, color);
    drawLine(image, topRight, {topRight.x - length, topRight.y}, width, color);
    drawLine(image, topRight, {topRight.x, topRight.y + length}, width, color);
    drawLine(image, bottomLeft, {bottomLeft.x + length, bottomLeft.y}, width, color);
    drawLine(image, bottomLeft, {bottomLeft.x, bottomLeft.y - length}, width, color);
    drawLine(image, bottomRight, {bottomRight.x - length, bottomRight.y}, width, color);
    drawLine(image, bottomRight, {bottomRight.x, bottomRight.y - length}, width, color);
}

void drawEllipse(Image& image, Rect bounds, float width, Color color) {
    constexpr float twoPi = 6.28318530717958647692F;
    const Point center = bounds.center();
    const float radiusX = bounds.width * 0.5F;
    const float radiusY = bounds.height * 0.5F;
    const float maximumRadius = std::max(radiusX, radiusY);
    // Choose segments from a sub-pixel chord-error target. The old one-segment
    // per circumference pixel rule generated thousands of tiny draw calls for
    // large region circles without a visible quality benefit.
    constexpr float maximumChordError = 0.35F;
    const float safeRadius = std::max(maximumRadius, maximumChordError);
    const float halfAngle = std::acos(std::clamp(
        1.0F - maximumChordError / safeRadius, -1.0F, 1.0F));
    const int segments = std::clamp(
        static_cast<int>(std::ceil(3.14159265358979323846F /
                                   std::max(halfAngle, 1.0e-3F))),
        24, 512);
    Point previous{center.x + radiusX, center.y};
    for (int i = 1; i <= segments; ++i) {
        const float angle = twoPi * static_cast<float>(i) / static_cast<float>(segments);
        const Point next{center.x + std::cos(angle) * radiusX,
                         center.y + std::sin(angle) * radiusY};
        drawLine(image, previous, next, width, color);
        previous = next;
    }
}

void drawMarker(Image& image, Point center, const OverlayStyle& style, Color color) {
    const float radius = std::max(1.0F, style.markerSize * 0.5F);
    const auto drawClosed = [&](const std::vector<Point>& points) {
        if (points.size() < 2U) return;
        for (std::size_t i = 0; i < points.size(); ++i) {
            drawLine(
                image, points[i], points[(i + 1U) % points.size()],
                style.markerStrokeWidth, color);
        }
    };
    switch (style.markerShape) {
    case MarkerShape::Dot:
        drawDisc(image, center, radius, color);
        break;
    case MarkerShape::Ring:
        drawEllipse(image, {center.x - radius, center.y - radius,
                            radius * 2.0F, radius * 2.0F},
                    style.markerStrokeWidth, color);
        break;
    case MarkerShape::Plus:
        drawLine(image, {center.x - radius, center.y},
                 {center.x + radius, center.y}, style.markerStrokeWidth, color);
        drawLine(image, {center.x, center.y - radius},
                 {center.x, center.y + radius}, style.markerStrokeWidth, color);
        break;
    case MarkerShape::Cross:
        drawLine(image, {center.x - radius, center.y - radius},
                 {center.x + radius, center.y + radius}, style.markerStrokeWidth, color);
        drawLine(image, {center.x - radius, center.y + radius},
                 {center.x + radius, center.y - radius}, style.markerStrokeWidth, color);
        break;
    case MarkerShape::Triangle:
        drawClosed({
            {center.x, center.y - radius},
            {center.x + radius * 0.8660254F, center.y + radius * 0.5F},
            {center.x - radius * 0.8660254F, center.y + radius * 0.5F}});
        break;
    case MarkerShape::Star: {
        std::vector<Point> points;
        points.reserve(10U);
        constexpr float pi = 3.14159265358979323846F;
        for (int i = 0; i < 10; ++i) {
            const float activeRadius = (i & 1) == 0 ? radius : radius * 0.42F;
            const float angle = -pi * 0.5F + static_cast<float>(i) * pi / 5.0F;
            points.push_back({
                center.x + std::cos(angle) * activeRadius,
                center.y + std::sin(angle) * activeRadius});
        }
        drawClosed(points);
        break;
    }
    case MarkerShape::Diamond:
        drawClosed({
            {center.x, center.y - radius}, {center.x + radius, center.y},
            {center.x, center.y + radius}, {center.x - radius, center.y}});
        break;
    case MarkerShape::Target:
        drawEllipse(
            image, {center.x - radius, center.y - radius,
                    radius * 2.0F, radius * 2.0F},
            style.markerStrokeWidth, color);
        drawEllipse(
            image, {center.x - radius * 0.45F, center.y - radius * 0.45F,
                    radius * 0.9F, radius * 0.9F},
            style.markerStrokeWidth, color);
        drawLine(image, {center.x - radius * 1.25F, center.y},
                 {center.x + radius * 1.25F, center.y},
                 style.markerStrokeWidth, color);
        drawLine(image, {center.x, center.y - radius * 1.25F},
                 {center.x, center.y + radius * 1.25F},
                 style.markerStrokeWidth, color);
        break;
    case MarkerShape::Chevron:
        drawLine(image, {center.x - radius, center.y - radius * 0.65F},
                 {center.x, center.y}, style.markerStrokeWidth, color);
        drawLine(image, {center.x, center.y},
                 {center.x - radius, center.y + radius * 0.65F},
                 style.markerStrokeWidth, color);
        drawLine(image, {center.x, center.y - radius * 0.65F},
                 {center.x + radius, center.y}, style.markerStrokeWidth, color);
        drawLine(image, {center.x + radius, center.y},
                 {center.x, center.y + radius * 0.65F},
                 style.markerStrokeWidth, color);
        break;
    }
}

bool overlaps(Rect first, Rect second) noexcept {
    return first.x < second.x + second.width && first.x + first.width > second.x &&
           first.y < second.y + second.height && first.y + first.height > second.y;
}

std::string labelText(
    const Region& region, LabelMode mode, const std::string& customText) {
    switch (mode) {
    case LabelMode::NodeId: return "NODE_" + std::to_string(region.componentId);
    case LabelMode::Area: return std::to_string(region.pixelArea);
    case LabelMode::Dimensions:
        return std::to_string(static_cast<int>(std::lround(region.bounds.width))) +
               "X" + std::to_string(static_cast<int>(std::lround(region.bounds.height)));
    case LabelMode::TrackId: return "ID_" + std::to_string(region.trackId % 100000U);
    case LabelMode::Confidence:
        return "C_" + std::to_string(static_cast<int>(std::lround(clamp01(region.confidence) * 100.0F))) + "%";
    case LabelMode::CustomText: return customText;
    case LabelMode::Off: return {};
    }
    return {};
}

void applyFill(
    Image& image, const Image* sourceImage,
    const std::vector<Region>& regions,
    const OverlayStyle& style) {
    if (style.fillMode == FillMode::Off || regions.empty()) return;
    if (sourceImage != nullptr &&
        (sourceImage->width() != image.width() ||
         sourceImage->height() != image.height())) {
        throw std::invalid_argument("Fill source dimensions must match output");
    }
    const float inset = std::max(0.0F, style.fillInset);
    const float feather = std::max(0.0F, style.fillFeather);
    int unionLeft = image.width();
    int unionTop = image.height();
    int unionRight = 0;
    int unionBottom = 0;
    for (const Region& region : regions) {
        const float left = region.bounds.x + inset;
        const float top = region.bounds.y + inset;
        const float right = region.bounds.x + region.bounds.width - inset;
        const float bottom = region.bounds.y + region.bounds.height - inset;
        if (right <= left || bottom <= top) continue;
        const int minimumX = std::clamp(
            static_cast<int>(std::floor(left)), 0, image.width());
        const int maximumX = std::clamp(
            static_cast<int>(std::ceil(right)), 0, image.width());
        const int minimumY = std::clamp(
            static_cast<int>(std::floor(top)), 0, image.height());
        const int maximumY = std::clamp(
            static_cast<int>(std::ceil(bottom)), 0, image.height());
        unionLeft = std::min(unionLeft, minimumX);
        unionTop = std::min(unionTop, minimumY);
        unionRight = std::max(unionRight, maximumX);
        unionBottom = std::max(unionBottom, maximumY);
    }
    if (unionRight <= unionLeft || unionBottom <= unionTop) return;
    const int coverageWidth = unionRight - unionLeft;
    const int coverageHeight = unionBottom - unionTop;
    Mask boxCoverage(coverageWidth, coverageHeight);
    auto& coverageValues = boxCoverage.values();
    const bool ellipseCoverage = style.boxShape == BoxShape::Ellipse;
    for (const Region& region : regions) {
        const float left = region.bounds.x + inset;
        const float top = region.bounds.y + inset;
        const float right = region.bounds.x + region.bounds.width - inset;
        const float bottom = region.bounds.y + region.bounds.height - inset;
        if (right <= left || bottom <= top) continue;
        const float centerRegionX = (left + right) * 0.5F;
        const float centerRegionY = (top + bottom) * 0.5F;
        const float radiusX = (right - left) * 0.5F;
        const float radiusY = (bottom - top) * 0.5F;
        const int minimumX = std::clamp(
            static_cast<int>(std::floor(left)), unionLeft, unionRight);
        const int maximumX = std::clamp(
            static_cast<int>(std::ceil(right)), unionLeft, unionRight);
        const int minimumY = std::clamp(
            static_cast<int>(std::floor(top)), unionTop, unionBottom);
        const int maximumY = std::clamp(
            static_cast<int>(std::ceil(bottom)), unionTop, unionBottom);
        for (int y = minimumY; y < maximumY; ++y) {
            const std::size_t coverageRow =
                static_cast<std::size_t>(y - unionTop) * coverageWidth;
            for (int x = minimumX; x < maximumX; ++x) {
                const float centerX = static_cast<float>(x) + 0.5F;
                const float centerY = static_cast<float>(y) + 0.5F;
                if (centerX < left || centerX >= right ||
                    centerY < top || centerY >= bottom) {
                    continue;
                }
                float edgeDistance = 0.0F;
                if (ellipseCoverage) {
                    const float normalizedX =
                        (centerX - centerRegionX) / radiusX;
                    const float normalizedY =
                        (centerY - centerRegionY) / radiusY;
                    const float normalizedRadiusSquared =
                        normalizedX * normalizedX + normalizedY * normalizedY;
                    if (normalizedRadiusSquared >= 1.0F) continue;

                    if (feather > 0.0F) {
                        // Convert normalized radial distance to pixels so one
                        // feather value remains predictable across shapes.
                        edgeDistance =
                            (1.0F - std::sqrt(normalizedRadiusSquared)) *
                            std::min(radiusX, radiusY);
                    }
                } else {
                    edgeDistance = std::min({
                        centerX - left, right - centerX,
                        centerY - top, bottom - centerY});
                }
                const float coverage = feather <= 0.0F
                    ? 1.0F
                    : clamp01(edgeDistance / feather);
                float& destination = coverageValues[
                    coverageRow + static_cast<std::size_t>(x - unionLeft)];
                destination = std::max(destination, coverage);
            }
        }
    }

    const float opacity = clamp01(style.fillOpacity * style.globalOpacity);
    const int spacing = std::clamp(style.hatchSpacing, 3, 128);
    const int hatchWidth = std::clamp(style.hatchWidth, 1, spacing);
    constexpr float degreesToRadians =
        3.14159265358979323846F / 180.0F;
    const float angle = style.hatchAngleDegrees * degreesToRadians;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float crossCosine = -sine;
    const float crossSine = cosine;
    const float spacingValue = static_cast<float>(spacing);
    const auto normalizedPhase = [&](float value) {
        value = std::fmod(value, spacingValue);
        return value < 0.0F ? value + spacingValue : value;
    };
    const auto advancePhase = [&](float value, float increment) {
        value += increment;
        if (value >= spacingValue) value -= spacingValue;
        if (value < 0.0F) value += spacingValue;
        return value;
    };
    constexpr std::size_t lutSize = 1024U;
    static const std::array<Color, lutSize> xrayLut = [] {
        std::array<Color, lutSize> values{};
        for (std::size_t i = 0; i < lutSize; ++i) {
            const float negative = static_cast<float>(i) /
                                   static_cast<float>(lutSize - 1U);
            values[i] = {
                std::pow(negative, 1.25F) * 0.72F,
                std::pow(negative, 0.92F) * 0.96F,
                std::min(1.0F, std::pow(negative, 0.78F) * 1.12F), 1.0F};
        }
        return values;
    }();
    auto& pixels = image.pixels();
    const auto& sourcePixels =
        sourceImage != nullptr ? sourceImage->pixels() : pixels;
    const bool hatchMode = style.fillMode == FillMode::Hatch ||
                           style.fillMode == FillMode::Crosshatch;
    for (int y = unionTop; y < unionBottom; ++y) {
        const std::size_t coverageRow =
            static_cast<std::size_t>(y - unionTop) * coverageWidth;
        const std::size_t imageRow = static_cast<std::size_t>(y) * image.width();
        float primaryPhase = hatchMode
            ? normalizedPhase(static_cast<float>(unionLeft) * cosine +
                              static_cast<float>(y) * sine)
            : 0.0F;
        float secondaryPhase = style.fillMode == FillMode::Crosshatch
            ? normalizedPhase(static_cast<float>(unionLeft) * crossCosine +
                              static_cast<float>(y) * crossSine)
            : 0.0F;
        for (int x = unionLeft; x < unionRight; ++x) {
            const bool primaryHatch =
                primaryPhase < static_cast<float>(hatchWidth);
            const bool secondaryHatch =
                secondaryPhase < static_cast<float>(hatchWidth);
            if (hatchMode) {
                primaryPhase = advancePhase(primaryPhase, cosine);
                secondaryPhase = advancePhase(secondaryPhase, crossCosine);
            }
            const float boxAlpha = clamp01(coverageValues[
                coverageRow + static_cast<std::size_t>(x - unionLeft)]);
            if (boxAlpha <= 0.0F) continue;
            Color color = style.fillColor;
            float coverage = boxAlpha * opacity;
            switch (style.fillMode) {
            case FillMode::Hatch:
                if (!primaryHatch) continue;
                break;
            case FillMode::Crosshatch:
                if (!primaryHatch && !secondaryHatch) {
                    continue;
                }
                break;
            case FillMode::XRay: {
                const Color original = sourcePixels[imageRow + x];
                const float luma = clamp01(
                    original.r * 0.2126F + original.g * 0.7152F +
                    original.b * 0.0722F);
                const std::size_t lutIndex = static_cast<std::size_t>(
                    std::lround((1.0F - luma) * (lutSize - 1U)));
                color = xrayLut[std::min(lutSize - 1U, lutIndex)];
                break;
            }
            case FillMode::Duotone: {
                const Color original = sourcePixels[imageRow + x];
                const float luma = original.r * 0.2126F + original.g * 0.7152F + original.b * 0.0722F;
                color = {
                    style.fillSecondaryColor.r * (1.0F - luma) +
                        style.fillColor.r * luma,
                    style.fillSecondaryColor.g * (1.0F - luma) +
                        style.fillColor.g * luma,
                    style.fillSecondaryColor.b * (1.0F - luma) +
                        style.fillColor.b * luma,
                    1.0F};
                break;
            }
            case FillMode::Invert: {
                const Color original = sourcePixels[imageRow + x];
                const float strength = clamp01(style.fillInvertStrength);
                coverage = boxAlpha * clamp01(style.globalOpacity) *
                    (clamp01(style.fillOpacity) +
                     (1.0F - clamp01(style.fillOpacity)) * strength);
                color = {
                    original.r + ((1.0F - original.r) - original.r) * strength,
                    original.g + ((1.0F - original.g) - original.g) * strength,
                    original.b + ((1.0F - original.b) - original.b) * strength,
                    1.0F};
                break;
            }
            case FillMode::Solid: case FillMode::Off: break;
            }
            Color& destination = pixels[imageRow + x];
            const float inverseAlpha = 1.0F - coverage;
            destination.r = color.r * coverage + destination.r * inverseAlpha;
            destination.g = color.g * coverage + destination.g * inverseAlpha;
            destination.b = color.b * coverage + destination.b * inverseAlpha;
            destination.a = coverage + destination.a * inverseAlpha;
        }
    }
}

void drawLabels(Image& image, const std::vector<Region>& regions,
                const OverlayStyle& style, Color lineColor) {
    if (style.labelMode == LabelMode::Off || style.maximumLabels == 0U) return;
    std::vector<Rect> occupied;
    const std::size_t count = std::min(regions.size(), style.maximumLabels);
    occupied.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::string text = labelText(
            regions[i], style.labelMode, style.customLabelText);
        const TextMetrics metrics = measureBitmapText(text, style.labelStyle);
        const Point anchor = regions[i].weightedCentroid;
        const std::array<Point, 4U> candidates{{
            {anchor.x + 10.0F + style.labelOffsetX,
             anchor.y - metrics.height - 8.0F + style.labelOffsetY},
            {anchor.x - metrics.width - 10.0F + style.labelOffsetX,
             anchor.y - metrics.height - 8.0F + style.labelOffsetY},
            {anchor.x + 10.0F + style.labelOffsetX,
             anchor.y + 8.0F + style.labelOffsetY},
            {anchor.x - metrics.width - 10.0F + style.labelOffsetX,
             anchor.y + 8.0F + style.labelOffsetY}}};
        Rect chosen{};
        bool found = false;
        for (Point candidate : candidates) {
            candidate.x = std::clamp(candidate.x, 0.0F,
                std::max(0.0F, static_cast<float>(image.width() - metrics.width)));
            candidate.y = std::clamp(candidate.y, 0.0F,
                std::max(0.0F, static_cast<float>(image.height() - metrics.height)));
            Rect box{candidate.x, candidate.y, static_cast<float>(metrics.width),
                     static_cast<float>(metrics.height)};
            if (std::none_of(occupied.begin(), occupied.end(),
                             [&](Rect value) { return overlaps(box, value); })) {
                chosen = box; found = true; break;
            }
        }
        if (!found) continue;
        if (style.labelLeaders) {
            drawLine(image, anchor, chosen.center(), 1.0F, lineColor);
            drawDisc(image, anchor, 2.0F, lineColor);
        }
        rasterizeBitmapText(image, {chosen.x, chosen.y}, text, style.labelStyle);
        occupied.push_back(chosen);
    }
}

void drawGrid(
    Image& image, const std::vector<Region>& regions,
    const OverlayStyle& style, Color gridColor, Color projectionColor) {
    const int columns = std::clamp(style.gridDivisionsX, 2, 64);
    const int rows = std::clamp(style.gridDivisionsY, 2, 64);
    const float width = static_cast<float>(image.width());
    const float height = static_cast<float>(image.height());
    const float maximumX = std::max(0.0F, width - 1.0F);
    const float maximumY = std::max(0.0F, height - 1.0F);
    const float centerX = maximumX * 0.5F;
    const float centerY = maximumY * 0.5F;
    const bool cartesian = style.gridMode == GridMode::Cartesian;
    const int firstColumn = cartesian ? 0 : 1;
    const int lastColumn = cartesian ? columns : columns - 1;
    const int firstRow = cartesian ? 0 : 1;
    const int lastRow = cartesian ? rows : rows - 1;

    if (gridColor.a > 0.0F) {
        for (int i = firstColumn; i <= lastColumn; ++i) {
            const float x = maximumX * static_cast<float>(i) /
                            static_cast<float>(columns);
            const bool axis = cartesian && i * 2 == columns;
            drawLine(image, {x, 0.0F}, {x, maximumY},
                     style.gridWidth * (axis ? 1.6F : 1.0F), gridColor);
            if (cartesian && style.gridTickSize > 0.0F) {
                drawLine(image, {x, centerY - style.gridTickSize},
                         {x, centerY + style.gridTickSize}, style.gridWidth,
                         gridColor);
            }
        }
        for (int i = firstRow; i <= lastRow; ++i) {
            const float y = maximumY * static_cast<float>(i) /
                            static_cast<float>(rows);
            const bool axis = cartesian && i * 2 == rows;
            drawLine(image, {0.0F, y}, {maximumX, y},
                     style.gridWidth * (axis ? 1.6F : 1.0F), gridColor);
            if (cartesian && style.gridTickSize > 0.0F) {
                drawLine(image, {centerX - style.gridTickSize, y},
                         {centerX + style.gridTickSize, y}, style.gridWidth,
                         gridColor);
            }
        }

        // Cartesian axes must remain centered even when the requested division
        // count is odd (for example, the reference layout's 5 x 5 grid).
        if (cartesian) {
            drawLine(image, {centerX, 0.0F}, {centerX, maximumY},
                     style.gridWidth * 1.6F, gridColor);
            drawLine(image, {0.0F, centerY}, {maximumX, centerY},
                     style.gridWidth * 1.6F, gridColor);
        }
    }

    if (cartesian && style.gridAxisLabels && gridColor.a > 0.0F) {
        BitmapTextStyle textStyle{};
        textStyle.scale = 1;
        textStyle.padding = 1;
        textStyle.characterSpacing = 1;
        textStyle.foreground = gridColor;
        textStyle.backgroundEnabled = false;
        const int columnStep = std::max(1, (columns + 10) / 11);
        for (int i = 0; i <= columns; i += columnStep) {
            const int value = -100 + static_cast<int>(std::lround(
                200.0 * static_cast<double>(i) / columns));
            const std::string text = std::to_string(value);
            const TextMetrics metrics = measureBitmapText(text, textStyle);
            const float x = std::clamp(
                maximumX * static_cast<float>(i) / static_cast<float>(columns) -
                    static_cast<float>(metrics.width) * 0.5F,
                0.0F, std::max(0.0F, width - metrics.width));
            const float y = std::clamp(
                centerY + style.gridTickSize + 3.0F,
                0.0F, std::max(0.0F, height - metrics.height));
            rasterizeBitmapText(image, {x, y}, text, textStyle);
        }
        const int rowStep = std::max(1, (rows + 8) / 9);
        for (int i = 0; i <= rows; i += rowStep) {
            const int value = 100 - static_cast<int>(std::lround(
                200.0 * static_cast<double>(i) / rows));
            const std::string text = std::to_string(value);
            const TextMetrics metrics = measureBitmapText(text, textStyle);
            const float x = std::clamp(
                centerX + style.gridTickSize + 3.0F,
                0.0F, std::max(0.0F, width - metrics.width));
            const float y = std::clamp(
                maximumY * static_cast<float>(i) / static_cast<float>(rows) -
                    static_cast<float>(metrics.height) * 0.5F,
                0.0F, std::max(0.0F, height - metrics.height));
            rasterizeBitmapText(image, {x, y}, text, textStyle);
        }
    }

    if (cartesian && style.gridProjections && projectionColor.a > 0.0F) {
        const std::size_t count = std::min<std::size_t>(regions.size(), 128U);
        for (std::size_t i = 0; i < count; ++i) {
            const Point anchor = regions[i].weightedCentroid;
            float horizontalPhase = 0.0F;
            drawPatternedLine(
                image, anchor, {centerX, anchor.y}, style.gridWidth,
                projectionColor, true, style.projectionDashLength,
                style.projectionGapLength, horizontalPhase);
            float verticalPhase = 0.0F;
            drawPatternedLine(
                image, anchor, {anchor.x, centerY}, style.gridWidth,
                projectionColor, true, style.projectionDashLength,
                style.projectionGapLength, verticalPhase);
        }
    }
}

} // namespace

std::vector<Connection> buildConnections(
    const std::vector<Region>& regions,
    ConnectionTopology topology,
    std::size_t maximumConnections,
    float maximumDistance) {
    std::vector<Connection> connections;
    if (regions.size() < 2U || maximumConnections == 0U) {
        return connections;
    }
    if (topology == ConnectionTopology::Sequential) {
        for (std::size_t i = 1; i < regions.size() &&
                                connections.size() < maximumConnections; ++i) {
            connections.push_back({i - 1U, i});
        }
        return connections;
    }
    if (topology == ConnectionTopology::NearestNeighbor) {
        for (std::size_t i = 0; i < regions.size() &&
                                connections.size() < maximumConnections; ++i) {
            std::size_t nearest = i;
            float nearestDistance = std::numeric_limits<float>::infinity();
            for (std::size_t j = 0; j < regions.size(); ++j) {
                if (i == j) {
                    continue;
                }
                const float candidate =
                    pointDistanceSquared(regions[i].centroid, regions[j].centroid);
                if (candidate < nearestDistance) {
                    nearestDistance = candidate;
                    nearest = j;
                }
            }
            const Connection connection{std::min(i, nearest), std::max(i, nearest)};
            const bool exists = std::any_of(
                connections.begin(), connections.end(), [&](const Connection& value) {
                    return value.first == connection.first &&
                           value.second == connection.second;
                });
            if (!exists) {
                connections.push_back(connection);
            }
        }
        return connections;
    }
    if (topology == ConnectionTopology::Star) {
        const auto hub = static_cast<std::size_t>(std::distance(
            regions.begin(), std::max_element(
                regions.begin(), regions.end(),
                [](const Region& lhs, const Region& rhs) {
                    if (lhs.confidence != rhs.confidence) {
                        return lhs.confidence < rhs.confidence;
                    }
                    return lhs.pixelArea < rhs.pixelArea;
                })));
        for (std::size_t i = 0; i < regions.size() &&
                                connections.size() < maximumConnections; ++i) {
            if (i != hub) connections.push_back({hub, i});
        }
        return connections;
    }
    if (topology == ConnectionTopology::FullMesh ||
        topology == ConnectionTopology::Radius) {
        for (std::size_t i = 0; i < regions.size(); ++i) {
            for (std::size_t j = i + 1U; j < regions.size(); ++j) {
                if (topology == ConnectionTopology::Radius &&
                    pointDistanceSquared(
                        regions[i].centroid, regions[j].centroid) >
                        std::max(0.0F, maximumDistance) *
                            std::max(0.0F, maximumDistance)) {
                    continue;
                }
                connections.push_back({i, j});
                if (connections.size() >= maximumConnections) return connections;
            }
        }
        return connections;
    }

    // Deterministic Prim minimum spanning tree.
    std::vector<bool> included(regions.size(), false);
    included[0] = true;
    while (connections.size() + 1U < regions.size() &&
           connections.size() < maximumConnections) {
        float best = std::numeric_limits<float>::infinity();
        Connection candidate{};
        bool found = false;
        for (std::size_t i = 0; i < regions.size(); ++i) {
            if (!included[i]) {
                continue;
            }
            for (std::size_t j = 0; j < regions.size(); ++j) {
                if (included[j]) {
                    continue;
                }
                const float distance = pointDistanceSquared(
                    regions[i].centroid, regions[j].centroid);
                if (distance < best) {
                    best = distance;
                    candidate = {i, j};
                    found = true;
                }
            }
        }
        if (!found) {
            break;
        }
        connections.push_back(candidate);
        included[candidate.second] = true;
    }
    return connections;
}

void rasterizePolylines(
    Image& image,
    const std::vector<Polyline>& paths,
    Color color,
    float strokeWidth) {
    if (!std::isfinite(strokeWidth) || strokeWidth <= 0.0F) {
        return;
    }
    for (const Polyline& path : paths) {
        if (path.points.size() < 2U) {
            continue;
        }
        for (std::size_t i = 1; i < path.points.size(); ++i) {
            drawLine(
                image, path.points[i - 1U], path.points[i], strokeWidth, color);
        }
        if (path.closed && path.points.size() > 2U) {
            drawLine(
                image, path.points.back(), path.points.front(), strokeWidth, color);
        }
    }
}

Image renderOverlay(
    Image source,
    const Mask& detectionMask,
    const std::vector<Region>& regions,
    const OverlayStyle& style,
    OutputMode outputMode) {
    if (outputMode == OutputMode::DetectionMask) {
        if (source.width() != detectionMask.width() ||
            source.height() != detectionMask.height()) {
            throw std::invalid_argument("Source and mask dimensions must match");
        }
        Image maskImage(source.width(), source.height(), {0.0F, 0.0F, 0.0F, 1.0F});
        for (std::size_t i = 0; i < detectionMask.values().size(); ++i) {
            const float value = clamp01(detectionMask.values()[i]);
            maskImage.pixels()[i] = {value, value, value, 1.0F};
        }
        return maskImage;
    }
    const Image* fillSource =
        outputMode == OutputMode::OverlayOnly ? &source : nullptr;
    Image output = outputMode == OutputMode::OverlayOnly
                       ? Image(source.width(), source.height(),
                               {0.0F, 0.0F, 0.0F, 0.0F})
                       : std::move(source);
    Color boxColor = style.boxColor;
    boxColor.a *= clamp01(style.globalOpacity * style.boxOpacity);
    Color markerColor = style.markerColor;
    markerColor.a *= clamp01(style.globalOpacity * style.markerOpacity);
    Color connectionColor = style.connectionColor;
    connectionColor.a *= clamp01(
        style.globalOpacity * style.connectionOpacity);
    Color arrowColor = style.connectionArrowColor;
    arrowColor.a *= clamp01(
        style.globalOpacity * style.connectionArrowOpacity);
    Color gridColor = style.gridColor;
    gridColor.a *= clamp01(style.globalOpacity * style.gridOpacity);
    Color projectionColor = style.projectionColor;
    projectionColor.a *= clamp01(
        style.globalOpacity * style.projectionOpacity);
    Color labelLeaderColor = style.labelLeaderColor;
    labelLeaderColor.a *= clamp01(
        style.globalOpacity * style.labelLeaderOpacity);
    applyFill(output, fillSource, regions, style);
    if (style.gridEnabled) {
        drawGrid(output, regions, style, gridColor, projectionColor);
    }
    if (style.connectionsEnabled) {
        for (const Connection& connection :
             buildConnections(regions, style.topology, style.maximumConnections,
                              style.connectionRadius)) {
            drawConnection(
                output, regions[connection.first].centroid,
                regions[connection.second].centroid, style,
                connectionColor, arrowColor,
                connection.first * 131U + connection.second * 17U);
        }
    }
    for (const Region& region : regions) {
        Rect bounds{region.bounds.x - style.padding, region.bounds.y - style.padding,
                    region.bounds.width + style.padding * 2.0F,
                    region.bounds.height + style.padding * 2.0F};
        if (style.boxesEnabled) {
            if (style.boxShape == BoxShape::Ellipse) {
                drawEllipse(output, bounds, style.strokeWidth, boxColor);
            } else {
                drawRectangle(output, bounds, style.strokeWidth, boxColor,
                              style.boxShape == BoxShape::CornerBrackets,
                              style.cornerLengthFraction);
            }
        }
        if (style.markersEnabled) {
            drawMarker(output, region.weightedCentroid, style, markerColor);
        }
    }
    drawLabels(output, regions, style, labelLeaderColor);
    return output;
}

} // namespace linear
