#pragma once

#include "linear/Regions.h"

#include <cstdint>
#include <vector>

namespace linear {

struct TrackingSettings {
    float minimumIou{0.05F};
    float maximumCentroidDistance{100.0F};
    float iouWeight{0.65F};
    float distanceWeight{0.35F};
    float sizeWeight{0.15F};
    float predictionWeight{0.25F};
    float minimumScore{0.12F};
    float velocitySmoothing{0.5F};
    float reacquisitionDistanceMultiplier{1.75F};
    int maximumMissedFrames{2};
    std::uint64_t deterministicSeed{0x4C494E454152ULL};
};

struct FrameRegions {
    std::int64_t timeKey{};
    std::vector<Region> regions;
};

// Input frames are explicit and are sorted by time internally. No mutable render
// history is used, so identical frame windows produce identical track IDs.
[[nodiscard]] std::vector<FrameRegions> trackFrameWindow(
    std::vector<FrameRegions> frames, const TrackingSettings& settings);

} // namespace linear
