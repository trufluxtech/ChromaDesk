#include "core/LayerStack.h"

#include <QJsonObject>
#include <QUuid>
#include <QtGlobal>

#include <cmath>

namespace chroma {
namespace {

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace

bool operator==(const AdjustmentLayer &lhs, const AdjustmentLayer &rhs)
{
    return lhs.id == rhs.id
        && lhs.name == rhs.name
        && lhs.visible == rhs.visible
        && std::abs(lhs.opacity - rhs.opacity) < 0.0001
        && lhs.edits == rhs.edits
        && lhs.mask == rhs.mask
        && lhs.retouchOperation == rhs.retouchOperation
        && lhs.textOverlay == rhs.textOverlay
        && lhs.watermarkOverlay == rhs.watermarkOverlay;
}

QJsonObject AdjustmentLayer::toJson() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("visible"), visible},
        {QStringLiteral("opacity"), opacity},
        {QStringLiteral("edits"), edits.toJson()},
        {QStringLiteral("mask"), mask.toJson()},
        {QStringLiteral("retouchOperation"), retouchOperation.toJson()},
        {QStringLiteral("textOverlay"), textOverlay.toJson()},
        {QStringLiteral("watermarkOverlay"), watermarkOverlay.toJson()}
    };
}

AdjustmentLayer AdjustmentLayer::fromJson(const QJsonObject &json, int position)
{
    AdjustmentLayer layer;
    layer.id = json.value(QStringLiteral("id")).toString();
    if (layer.id.isEmpty())
        layer.id = newId();
    layer.name = json.value(QStringLiteral("name")).toString();
    if (layer.name.trimmed().isEmpty()) {
        layer.name = position == 0
            ? QStringLiteral("Base Adjustments")
            : QStringLiteral("Adjustment %1").arg(position);
    }
    layer.visible = json.value(QStringLiteral("visible")).toBool(true);
    layer.opacity = qBound(0.0,
        json.value(QStringLiteral("opacity")).toDouble(1.0), 1.0);
    layer.edits = EditState::fromJson(json.value(QStringLiteral("edits")).toObject());
    layer.mask = LayerMask::fromJson(json.value(QStringLiteral("mask")).toObject());
    layer.retouchOperation = RetouchOperation::fromJson(
        json.value(QStringLiteral("retouchOperation")).toObject());
    layer.textOverlay = TextOverlay::fromJson(
        json.value(QStringLiteral("textOverlay")).toObject());
    layer.watermarkOverlay = WatermarkOverlay::fromJson(
        json.value(QStringLiteral("watermarkOverlay")).toObject());
    return layer;
}

AdjustmentLayer LayerStack::makeLayer(const QString &name)
{
    return {
        newId(),
        name.trimmed().isEmpty() ? QStringLiteral("Adjustment") : name.trimmed(),
        true,
        1.0,
        {},
        {},
        {},
        {},
        {}
    };
}

LayerStack::LayerStack()
{
    m_layers.append(makeLayer(QStringLiteral("Base Adjustments")));
}

void LayerStack::ensureBaseLayer()
{
    if (m_layers.isEmpty())
        m_layers.append(makeLayer(QStringLiteral("Base Adjustments")));
    if (m_layers.first().name.trimmed().isEmpty())
        m_layers.first().name = QStringLiteral("Base Adjustments");
    m_layers.first().mask = {};
    m_layers.first().retouchOperation = {};
    m_layers.first().textOverlay = {};
    m_layers.first().watermarkOverlay = {};
}

const AdjustmentLayer *LayerStack::baseLayer() const
{
    return m_layers.isEmpty() ? nullptr : &m_layers.first();
}

AdjustmentLayer *LayerStack::baseLayer()
{
    ensureBaseLayer();
    return &m_layers.first();
}

const AdjustmentLayer *LayerStack::find(const QString &id) const
{
    const int index = indexOf(id);
    return index < 0 ? nullptr : &m_layers.at(index);
}

AdjustmentLayer *LayerStack::find(const QString &id)
{
    const int index = indexOf(id);
    return index < 0 ? nullptr : &m_layers[index];
}

int LayerStack::indexOf(const QString &id) const
{
    for (int index = 0; index < m_layers.size(); ++index) {
        if (m_layers.at(index).id == id)
            return index;
    }
    return -1;
}

QString LayerStack::addLayer(const QString &name)
{
    const QString resolvedName = name.trimmed().isEmpty()
        ? QStringLiteral("Adjustment %1").arg(m_layers.size())
        : name.trimmed();
    m_layers.append(makeLayer(resolvedName));
    return m_layers.last().id;
}

QString LayerStack::duplicateLayer(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0)
        return {};
    AdjustmentLayer copy = m_layers.at(index);
    copy.id = newId();
    copy.name += QStringLiteral(" Copy");
    m_layers.insert(index + 1, copy);
    return copy.id;
}

bool LayerStack::removeLayer(const QString &id)
{
    const int index = indexOf(id);
    if (index <= 0)
        return false; // The base layer is permanent.
    m_layers.removeAt(index);
    return true;
}

bool LayerStack::moveLayer(const QString &id, int offset)
{
    const int from = indexOf(id);
    if (from <= 0 || offset == 0)
        return false;
    const int to = qBound(1, from + offset, m_layers.size() - 1);
    if (from == to)
        return false;
    m_layers.move(from, to);
    return true;
}

QJsonArray LayerStack::toJson() const
{
    QJsonArray json;
    for (const auto &layer : m_layers)
        json.append(layer.toJson());
    return json;
}

LayerStack LayerStack::fromJson(const QJsonArray &json)
{
    LayerStack result;
    result.m_layers.clear();
    int position = 0;
    for (const QJsonValue &value : json) {
        if (value.isObject())
            result.m_layers.append(AdjustmentLayer::fromJson(value.toObject(), position++));
    }
    result.ensureBaseLayer();
    return result;
}

LayerStack LayerStack::fromLegacyEdits(const EditState &edits)
{
    LayerStack result;
    result.m_layers.first().edits = edits;
    return result;
}

bool operator==(const LayerStack &lhs, const LayerStack &rhs)
{
    return lhs.m_layers == rhs.m_layers;
}

} // namespace chroma
