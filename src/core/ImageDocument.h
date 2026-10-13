#pragma once

#include "core/LayerStack.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUndoStack>

namespace chroma {

class ImageDocument final : public QObject {
    Q_OBJECT

public:
    explicit ImageDocument(QObject *parent = nullptr);

    bool open(const QString &path, const LayerStack &initialLayers = {},
              QString *error = nullptr);
    void close();
    void setLayerStack(const LayerStack &layers, const QString &description,
                       int mergeId = -1);
    void replaceLayerStack(const LayerStack &layers);
    void setActiveLayer(const QString &layerId);
    void requestPreview(int maxEdge = 2200);

    [[nodiscard]] bool isOpen() const { return !m_original.isNull(); }
    [[nodiscard]] const QString &path() const { return m_path; }
    [[nodiscard]] const QImage &original() const { return m_original; }
    [[nodiscard]] const QImage &preview() const { return m_preview; }
    [[nodiscard]] const LayerStack &layerStack() const { return m_layers; }
    [[nodiscard]] const QString &activeLayerId() const { return m_activeLayerId; }
    [[nodiscard]] const AdjustmentLayer *activeLayer() const
    {
        return m_layers.find(m_activeLayerId);
    }
    [[nodiscard]] QUndoStack *undoStack() { return &m_undoStack; }

signals:
    void opened();
    void closed();
    void layerStackChanged(const chroma::LayerStack &layers);
    void activeLayerChanged(const QString &layerId);
    void previewStarted();
    void previewReady(const QImage &preview);
    void errorOccurred(const QString &message);

private:
    friend class LayerStackCommand;
    void applyLayersFromCommand(const LayerStack &layers);
    void renderPreviewNow();

    QString m_path;
    QImage m_original;
    QImage m_preview;
    LayerStack m_layers;
    QString m_activeLayerId;
    QUndoStack m_undoStack;
    QTimer m_previewTimer;
    int m_pendingMaxEdge = 2200;
    quint64 m_renderGeneration = 0;
};

} // namespace chroma
