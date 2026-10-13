#pragma once

#include "core/EditState.h"
#include "core/LayerMask.h"
#include "core/RetouchOperation.h"
#include "core/TextOverlay.h"
#include "core/WatermarkOverlay.h"

#include <QJsonArray>
#include <QList>
#include <QString>

namespace chroma {

struct AdjustmentLayer {
    QString id;
    QString name;
    bool visible = true;
    double opacity = 1.0; // 0.0..1.0
    EditState edits;
    LayerMask mask;
    RetouchOperation retouchOperation;
    TextOverlay textOverlay;
    WatermarkOverlay watermarkOverlay;

    [[nodiscard]] QJsonObject toJson() const;
    static AdjustmentLayer fromJson(const QJsonObject &json, int position);

    friend bool operator==(const AdjustmentLayer &lhs, const AdjustmentLayer &rhs);
};

class LayerStack final {
public:
    LayerStack();

    [[nodiscard]] const QList<AdjustmentLayer> &layers() const { return m_layers; }
    [[nodiscard]] QList<AdjustmentLayer> &layers() { return m_layers; }
    [[nodiscard]] int size() const { return m_layers.size(); }
    [[nodiscard]] bool isEmpty() const { return m_layers.isEmpty(); }
    [[nodiscard]] const AdjustmentLayer *baseLayer() const;
    [[nodiscard]] AdjustmentLayer *baseLayer();
    [[nodiscard]] const AdjustmentLayer *find(const QString &id) const;
    [[nodiscard]] AdjustmentLayer *find(const QString &id);
    [[nodiscard]] int indexOf(const QString &id) const;

    QString addLayer(const QString &name = {});
    QString duplicateLayer(const QString &id);
    bool removeLayer(const QString &id);
    bool moveLayer(const QString &id, int offset);

    [[nodiscard]] QJsonArray toJson() const;
    static LayerStack fromJson(const QJsonArray &json);
    static LayerStack fromLegacyEdits(const EditState &edits);

    friend bool operator==(const LayerStack &lhs, const LayerStack &rhs);
    friend bool operator!=(const LayerStack &lhs, const LayerStack &rhs)
    {
        return !(lhs == rhs);
    }

private:
    static AdjustmentLayer makeLayer(const QString &name);
    void ensureBaseLayer();

    QList<AdjustmentLayer> m_layers;
};

} // namespace chroma

Q_DECLARE_METATYPE(chroma::AdjustmentLayer)
Q_DECLARE_METATYPE(chroma::LayerStack)
