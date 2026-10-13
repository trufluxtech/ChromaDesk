#pragma once

#include <QImage>
#include <QList>
#include <QPoint>
#include <QRectF>
#include <QWidget>

class QPainter;

namespace chroma {

class ImageCanvas final : public QWidget {
    Q_OBJECT

public:
    enum class CompareMode { Edited, Original, SideBySide, Split };
    Q_ENUM(CompareMode)
    enum class ShapeMaskMode { None, Rectangle, Ellipse, GenerativeFill };
    Q_ENUM(ShapeMaskMode)
    enum class RetouchMode { None, DustHeal, CloneStamp };
    Q_ENUM(RetouchMode)
    enum class GridMode { None, Thirds, Quarters, GoldenRatio };
    Q_ENUM(GridMode)

    explicit ImageCanvas(QWidget *parent = nullptr);
    void setImages(const QImage &original, const QImage &edited);
    void setCompareMode(CompareMode mode);
    [[nodiscard]] CompareMode compareMode() const { return m_mode; }
    void setCropMode(bool enabled);
    void setCropAspectRatio(double widthToHeight);
    void setShapeMaskMode(ShapeMaskMode mode);
    void setRetouchMode(RetouchMode mode);
    void setGridMode(GridMode mode);
    void setRulersVisible(bool visible);
    void setActiveLayerOverlay(const QImage &mask, bool visible);
    void setActiveLayerOverlayVisible(bool visible);
    void setMovableWatermark(const QPointF &position,
                             const QSizeF &normalizedSize, bool visible);
    void clearGuides();
    void fitToWindow();
    void actualPixels();
    void zoomIn();
    void zoomOut();
    [[nodiscard]] const QList<double> &horizontalGuides() const
    {
        return m_horizontalGuides;
    }
    [[nodiscard]] const QList<double> &verticalGuides() const
    {
        return m_verticalGuides;
    }

signals:
    void cropRequested(const QRectF &normalizedCrop);
    void shapeMaskRequested(const QRectF &normalizedBounds,
                            ShapeMaskMode mode);
    void zoomChanged(double percent);
    void watermarkMoveRequested(const QPointF &normalizedPosition);
    void dustRemovalRequested(const QPointF &normalizedTarget);
    void cloneSourceSelected(const QPointF &normalizedSource);
    void cloneRequested(const QPointF &normalizedSource,
                        const QPointF &normalizedTarget);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    [[nodiscard]] QRectF fittedRect(const QSize &imageSize, const QRectF &area) const;
    [[nodiscard]] QRectF activeImageRect() const;
    [[nodiscard]] QPointF constrainedCropPoint(const QPointF &point) const;
    void drawImage(QPainter &painter, const QImage &image, const QRectF &target);
    void drawGuides(QPainter &painter, const QRectF &target);
    void drawActiveLayerOverlay(QPainter &painter, const QRectF &target);
    void drawMovableWatermark(QPainter &painter, const QRectF &target);
    [[nodiscard]] QRectF watermarkRect(const QRectF &target) const;
    void changeZoom(double factor);

    enum class GuideDragMode { None, Horizontal, Vertical };

    QImage m_original;
    QImage m_edited;
    CompareMode m_mode = CompareMode::Edited;
    double m_zoom = 0.0; // 0 means fit
    QPointF m_pan;
    bool m_panning = false;
    QPoint m_lastMouse;
    bool m_cropMode = false;
    double m_cropAspectRatio = 0.0;
    bool m_selectingCrop = false;
    QPointF m_cropStart;
    QRectF m_cropSelection;
    ShapeMaskMode m_shapeMaskMode = ShapeMaskMode::None;
    bool m_selectingShapeMask = false;
    QPointF m_shapeMaskStart;
    QRectF m_shapeMaskSelection;
    RetouchMode m_retouchMode = RetouchMode::None;
    QPointF m_cloneSource;
    bool m_hasCloneSource = false;
    GridMode m_gridMode = GridMode::None;
    bool m_rulersVisible = false;
    QList<double> m_horizontalGuides;
    QList<double> m_verticalGuides;
    GuideDragMode m_guideDragMode = GuideDragMode::None;
    double m_pendingGuide = 0.0;
    QImage m_activeLayerTint;
    QImage m_activeLayerBoundary;
    bool m_activeLayerOverlayVisible = false;
    QPointF m_watermarkPosition = QPointF(0.5, 0.5);
    QSizeF m_watermarkSize;
    bool m_watermarkVisible = false;
    bool m_movingWatermark = false;
    QPointF m_watermarkDragOffset;
};

} // namespace chroma
