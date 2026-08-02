#include "linear/Detection.h"
#include "linear/Graphics.h"
#include "linear/PathGeometry.h"
#include "linear/Pixel.h"
#include "linear/Regions.h"

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

linear::Image makeFrame(int width, int height, int offset) {
    linear::Image image(width, height, {0.04F, 0.05F, 0.07F, 1.0F});
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (((x + offset) / 64 + y / 64) % 7 == 0) {
                image.at(x, y) = {0.1F, 0.9F, 0.65F, 1.0F};
            }
        }
    }
    return image;
}

template <typename Function>
double measure(Function function, int iterations) {
    const auto start = Clock::now();
    for (int i = 0; i < iterations; ++i) {
        function();
    }
    const auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count() /
           iterations;
}

void runCase(const std::string& name, int width, int height, int iterations) {
    const auto previous = makeFrame(width, height, 0);
    const auto current = makeFrame(width, height, 8);
    linear::ColorDetectionSettings colorSettings{};
    colorSettings.target = {0.1F, 0.9F, 0.65F, 1.0F};
    colorSettings.morphologyRadius = 1;
    linear::MotionDetectionSettings motionSettings{};
    linear::Mask colorMask;
    linear::MotionResult motionResult;
    std::vector<linear::Region> regions;
    linear::RegionFilter filter{};
    filter.minimumArea = 16U;
    filter.maximumRegions = 500U;
    linear::OverlayStyle style{};
    style.connectionsEnabled = true;
    style.topology = linear::ConnectionTopology::MinimumSpanningTree;

    const double colorMs = measure(
        [&] { colorMask = linear::detectColor(current, colorSettings); }, iterations);
    const double motionMs = measure(
        [&] { motionResult = linear::detectMotion(current, previous, motionSettings); },
        iterations);
    const double regionsMs = measure(
        [&] { regions = linear::connectedComponents(colorMask, nullptr, filter); },
        iterations);
    const double graphicsMs = measure(
        [&] {
            auto output = linear::renderOverlay(
                current, colorMask, regions, style,
                linear::OutputMode::SourceAndOverlay);
            if (output.empty()) {
                std::terminate();
            }
        },
        iterations);

    std::cout << std::left << std::setw(16) << name << std::right
              << std::setw(11) << std::fixed << std::setprecision(2) << colorMs
              << std::setw(11) << motionMs << std::setw(11) << regionsMs
              << std::setw(11) << graphicsMs
              << std::setw(11) << colorMs + motionMs + regionsMs + graphicsMs
              << std::setw(9) << regions.size() << '\n';
}

void runAnalysisResolutionCase(
    const std::string& name, int width, int height, int divisor, int iterations) {
    const auto source = makeFrame(width, height, 8);
    linear::ColorDetectionSettings settings{};
    settings.target = {0.1F, 0.9F, 0.65F, 1.0F};
    settings.morphologyRadius = 1;
    linear::RegionFilter filter{};
    filter.minimumArea = static_cast<std::size_t>(
        std::max(1, 16 / (divisor * divisor)));
    filter.maximumRegions = 256U;
    std::size_t regionCount = 0U;
    const double elapsed = measure([&] {
        const auto sampled = divisor == 1 ? source : linear::resizeImageNearest(
            source, (width + divisor - 1) / divisor,
            (height + divisor - 1) / divisor);
        const auto mask = linear::detectColor(sampled, settings);
        regionCount = linear::connectedComponents(mask, nullptr, filter).size();
    }, iterations);
    std::cout << "analysis " << std::left << std::setw(12) << name
              << std::right << std::setw(9) << std::fixed << std::setprecision(2)
              << elapsed << " ms" << std::setw(9) << regionCount << " regions\n";
}

void runProductionOverlayCase(int iterations) {
    const auto source = makeFrame(1920, 1080, 8);
    linear::ColorDetectionSettings detection{};
    detection.target = {0.1F, 0.9F, 0.65F, 1.0F};
    detection.morphologyRadius = 1;
    const auto sampled = linear::resizeImageNearest(source, 960, 540);
    const auto mask = linear::detectColor(sampled, detection);
    linear::RegionFilter filter{};
    filter.minimumArea = 4U;
    filter.maximumRegions = 256U;
    auto regions = linear::connectedComponents(mask, nullptr, filter);
    for (auto& region : regions) {
        region.bounds.x *= 2.0F;
        region.bounds.y *= 2.0F;
        region.bounds.width *= 2.0F;
        region.bounds.height *= 2.0F;
        region.centroid.x *= 2.0F;
        region.centroid.y *= 2.0F;
        region.weightedCentroid.x *= 2.0F;
        region.weightedCentroid.y *= 2.0F;
    }
    linear::OverlayStyle style{};
    style.boxShape = linear::BoxShape::Ellipse;
    style.connectionsEnabled = true;
    style.topology = linear::ConnectionTopology::MinimumSpanningTree;
    style.connectionStyle = linear::ConnectionStyle::SmoothBend;
    style.connectionDashEnabled = true;
    style.connectionArrowsEnabled = true;
    style.gridEnabled = true;
    style.gridProjections = true;
    style.labelMode = linear::LabelMode::Dimensions;
    style.maximumLabels = 64U;
    style.fillMode = linear::FillMode::Crosshatch;
    style.fillOpacity = 0.25F;
    linear::Mask unusedMask;
    const double elapsed = measure([&] {
        auto output = linear::renderOverlay(
            source, unusedMask, regions, style,
            linear::OutputMode::SourceAndOverlay);
        if (output.empty()) std::terminate();
    }, iterations);
    std::cout << "production overlay 1080p (ellipse + network + grid + labels + fill): "
              << std::fixed << std::setprecision(2) << elapsed << " ms\n";
}

void runContourCase(int divisor, int iterations) {
    const auto source = makeFrame(3840, 2160, 8);
    linear::ColorDetectionSettings detection{};
    detection.target = {0.1F, 0.9F, 0.65F, 1.0F};
    const auto sampled = divisor == 1 ? source : linear::resizeImageNearest(
        source, (source.width() + divisor - 1) / divisor,
        (source.height() + divisor - 1) / divisor);
    const auto mask = linear::detectColor(sampled, detection);
    linear::ContourSettings settings{};
    settings.minimumArea = static_cast<std::size_t>(
        std::max(1, 64 / (divisor * divisor)));
    settings.maximumContours = 256U;
    settings.maximumPointsPerContour = 2048U;
    settings.simplificationTolerance = 0.5F / divisor;
    std::size_t pointCount = 0U;
    const double elapsed = measure([&] {
        const auto paths = linear::extractContourPaths(mask, settings);
        pointCount = 0U;
        for (const auto& path : paths) pointCount += path.points.size();
    }, iterations);
    std::cout << "contour 4K /" << divisor << std::setw(19)
              << std::fixed << std::setprecision(2) << elapsed << " ms"
              << std::setw(9) << pointCount << " points\n";
}

} // namespace

int main() {
    std::cout << "Linear dependency-free CPU benchmark\n";
    std::cout << std::left << std::setw(16) << "case" << std::right
              << std::setw(11) << "color ms" << std::setw(11) << "motion ms"
              << std::setw(11) << "region ms" << std::setw(11) << "draw ms"
              << std::setw(11) << "total ms" << std::setw(9) << "regions" << '\n';
    runCase("1080p", 1920, 1080, 2);
    runCase("4K", 3840, 2160, 1);
    std::cout << "\nAnalysis-resolution comparison (resize + color + regions)\n";
    runAnalysisResolutionCase("1080p full", 1920, 1080, 1, 2);
    runAnalysisResolutionCase("1080p half", 1920, 1080, 2, 3);
    runAnalysisResolutionCase("1080p quarter", 1920, 1080, 4, 4);
    runAnalysisResolutionCase("4K full", 3840, 2160, 1, 1);
    runAnalysisResolutionCase("4K half", 3840, 2160, 2, 2);
    runAnalysisResolutionCase("4K quarter", 3840, 2160, 4, 3);
    std::cout << "\nFeature-heavy rendering diagnostics\n";
    runProductionOverlayCase(2);
    runContourCase(1, 1);
    runContourCase(2, 2);
    runContourCase(4, 3);
    return 0;
}
