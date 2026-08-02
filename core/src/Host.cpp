#include "linear/Host.h"

namespace linear {

RenderPath chooseRenderPath(const HostCapabilities& capabilities) {
    RenderPath path{};
    if (capabilities.application == HostApplication::PremierePro ||
        capabilities.application == HostApplication::MediaEncoder) {
        // Premiere's native RGB memory order is BGRA. The host adapter negotiates
        // it and the shared core converts through PixelBuffer.
        path.preferredFormat =
            capabilities.supports32Float ? PixelFormat::Rgba32Float
            : capabilities.supportsBgra  ? PixelFormat::Bgra8
                                        : PixelFormat::Rgba8;
    } else {
        path.preferredFormat =
            capabilities.supports32Float ? PixelFormat::Rgba32Float
            : capabilities.supports16Bit ? PixelFormat::Rgba16
                                         : PixelFormat::Rgba8;
    }
    path.useSmartRender = capabilities.smartRender;
    path.useTemporalCheckout = capabilities.frameCheckout;
    if (!capabilities.frameCheckout) {
        path.limitation =
            "Temporal frame checkout is unavailable; motion analysis must use "
            "the host-approved fallback for this render.";
    }
    return path;
}

} // namespace linear
