#pragma once

#include "linear/Color.h"
#include "linear/Mask.h"
#include "linear/Regions.h"

#include <vector>

namespace linear {

struct ColorDetectionSettings {
    Color target{1.0F, 0.0F, 0.0F, 1.0F};
    ColorSpace colorSpace{ColorSpace::Rgb};
    float threshold{0.18F};
    float softness{0.05F};
    float alphaThreshold{0.01F};
    bool ignoreTransparent{true};
    bool invert{};
    int preBlurRadius{};
    int postBlurRadius{};
    int morphologyRadius{1};
};

struct MotionDetectionSettings {
    float lumaWeight{0.75F};
    float chromaWeight{0.25F};
    float threshold{0.08F};
    float softness{0.04F};
    float sceneCutThreshold{0.65F};
    int noiseRadius{1};
    bool compensateTranslation{};
    int maximumTranslation{8};
};

enum class CombinedOperation {
    ColorOrMotion,
    ColorAndMotion,
    ColorMinusMotion,
    MotionMinusColor
};

struct MotionResult {
    Mask confidence;
    Point estimatedTranslation{};
    bool sceneCut{};
};

[[nodiscard]] Mask detectColor(const Image& image, const ColorDetectionSettings& settings);
[[nodiscard]] MotionResult detectMotion(
    const Image& current, const Image& reference, const MotionDetectionSettings& settings);
[[nodiscard]] Mask combineMasks(
    const Mask& color, const Mask& motion, CombinedOperation operation);
// Chronological asymmetric attack/release filter. Persistence zero returns the
// newest mask exactly; higher values bridge short confidence dropouts.
[[nodiscard]] Mask temporalConfidenceWindow(
    const std::vector<Mask>& masks, float persistence);
[[nodiscard]] Mask temporalConfidenceWindow(
    const std::vector<const Mask*>& masks, float persistence);

} // namespace linear
