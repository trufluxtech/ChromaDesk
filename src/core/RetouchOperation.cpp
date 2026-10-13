#include "core/RetouchOperation.h"

#include <QtGlobal>

#include <cmath>

namespace chroma {
namespace {

QString typeKey(RetouchType type)
{
    switch (type) {
    case RetouchType::HealSpot: return QStringLiteral("healSpot");
    case RetouchType::CloneStamp: return QStringLiteral("cloneStamp");
    case RetouchType::GeneratedImage: return QStringLiteral("generatedImage");
    case RetouchType::None: return QStringLiteral("none");
    }
    return QStringLiteral("none");
}

RetouchType typeFromKey(const QString &key)
{
    if (key == QStringLiteral("healSpot"))
        return RetouchType::HealSpot;
    if (key == QStringLiteral("cloneStamp"))
        return RetouchType::CloneStamp;
    if (key == QStringLiteral("generatedImage"))
        return RetouchType::GeneratedImage;
    return RetouchType::None;
}

QJsonObject pointToJson(const QPointF &point)
{
    return {{QStringLiteral("x"), point.x()},
            {QStringLiteral("y"), point.y()}};
}

QPointF pointFromJson(const QJsonObject &json, const QPointF &fallback)
{
    return QPointF(qBound(0.0, json.value(QStringLiteral("x")).toDouble(fallback.x()), 1.0),
                   qBound(0.0, json.value(QStringLiteral("y")).toDouble(fallback.y()), 1.0));
}

} // namespace

bool RetouchOperation::isEnabled() const
{
    if (type == RetouchType::GeneratedImage)
        return !generatedImagePath.trimmed().isEmpty();
    return type == RetouchType::HealSpot || type == RetouchType::CloneStamp;
}

QJsonObject RetouchOperation::toJson() const
{
    return {
        {QStringLiteral("type"), typeKey(type)},
        {QStringLiteral("source"), pointToJson(source)},
        {QStringLiteral("target"), pointToJson(target)},
        {QStringLiteral("radius"), radius},
        {QStringLiteral("generatedImagePath"), generatedImagePath}
    };
}

RetouchOperation RetouchOperation::fromJson(const QJsonObject &json)
{
    RetouchOperation operation;
    operation.type = typeFromKey(json.value(QStringLiteral("type")).toString());
    operation.source = pointFromJson(json.value(QStringLiteral("source")).toObject(),
                                      operation.source);
    operation.target = pointFromJson(json.value(QStringLiteral("target")).toObject(),
                                      operation.target);
    operation.radius = qBound(0.002,
        json.value(QStringLiteral("radius")).toDouble(operation.radius), 0.35);
    operation.generatedImagePath = json.value(
        QStringLiteral("generatedImagePath")).toString();
    return operation;
}

bool operator==(const RetouchOperation &lhs, const RetouchOperation &rhs)
{
    return lhs.type == rhs.type
        && std::abs(lhs.source.x() - rhs.source.x()) < 0.0001
        && std::abs(lhs.source.y() - rhs.source.y()) < 0.0001
        && std::abs(lhs.target.x() - rhs.target.x()) < 0.0001
        && std::abs(lhs.target.y() - rhs.target.y()) < 0.0001
        && std::abs(lhs.radius - rhs.radius) < 0.0001
        && lhs.generatedImagePath == rhs.generatedImagePath;
}

QString retouchTypeName(RetouchType type)
{
    switch (type) {
    case RetouchType::HealSpot: return QStringLiteral("Dust / Spot Heal");
    case RetouchType::CloneStamp: return QStringLiteral("Clone Stamp");
    case RetouchType::GeneratedImage: return QStringLiteral("Generative Fill");
    case RetouchType::None: return QStringLiteral("No retouch operation");
    }
    return QStringLiteral("No retouch operation");
}

} // namespace chroma

