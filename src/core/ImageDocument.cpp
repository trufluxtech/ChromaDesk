#include "core/ImageDocument.h"

#include "core/ImageLoader.h"
#include "core/ImageProcessor.h"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QUndoCommand>

#include <utility>

namespace chroma {

class LayerStackCommand final : public QUndoCommand {
public:
    LayerStackCommand(ImageDocument *document, LayerStack before, LayerStack after,
                      const QString &description, int mergeId)
        : QUndoCommand(description), m_document(document), m_before(std::move(before)),
          m_after(std::move(after)), m_mergeId(mergeId)
    {
    }

    void undo() override { m_document->applyLayersFromCommand(m_before); }
    void redo() override { m_document->applyLayersFromCommand(m_after); }
    int id() const override { return m_mergeId; }

    bool mergeWith(const QUndoCommand *other) override
    {
        if (m_mergeId < 0 || other->id() != m_mergeId)
            return false;
        const auto *command = dynamic_cast<const LayerStackCommand *>(other);
        if (!command || command->m_document != m_document)
            return false;
        m_after = command->m_after;
        return true;
    }

private:
    ImageDocument *m_document;
    LayerStack m_before;
    LayerStack m_after;
    int m_mergeId;
};

ImageDocument::ImageDocument(QObject *parent)
    : QObject(parent)
{
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(45);
    connect(&m_previewTimer, &QTimer::timeout, this, &ImageDocument::renderPreviewNow);
}

bool ImageDocument::open(const QString &path, const LayerStack &initialLayers, QString *error)
{
    const ImageLoadResult result = ImageLoader::load(path);
    if (!result) {
        if (error) *error = result.error;
        return false;
    }

    ++m_renderGeneration;
    m_path = path;
    m_original = result.image;
    m_layers = initialLayers;
    m_activeLayerId = m_layers.baseLayer()->id;
    m_undoStack.clear();
    m_undoStack.setClean();
    emit opened();
    emit layerStackChanged(m_layers);
    emit activeLayerChanged(m_activeLayerId);
    requestPreview();
    return true;
}

void ImageDocument::close()
{
    m_previewTimer.stop();
    ++m_renderGeneration;
    m_path.clear();
    m_original = {};
    m_preview = {};
    m_layers = {};
    m_activeLayerId.clear();
    m_undoStack.clear();
    emit closed();
}

void ImageDocument::setLayerStack(const LayerStack &layers, const QString &description,
                                  int mergeId)
{
    if (!isOpen() || layers == m_layers)
        return;
    m_undoStack.push(new LayerStackCommand(this, m_layers, layers, description, mergeId));
}

void ImageDocument::replaceLayerStack(const LayerStack &layers)
{
    if (!isOpen())
        return;
    m_layers = layers;
    if (!m_layers.find(m_activeLayerId))
        m_activeLayerId = m_layers.baseLayer()->id;
    emit layerStackChanged(m_layers);
    emit activeLayerChanged(m_activeLayerId);
    requestPreview();
}

void ImageDocument::setActiveLayer(const QString &layerId)
{
    if (!isOpen() || layerId == m_activeLayerId || !m_layers.find(layerId))
        return;
    m_activeLayerId = layerId;
    emit activeLayerChanged(m_activeLayerId);
}

void ImageDocument::applyLayersFromCommand(const LayerStack &layers)
{
    m_layers = layers;
    if (!m_layers.find(m_activeLayerId))
        m_activeLayerId = m_layers.baseLayer()->id;
    emit layerStackChanged(m_layers);
    emit activeLayerChanged(m_activeLayerId);
    requestPreview();
}

void ImageDocument::requestPreview(int maxEdge)
{
    if (!isOpen())
        return;
    ++m_renderGeneration; // Invalidates a result already in flight.
    m_pendingMaxEdge = maxEdge;
    m_previewTimer.start();
}

void ImageDocument::renderPreviewNow()
{
    if (!isOpen())
        return;
    const quint64 generation = m_renderGeneration;
    const QImage source = m_original;
    const LayerStack layers = m_layers;
    const int maxEdge = m_pendingMaxEdge;
    emit previewStarted();

    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this,
            [this, watcher, generation] {
        const QImage rendered = watcher->result();
        watcher->deleteLater();
        if (generation != m_renderGeneration)
            return;
        if (rendered.isNull()) {
            emit errorOccurred(QStringLiteral("The preview renderer returned no image."));
            return;
        }
        m_preview = rendered;
        emit previewReady(m_preview);
    });
    watcher->setFuture(QtConcurrent::run([source, layers, maxEdge] {
        return ImageProcessor::renderLayers(source, layers, maxEdge);
    }));
}

} // namespace chroma
