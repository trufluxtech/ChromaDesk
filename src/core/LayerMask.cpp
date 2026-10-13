#include "core/LayerMask.h"

#include <QtGlobal>

#include <cmath>

namespace chroma {
namespace {

constexpr double epsilon = 0.0001;

QString typeKey(LayerMaskType type)
{
    switch (type) {
    case LayerMaskType::AiSubject: return QStringLiteral("aiSubject");
    case LayerMaskType::AiBackground: return QStringLiteral("aiBackground");
    case LayerMaskType::AiSky: return QStringLiteral("aiSky");
    case LayerMaskType::ColorRange: return QStringLiteral("colorRange");
    case LayerMaskType::LuminanceRange: return QStringLiteral("luminanceRange");
    case LayerMaskType::Rectangle: return QStringLiteral("rectangle");
    case LayerMaskType::Ellipse: return QStringLiteral("ellipse");
    case LayerMaskType::None: return QStringLiteral("none");
    }
    return QStringLiteral("none");
}

LayerMaskType typeFromKey(const QString &key)
{
    if (key == QStringLiteral("aiSubject")) return LayerMaskType::AiSubject;
    if (key == QStringLiteral("aiBackground")) return LayerMaskType::AiBackground;
    if (key == QStringLiteral("aiSky")) return LayerMaskType::AiSky;
    if (key == QStringLiteral("colorRange")) return LayerMaskType::ColorRange;
    if (key == QStringLiteral("luminanceRange")) return LayerMaskType::LuminanceRange;
    if (key == QStringLiteral("rectangle")) return LayerMaskType::Rectangle;
    if (key == QStringLiteral("ellipse")) return LayerMaskType::Ellipse;
    return LayerMaskType::None;
}

bool closeEnough(double lhs, double rhs)
{
    return std::abs(lhs - rhs) < epsilon;
}

} // namespace

bool operator==(const LayerMask &lhs, const LayerMask &rhs)
{
    return lhs.type == rhs.type
        && lhs.targetColor.rgba() == rhs.targetColor.rgba()
        && closeEnough(lhs.colorTolerance, rhs.colorTolerance)
        && closeEnough(lhs.luminanceMinimum, rhs.luminanceMinimum)
        && closeEnough(lhs.luminanceMaximum, rhs.luminanceMaximum)
        && closeEnough(lhs.bounds.x(), rhs.bounds.x())
        && closeEnough(lhs.bounds.y(), rhs.bounds.y())
        && closeEnough(lhs.bounds.width(), rhs.bounds.width())
        && closeEnough(lhs.bounds.height(), rhs.bounds.height())
        && closeEnough(lhs.feather, rhs.feather);
}

QJsonObject LayerMask::toJson() const
{
    return {
        {QStringLiteral("type"), typeKey(type)},
        {QStringLiteral("targetColor"), targetColor.name(QColor::HexArgb)},
        {QStringLiteral("colorTolerance"), colorTolerance},
        {QStringLiteral("luminanceMinimum"), luminanceMinimum},
        {QStringLiteral("luminanceMaximum"), luminanceMaximum},
        {QStringLiteral("x"), bounds.x()},
        {QStringLiteral("y"), bounds.y()},
        {QStringLiteral("width"), bounds.width()},
        {QStringLiteral("height"), bounds.height()},
        {QStringLiteral("feather"), feather}
    };
}

LayerMask LayerMask::fromJson(const QJsonObject &json)
{
    LayerMask mask;
    mask.type = typeFromKey(json.value(QStringLiteral("type")).toString());
    const QColor requestedColor(json.value(QStringLiteral("targetColor")).toString());
    if (requestedColor.isValid())
        mask.targetColor = requestedColor;
    mask.colorTolerance = qBound(0.01,
        json.value(QStringLiteral("colorTolerance")).toDouble(0.18), 1.0);
    mask.luminanceMinimum = qBound(0.0,
        json.value(QStringLiteral("luminanceMinimum")).toDouble(0.25), 1.0);
    mask.luminanceMaximum = qBound(mask.luminanceMinimum,
        json.value(QStringLiteral("luminanceMaximum")).toDouble(0.75), 1.0);
    const QRectF requestedBounds(
        qBound(0.0, json.value(QStringLiteral("x")).toDouble(0.2), 1.0),
        qBound(0.0, json.value(QStringLiteral("y")).toDouble(0.2), 1.0),
        qBound(0.001, json.value(QStringLiteral("width")).toDouble(0.6), 1.0),
        qBound(0.001, json.value(QStringLiteral("height")).toDouble(0.6), 1.0));
    mask.bounds = requestedBounds.intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (mask.bounds.isEmpty())
        mask.bounds = QRectF(0.2, 0.2, 0.6, 0.6);
    mask.feather = qBound(0.0,
        json.value(QStringLiteral("feather")).toDouble(0.08), 0.5);
    return mask;
}

QString layerMaskTypeName(LayerMaskType type)
{
    switch (type) {
    case LayerMaskType::AiSubject: return QStringLiteral("AI Subject");
    case LayerMaskType::AiBackground: return QStringLiteral("AI Background");
    case LayerMaskType::AiSky: return QStringLiteral("AI Sky");
    case LayerMaskType::ColorRange: return QStringLiteral("Color Range");
    case LayerMaskType::LuminanceRange: return QStringLiteral("Luminance Range");
    case LayerMaskType::Rectangle: return QStringLiteral("Rectangle");
    case LayerMaskType::Ellipse: return QStringLiteral("Ellipse");
    case LayerMaskType::None: return QStringLiteral("Entire Image");
    }
    return QStringLiteral("Entire Image");
}

} // namespace chroma
