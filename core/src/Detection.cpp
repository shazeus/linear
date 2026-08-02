#include "linear/Detection.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace linear {
namespace {

void requireSameDimensions(const Image& lhs, const Image& rhs) {
    if (lhs.width() != rhs.width() || lhs.height() != rhs.height()) {
        throw std::invalid_argument("Input image dimensions must match");
    }
}

float motionDifference(Color current, Color reference,
                       const MotionDetectionSettings& settings) noexcept {
    const float currentLuma = luminance(current);
    const float referenceLuma = luminance(reference);
    const float lumaDifference = std::fabs(currentLuma - referenceLuma);
    const float currentCr = current.r - currentLuma;
    const float currentCb = current.b - currentLuma;
    const float referenceCr = reference.r - referenceLuma;
    const float referenceCb = reference.b - referenceLuma;
    const float chromaDifference =
        std::sqrt((currentCr - referenceCr) * (currentCr - referenceCr) +
                  (currentCb - referenceCb) * (currentCb - referenceCb)) /
        std::sqrt(2.0F);
    const float weight = settings.lumaWeight + settings.chromaWeight;
    return weight > 0.0F
               ? (settings.lumaWeight * lumaDifference +
                  settings.chromaWeight * chromaDifference) /
                     weight
               : 0.0F;
}

Point estimateTranslation(const Image& current, const Image& reference, int maximum) {
    const int step = std::max(1, std::min(current.width(), current.height()) / 128);
    double bestCost = std::numeric_limits<double>::infinity();
    Point best{};
    for (int dy = -maximum; dy <= maximum; ++dy) {
        for (int dx = -maximum; dx <= maximum; ++dx) {
            double cost = 0.0;
            std::size_t samples = 0U;
            for (int y = maximum; y < current.height() - maximum; y += step) {
                for (int x = maximum; x < current.width() - maximum; x += step) {
                    const int rx = x - dx;
                    const int ry = y - dy;
                    if (rx < 0 || ry < 0 || rx >= reference.width() ||
                        ry >= reference.height()) {
                        continue;
                    }
                    cost += std::fabs(luminance(current.at(x, y)) -
                                      luminance(reference.at(rx, ry)));
                    ++samples;
                }
            }
            if (samples > 0U) {
                cost /= static_cast<double>(samples);
            }
            if (cost < bestCost) {
                bestCost = cost;
                best = {static_cast<float>(dx), static_cast<float>(dy)};
            }
        }
    }
    return best;
}

} // namespace

Mask detectColor(const Image& image, const ColorDetectionSettings& settings) {
    Mask confidence(image.width(), image.height());
    const auto& pixels = image.pixels();
    auto& values = confidence.values();
    const bool thresholdInScan = settings.preBlurRadius <= 0;
    const float threshold = 1.0F - settings.threshold;
    const float softness = std::max(0.0F, settings.softness);
    const float lower = threshold - softness * 0.5F;
    const float upper = threshold + softness * 0.5F;
    const float inverseRange = softness > 1.0e-7F ? 1.0F / softness : 0.0F;
    const auto applyThreshold = [&](float value) noexcept {
        if (!thresholdInScan) return clamp01(value);
        return softness <= 1.0e-7F
            ? (value >= threshold ? 1.0F : 0.0F)
            : clamp01((value - lower) * inverseRange);
    };
    const auto scan = [&](const auto& distance) {
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            const Color pixel = pixels[i];
            if (settings.ignoreTransparent && pixel.a <= settings.alphaThreshold) {
                values[i] = applyThreshold(settings.invert ? 1.0F : 0.0F);
                continue;
            }
            const float measured = clamp01(distance(pixel));
            const float value = settings.invert ? measured : 1.0F - measured;
            values[i] = applyThreshold(value);
        }
    };
    switch (settings.colorSpace) {
    case ColorSpace::Rgb: {
        constexpr float inverseSqrtThree = 0.57735026918962576451F;
        const Color target = settings.target;
        if (thresholdInScan) {
            const float solidBoundary = settings.invert
                ? std::max(0.0F, lower)
                : std::max(0.0F, 1.0F - upper);
            const float emptyBoundary = settings.invert
                ? std::min(1.0F, upper)
                : std::min(1.0F, 1.0F - lower);
            const float solidSquared = solidBoundary * solidBoundary;
            const float emptySquared = emptyBoundary * emptyBoundary;
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                const Color pixel = pixels[i];
                if (settings.ignoreTransparent &&
                    pixel.a <= settings.alphaThreshold) {
                    values[i] = applyThreshold(settings.invert ? 1.0F : 0.0F);
                    continue;
                }
                const float dr = pixel.r - target.r;
                const float dg = pixel.g - target.g;
                const float db = pixel.b - target.b;
                const float squared = (dr * dr + dg * dg + db * db) / 3.0F;
                if (softness <= 1.0e-7F) {
                    values[i] = settings.invert
                        ? (squared >= threshold * threshold ? 1.0F : 0.0F)
                        : (squared <= settings.threshold * settings.threshold
                               ? 1.0F : 0.0F);
                } else if (settings.invert) {
                    if (squared <= solidSquared) {
                        values[i] = 0.0F;
                    } else if (squared >= emptySquared) {
                        values[i] = 1.0F;
                    } else {
                        values[i] = clamp01(
                            (std::sqrt(squared) - lower) * inverseRange);
                    }
                } else {
                    if (squared <= solidSquared) {
                        values[i] = 1.0F;
                    } else if (squared >= emptySquared) {
                        values[i] = 0.0F;
                    } else {
                        values[i] = clamp01(
                            (1.0F - std::sqrt(squared) - lower) * inverseRange);
                    }
                }
            }
            break;
        }
        scan([&](Color pixel) noexcept {
            const float dr = pixel.r - target.r;
            const float dg = pixel.g - target.g;
            const float db = pixel.b - target.b;
            return std::sqrt(dr * dr + dg * dg + db * db) * inverseSqrtThree;
        });
        break;
    }
    case ColorSpace::Hsv: {
        constexpr float inverseSqrtThree = 0.57735026918962576451F;
        const Hsv target = rgbToHsv(settings.target);
        scan([&](Color pixel) noexcept {
            const Hsv value = rgbToHsv(pixel);
            const float rawHue = std::fabs(value.h - target.h);
            const float hue = std::min(rawHue, 1.0F - rawHue);
            const float saturation = value.s - target.s;
            const float brightness = value.v - target.v;
            return std::sqrt(4.0F * hue * hue + saturation * saturation +
                             brightness * brightness) * inverseSqrtThree;
        });
        break;
    }
    case ColorSpace::Lab: {
        const Lab target = rgbToLab(settings.target);
        scan([&](Color pixel) noexcept {
            const Lab value = rgbToLab(pixel);
            const float dl = value.l - target.l;
            const float da = value.a - target.a;
            const float db = value.b - target.b;
            return std::min(
                1.0F, std::sqrt(dl * dl + da * da + db * db) / 258.0F);
        });
        break;
    }
    }
    if (settings.preBlurRadius > 0) {
        confidence = boxBlur(confidence, settings.preBlurRadius);
        confidence = thresholdSoft(confidence, threshold, settings.softness);
    }
    if (settings.postBlurRadius > 0) {
        confidence = boxBlur(confidence, settings.postBlurRadius);
    }
    if (settings.morphologyRadius > 0) {
        confidence = morphologicalOpenClose(
            confidence, settings.morphologyRadius);
    }
    return confidence;
}

MotionResult detectMotion(
    const Image& current, const Image& reference, const MotionDetectionSettings& settings) {
    requireSameDimensions(current, reference);
    MotionResult result{Mask(current.width(), current.height()), {}, false};
    if (current.empty()) {
        return result;
    }
    if (settings.compensateTranslation && settings.maximumTranslation > 0) {
        result.estimatedTranslation =
            estimateTranslation(current, reference, settings.maximumTranslation);
    }
    double total = 0.0;
    std::size_t compared = 0U;
    const int dx = static_cast<int>(std::lround(result.estimatedTranslation.x));
    const int dy = static_cast<int>(std::lround(result.estimatedTranslation.y));
    const int width = current.width();
    const int height = current.height();
    const auto& currentPixels = current.pixels();
    const auto& referencePixels = reference.pixels();
    auto& confidence = result.confidence.values();
    for (int y = 0; y < current.height(); ++y) {
        for (int x = 0; x < current.width(); ++x) {
            const int rx = x - dx;
            const int ry = y - dy;
            const std::size_t destination =
                static_cast<std::size_t>(y) * width + x;
            if (rx < 0 || ry < 0 || rx >= width || ry >= height) {
                confidence[destination] = 0.0F;
                continue;
            }
            const float difference =
                motionDifference(
                    currentPixels[destination],
                    referencePixels[static_cast<std::size_t>(ry) * width + rx],
                    settings);
            confidence[destination] = difference;
            total += difference;
            ++compared;
        }
    }
    const float meanDifference =
        compared > 0U ? static_cast<float>(total / static_cast<double>(compared)) : 0.0F;
    result.sceneCut = meanDifference >= settings.sceneCutThreshold;
    if (result.sceneCut) {
        std::fill(result.confidence.values().begin(), result.confidence.values().end(), 0.0F);
        return result;
    }
    if (settings.noiseRadius > 0) {
        result.confidence = boxBlur(result.confidence, settings.noiseRadius);
    }
    result.confidence =
        thresholdSoft(result.confidence, settings.threshold, settings.softness);
    if (settings.noiseRadius > 0) {
        result.confidence = morphologicalOpen(result.confidence, settings.noiseRadius);
    }
    return result;
}

Mask combineMasks(const Mask& color, const Mask& motion, CombinedOperation operation) {
    if (color.width() != motion.width() || color.height() != motion.height()) {
        throw std::invalid_argument("Mask dimensions must match");
    }
    Mask output(color.width(), color.height());
    for (std::size_t i = 0; i < output.values().size(); ++i) {
        const float c = clamp01(color.values()[i]);
        const float m = clamp01(motion.values()[i]);
        switch (operation) {
        case CombinedOperation::ColorOrMotion:
            output.values()[i] = std::max(c, m);
            break;
        case CombinedOperation::ColorAndMotion:
            output.values()[i] = std::min(c, m);
            break;
        case CombinedOperation::ColorMinusMotion:
            output.values()[i] = c * (1.0F - m);
            break;
        case CombinedOperation::MotionMinusColor:
            output.values()[i] = m * (1.0F - c);
            break;
        }
    }
    return output;
}

Mask temporalConfidenceWindow(
    const std::vector<Mask>& masks, float persistence) {
    std::vector<const Mask*> views;
    views.reserve(masks.size());
    for (const Mask& mask : masks) views.push_back(&mask);
    return temporalConfidenceWindow(views, persistence);
}

Mask temporalConfidenceWindow(
    const std::vector<const Mask*>& masks, float persistence) {
    if (masks.empty() || masks.front() == nullptr) return {};
    Mask state = *masks.front();
    const float release = clamp01(persistence);
    for (std::size_t sample = 1; sample < masks.size(); ++sample) {
        if (masks[sample] == nullptr ||
            state.width() != masks[sample]->width() ||
            state.height() != masks[sample]->height()) {
            throw std::invalid_argument("Mask dimensions must match");
        }
        const auto& currentValues = masks[sample]->values();
        auto& stateValues = state.values();
        for (std::size_t i = 0; i < state.values().size(); ++i) {
            const float current = clamp01(currentValues[i]);
            const float previous = clamp01(stateValues[i]);
            stateValues[i] = current >= previous
                ? current
                : current + (previous - current) * release;
        }
    }
    return state;
}

} // namespace linear
