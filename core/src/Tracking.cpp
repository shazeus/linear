#include "linear/Tracking.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace linear {
namespace {

std::uint64_t mix64(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

float distanceSquared(Point lhs, Point rhs) noexcept {
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    return dx * dx + dy * dy;
}

std::uint64_t newTrackId(const Region& region, std::int64_t timeKey,
                         std::uint64_t seed, std::size_t ordinal) noexcept {
    const auto x = static_cast<std::uint64_t>(
        std::max(0.0F, std::round(region.centroid.x * 16.0F)));
    const auto y = static_cast<std::uint64_t>(
        std::max(0.0F, std::round(region.centroid.y * 16.0F)));
    const std::uint64_t hash =
        mix64(seed ^ mix64(static_cast<std::uint64_t>(timeKey)) ^
              mix64((x << 32U) | y) ^ mix64(region.pixelArea) ^
              mix64(static_cast<std::uint64_t>(ordinal)));
    return hash == 0U ? 1U : hash;
}

struct TrackState {
    Region last{};
    Point velocity{};
    std::size_t lastFrame{};
    int missed{};
};

struct Candidate {
    std::size_t track{};
    std::size_t region{};
    float score{};
    float distance{};
};

Rect predictedBounds(const TrackState& state, std::size_t frameIndex) noexcept {
    const float steps =
        static_cast<float>(std::max<std::size_t>(1U, frameIndex - state.lastFrame));
    Rect result = state.last.bounds;
    result.x += state.velocity.x * steps;
    result.y += state.velocity.y * steps;
    return result;
}

} // namespace

std::vector<FrameRegions> trackFrameWindow(
    std::vector<FrameRegions> frames, const TrackingSettings& settings) {
    std::stable_sort(frames.begin(), frames.end(),
                     [](const FrameRegions& lhs, const FrameRegions& rhs) {
                         return lhs.timeKey < rhs.timeKey;
                     });
    std::vector<TrackState> tracks;
    for (std::size_t frameIndex = 0; frameIndex < frames.size(); ++frameIndex) {
        auto& regions = frames[frameIndex].regions;
        std::vector<Candidate> candidates;
        for (std::size_t t = 0; t < tracks.size(); ++t) {
            const Rect predicted = predictedBounds(tracks[t], frameIndex);
            const Point predictedCenter = predicted.center();
            const float gate = std::max(1.0F, settings.maximumCentroidDistance) *
                (1.0F + static_cast<float>(tracks[t].missed) *
                            std::max(0.0F, settings.reacquisitionDistanceMultiplier - 1.0F));
            const float gateSquared = gate * gate;
            for (std::size_t r = 0; r < regions.size(); ++r) {
                const float centerDistanceSquared =
                    distanceSquared(predictedCenter, regions[r].centroid);
                float iou = 0.0F;
                if (centerDistanceSquared > gateSquared) {
                    iou = intersectionOverUnion(predicted, regions[r].bounds);
                    if (iou < settings.minimumIou) continue;
                } else {
                    iou = intersectionOverUnion(predicted, regions[r].bounds);
                }
                const float centerDistance = std::sqrt(centerDistanceSquared);
                const float distanceScore = 1.0F - std::min(1.0F, centerDistance / gate);
                const float areaA = std::max(1.0F, predicted.area());
                const float areaB = std::max(1.0F, regions[r].bounds.area());
                const float sizeScore = std::min(areaA, areaB) / std::max(areaA, areaB);
                const float predictedScore =
                    1.0F - std::min(1.0F, centerDistance /
                        std::max(1.0F, std::sqrt(areaA) + gate * 0.25F));
                const float totalWeight = std::max(
                    1.0e-6F, settings.iouWeight + settings.distanceWeight +
                                  settings.sizeWeight + settings.predictionWeight);
                const float score =
                    (settings.iouWeight * iou +
                     settings.distanceWeight * distanceScore +
                     settings.sizeWeight * sizeScore +
                     settings.predictionWeight * predictedScore) / totalWeight;
                if (score >= settings.minimumScore) {
                    candidates.push_back({t, r, score, centerDistance});
                }
            }
        }
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate& lhs, const Candidate& rhs) {
                             if (lhs.score != rhs.score) return lhs.score > rhs.score;
                             if (lhs.distance != rhs.distance) return lhs.distance < rhs.distance;
                             if (lhs.track != rhs.track) return lhs.track < rhs.track;
                             return lhs.region < rhs.region;
                         });
        std::vector<bool> usedTracks(tracks.size(), false);
        std::vector<bool> usedRegions(regions.size(), false);
        for (const Candidate& candidate : candidates) {
            if (usedTracks[candidate.track] || usedRegions[candidate.region]) continue;
            TrackState& state = tracks[candidate.track];
            Region& region = regions[candidate.region];
            const float steps = static_cast<float>(
                std::max<std::size_t>(1U, frameIndex - state.lastFrame));
            const Point observed{(region.centroid.x - state.last.centroid.x) / steps,
                                 (region.centroid.y - state.last.centroid.y) / steps};
            const float blend = clamp01(settings.velocitySmoothing);
            if (state.last.age <= 1) {
                state.velocity = observed;
            } else {
                state.velocity = {
                    state.velocity.x * blend + observed.x * (1.0F - blend),
                    state.velocity.y * blend + observed.y * (1.0F - blend)};
            }
            region.trackId = state.last.trackId;
            region.age = state.last.age + 1;
            region.velocity = state.velocity;
            state.last = region;
            state.lastFrame = frameIndex;
            state.missed = 0;
            usedTracks[candidate.track] = true;
            usedRegions[candidate.region] = true;
        }
        for (std::size_t t = 0; t < tracks.size(); ++t) {
            if (!usedTracks[t]) ++tracks[t].missed;
        }
        tracks.erase(std::remove_if(tracks.begin(), tracks.end(),
                                    [&](const TrackState& state) {
                                        return state.missed >
                                               std::max(0, settings.maximumMissedFrames);
                                    }),
                     tracks.end());
        for (std::size_t r = 0; r < regions.size(); ++r) {
            if (usedRegions[r]) continue;
            regions[r].trackId = newTrackId(
                regions[r], frames[frameIndex].timeKey,
                settings.deterministicSeed, r);
            regions[r].age = 1;
            regions[r].velocity = {};
            tracks.push_back({regions[r], {}, frameIndex, 0});
        }
    }
    return frames;
}

} // namespace linear
