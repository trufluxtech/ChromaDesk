#include "core/AppleVisionMaskEngine.h"

namespace chroma {

bool AppleVisionMaskEngine::available()
{
    return false;
}

QImage AppleVisionMaskEngine::foregroundMask(const QImage &)
{
    return {};
}

} // namespace chroma
