#pragma once

#include "core/LayerMask.h"

#include <QImage>
#include <QString>

namespace chroma {

// Local mask generation. On macOS 14+, Subject/Background use Apple Vision
// foreground instance segmentation. Other systems retain a deterministic
// saliency fallback. Pixels never leave the computer.
class SmartMaskEngine final {
public:
    [[nodiscard]] static QImage generate(const QImage &reference,
                                         const LayerMask &mask);
    [[nodiscard]] static QString engineName(LayerMaskType type);
    [[nodiscard]] static bool highAccuracyAvailable();
};

} // namespace chroma
