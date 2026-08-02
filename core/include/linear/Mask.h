#pragma once

#include "linear/Types.h"

namespace linear {

[[nodiscard]] Mask boxBlur(const Mask& input, int radius);
[[nodiscard]] Mask erode(const Mask& input, int radius);
[[nodiscard]] Mask dilate(const Mask& input, int radius);
[[nodiscard]] Mask morphologicalOpen(const Mask& input, int radius);
[[nodiscard]] Mask morphologicalClose(const Mask& input, int radius);
[[nodiscard]] Mask morphologicalOpenClose(const Mask& input, int radius);
[[nodiscard]] Mask thresholdSoft(const Mask& input, float threshold, float softness);
[[nodiscard]] Mask resizeNearest(const Mask& input, int width, int height);

} // namespace linear
