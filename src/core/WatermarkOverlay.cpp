#include "core/WatermarkOverlay.h"

#include <QFont>
#include <QFontMetricsF>
#include <QImageReader>
#include <QtGlobal>

#include <cmath>

namespace chroma {
namespace {

constexpr double epsilon = 0.0001;

bool closeEnough(double lhs, double rhs)
{
    return std::abs(lhs - rhs) < epsilon;
}

QString typeKey(WatermarkType type)
{
    switch (type) {
    case WatermarkType::Text: return QStringLiteral("text");
    case WatermarkType::Image: return QStringLiteral("image");
    case WatermarkType::None: break;
    }
    return QStringLiteral("none");
}

} // namespace

bool WatermarkOverlay::isEnabled() const
{
    return opacity > 0.0 && ((type == WatermarkType::Text && !text.trimmed().isEmpty())
        || (type == WatermarkType::Image && !imagePath.trimmed().isEmpty()));
}

QSizeF WatermarkOverlay::normalizedSize(const QSize &imageSize) const
{
    if (!isEnabled() || !imageSize.isValid())
        return {};
    if (type == WatermarkType::Image) {
        QImageReader reader(imagePath);
        const QSize sourceSize = reader.size();
        const double aspect = sourceSize.isValid()
            ? static_cast<double>(sourceSize.width()) / sourceSize.height() : 1.0;
        const double height = size * imageSize.width()
            / qMax(0.01, aspect * imageSize.height());
        return QSizeF(size, qBound(0.01, height, 0.90));
    }

    QFont font(fontFamily);
    font.setPixelSize(100);
    font.setBold(bold);
    const QFontMetricsF metrics(font);
    const QRectF bounds = metrics.boundingRect(text);
    const double width = qMax(1.0, bounds.width()) / qMax(1.0, bounds.height())
        * size * imageSize.height() / imageSize.width();
    return QSizeF(qBound(0.02, width, 0.95), size);
}

bool operator==(const WatermarkOverlay &lhs, const WatermarkOverlay &rhs)
{
    return lhs.type == rhs.type && lhs.text == rhs.text
        && lhs.imagePath == rhs.imagePath && lhs.fontFamily == rhs.fontFamily
        && lhs.color == rhs.color && lhs.outlineColor == rhs.outlineColor
        && closeEnough(lhs.position.x(), rhs.position.x())
        && closeEnough(lhs.position.y(), rhs.position.y())
        && closeEnough(lhs.size, rhs.size) && closeEnough(lhs.opacity, rhs.opacity)
        && lhs.bold == rhs.bold && lhs.outline == rhs.outline;
}

QJsonObject WatermarkOverlay::toJson() const
{
    return {
        {QStringLiteral("type"), typeKey(type)},
        {QStringLiteral("text"), text},
        {QStringLiteral("imagePath"), imagePath},
        {QStringLiteral("fontFamily"), fontFamily},
        {QStringLiteral("color"), color.name(QColor::HexArgb)},
        {QStringLiteral("outlineColor"), outlineColor.name(QColor::HexArgb)},
        {QStringLiteral("x"), position.x()},
        {QStringLiteral("y"), position.y()},
        {QStringLiteral("size"), size},
        {QStringLiteral("opacity"), opacity},
        {QStringLiteral("bold"), bold},
        {QStringLiteral("outline"), outline}
    };
}

WatermarkOverlay WatermarkOverlay::fromJson(const QJsonObject &json)
{
    WatermarkOverlay result;
    const QString requestedType = json.value(QStringLiteral("type")).toString();
    if (requestedType == QStringLiteral("text"))
        result.type = WatermarkType::Text;
    else if (requestedType == QStringLiteral("image"))
        result.type = WatermarkType::Image;
    result.text = json.value(QStringLiteral("text")).toString();
    result.imagePath = json.value(QStringLiteral("imagePath")).toString();
    const QString family = json.value(QStringLiteral("fontFamily")).toString();
    if (!family.trimmed().isEmpty())
        result.fontFamily = family;
    const QColor color(json.value(QStringLiteral("color")).toString());
    if (color.isValid()) result.color = color;
    const QColor outlineColor(json.value(QStringLiteral("outlineColor")).toString());
    if (outlineColor.isValid()) result.outlineColor = outlineColor;
    result.position = QPointF(
        qBound(0.0, json.value(QStringLiteral("x")).toDouble(0.82), 1.0),
        qBound(0.0, json.value(QStringLiteral("y")).toDouble(0.90), 1.0));
    result.size = qBound(0.01, json.value(QStringLiteral("size")).toDouble(0.12), 0.50);
    result.opacity = qBound(0.0,
        json.value(QStringLiteral("opacity")).toDouble(0.55), 1.0);
    result.bold = json.value(QStringLiteral("bold")).toBool(false);
    result.outline = json.value(QStringLiteral("outline")).toBool(true);
    return result;
}

} // namespace chroma
