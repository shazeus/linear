#pragma once

#include "linear/Pixel.h"

#include <string>
#include <vector>

namespace linear {

enum class HostApplication { Unknown, AfterEffects, PremierePro, MediaEncoder };

struct HostCapabilities {
    HostApplication application{HostApplication::Unknown};
    int versionMajor{};
    int versionMinor{};
    bool smartRender{};
    bool multiFrameRendering{};
    bool customUi{};
    bool frameCheckout{};
    bool supports8Bit{true};
    bool supports16Bit{};
    bool supports32Float{};
    bool supportsBgra{};
    bool supportsYuv{};
};

struct RenderPath {
    PixelFormat preferredFormat{PixelFormat::Rgba8};
    bool useSmartRender{};
    bool useTemporalCheckout{};
    std::string limitation;
};

[[nodiscard]] RenderPath chooseRenderPath(const HostCapabilities& capabilities);

} // namespace linear
