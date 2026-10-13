#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMetaType>
#include <QRectF>
#include <QString>

namespace chroma {

enum class LayerMaskType {
    None,
    AiSubject,
    AiBackground,
    AiSky,
    ColorRange,
    LuminanceRange,
    Rectangle,
    Ellipse
};

struct LayerMask {
    LayerMaskType type = LayerMaskType::None;
    QColor targetColor = QColor(Qt::white);
    double colorTolerance = 0.18; // Normalized RGB distance, 0.01..1.0.
    double luminanceMinimum = 0.25;
    double luminanceMaximum = 0.75;
    QRectF bounds = QRectF(0.2, 0.2, 0.6, 0.6); // Normalized image coordinates.
    double feather = 0.08; // Normalized edge softness, 0.0..0.5.

    [[nodiscard]] bool isGlobal() const { return type == LayerMaskType::None; }
    [[nodiscard]] QJsonObject toJson() const;
    static LayerMask fromJson(const QJsonObject &json);

    friend bool operator==(const LayerMask &lhs, const LayerMask &rhs);
    friend bool operator!=(const LayerMask &lhs, const LayerMask &rhs)
    {
        return !(lhs == rhs);
    }
};

[[nodiscard]] QString layerMaskTypeName(LayerMaskType type);

} // namespace chroma

Q_DECLARE_METATYPE(chroma::LayerMask)
Q_DECLARE_METATYPE(chroma::LayerMaskType)
