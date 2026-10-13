#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMetaType>
#include <QPointF>
#include <QSizeF>
#include <QString>

namespace chroma {

enum class WatermarkType {
    None,
    Text,
    Image
};

struct WatermarkOverlay {
    WatermarkType type = WatermarkType::None;
    QString text;
    QString imagePath;
    QString fontFamily = QStringLiteral("Arial");
    QColor color = QColor(Qt::white);
    QColor outlineColor = QColor(Qt::black);
    QPointF position = QPointF(0.82, 0.90); // Normalized centre point.
    double size = 0.12; // Text: image height; image: image width.
    double opacity = 0.55;
    bool bold = false;
    bool outline = true;

    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] QSizeF normalizedSize(const QSize &imageSize) const;
    [[nodiscard]] QJsonObject toJson() const;
    static WatermarkOverlay fromJson(const QJsonObject &json);

    friend bool operator==(const WatermarkOverlay &lhs,
                           const WatermarkOverlay &rhs);
    friend bool operator!=(const WatermarkOverlay &lhs,
                           const WatermarkOverlay &rhs)
    {
        return !(lhs == rhs);
    }
};

} // namespace chroma

Q_DECLARE_METATYPE(chroma::WatermarkOverlay)
Q_DECLARE_METATYPE(chroma::WatermarkType)
