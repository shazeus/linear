#pragma once

#include "linear/Tracking.h"

#include <cstdint>
#include <vector>

namespace linear {

enum class StabilizationMode {
    LegacyRaw = 0,
    BasicSmooth = 1,
    AdaptiveSmooth = 2,
    CinematicSmooth = 3,
    PredictiveSmooth = 4
};

struct StabilizationSettings {
    StabilizationMode mode{StabilizationMode::LegacyRaw};
    // Zero preserves raw geometry. One applies the strongest supported follow
    // damping while retaining a small response to genuine motion.
    float positionStrength{0.65F};
    float sizeStrength{0.65F};
    float minimumFollow{0.08F};
    float motionResponse{0.65F};
    float confidenceResponse{0.35F};
    float predictionLookahead{0.65F};
    float velocitySmoothing{0.55F};
    int cinematicRadius{2};
};

struct TimedPath {
    std::int64_t timeKey{};
    std::uint64_t stableId{};
    std::vector<Point> points;
};

// LegacyRaw returns the input byte-for-byte at the value level, including frame
// order. Other modes use a time-sorted index internally and restore caller order.
[[nodiscard]] std::vector<FrameRegions> stabilizeFrameWindow(
    const std::vector<FrameRegions>& frames,
    const StabilizationSettings& settings);

// Applies the same explicit-window rule to fixed-topology derived paths.
// Paths with a new ID or changed point count start a new deterministic state.
[[nodiscard]] std::vector<TimedPath> stabilizePathWindow(
    const std::vector<TimedPath>& paths,
    const StabilizationSettings& settings);

} // namespace linear
