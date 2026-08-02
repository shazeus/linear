#include "linear/Stabilization.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <unordered_map>
#include <vector>

namespace linear {
namespace {

float followAmount(float strength, float minimumFollow) noexcept {
    const float clampedStrength = clamp01(strength);
    const float clampedMinimum = clamp01(minimumFollow);
    return std::clamp(1.0F - clampedStrength, clampedMinimum, 1.0F);
}

float follow(float previous, float current, float amount) noexcept {
    return previous + (current - previous) * amount;
}

Point follow(Point previous, Point current, float amount) noexcept {
    return {follow(previous.x, current.x, amount),
            follow(previous.y, current.y, amount)};
}

std::vector<std::size_t> timeOrder(const std::vector<FrameRegions>& frames) {
    std::vector<std::size_t> order(frames.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t lhs, std::size_t rhs) {
                         return frames[lhs].timeKey < frames[rhs].timeKey;
                     });
    return order;
}

std::vector<std::size_t> timeOrder(const std::vector<TimedPath>& paths) {
    std::vector<std::size_t> order(paths.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t lhs, std::size_t rhs) {
                         if (paths[lhs].timeKey != paths[rhs].timeKey) {
                             return paths[lhs].timeKey < paths[rhs].timeKey;
                         }
                         return paths[lhs].stableId < paths[rhs].stableId;
                     });
    return order;
}

struct RegionState {
    Point centroid{};
    Point velocity{};
    float width{};
    float height{};
    bool initialized{};
};

} // namespace

std::vector<FrameRegions> stabilizeFrameWindow(
    const std::vector<FrameRegions>& frames,
    const StabilizationSettings& settings) {
    if (settings.mode == StabilizationMode::LegacyRaw || frames.empty()) {
        return frames;
    }
    std::vector<FrameRegions> output = frames;
    const auto order = timeOrder(frames);
    if (settings.mode == StabilizationMode::CinematicSmooth) {
        const int radius = std::max(1, settings.cinematicRadius);
        std::vector<std::unordered_map<std::uint64_t, std::size_t>> trackIndices(
            frames.size());
        for (std::size_t frameIndex = 0; frameIndex < frames.size(); ++frameIndex) {
            auto& indices = trackIndices[frameIndex];
            indices.reserve(frames[frameIndex].regions.size());
            for (std::size_t regionIndex = 0;
                 regionIndex < frames[frameIndex].regions.size(); ++regionIndex) {
                const std::uint64_t trackId =
                    frames[frameIndex].regions[regionIndex].trackId;
                if (trackId != 0U) indices.emplace(trackId, regionIndex);
            }
        }
        for (std::size_t ordered = 0; ordered < order.size(); ++ordered) {
            const std::size_t frameIndex = order[ordered];
            for (std::size_t regionIndex = 0;
                 regionIndex < frames[frameIndex].regions.size(); ++regionIndex) {
                const Region& raw = frames[frameIndex].regions[regionIndex];
                if (raw.trackId == 0U) continue;
                double weightSum = 0.0;
                double x = 0.0, y = 0.0, width = 0.0, height = 0.0;
                const std::size_t begin =
                    ordered > static_cast<std::size_t>(radius)
                        ? ordered - static_cast<std::size_t>(radius) : 0U;
                const std::size_t end = std::min(
                    order.size() - 1U, ordered + static_cast<std::size_t>(radius));
                for (std::size_t sample = begin; sample <= end; ++sample) {
                    const std::size_t sampleFrame = order[sample];
                    const auto found = trackIndices[sampleFrame].find(raw.trackId);
                    if (found == trackIndices[sampleFrame].end()) continue;
                    const Region& matched =
                        frames[sampleFrame].regions[found->second];
                    const double delta = static_cast<double>(
                        sample > ordered ? sample - ordered : ordered - sample);
                    const double weight = std::exp(-0.5 * delta * delta /
                        std::max(1.0, static_cast<double>(radius)));
                    weightSum += weight;
                    x += matched.centroid.x * weight;
                    y += matched.centroid.y * weight;
                    width += matched.bounds.width * weight;
                    height += matched.bounds.height * weight;
                }
                if (weightSum <= 0.0) continue;
                Region& result = output[frameIndex].regions[regionIndex];
                const Point center{static_cast<float>(x / weightSum),
                                   static_cast<float>(y / weightSum)};
                const float w = static_cast<float>(width / weightSum);
                const float h = static_cast<float>(height / weightSum);
                const Point delta{center.x - raw.centroid.x, center.y - raw.centroid.y};
                result.centroid = center;
                result.weightedCentroid = {raw.weightedCentroid.x + delta.x,
                                           raw.weightedCentroid.y + delta.y};
                result.bounds = {raw.bounds.center().x + delta.x - w * 0.5F,
                                 raw.bounds.center().y + delta.y - h * 0.5F, w, h};
            }
        }
        return output;
    }
    const float positionFollow =
        followAmount(settings.positionStrength, settings.minimumFollow);
    const float sizeFollow =
        followAmount(settings.sizeStrength, settings.minimumFollow);
    std::unordered_map<std::uint64_t, RegionState> states;

    for (const std::size_t frameIndex : order) {
        for (std::size_t regionIndex = 0;
             regionIndex < frames[frameIndex].regions.size(); ++regionIndex) {
            const Region& raw = frames[frameIndex].regions[regionIndex];
            Region& stabilized = output[frameIndex].regions[regionIndex];
            RegionState& state = states[raw.trackId];
            if (!state.initialized || raw.trackId == 0U) {
                state.centroid = raw.centroid;
                state.width = raw.bounds.width;
                state.height = raw.bounds.height;
                state.initialized = true;
                stabilized.velocity = {};
                continue;
            }

            const Point previousCentroid = state.centroid;
            float activeFollow = positionFollow;
            if (settings.mode == StabilizationMode::AdaptiveSmooth) {
                const float scale = std::max(
                    1.0F, std::hypot(raw.bounds.width, raw.bounds.height));
                const float motion = clamp01(
                    std::hypot(raw.centroid.x - state.centroid.x,
                               raw.centroid.y - state.centroid.y) / scale);
                activeFollow += (1.0F - activeFollow) *
                                clamp01(settings.motionResponse) * motion;
                activeFollow *=
                    1.0F - clamp01(settings.confidenceResponse) *
                               (1.0F - clamp01(raw.confidence)) * 0.5F;
                activeFollow = std::clamp(
                    activeFollow, settings.minimumFollow, 1.0F);
            }
            state.centroid = follow(state.centroid, raw.centroid, activeFollow);
            state.width = follow(state.width, raw.bounds.width, sizeFollow);
            state.height = follow(state.height, raw.bounds.height, sizeFollow);
            if (settings.mode == StabilizationMode::PredictiveSmooth) {
                const Point observed{state.centroid.x - previousCentroid.x,
                                     state.centroid.y - previousCentroid.y};
                const float blend = clamp01(settings.velocitySmoothing);
                state.velocity = {
                    state.velocity.x * blend + observed.x * (1.0F - blend),
                    state.velocity.y * blend + observed.y * (1.0F - blend)};
                const float limit = std::max(1.0F, std::max(state.width, state.height) * 0.35F);
                const Point predicted{
                    state.centroid.x + state.velocity.x *
                        clamp01(settings.predictionLookahead),
                    state.centroid.y + state.velocity.y *
                        clamp01(settings.predictionLookahead)};
                const Point error{predicted.x - raw.centroid.x,
                                  predicted.y - raw.centroid.y};
                const float magnitude = std::hypot(error.x, error.y);
                if (magnitude > limit) {
                    state.centroid = {raw.centroid.x + error.x * limit / magnitude,
                                      raw.centroid.y + error.y * limit / magnitude};
                } else {
                    state.centroid = predicted;
                }
            }

            const Point delta{state.centroid.x - raw.centroid.x,
                              state.centroid.y - raw.centroid.y};
            const Point translatedBoundsCenter{
                raw.bounds.center().x + delta.x,
                raw.bounds.center().y + delta.y};
            stabilized.centroid = state.centroid;
            stabilized.weightedCentroid = {
                raw.weightedCentroid.x + delta.x,
                raw.weightedCentroid.y + delta.y};
            stabilized.bounds = {
                translatedBoundsCenter.x - state.width * 0.5F,
                translatedBoundsCenter.y - state.height * 0.5F,
                state.width,
                state.height};
            stabilized.velocity = {
                state.centroid.x - previousCentroid.x,
                state.centroid.y - previousCentroid.y};
        }
    }
    return output;
}

std::vector<TimedPath> stabilizePathWindow(
    const std::vector<TimedPath>& paths,
    const StabilizationSettings& settings) {
    if (settings.mode == StabilizationMode::LegacyRaw || paths.empty()) {
        return paths;
    }
    std::vector<TimedPath> output = paths;
    const float amount =
        followAmount(settings.positionStrength, settings.minimumFollow);
    std::unordered_map<std::uint64_t, std::vector<Point>> states;

    for (const std::size_t pathIndex : timeOrder(paths)) {
        const TimedPath& raw = paths[pathIndex];
        auto found = states.find(raw.stableId);
        if (raw.stableId == 0U || found == states.end() ||
            found->second.size() != raw.points.size()) {
            states[raw.stableId] = raw.points;
            continue;
        }
        auto& state = found->second;
        for (std::size_t pointIndex = 0; pointIndex < raw.points.size();
             ++pointIndex) {
            state[pointIndex] =
                follow(state[pointIndex], raw.points[pointIndex], amount);
            output[pathIndex].points[pointIndex] = state[pointIndex];
        }
    }
    return output;
}

} // namespace linear
