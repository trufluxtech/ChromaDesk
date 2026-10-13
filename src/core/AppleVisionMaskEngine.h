#pragma once

#include <QImage>

namespace chroma {

// Native foreground instance segmentation on macOS 14 and later. Other
// platforms and older macOS releases report unavailable and let callers use
// their deterministic fallback.
class AppleVisionMaskEngine final {
public:
    [[nodiscard]] static bool available();
    [[nodiscard]] static QImage foregroundMask(const QImage &reference);
};

} // namespace chroma
