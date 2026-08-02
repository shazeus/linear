#include "linear/Color.h"
#include "linear/Detection.h"
#include "linear/Graphics.h"
#include "linear/Host.h"
#include "linear/Labels.h"
#include "linear/Mask.h"
#include "linear/Pixel.h"
#include "linear/PathGeometry.h"
#include "linear/Regions.h"
#include "linear/Stabilization.h"
#include "linear/TextRaster.h"
#include "linear/Tracking.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Test = std::pair<std::string, std::function<void()>>;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireNear(float actual, float expected, float tolerance,
                 const std::string& message) {
    if (std::fabs(actual - expected) > tolerance) {
        throw std::runtime_error(message + ": actual=" + std::to_string(actual) +
                                 " expected=" + std::to_string(expected));
    }
}

linear::Region makeRegion(float x, float y, float width, float height,
                          std::size_t area = 16U) {
    linear::Region region{};
    region.bounds = {x, y, width, height};
    region.centroid = region.bounds.center();
    region.weightedCentroid = region.centroid;
    region.pixelArea = area;
    region.confidence = 1.0F;
    return region;
}

float xRange(const std::vector<linear::FrameRegions>& frames) {
    float minimum = frames.front().regions.front().centroid.x;
    float maximum = minimum;
    for (const auto& frame : frames) {
        minimum = std::min(minimum, frame.regions.front().centroid.x);
        maximum = std::max(maximum, frame.regions.front().centroid.x);
    }
    return maximum - minimum;
}

std::vector<Test> tests() {
    return {
        {"RGB distance", [] {
             requireNear(linear::rgbDistance({1, 0, 0, 1}, {1, 0, 0, 1}),
                         0.0F, 1.0e-6F, "equal colors");
             require(linear::rgbDistance({1, 0, 0, 1}, {0, 1, 0, 1}) > 0.7F,
                     "distinct colors must be distant");
         }},
        {"HSV hue wraparound", [] {
             const linear::Color redA{1.0F, 0.0F, 0.02F, 1.0F};
             const linear::Color redB{1.0F, 0.02F, 0.0F, 1.0F};
             require(linear::hsvDistance(redA, redB) < 0.03F,
                     "hue distance must wrap at one");
         }},
        {"Lab distance", [] {
             requireNear(linear::labDistance({0.2F, 0.4F, 0.7F, 1.0F},
                                             {0.2F, 0.4F, 0.7F, 1.0F}),
                         0.0F, 1.0e-6F, "equal Lab colors");
             require(linear::labDistance({0, 0, 0, 1}, {1, 1, 1, 1}) > 0.3F,
                     "black and white must differ perceptually");
         }},
        {"Threshold softness", [] {
             linear::Mask mask(3, 1);
             mask.at(0, 0) = 0.4F;
             mask.at(1, 0) = 0.5F;
             mask.at(2, 0) = 0.6F;
             const auto output = linear::thresholdSoft(mask, 0.5F, 0.2F);
             requireNear(output.at(0, 0), 0.0F, 1.0e-6F, "lower threshold");
             requireNear(output.at(1, 0), 0.5F, 1.0e-6F, "mid threshold");
             requireNear(output.at(2, 0), 1.0F, 1.0e-6F, "upper threshold");
         }},
        {"Blur", [] {
             linear::Mask mask(3, 1);
             mask.at(1, 0) = 1.0F;
             const auto output = linear::boxBlur(mask, 1);
             requireNear(output.at(1, 0), 1.0F / 3.0F, 1.0e-6F, "box blur");
         }},
        {"Morphology", [] {
             linear::Mask mask(5, 5);
             mask.at(2, 2) = 1.0F;
             const auto grown = linear::dilate(mask, 1);
             requireNear(grown.at(1, 1), 1.0F, 1.0e-6F, "dilation");
             const auto restored = linear::erode(grown, 1);
             requireNear(restored.at(2, 2), 1.0F, 1.0e-6F, "erosion");
             requireNear(linear::morphologicalOpen(mask, 1).at(2, 2),
                         0.0F, 1.0e-6F, "open removes impulse");
             linear::Mask hole(5, 5, 1.0F);
             hole.at(2, 2) = 0.0F;
             requireNear(linear::morphologicalClose(hole, 1).at(2, 2),
                         1.0F, 1.0e-6F, "close fills hole");
         }},
        {"Optimized morphology matches square-neighborhood reference", [] {
             linear::Mask input(23, 17);
             for (int y = 0; y < input.height(); ++y) {
                 for (int x = 0; x < input.width(); ++x) {
                     input.at(x, y) = static_cast<float>(
                         ((x * 37 + y * 19 + x * y * 3) % 101)) / 100.0F;
                 }
             }
             const auto naive = [](const linear::Mask& source, int radius,
                                   bool maximum) {
                 linear::Mask output(source.width(), source.height());
                 for (int y = 0; y < source.height(); ++y) {
                     for (int x = 0; x < source.width(); ++x) {
                         float value = maximum ? 0.0F : 1.0F;
                         for (int yy = std::max(0, y - radius);
                              yy <= std::min(source.height() - 1, y + radius);
                              ++yy) {
                             for (int xx = std::max(0, x - radius);
                                  xx <= std::min(source.width() - 1, x + radius);
                                  ++xx) {
                                 value = maximum
                                     ? std::max(value, source.at(xx, yy))
                                     : std::min(value, source.at(xx, yy));
                             }
                         }
                         output.at(x, y) = value;
                     }
                 }
                 return output;
             };
             for (int radius = 1; radius <= 4; ++radius) {
                 const auto referenceOpen = naive(naive(input, radius, false),
                                                  radius, true);
                 const auto reference = naive(naive(referenceOpen, radius, true),
                                              radius, false);
                 const auto optimized =
                     linear::morphologicalOpenClose(input, radius);
                 for (std::size_t i = 0; i < reference.values().size(); ++i) {
                     requireNear(optimized.values()[i], reference.values()[i],
                                 0.0F, "separable morphology equivalence");
                 }
             }
         }},
        {"Fast RGB detector preserves soft-threshold semantics", [] {
             linear::Image image(257, 1);
             for (int x = 0; x < image.width(); ++x) {
                 const float value = static_cast<float>(x) / 256.0F;
                 image.at(x, 0) = {value, 0.35F, 0.8F - value * 0.4F, 1.0F};
             }
             for (const bool invert : {false, true}) {
                 linear::ColorDetectionSettings settings{};
                 settings.target = {0.42F, 0.2F, 0.55F, 1.0F};
                 settings.threshold = 0.31F;
                 settings.softness = 0.07F;
                 settings.invert = invert;
                 settings.morphologyRadius = 0;
                 const auto output = linear::detectColor(image, settings);
                 const float threshold = 1.0F - settings.threshold;
                 const float lower = threshold - settings.softness * 0.5F;
                 for (int x = 0; x < image.width(); ++x) {
                     float similarity = 1.0F - linear::rgbDistance(
                         image.at(x, 0), settings.target);
                     if (invert) similarity = 1.0F - similarity;
                     const float expected = linear::clamp01(
                         (similarity - lower) / settings.softness);
                     requireNear(output.at(x, 0), expected, 1.0e-6F,
                                 "optimized RGB threshold output");
                 }
             }
         }},
        {"Alpha-aware color detection", [] {
             linear::Image image(2, 1);
             image.at(0, 0) = {1, 0, 0, 0};
             image.at(1, 0) = {1, 0, 0, 1};
             linear::ColorDetectionSettings settings{};
             settings.target = {1, 0, 0, 1};
             settings.threshold = 0.2F;
             settings.softness = 0.0F;
             settings.morphologyRadius = 0;
             const auto mask = linear::detectColor(image, settings);
             requireNear(mask.at(0, 0), 0.0F, 1.0e-6F, "transparent pixel");
             requireNear(mask.at(1, 0), 1.0F, 1.0e-6F, "opaque pixel");
         }},
        {"Connected components and descriptors", [] {
             linear::Mask mask(8, 6);
             for (int y = 1; y <= 3; ++y) {
                 for (int x = 2; x <= 5; ++x) {
                     mask.at(x, y) = 1.0F;
                 }
             }
             linear::RegionFilter filter{};
             filter.minimumArea = 4U;
             const auto regions = linear::connectedComponents(mask, nullptr, filter);
             require(regions.size() == 1U, "one component expected");
             require(regions[0].pixelArea == 12U, "component area");
             requireNear(regions[0].bounds.width, 4.0F, 1.0e-6F, "bounds width");
             requireNear(regions[0].centroid.x, 3.5F, 1.0e-6F, "centroid");
             requireNear(regions[0].perimeter, 14.0F, 1.0e-6F, "perimeter");
         }},
        {"Region filtering and maximum count", [] {
             linear::Mask mask(9, 1);
             for (int x : {0, 2, 4, 6, 8}) {
                 mask.at(x, 0) = 1.0F;
             }
             linear::RegionFilter filter{};
             filter.maximumRegions = 3U;
             const auto regions = linear::connectedComponents(mask, nullptr, filter);
             require(regions.size() == 3U, "maximum region limit");
         }},
        {"IoU", [] {
             requireNear(linear::intersectionOverUnion({0, 0, 10, 10},
                                                       {5, 0, 10, 10}),
                         1.0F / 3.0F, 1.0e-6F, "IoU");
         }},
        {"Motion and scene cut", [] {
             linear::Image previous(16, 16, {0, 0, 0, 1});
             linear::Image current = previous;
             current.at(8, 8) = {1, 1, 1, 1};
             linear::MotionDetectionSettings settings{};
             settings.noiseRadius = 0;
             settings.threshold = 0.1F;
             settings.softness = 0.0F;
             auto result = linear::detectMotion(current, previous, settings);
             require(!result.sceneCut, "single changed pixel is not a scene cut");
             requireNear(result.confidence.at(8, 8), 1.0F, 1.0e-6F,
                         "motion pixel");
             linear::Image white(16, 16, {1, 1, 1, 1});
             result = linear::detectMotion(white, previous, settings);
             require(result.sceneCut, "full-frame replacement is a scene cut");
         }},
        {"Motion regions feed deterministic tracking", [] {
             linear::Image previous(48, 32, {0, 0, 0, 1});
             linear::Image current = previous;
             for (int y = 10; y < 18; ++y) {
                 for (int x = 10; x < 18; ++x) {
                     previous.at(x, y) = {1, 1, 1, 1};
                 }
                 for (int x = 14; x < 22; ++x) {
                     current.at(x, y) = {1, 1, 1, 1};
                 }
             }
             linear::ColorDetectionSettings color{};
             color.target = {1, 1, 1, 1};
             color.threshold = 0.05F;
             color.morphologyRadius = 0;
             linear::MotionDetectionSettings motion{};
             motion.threshold = 0.03F;
             motion.softness = 0.01F;
             motion.noiseRadius = 0;
             linear::RegionFilter filter{};
             filter.minimumArea = 4U;
             auto previousRegions = linear::connectedComponents(
                 linear::detectColor(previous, color), nullptr, filter);
             auto movingRegions = linear::connectedComponents(
                 linear::detectMotion(current, previous, motion).confidence,
                 nullptr, filter);
             require(!previousRegions.empty() && !movingRegions.empty(),
                     "motion confidence becomes trackable regions");
             std::vector<linear::FrameRegions> frames;
             frames.push_back({0, std::move(previousRegions)});
             frames.push_back({1, std::move(movingRegions)});
             auto tracked = linear::trackFrameWindow(std::move(frames), {});
             require(tracked.size() == 2U &&
                     !tracked[1].regions.empty() &&
                     tracked[1].regions.front().trackId != 0U,
                     "motion-derived regions receive deterministic track IDs");
         }},
        {"Combined masks", [] {
             linear::Mask color(1, 1, 0.75F);
             linear::Mask motion(1, 1, 0.5F);
             requireNear(linear::combineMasks(
                             color, motion,
                             linear::CombinedOperation::ColorAndMotion).at(0, 0),
                         0.5F, 1.0e-6F, "AND");
             requireNear(linear::combineMasks(
                             color, motion,
                             linear::CombinedOperation::ColorMinusMotion).at(0, 0),
                         0.375F, 1.0e-6F, "minus");
         }},
        {"Temporal capture bridges one-frame miss", [] {
             linear::Mask strong(1, 1, 1.0F);
             linear::Mask missed(1, 1, 0.0F);
             const auto raw =
                 linear::temporalConfidenceWindow({strong, missed}, 0.0F);
             const auto persistent =
                 linear::temporalConfidenceWindow({strong, missed}, 0.75F);
             requireNear(raw.at(0, 0), 0.0F, 1.0e-6F,
                         "zero persistence preserves newest mask");
             requireNear(persistent.at(0, 0), 0.75F, 1.0e-6F,
                         "capture confidence decays through short miss");
             const auto recovered = linear::temporalConfidenceWindow(
                 {strong, missed, strong}, 0.75F);
             requireNear(recovered.at(0, 0), 1.0F, 1.0e-6F,
                         "new confidence has immediate attack");
         }},
        {"Deterministic tracking", [] {
             linear::FrameRegions late{20, {makeRegion(12, 10, 4, 4)}};
             linear::FrameRegions early{10, {makeRegion(10, 10, 4, 4)}};
             const auto first = linear::trackFrameWindow({late, early}, {});
             const auto second = linear::trackFrameWindow({early, late}, {});
             require(first[0].regions[0].trackId == first[1].regions[0].trackId,
                     "track persists");
             require(first[0].regions[0].trackId == second[0].regions[0].trackId,
                     "render order independent ID");
             requireNear(first[1].regions[0].velocity.x, 2.0F, 1.0e-6F,
                         "velocity");
         }},
        {"Tracking survives short detection dropout", [] {
             std::vector<linear::FrameRegions> frames{
                 {0, {makeRegion(0, 10, 8, 8)}},
                 {1, {makeRegion(5, 10, 8, 8)}},
                 {2, {}},
                 {3, {makeRegion(15, 10, 8, 8)}}};
             linear::TrackingSettings settings{};
             settings.maximumMissedFrames = 2;
             settings.maximumCentroidDistance = 20.0F;
             const auto tracked = linear::trackFrameWindow(frames, settings);
             require(tracked[0].regions[0].trackId ==
                         tracked[3].regions[0].trackId,
                     "track ID must survive one missing frame");
             require(tracked[3].regions[0].age == 3,
                     "reacquired track retains age");
         }},
        {"Legacy Raw preserves one-pixel jitter", [] {
             std::vector<linear::FrameRegions> jitter;
             for (int frame = 0; frame < 5; ++frame) {
                 const float jitterAmount = static_cast<float>(frame % 2);
                 const float width = 20.0F + jitterAmount;
                 const float centerX = 20.0F + jitterAmount;
                 auto region = makeRegion(
                     centerX - width * 0.5F, 20.0F, width, 12.0F);
                 region.trackId = 77U;
                 jitter.push_back({frame, {region}});
             }
             linear::StabilizationSettings settings{};
             settings.mode = linear::StabilizationMode::LegacyRaw;
             const auto raw = linear::stabilizeFrameWindow(jitter, settings);
             requireNear(xRange(raw), 1.0F, 1.0e-6F,
                         "raw must preserve full jitter");
             for (std::size_t i = 0; i < jitter.size(); ++i) {
                 requireNear(raw[i].regions[0].centroid.x,
                             jitter[i].regions[0].centroid.x, 0.0F,
                             "raw centroid is exact");
                 requireNear(raw[i].regions[0].bounds.width,
                             jitter[i].regions[0].bounds.width, 0.0F,
                             "raw size is exact");
             }
         }},
        {"Basic Smooth reduces position and size jitter", [] {
             std::vector<linear::FrameRegions> jitter;
             for (int frame = 0; frame < 5; ++frame) {
                 const float jitterAmount = static_cast<float>(frame % 2);
                 const float width = 20.0F + jitterAmount;
                 const float centerX = 20.0F + jitterAmount;
                 auto region = makeRegion(
                     centerX - width * 0.5F, 20.0F, width, 12.0F);
                 region.trackId = 77U;
                 jitter.push_back({frame, {region}});
             }
             linear::StabilizationSettings settings{};
             settings.mode = linear::StabilizationMode::BasicSmooth;
             settings.positionStrength = 0.75F;
             settings.sizeStrength = 0.75F;
             const auto smooth =
                 linear::stabilizeFrameWindow(jitter, settings);
             require(xRange(smooth) < xRange(jitter) * 0.4F,
                     "Basic Smooth must reduce centroid jitter");
             float minimumWidth = smooth.front().regions[0].bounds.width;
             float maximumWidth = minimumWidth;
             for (const auto& frame : smooth) {
                 minimumWidth =
                     std::min(minimumWidth, frame.regions[0].bounds.width);
                 maximumWidth =
                     std::max(maximumWidth, frame.regions[0].bounds.width);
             }
             require(maximumWidth - minimumWidth < 0.4F,
                     "Basic Smooth must reduce size jitter");
             require(smooth[1].regions[0].centroid.x > 20.0F,
                     "smoother must follow genuine movement");
         }},
        {"Basic Smooth is input-order independent", [] {
             auto first = makeRegion(0, 0, 4, 4);
             auto second = makeRegion(2, 0, 4, 4);
             auto third = makeRegion(4, 0, 4, 4);
             first.trackId = second.trackId = third.trackId = 9U;
             const std::vector<linear::FrameRegions> ordered{
                 {10, {first}}, {20, {second}}, {30, {third}}};
             const std::vector<linear::FrameRegions> shuffled{
                 {30, {third}}, {10, {first}}, {20, {second}}};
             linear::StabilizationSettings settings{};
             settings.mode = linear::StabilizationMode::BasicSmooth;
             const auto a = linear::stabilizeFrameWindow(ordered, settings);
             const auto b = linear::stabilizeFrameWindow(shuffled, settings);
             for (const auto& frameA : a) {
                 const auto frameB = std::find_if(
                     b.begin(), b.end(), [&](const linear::FrameRegions& value) {
                         return value.timeKey == frameA.timeKey;
                     });
                 require(frameB != b.end(), "matching time key");
                 requireNear(frameA.regions[0].centroid.x,
                             frameB->regions[0].centroid.x, 1.0e-6F,
                             "render-order independent smoothing");
             }
         }},
        {"Advanced smoothing modes are distinct", [] {
             std::vector<linear::FrameRegions> motion;
             for (int frame = 0; frame < 7; ++frame) {
                 const float x = static_cast<float>(frame * 6) +
                                 static_cast<float>(frame % 2);
                 auto region = makeRegion(x, 10, 10, 10);
                 region.trackId = 44U;
                 region.confidence = frame == 3 ? 0.25F : 1.0F;
                 motion.push_back({frame, {region}});
             }
             linear::StabilizationSettings basicSettings{};
             basicSettings.mode = linear::StabilizationMode::BasicSmooth;
             const auto basic =
                 linear::stabilizeFrameWindow(motion, basicSettings);
             linear::StabilizationSettings adaptiveSettings = basicSettings;
             adaptiveSettings.mode =
                 linear::StabilizationMode::AdaptiveSmooth;
             const auto adaptive =
                 linear::stabilizeFrameWindow(motion, adaptiveSettings);
             linear::StabilizationSettings cinematicSettings = basicSettings;
             cinematicSettings.mode =
                 linear::StabilizationMode::CinematicSmooth;
             const auto cinematic =
                 linear::stabilizeFrameWindow(motion, cinematicSettings);
             linear::StabilizationSettings predictiveSettings = basicSettings;
             predictiveSettings.mode =
                 linear::StabilizationMode::PredictiveSmooth;
             const auto predictive =
                 linear::stabilizeFrameWindow(motion, predictiveSettings);
             require(adaptive.back().regions[0].centroid.x >
                         basic.back().regions[0].centroid.x,
                     "adaptive mode follows sustained motion faster");
             require(cinematic.front().regions[0].centroid.x >
                         motion.front().regions[0].centroid.x,
                     "cinematic mode uses a centered window");
             require(predictive.back().regions[0].centroid.x >
                         basic.back().regions[0].centroid.x,
                     "predictive mode reduces causal lag");
             require(std::fabs(
                         adaptive[3].regions[0].centroid.x -
                         basic[3].regions[0].centroid.x) > 0.01F,
                     "low-confidence adaptive response is distinct");
         }},
        {"Path endpoint stabilization", [] {
             std::vector<linear::TimedPath> paths{
                 {0, 100U, {{0, 0}, {10, 0}}},
                 {1, 100U, {{1, 0}, {11, 0}}},
                 {2, 100U, {{0, 0}, {10, 0}}}};
             linear::StabilizationSettings rawSettings{};
             const auto raw =
                 linear::stabilizePathWindow(paths, rawSettings);
             requireNear(raw[1].points[0].x, 1.0F, 0.0F,
                         "raw path endpoint");
             linear::StabilizationSettings smoothSettings{};
             smoothSettings.mode = linear::StabilizationMode::BasicSmooth;
             smoothSettings.positionStrength = 0.75F;
             const auto smooth =
                 linear::stabilizePathWindow(paths, smoothSettings);
             requireNear(smooth[1].points[0].x, 0.25F, 1.0e-6F,
                         "smoothed start endpoint");
             requireNear(smooth[1].points[1].x, 10.25F, 1.0e-6F,
                         "smoothed end endpoint");
             require(smooth[2].points[0].x > 0.0F,
                     "path smoothing retains deterministic follow state");
         }},
        {"Orbital path geometry", [] {
             linear::OrbitSettings settings{};
             settings.segments = 32U;
             settings.padding = 2.0F;
             const auto orbit =
                 linear::makeOrbitalPath({10, 20, 20, 10}, 91U, settings);
             require(orbit.stableId == 91U && orbit.closed,
                     "full orbit identity and closure");
             require(orbit.points.size() == 32U, "bounded orbit tessellation");
             requireNear(orbit.points.front().x, 32.0F, 1.0e-5F,
                         "orbit horizontal radius");
             requireNear(orbit.points.front().y, 25.0F, 1.0e-5F,
                         "orbit center");
             settings.sweepAngleRadians = 3.14159265358979323846F;
             const auto arc =
                 linear::makeOrbitalPath({0, 0, 10, 10}, 92U, settings);
             require(!arc.closed, "partial orbit is an open arc");
             requireNear(arc.points.back().x, -2.0F, 1.0e-4F,
                         "arc reaches requested sweep endpoint");
         }},
        {"Mask contour geometry", [] {
             linear::Mask mask(8, 8);
             for (int y = 2; y < 5; ++y) {
                 for (int x = 1; x < 5; ++x) {
                     mask.at(x, y) = 1.0F;
                 }
             }
             const auto contours = linear::extractContourPaths(mask);
             require(contours.size() == 1U, "one component contour");
             require(contours.front().closed, "contour is closed");
             require(contours.front().points.size() == 4U,
                     "rectangle contour simplifies to four corners");
             float minimumX = contours.front().points.front().x;
             float maximumX = minimumX;
             float minimumY = contours.front().points.front().y;
             float maximumY = minimumY;
             for (const auto point : contours.front().points) {
                 minimumX = std::min(minimumX, point.x);
                 maximumX = std::max(maximumX, point.x);
                 minimumY = std::min(minimumY, point.y);
                 maximumY = std::max(maximumY, point.y);
             }
             requireNear(minimumX, 1.0F, 0.0F, "contour minimum x");
             requireNear(maximumX, 5.0F, 0.0F, "contour maximum x");
             requireNear(minimumY, 2.0F, 0.0F, "contour minimum y");
             requireNear(maximumY, 5.0F, 0.0F, "contour maximum y");
         }},
        {"Tangent-aligned path repeaters", [] {
             linear::Polyline path{7U, false, {{0, 0}, {100, 0}}};
             linear::PathRepeaterSettings settings{};
             settings.primitive = linear::RepeaterPrimitive::Arrow;
             settings.spacing = 25.0F;
             settings.scale = 2.0F;
             const auto instances = linear::repeatAlongPath(path, settings);
             require(instances.size() == 5U, "repeaters include open endpoints");
             requireNear(instances[2].position.x, 50.0F, 1.0e-6F,
                         "repeater arc-length placement");
             requireNear(instances[2].rotationRadians, 0.0F, 1.0e-6F,
                         "horizontal tangent rotation");
             const auto glyph = linear::buildRepeaterGlyph(instances[2]);
             require(glyph.size() == 1U && glyph.front().points.size() == 5U,
                     "arrow is real stroke geometry");
             requireNear(glyph.front().points.front().x, 48.0F, 1.0e-6F,
                         "glyph scale and transform");

             path.points = {{0, 0}, {0, 100}};
             settings.maximumInstances = 3U;
             const auto limited = linear::repeatAlongPath(path, settings);
             require(limited.size() == 3U, "repeater hard limit");
             requireNear(limited.front().rotationRadians,
                         1.57079632679489661923F, 1.0e-6F,
                         "vertical tangent alignment");
         }},
        {"Path primitive rasterization", [] {
             linear::Image image(64, 64, {0, 0, 0, 0});
             linear::OrbitSettings orbitSettings{};
             orbitSettings.segments = 48U;
             const auto orbit = linear::makeOrbitalPath(
                 {16, 20, 32, 24}, 1U, orbitSettings);
             linear::rasterizePolylines(
                 image, {orbit}, {0, 1, 0, 1}, 2.0F);
             std::size_t visible = 0U;
             for (const auto& pixel : image.pixels()) {
                 if (pixel.a > 0.0F) {
                     ++visible;
                 }
             }
             require(visible > 100U, "orbit strokes produce visible pixels");

             linear::RepeaterInstance instance{};
             instance.primitive = linear::RepeaterPrimitive::Box;
             instance.position = {32, 32};
             instance.scale = 5.0F;
             const auto glyph = linear::buildRepeaterGlyph(instance);
             const std::size_t before = visible;
             linear::rasterizePolylines(
                 image, glyph, {1, 0, 0, 1}, 1.5F);
             visible = 0U;
             for (const auto& pixel : image.pixels()) {
                 if (pixel.a > 0.0F) {
                     ++visible;
                 }
             }
             require(visible > before, "repeater glyph rasterizes real strokes");
         }},
        {"Bitmap labels rasterize technical panels", [] {
             linear::Image image(160, 48, {0, 0, 0, 0});
             linear::BitmapTextStyle textStyle{};
             textStyle.scale = 2;
             const auto metrics = linear::measureBitmapText("NODE_17", textStyle);
             require(metrics.width > 50 && metrics.height == 20,
                     "label metrics include panel padding");
             linear::rasterizeBitmapText(
                 image, {4, 4}, "NODE_17", textStyle);
             std::size_t visible = 0U;
             for (const auto& pixel : image.pixels()) {
                 visible += pixel.a > 0.0F ? 1U : 0U;
             }
             require(visible > 500U, "label panel and glyphs are visible");

             linear::Image styled(160, 48, {0, 0, 0, 0});
             textStyle.cornerRadius = 6.0F;
             textStyle.border = {1, 0, 0, 1};
             textStyle.borderThickness = 2;
             textStyle.shadow = {0, 0, 1, 0.8F};
             textStyle.shadowOffset = {4, 4};
             linear::rasterizeBitmapText(
                 styled, {4, 4}, "CUSTOM", textStyle);
             require(styled.at(4, 4).a == 0.0F,
                     "rounded panel removes the square corner pixel");
             bool redBorder = false;
             bool blueShadow = false;
             for (const auto& pixel : styled.pixels()) {
                 redBorder = redBorder ||
                     (pixel.r > 0.8F && pixel.g < 0.2F && pixel.b < 0.2F);
                 blueShadow = blueShadow ||
                     (pixel.b > 0.5F && pixel.r < 0.2F);
             }
             require(redBorder && blueShadow,
                     "label panel border and shadow have independent styling");
         }},
        {"Installed system fonts render custom UTF-8 labels", [] {
#ifdef _WIN32
             linear::BitmapTextStyle systemStyle{};
             systemStyle.fontFamily = "Segoe UI";
             systemStyle.scale = 3;
             systemStyle.padding = 4;
             systemStyle.backgroundEnabled = false;
             systemStyle.foreground = {1, 1, 1, 1};
             const std::string text = "Çağrı 42";
             const auto systemMetrics =
                 linear::measureBitmapText(text, systemStyle);
             require(systemMetrics.width > 20 && systemMetrics.height > 10,
                     "system font provides useful UTF-8 label metrics");
             linear::Image systemImage(240, 80, {0, 0, 0, 0});
             linear::rasterizeBitmapText(
                 systemImage, {4, 4}, text, systemStyle);

             linear::BitmapTextStyle builtInStyle = systemStyle;
             builtInStyle.fontFamily.clear();
             linear::Image builtInImage(240, 80, {0, 0, 0, 0});
             linear::rasterizeBitmapText(
                 builtInImage, {4, 4}, text, builtInStyle);
             bool systemVisible = false;
             bool differsFromBuiltIn = false;
             for (std::size_t i = 0; i < systemImage.pixels().size(); ++i) {
                 systemVisible = systemVisible || systemImage.pixels()[i].a > 0.0F;
                 differsFromBuiltIn = differsFromBuiltIn ||
                     std::fabs(systemImage.pixels()[i].a -
                               builtInImage.pixels()[i].a) > 1.0e-5F;
             }
             require(systemVisible,
                     "selected installed font rasterizes visible glyphs");
             require(differsFromBuiltIn,
                     "selected installed font changes label typography");
#else
             require(true, "system-font rasterization is Windows-specific");
#endif
         }},
        {"Connection graphs", [] {
             std::vector<linear::Region> regions{
                 makeRegion(0, 0, 2, 2), makeRegion(10, 0, 2, 2),
                 makeRegion(5, 8, 2, 2)};
             require(linear::buildConnections(
                         regions, linear::ConnectionTopology::MinimumSpanningTree,
                         10U).size() == 2U,
                     "MST edge count");
             require(linear::buildConnections(
                         regions, linear::ConnectionTopology::NearestNeighbor,
                         1U).size() == 1U,
                     "connection cap");
             require(linear::buildConnections(
                         regions, linear::ConnectionTopology::FullMesh,
                         10U).size() == 3U,
                     "full mesh edge count");
             require(linear::buildConnections(
                         regions, linear::ConnectionTopology::Star,
                         10U).size() == 2U,
                     "star edge count");
             require(linear::buildConnections(
                         regions, linear::ConnectionTopology::Radius,
                         10U, 6.0F).empty(),
                     "radius topology rejects distant nodes");
             require(linear::buildConnections(
                         regions, linear::ConnectionTopology::Radius,
                         10U, 12.0F).size() == 3U,
                     "radius topology connects every nearby pair");
         }},
        {"Connection network rasterization", [] {
             linear::Image source(64, 64, {0, 0, 0, 1});
             linear::Mask mask(64, 64);
             std::vector<linear::Region> regions{
                 makeRegion(8, 8, 4, 4), makeRegion(48, 8, 4, 4),
                 makeRegion(28, 48, 4, 4)};
             linear::OverlayStyle withoutNetwork{};
             withoutNetwork.boxesEnabled = false;
             withoutNetwork.markersEnabled = false;
             const auto plain = linear::renderOverlay(
                 source, mask, regions, withoutNetwork,
                 linear::OutputMode::OverlayOnly);

             linear::OverlayStyle network = withoutNetwork;
             network.connectionsEnabled = true;
             network.topology =
                 linear::ConnectionTopology::MinimumSpanningTree;
             network.maximumConnections = 2U;
             network.strokeWidth = 2.0F;
             const auto connected = linear::renderOverlay(
                 source, mask, regions, network,
                 linear::OutputMode::OverlayOnly);
             linear::OverlayStyle rounded = network;
             rounded.connectionStyle = linear::ConnectionStyle::RoundedArc;
             rounded.connectionCurvature = 0.65F;
             const auto roundedImage = linear::renderOverlay(
                 source, mask, regions, rounded,
                 linear::OutputMode::OverlayOnly);
             std::size_t plainPixels = 0U;
             std::size_t networkPixels = 0U;
             bool roundedDiffers = false;
             for (const auto& pixel : plain.pixels()) {
                 plainPixels += pixel.a > 0.0F ? 1U : 0U;
             }
             for (const auto& pixel : connected.pixels()) {
                 networkPixels += pixel.a > 0.0F ? 1U : 0U;
             }
             for (std::size_t i = 0; i < connected.pixels().size(); ++i) {
                 roundedDiffers = roundedDiffers ||
                     std::fabs(connected.pixels()[i].a -
                               roundedImage.pixels()[i].a) > 1.0e-6F;
             }
             require(plainPixels == 0U, "disabled network is invisible");
             require(networkPixels > 100U, "enabled network draws visible edges");
             require(roundedDiffers,
                     "rounded arc style must differ from straight edges");

             for (const auto route : {
                      linear::ConnectionStyle::SmoothBend,
                      linear::ConnectionStyle::StepBend,
                      linear::ConnectionStyle::PcbTrace}) {
                 rounded.connectionStyle = route;
                 const auto routedImage = linear::renderOverlay(
                     source, mask, regions, rounded,
                     linear::OutputMode::OverlayOnly);
                 bool routeDiffers = false;
                 for (std::size_t i = 0; i < connected.pixels().size(); ++i) {
                     routeDiffers = routeDiffers ||
                         std::fabs(connected.pixels()[i].a -
                                   routedImage.pixels()[i].a) > 1.0e-6F;
                 }
                 require(routeDiffers,
                         "technical connection route must differ from straight edges");
             }

             linear::OverlayStyle decorated = network;
             decorated.connectionDashEnabled = true;
             decorated.connectionDashLength = 6.0F;
             decorated.connectionGapLength = 5.0F;
             decorated.connectionArrowsEnabled = true;
             decorated.connectionArrowColor = {1, 0, 0, 1};
             decorated.connectionArrowSize = 10.0F;
             const auto decoratedImage = linear::renderOverlay(
                 source, mask, regions, decorated,
                 linear::OutputMode::OverlayOnly);
             bool dashDiffers = false;
             bool arrowColorVisible = false;
             for (std::size_t i = 0; i < connected.pixels().size(); ++i) {
                 dashDiffers = dashDiffers ||
                     std::fabs(connected.pixels()[i].a -
                               decoratedImage.pixels()[i].a) > 1.0e-6F;
                 const auto& pixel = decoratedImage.pixels()[i];
                 arrowColorVisible = arrowColorVisible ||
                     (pixel.r > 0.8F && pixel.g < 0.2F && pixel.b < 0.2F);
             }
             require(dashDiffers && arrowColorVisible,
                     "dash pattern and independent arrow color are visible");
         }},
        {"Box marker network grid and label colors are independent", [] {
             linear::Image source(96, 72, {0, 0, 0, 0});
             linear::Mask mask(96, 72);
             std::vector<linear::Region> regions{
                 makeRegion(12, 18, 18, 14), makeRegion(62, 38, 18, 14)};
             regions[0].componentId = 1U;
             regions[1].componentId = 2U;
             linear::OverlayStyle style{};
             style.boxColor = {1, 0, 0, 1};
             style.markerColor = {0, 1, 0, 1};
             style.connectionColor = {0, 0, 1, 1};
             style.gridColor = {1, 1, 0, 1};
             style.gridOpacity = 1.0F;
             style.gridWidth = 1.0F;
             style.gridEnabled = true;
             style.gridDivisionsX = 3;
             style.gridDivisionsY = 3;
             style.connectionsEnabled = true;
             style.topology = linear::ConnectionTopology::Sequential;
             style.labelMode = linear::LabelMode::NodeId;
             style.labelStyle.foreground = {0, 1, 1, 1};
             style.labelStyle.backgroundEnabled = false;
             style.labelLeaderColor = {1, 0, 1, 1};
             const auto output = linear::renderOverlay(
                 source, mask, regions, style,
                 linear::OutputMode::OverlayOnly);
             bool red = false;
             bool green = false;
             bool blue = false;
             bool yellow = false;
             bool cyan = false;
             bool magenta = false;
             for (const auto& pixel : output.pixels()) {
                 red = red || (pixel.r > 0.8F && pixel.g < 0.2F && pixel.b < 0.2F);
                 green = green || (pixel.g > 0.8F && pixel.r < 0.2F && pixel.b < 0.2F);
                 blue = blue || (pixel.b > 0.8F && pixel.r < 0.2F && pixel.g < 0.2F);
                 yellow = yellow || (pixel.r > 0.8F && pixel.g > 0.8F && pixel.b < 0.2F);
                 cyan = cyan || (pixel.g > 0.8F && pixel.b > 0.8F && pixel.r < 0.2F);
                 magenta = magenta || (pixel.r > 0.8F && pixel.b > 0.8F && pixel.g < 0.2F);
             }
             require(red && green && blue && yellow && cyan && magenta,
                     "each renderer keeps its own color channel");
         }},
        {"Grid and shape modes are visibly distinct", [] {
             linear::Image source(64, 64, {0, 0, 0, 0});
             linear::Mask mask(64, 64);
             std::vector<linear::Region> regions{
                 makeRegion(20, 20, 20, 12)};
             linear::OverlayStyle rectangle{};
             rectangle.markersEnabled = false;
             rectangle.boxShape = linear::BoxShape::Rectangle;
             const auto rectangleImage = linear::renderOverlay(
                 source, mask, regions, rectangle,
                 linear::OutputMode::OverlayOnly);
             linear::OverlayStyle ellipse = rectangle;
             ellipse.boxShape = linear::BoxShape::Ellipse;
             const auto ellipseImage = linear::renderOverlay(
                 source, mask, regions, ellipse,
                 linear::OutputMode::OverlayOnly);
             bool differs = false;
             for (std::size_t i = 0; i < rectangleImage.pixels().size(); ++i) {
                 differs = differs ||
                           std::fabs(rectangleImage.pixels()[i].a -
                                     ellipseImage.pixels()[i].a) > 1.0e-6F;
             }
             require(differs, "box shape popup modes must not alias");

             linear::OverlayStyle grid{};
             grid.boxesEnabled = false;
             grid.markersEnabled = false;
             grid.gridEnabled = true;
             grid.gridDivisionsX = 4;
             grid.gridDivisionsY = 4;
             const auto gridImage = linear::renderOverlay(
                 source, mask, {}, grid,
                 linear::OutputMode::OverlayOnly);
             std::size_t gridPixels = 0U;
             for (const auto& pixel : gridImage.pixels()) {
                 gridPixels += pixel.a > 0.0F ? 1U : 0U;
             }
             require(gridPixels > 200U, "enabled grid draws visible pixels");

             grid.gridMode = linear::GridMode::Simple;
             const auto simpleGrid = linear::renderOverlay(
                 source, mask, {}, grid,
                 linear::OutputMode::OverlayOnly);
             bool gridModesDiffer = false;
             for (std::size_t i = 0; i < gridImage.pixels().size(); ++i) {
                 gridModesDiffer = gridModesDiffer ||
                     std::fabs(gridImage.pixels()[i].a -
                               simpleGrid.pixels()[i].a) > 1.0e-6F;
             }
             require(gridModesDiffer,
                     "Cartesian frame, axes, ticks and values differ from simple grid");

             grid.gridMode = linear::GridMode::Cartesian;
             grid.gridAxisLabels = false;
             grid.gridProjections = true;
             grid.projectionColor = {1, 0, 1, 1};
             grid.projectionOpacity = 1.0F;
             const auto projected = linear::renderOverlay(
                 source, mask, regions, grid,
                 linear::OutputMode::OverlayOnly);
             bool projectionColorVisible = false;
             for (const auto& pixel : projected.pixels()) {
                 projectionColorVisible = projectionColorVisible ||
                     (pixel.r > 0.8F && pixel.b > 0.8F && pixel.g < 0.2F);
             }
             require(projectionColorVisible,
                     "Cartesian node projections use their independent color");
         }},
        {"Extended marker shapes are visible and distinct", [] {
             linear::Image source(64, 64, {0, 0, 0, 0});
             linear::Mask mask(64, 64);
             std::vector<linear::Region> regions{
                 makeRegion(24, 24, 16, 16)};
             linear::OverlayStyle style{};
             style.boxesEnabled = false;
             style.markersEnabled = true;
             style.markerSize = 24.0F;
             style.markerStrokeWidth = 2.0F;
             const std::array<linear::MarkerShape, 9U> shapes{{
                 linear::MarkerShape::Ring, linear::MarkerShape::Dot,
                 linear::MarkerShape::Plus, linear::MarkerShape::Cross,
                 linear::MarkerShape::Triangle, linear::MarkerShape::Star,
                 linear::MarkerShape::Diamond, linear::MarkerShape::Target,
                 linear::MarkerShape::Chevron}};
             std::vector<linear::Image> outputs;
             for (const auto shape : shapes) {
                 style.markerShape = shape;
                 outputs.push_back(linear::renderOverlay(
                     source, mask, regions, style,
                     linear::OutputMode::OverlayOnly));
             }
             for (const auto& output : outputs) {
                 const auto visible = std::count_if(
                     output.pixels().begin(), output.pixels().end(),
                     [](const linear::Color& color) { return color.a > 0.0F; });
                 require(visible > 8,
                         "every advertised marker shape rasterizes");
             }
             for (std::size_t first = 0; first < outputs.size(); ++first) {
                 for (std::size_t second = first + 1U;
                      second < outputs.size(); ++second) {
                     bool differs = false;
                     for (std::size_t pixel = 0;
                          pixel < outputs[first].pixels().size(); ++pixel) {
                         if (std::fabs(outputs[first].pixels()[pixel].a -
                                       outputs[second].pixels()[pixel].a) >
                             1.0e-6F) {
                             differs = true;
                             break;
                         }
                     }
                     require(differs,
                             "marker popup entries must not alias");
                 }
             }
         }},
        {"Labels and hatch fill integrate with overlay", [] {
             linear::Image source(96, 64, {0.1F, 0.1F, 0.1F, 1});
             linear::Mask mask(96, 64);
             for (int y = 16; y < 48; ++y) {
                 for (int x = 20; x < 76; ++x) mask.at(x, y) = 1.0F;
             }
             std::vector<linear::Region> regions{
                 makeRegion(20, 16, 56, 32, 1792U)};
             regions[0].componentId = 7U;
             linear::OverlayStyle style{};
             style.boxesEnabled = false;
             style.markersEnabled = false;
             style.labelMode = linear::LabelMode::NodeId;
             style.fillMode = linear::FillMode::Hatch;
             style.hatchSpacing = 8;
             const auto output = linear::renderOverlay(
                 source, mask, regions, style,
                 linear::OutputMode::SourceAndOverlay);
             bool foundLabelColor = false;
             bool foundHatch = false;
             for (const auto& pixel : output.pixels()) {
                 foundLabelColor = foundLabelColor || pixel.g > 0.5F;
                 foundHatch = foundHatch || pixel.g > 0.15F;
             }
             require(foundLabelColor && foundHatch,
                     "label and hatch produce visible styled output");
         }},
        {"Region fill stays inside boxes and honors inset feather", [] {
             linear::Image source(40, 32, {0, 0, 0, 0});
             linear::Mask mask(40, 32, 1.0F);
             std::vector<linear::Region> regions{
                 makeRegion(8, 8, 16, 12, 192U)};
             linear::OverlayStyle style{};
             style.boxesEnabled = false;
             style.markersEnabled = false;
             style.fillMode = linear::FillMode::Solid;
             style.fillOpacity = 1.0F;
             style.fillInset = 2.0F;
             const auto inset = linear::renderOverlay(
                 source, mask, regions, style,
                 linear::OutputMode::OverlayOnly);
             require(inset.at(1, 1).a == 0.0F,
                     "fill never follows mask outside a region box");
             require(inset.at(8, 8).a == 0.0F,
                     "box inset removes pixels at the region edge");
             require(inset.at(16, 14).a > 0.9F,
                     "box interior remains filled");

             style.fillInset = 0.0F;
             style.fillFeather = 4.0F;
             const auto feathered = linear::renderOverlay(
                 source, mask, regions, style,
                 linear::OutputMode::OverlayOnly);
             require(feathered.at(8, 12).a < feathered.at(16, 14).a,
                     "edge feather ramps inward inside the box");
         }},
        {"Ellipse region fill follows ellipse geometry", [] {
             linear::Image source(40, 40, {0, 0, 0, 0});
             linear::Mask mask(40, 40, 1.0F);
             std::vector<linear::Region> regions{
                 makeRegion(8, 8, 24, 24, 576U)};
             linear::OverlayStyle style{};
             style.boxesEnabled = false;
             style.markersEnabled = false;
             style.fillMode = linear::FillMode::Solid;
             style.fillOpacity = 1.0F;
             style.boxShape = linear::BoxShape::Ellipse;

             const auto ellipse = linear::renderOverlay(
                 source, mask, regions, style,
                 linear::OutputMode::OverlayOnly);
             require(ellipse.at(20, 20).a > 0.9F,
                     "ellipse center remains filled");
             require(ellipse.at(9, 9).a == 0.0F,
                     "ellipse fill clips rectangular bounding-box corners");
             require(ellipse.at(8, 20).a > 0.9F,
                     "ellipse fill reaches its curved side boundary");

             style.boxShape = linear::BoxShape::Rectangle;
             const auto rectangle = linear::renderOverlay(
                 source, mask, regions, style,
                 linear::OutputMode::OverlayOnly);
             require(rectangle.at(9, 9).a > 0.9F,
                     "rectangle fill retains bounding-box coverage");

             style.boxShape = linear::BoxShape::Ellipse;
             style.fillFeather = 4.0F;
             const auto feathered = linear::renderOverlay(
                 source, mask, regions, style,
                 linear::OutputMode::OverlayOnly);
             require(feathered.at(8, 20).a < feathered.at(20, 20).a,
                     "ellipse feather ramps inward from the curved boundary");
         }},
        {"Every region fill mode renders a distinct box-confined result", [] {
             linear::Image source(32, 24);
             for (int y = 0; y < source.height(); ++y) {
                 for (int x = 0; x < source.width(); ++x) {
                     source.at(x, y) = {
                         0.1F + static_cast<float>(x) / 64.0F,
                         0.15F + static_cast<float>(y) / 48.0F,
                         0.25F, 1.0F};
                 }
             }
             linear::Mask mask(32, 24);
             std::vector<linear::Region> regions{
                 makeRegion(8, 6, 16, 12, 192U)};
             linear::OverlayStyle style{};
             style.boxesEnabled = false;
             style.markersEnabled = false;
             style.fillOpacity = 1.0F;
             style.fillColor = {1, 0.05F, 0.05F, 1};
             style.fillSecondaryColor = {0.05F, 0.05F, 1, 1};
             style.hatchSpacing = 6;
             style.hatchWidth = 2;
             style.hatchAngleDegrees = 35.0F;

             const auto render = [&](linear::FillMode mode) {
                 style.fillMode = mode;
                 return linear::renderOverlay(
                     source, mask, regions, style,
                     linear::OutputMode::SourceAndOverlay);
             };
             const std::array<linear::FillMode, 6U> modes{{
                 linear::FillMode::Solid,
                 linear::FillMode::Hatch,
                 linear::FillMode::Crosshatch,
                 linear::FillMode::XRay,
                 linear::FillMode::Duotone,
                 linear::FillMode::Invert}};
             std::vector<linear::Image> outputs;
             outputs.reserve(modes.size());
             for (const auto mode : modes) outputs.push_back(render(mode));

             for (std::size_t modeIndex = 0; modeIndex < outputs.size(); ++modeIndex) {
                 std::size_t changedInside = 0U;
                 for (int y = 6; y < 18; ++y) {
                     for (int x = 8; x < 24; ++x) {
                         const auto& before = source.at(x, y);
                         const auto& after = outputs[modeIndex].at(x, y);
                         changedInside +=
                             (std::fabs(before.r - after.r) > 1.0e-5F ||
                              std::fabs(before.g - after.g) > 1.0e-5F ||
                              std::fabs(before.b - after.b) > 1.0e-5F)
                                 ? 1U : 0U;
                     }
                 }
                 require(changedInside > 0U,
                         "each advertised fill mode changes pixels inside its box");
                 const auto& outside = outputs[modeIndex].at(2, 2);
                 requireNear(outside.r, source.at(2, 2).r, 1.0e-6F,
                             "fill mode leaves pixels outside boxes untouched");
                 requireNear(outside.g, source.at(2, 2).g, 1.0e-6F,
                             "fill mode leaves pixels outside boxes untouched");
             }

             style.boxShape = linear::BoxShape::Ellipse;
             for (const auto mode : modes) {
                 const auto ellipseOutput = render(mode);
                 const auto& ellipseCorner = ellipseOutput.at(8, 6);
                 const auto& sourceCorner = source.at(8, 6);
                 requireNear(ellipseCorner.r, sourceCorner.r, 1.0e-6F,
                             "every fill mode clips an ellipse corner");
                 requireNear(ellipseCorner.g, sourceCorner.g, 1.0e-6F,
                             "every fill mode clips an ellipse corner");
                 requireNear(ellipseCorner.b, sourceCorner.b, 1.0e-6F,
                             "every fill mode clips an ellipse corner");
             }
             style.boxShape = linear::BoxShape::Rectangle;

             for (std::size_t first = 0; first < outputs.size(); ++first) {
                 for (std::size_t second = first + 1U;
                      second < outputs.size(); ++second) {
                     bool differs = false;
                     for (int y = 6; y < 18 && !differs; ++y) {
                         for (int x = 8; x < 24; ++x) {
                             const auto& a = outputs[first].at(x, y);
                             const auto& b = outputs[second].at(x, y);
                             if (std::fabs(a.r - b.r) > 1.0e-5F ||
                                 std::fabs(a.g - b.g) > 1.0e-5F ||
                                 std::fabs(a.b - b.b) > 1.0e-5F) {
                                 differs = true;
                                 break;
                             }
                         }
                     }
                     require(differs,
                             "fill popup modes must not alias one another");
                 }
             }

             style.fillMode = linear::FillMode::Invert;
             style.fillOpacity = 0.35F;
             style.fillInvertStrength = 1.0F;
             const auto fullStrength = linear::renderOverlay(
                 source, mask, regions, style,
                 linear::OutputMode::SourceAndOverlay);
             const auto& originalCenter = source.at(16, 12);
             requireNear(fullStrength.at(16, 12).r,
                         1.0F - originalCenter.r, 1.0e-5F,
                         "full invert strength is clearly visible at default opacity");
             requireNear(fullStrength.at(16, 12).g,
                         1.0F - originalCenter.g, 1.0e-5F,
                         "full invert strength inverts green");

             style.fillOpacity = 0.0F;
             const auto zeroOpacityFullStrength = linear::renderOverlay(
                 source, mask, regions, style,
                 linear::OutputMode::SourceAndOverlay);
             requireNear(zeroOpacityFullStrength.at(16, 12).r,
                         1.0F - originalCenter.r, 1.0e-5F,
                         "invert strength remains active at zero fill opacity");
             requireNear(zeroOpacityFullStrength.at(16, 12).g,
                         1.0F - originalCenter.g, 1.0e-5F,
                         "zero fill opacity does not host-gate full invert");

             const auto overlayOnlyInvert = linear::renderOverlay(
                 source, mask, regions, style,
                 linear::OutputMode::OverlayOnly);
             requireNear(overlayOnlyInvert.at(16, 12).r,
                         1.0F - originalCenter.r, 1.0e-5F,
                         "overlay-only invert samples source red");
             requireNear(overlayOnlyInvert.at(16, 12).g,
                         1.0F - originalCenter.g, 1.0e-5F,
                         "overlay-only invert samples source green");
             requireNear(overlayOnlyInvert.at(16, 12).b,
                         1.0F - originalCenter.b, 1.0e-5F,
                         "overlay-only invert samples source blue");
             requireNear(overlayOnlyInvert.at(16, 12).a, 1.0F, 1.0e-5F,
                         "full overlay-only invert is opaque in-region");

             style.fillOpacity = 0.35F;
             style.fillInvertStrength = 0.0F;
             const auto zeroStrength = linear::renderOverlay(
                 source, mask, regions, style,
                 linear::OutputMode::SourceAndOverlay);
             for (int y = 6; y < 18; ++y) {
                 for (int x = 8; x < 24; ++x) {
                     requireNear(zeroStrength.at(x, y).r, source.at(x, y).r,
                                 1.0e-6F, "zero invert strength preserves red");
                     requireNear(zeroStrength.at(x, y).g, source.at(x, y).g,
                                 1.0e-6F, "zero invert strength preserves green");
                     requireNear(zeroStrength.at(x, y).b, source.at(x, y).b,
                                 1.0e-6F, "zero invert strength preserves blue");
                 }
             }
         }},
        {"Label formatting", [] {
             linear::LabelContext context{};
             context.region = makeRegion(10, 20, 6, 4, 24U);
             context.region.trackId = 42U;
             context.region.confidence = 0.875F;
             context.frameNumber = 12;
             const std::string label =
                 linear::formatLabel("ID {track_id}\nCONF {confidence:0.00}", context);
             require(label == "ID 42\nCONF 0.88", "formatted label");
             bool rejected = false;
             try {
                 static_cast<void>(linear::formatLabel("{unknown}", context));
             } catch (const std::invalid_argument&) {
                 rejected = true;
             }
             require(rejected, "unknown tokens must be rejected");
         }},
        {"Pixel formats and row bytes", [] {
             std::vector<std::uint8_t> bytes(16U, 0U);
             bytes[0] = 128U;
             bytes[1] = 64U;
             bytes[2] = 32U;
             bytes[3] = 128U;
             const auto image = linear::decodePixels(
                 {bytes.data(), 1, 1, 16, linear::PixelFormat::Rgba8, true});
             requireNear(image.at(0, 0).r, 1.0F, 0.01F, "unpremultiplied red");
             requireNear(image.at(0, 0).g, 0.5F, 0.01F, "unpremultiplied green");
             std::vector<float> output(4U);
             linear::encodePixels(
                 image, {output.data(), 1, 1, 16,
                         linear::PixelFormat::Rgba32Float, false});
             requireNear(output[0], 1.0F, 0.01F, "float output");
             requireNear(output[3], 128.0F / 255.0F, 0.01F, "float alpha");
             linear::Image grid(4, 4);
             grid.at(0, 0) = {1, 0, 0, 1};
             grid.at(2, 2) = {0, 1, 0, 1};
             const auto reduced = linear::resizeImageNearest(grid, 2, 2);
             requireNear(reduced.at(0, 0).r, 1.0F, 0.0F,
                         "image resize first sample");
             requireNear(reduced.at(1, 1).g, 1.0F, 0.0F,
                         "image resize scaled sample");
         }},
        {"Premiere render-path negotiation", [] {
             linear::HostCapabilities capabilities{};
             capabilities.application = linear::HostApplication::PremierePro;
             capabilities.supportsBgra = true;
             capabilities.smartRender = true;
             const auto path = linear::chooseRenderPath(capabilities);
             require(path.preferredFormat == linear::PixelFormat::Bgra8,
                     "Premiere selects native BGRA fallback");
             require(path.useSmartRender, "smart render capability is preserved");
             require(!path.limitation.empty(),
                     "unavailable temporal checkout is explained");
         }},
        {"Overlay-only alpha", [] {
             linear::Image source(32, 32, {0.2F, 0.2F, 0.2F, 1.0F});
             linear::Mask mask(32, 32);
             std::vector<linear::Region> regions{makeRegion(8, 8, 8, 8, 64U)};
             linear::OverlayStyle style{};
             const auto output = linear::renderOverlay(
                 source, mask, regions, style, linear::OutputMode::OverlayOnly);
             requireNear(output.at(0, 0).a, 0.0F, 1.0e-6F,
                         "untouched overlay is transparent");
             bool foundAlpha = false;
             for (const auto& pixel : output.pixels()) {
                 foundAlpha = foundAlpha || pixel.a > 0.0F;
             }
             require(foundAlpha, "overlay primitives produce alpha");
         }},
        {"Reduced analysis masks avoid full-resolution overlay upscaling", [] {
             linear::Image source(64, 48, {0.15F, 0.2F, 0.25F, 1.0F});
             linear::Mask reducedMask(16, 12, 1.0F);
             linear::OverlayStyle style{};
             style.boxesEnabled = false;
             style.markersEnabled = false;
             const auto overlay = linear::renderOverlay(
                 source, reducedMask, {}, style,
                 linear::OutputMode::SourceAndOverlay);
             require(overlay.width() == 64 && overlay.height() == 48,
                     "non-mask output does not require an upscaled analysis mask");
             bool rejected = false;
             try {
                 static_cast<void>(linear::renderOverlay(
                     source, reducedMask, {}, style,
                     linear::OutputMode::DetectionMask));
             } catch (const std::invalid_argument&) {
                 rejected = true;
             }
             require(rejected,
                     "detection-mask output still requires exact dimensions");
         }},
    };
}

} // namespace

int main() {
    int failures = 0;
    for (const auto& test : tests()) {
        try {
            test.second();
            std::cout << "[PASS] " << test.first << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.first << ": " << error.what() << '\n';
        }
    }
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All core tests passed\n";
    return 0;
}
