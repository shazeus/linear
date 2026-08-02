#pragma once

#include "linear/Regions.h"

#include <cstdint>
#include <string>

namespace linear {

struct LabelContext {
    Region region;
    int frameWidth{};
    int frameHeight{};
    std::int64_t frameNumber{};
};

[[nodiscard]] std::string formatLabel(
    const std::string& labelTemplate, const LabelContext& context);

} // namespace linear
