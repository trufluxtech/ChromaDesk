#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QString>

namespace chroma {

enum class RetouchType {
    None,
    HealSpot,
    CloneStamp,
    GeneratedImage
};

struct RetouchOperation {
    RetouchType type = RetouchType::None;
    QPointF source = QPointF(0.5, 0.5);
    QPointF target = QPointF(0.5, 0.5);
    double radius = 0.025; // Fraction of the image's shortest edge.
    QString generatedImagePath;

    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] QJsonObject toJson() const;
    static RetouchOperation fromJson(const QJsonObject &json);

    friend bool operator==(const RetouchOperation &lhs,
                           const RetouchOperation &rhs);
};

[[nodiscard]] QString retouchTypeName(RetouchType type);

} // namespace chroma

