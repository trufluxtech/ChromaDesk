#pragma once

#include "core/EditState.h"
#include "core/LayerStack.h"

#include <QImage>

namespace chroma {

class ImageProcessor final {
public:
    // maxEdge == 0 keeps full resolution. Processing never mutates source.
    [[nodiscard]] static QImage render(const QImage &source, const EditState &state,
                                       int maxEdge = 0);
    [[nodiscard]] static QImage renderLayers(const QImage &source,
                                             const LayerStack &layers,
                                             int maxEdge = 0);
    [[nodiscard]] static QImage orientedAndCropped(const QImage &source,
                                                   const EditState &state,
                                                   int maxEdge = 0);
    [[nodiscard]] static QImage withBorder(const QImage &source,
                                           const EditState &state,
                                           int maxEdge = 0);
    [[nodiscard]] static EditState suggestAutoEnhance(const QImage &source);

private:
    [[nodiscard]] static QImage blend(const QImage &bottom, const QImage &top,
                                      double opacity, const QImage &mask = {});
};

} // namespace chroma
