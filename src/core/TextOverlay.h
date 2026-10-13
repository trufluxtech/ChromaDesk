#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMetaType>
#include <QPointF>
#include <QString>

namespace chroma {

struct TextOverlay {
    QString text;
    QString fontFamily = QStringLiteral("Arial");
    QColor color = QColor(Qt::white);
    QColor outlineColor = QColor(Qt::black);
    double size = 0.07; // Fraction of image height, 0.01..0.30.
    QPointF position = QPointF(0.5, 0.88); // Normalized anchor.
    bool bold = false;
    bool italic = false;
    bool outline = true;

    [[nodiscard]] bool isEnabled() const { return !text.trimmed().isEmpty(); }
    [[nodiscard]] QJsonObject toJson() const;
    static TextOverlay fromJson(const QJsonObject &json);

    friend bool operator==(const TextOverlay &lhs, const TextOverlay &rhs);
    friend bool operator!=(const TextOverlay &lhs, const TextOverlay &rhs)
    {
        return !(lhs == rhs);
    }
};

} // namespace chroma

Q_DECLARE_METATYPE(chroma::TextOverlay)
