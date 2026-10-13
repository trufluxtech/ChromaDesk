#include "core/TextOverlay.h"

#include <QtGlobal>

#include <cmath>

namespace chroma {
namespace {

constexpr double epsilon = 0.0001;

bool closeEnough(double lhs, double rhs)
{
    return std::abs(lhs - rhs) < epsilon;
}

} // namespace

bool operator==(const TextOverlay &lhs, const TextOverlay &rhs)
{
    return lhs.text == rhs.text
        && lhs.fontFamily == rhs.fontFamily
        && lhs.color == rhs.color
        && lhs.outlineColor == rhs.outlineColor
        && closeEnough(lhs.size, rhs.size)
        && closeEnough(lhs.position.x(), rhs.position.x())
        && closeEnough(lhs.position.y(), rhs.position.y())
        && lhs.bold == rhs.bold
        && lhs.italic == rhs.italic
        && lhs.outline == rhs.outline;
}

QJsonObject TextOverlay::toJson() const
{
    return {
        {QStringLiteral("text"), text},
        {QStringLiteral("fontFamily"), fontFamily},
        {QStringLiteral("color"), color.name(QColor::HexArgb)},
        {QStringLiteral("outlineColor"), outlineColor.name(QColor::HexArgb)},
        {QStringLiteral("size"), size},
        {QStringLiteral("x"), position.x()},
        {QStringLiteral("y"), position.y()},
        {QStringLiteral("bold"), bold},
        {QStringLiteral("italic"), italic},
        {QStringLiteral("outline"), outline}
    };
}

TextOverlay TextOverlay::fromJson(const QJsonObject &json)
{
    TextOverlay overlay;
    overlay.text = json.value(QStringLiteral("text")).toString();
    const QString requestedFamily = json.value(QStringLiteral("fontFamily")).toString();
    if (!requestedFamily.trimmed().isEmpty())
        overlay.fontFamily = requestedFamily;
    const QColor requestedColor(json.value(QStringLiteral("color")).toString());
    if (requestedColor.isValid())
        overlay.color = requestedColor;
    const QColor requestedOutline(json.value(QStringLiteral("outlineColor")).toString());
    if (requestedOutline.isValid())
        overlay.outlineColor = requestedOutline;
    overlay.size = qBound(0.01, json.value(QStringLiteral("size")).toDouble(0.07), 0.30);
    overlay.position = QPointF(
        qBound(0.0, json.value(QStringLiteral("x")).toDouble(0.5), 1.0),
        qBound(0.0, json.value(QStringLiteral("y")).toDouble(0.88), 1.0));
    overlay.bold = json.value(QStringLiteral("bold")).toBool(false);
    overlay.italic = json.value(QStringLiteral("italic")).toBool(false);
    overlay.outline = json.value(QStringLiteral("outline")).toBool(true);
    return overlay;
}

} // namespace chroma
