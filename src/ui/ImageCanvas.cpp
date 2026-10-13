#include "ui/ImageCanvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QList>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace chroma {

ImageCanvas::ImageCanvas(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(420, 320);
    setMouseTracking(true);
}

void ImageCanvas::setImages(const QImage &original, const QImage &edited)
{
    m_original = original;
    m_edited = edited;
    m_cropSelection = {};
    if (!m_selectingShapeMask)
        m_shapeMaskSelection = {};
    update();
}

void ImageCanvas::setCompareMode(CompareMode mode)
{
    m_mode = mode;
    update();
}

void ImageCanvas::setCropMode(bool enabled)
{
    m_cropMode = enabled;
    if (enabled) {
        m_shapeMaskMode = ShapeMaskMode::None;
        m_retouchMode = RetouchMode::None;
        m_hasCloneSource = false;
    }
    m_cropSelection = {};
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void ImageCanvas::setCropAspectRatio(double widthToHeight)
{
    m_cropAspectRatio = qMax(0.0, widthToHeight);
    m_cropSelection = {};
    update();
}

void ImageCanvas::setShapeMaskMode(ShapeMaskMode mode)
{
    m_shapeMaskMode = mode;
    if (mode != ShapeMaskMode::None) {
        m_cropMode = false;
        m_retouchMode = RetouchMode::None;
        m_hasCloneSource = false;
    }
    m_shapeMaskSelection = {};
    setCursor(mode == ShapeMaskMode::None ? Qt::ArrowCursor : Qt::CrossCursor);
    update();
}

void ImageCanvas::setRetouchMode(RetouchMode mode)
{
    m_retouchMode = mode;
    m_hasCloneSource = false;
    if (mode != RetouchMode::None) {
        m_cropMode = false;
        m_shapeMaskMode = ShapeMaskMode::None;
        m_cropSelection = {};
        m_shapeMaskSelection = {};
    }
    setCursor(mode == RetouchMode::None ? Qt::ArrowCursor : Qt::CrossCursor);
    update();
}

void ImageCanvas::setGridMode(GridMode mode)
{
    m_gridMode = mode;
    update();
}

void ImageCanvas::setRulersVisible(bool visible)
{
    m_rulersVisible = visible;
    update();
}

void ImageCanvas::setActiveLayerOverlay(const QImage &mask, bool visible)
{
    m_activeLayerOverlayVisible = visible;
    m_activeLayerTint = {};
    m_activeLayerBoundary = {};
    if (!mask.isNull()) {
        const QImage coverage = mask.convertToFormat(QImage::Format_Grayscale8);
        m_activeLayerTint = QImage(coverage.size(), QImage::Format_ARGB32);
        m_activeLayerBoundary = QImage(coverage.size(), QImage::Format_ARGB32);
        m_activeLayerTint.fill(Qt::transparent);
        m_activeLayerBoundary.fill(Qt::transparent);
        for (int y = 0; y < coverage.height(); ++y) {
            const uchar *source = coverage.constScanLine(y);
            auto *tint = reinterpret_cast<QRgb *>(m_activeLayerTint.scanLine(y));
            auto *boundary = reinterpret_cast<QRgb *>(m_activeLayerBoundary.scanLine(y));
            for (int x = 0; x < coverage.width(); ++x) {
                const int amount = source[x];
                tint[x] = qRgba(76, 205, 240, qRound(amount * 0.14));
                if (amount < 128)
                    continue;
                const bool edge = x == 0 || y == 0
                    || x == coverage.width() - 1 || y == coverage.height() - 1
                    || source[x - 1] < 128 || source[x + 1] < 128
                    || coverage.constScanLine(y - 1)[x] < 128
                    || coverage.constScanLine(y + 1)[x] < 128;
                if (edge && ((x / 5 + y / 5) % 2 == 0))
                    boundary[x] = qRgba(110, 231, 249, 235);
            }
        }
    }
    update();
}

void ImageCanvas::setActiveLayerOverlayVisible(bool visible)
{
    m_activeLayerOverlayVisible = visible;
    update();
}

void ImageCanvas::setMovableWatermark(const QPointF &position,
                                      const QSizeF &normalizedSize, bool visible)
{
    m_watermarkPosition = position;
    m_watermarkSize = normalizedSize;
    m_watermarkVisible = visible && normalizedSize.isValid()
        && normalizedSize.width() > 0.0 && normalizedSize.height() > 0.0;
    if (!m_watermarkVisible)
        m_movingWatermark = false;
    update();
}

void ImageCanvas::clearGuides()
{
    m_horizontalGuides.clear();
    m_verticalGuides.clear();
    m_guideDragMode = GuideDragMode::None;
    update();
}

void ImageCanvas::fitToWindow()
{
    m_zoom = 0.0;
    m_pan = {};
    emit zoomChanged(0.0);
    update();
}

void ImageCanvas::actualPixels()
{
    m_zoom = 1.0;
    m_pan = {};
    emit zoomChanged(100.0);
    update();
}

void ImageCanvas::changeZoom(double factor)
{
    const QImage &image = m_edited.isNull() ? m_original : m_edited;
    if (image.isNull())
        return;
    if (m_zoom <= 0.0)
        m_zoom = activeImageRect().width() / qMax(1, image.width());
    m_zoom = std::clamp(m_zoom * factor, 0.05, 8.0);
    emit zoomChanged(m_zoom * 100.0);
    update();
}

void ImageCanvas::zoomIn()
{
    changeZoom(1.25);
}

void ImageCanvas::zoomOut()
{
    changeZoom(0.8);
}

QRectF ImageCanvas::fittedRect(const QSize &imageSize, const QRectF &area) const
{
    if (!imageSize.isValid())
        return {};
    QSizeF target(imageSize);
    if (m_zoom <= 0.0) {
        target.scale(area.size() - QSizeF(28.0, 28.0), Qt::KeepAspectRatio);
    } else {
        target *= m_zoom;
    }
    const QPointF center = area.center() + m_pan;
    return QRectF(center.x() - target.width() / 2.0,
                  center.y() - target.height() / 2.0,
                  target.width(), target.height());
}

QRectF ImageCanvas::activeImageRect() const
{
    return fittedRect(m_edited.isNull() ? m_original.size() : m_edited.size(), rect());
}

QPointF ImageCanvas::constrainedCropPoint(const QPointF &point) const
{
    const QRectF imageRect = activeImageRect();
    const QPointF bounded(qBound(imageRect.left(), point.x(), imageRect.right()),
                          qBound(imageRect.top(), point.y(), imageRect.bottom()));
    if (m_cropAspectRatio <= 0.0)
        return bounded;

    const double deltaX = bounded.x() - m_cropStart.x();
    const double deltaY = bounded.y() - m_cropStart.y();
    const double directionX = deltaX < 0.0 ? -1.0 : 1.0;
    const double directionY = deltaY < 0.0 ? -1.0 : 1.0;
    double selectionWidth = std::abs(deltaX);
    double selectionHeight = std::abs(deltaY);
    if (selectionWidth < 0.5 && selectionHeight < 0.5)
        return m_cropStart;
    if (selectionHeight < 0.5 || selectionWidth / selectionHeight > m_cropAspectRatio)
        selectionHeight = selectionWidth / m_cropAspectRatio;
    else
        selectionWidth = selectionHeight * m_cropAspectRatio;

    const double maximumWidth = directionX > 0.0
        ? imageRect.right() - m_cropStart.x() : m_cropStart.x() - imageRect.left();
    const double maximumHeight = directionY > 0.0
        ? imageRect.bottom() - m_cropStart.y() : m_cropStart.y() - imageRect.top();
    const double scale = std::min({1.0,
        maximumWidth / qMax(0.0001, selectionWidth),
        maximumHeight / qMax(0.0001, selectionHeight)});
    return QPointF(m_cropStart.x() + directionX * selectionWidth * scale,
                   m_cropStart.y() + directionY * selectionHeight * scale);
}

void ImageCanvas::drawImage(QPainter &painter, const QImage &image, const QRectF &target)
{
    if (!image.isNull())
        painter.drawImage(target, image);
}

void ImageCanvas::drawGuides(QPainter &painter, const QRectF &target)
{
    if (target.isEmpty() || (m_gridMode == GridMode::None && !m_rulersVisible
        && m_horizontalGuides.isEmpty() && m_verticalGuides.isEmpty()
        && m_guideDragMode == GuideDragMode::None))
        return;
    painter.save();
    painter.setClipRect(target);

    if (m_gridMode != GridMode::None) {
        painter.setPen(QPen(QColor(255, 255, 255, 145), 1));
        QList<double> fractions;
        if (m_gridMode == GridMode::Thirds)
            fractions = {1.0 / 3.0, 2.0 / 3.0};
        else if (m_gridMode == GridMode::Quarters)
            fractions = {0.25, 0.5, 0.75};
        else
            fractions = {0.382, 0.618};
        for (const double fraction : fractions) {
            painter.drawLine(QPointF(target.left() + target.width() * fraction, target.top()),
                             QPointF(target.left() + target.width() * fraction, target.bottom()));
            painter.drawLine(QPointF(target.left(), target.top() + target.height() * fraction),
                             QPointF(target.right(), target.top() + target.height() * fraction));
        }
    }

    if (m_rulersVisible) {
        const double topHeight = qMin(22.0, target.height() * 0.12);
        const double leftWidth = qMin(30.0, target.width() * 0.12);
        painter.fillRect(QRectF(target.left(), target.top(), target.width(), topHeight),
                         QColor(8, 10, 15, 185));
        painter.fillRect(QRectF(target.left(), target.top(), leftWidth, target.height()),
                         QColor(8, 10, 15, 185));
        QFont rulerFont = painter.font();
        rulerFont.setPixelSize(9);
        painter.setFont(rulerFont);
        painter.setPen(QColor(255, 255, 255, 190));
        for (int step = 0; step <= 10; ++step) {
            const double fraction = static_cast<double>(step) / 10.0;
            const double x = target.left() + target.width() * fraction;
            const double y = target.top() + target.height() * fraction;
            const double tick = step % 5 == 0 ? 9.0 : step % 2 == 0 ? 6.0 : 4.0;
            painter.drawLine(QPointF(x, target.top()), QPointF(x, target.top() + tick));
            painter.drawLine(QPointF(target.left(), y), QPointF(target.left() + tick, y));
            if (step % 2 == 0 && step < 10) {
                painter.drawText(QRectF(x + 2.0, target.top() + 7.0, 34.0, 12.0),
                                 QStringLiteral("%1%").arg(step * 10));
                painter.drawText(QRectF(target.left() + 8.0, y + 1.0, 24.0, 12.0),
                                 QString::number(step * 10));
            }
        }
    }

    painter.setPen(QPen(QColor(80, 220, 255, 220), 1));
    for (const double position : m_horizontalGuides) {
        const double y = target.top() + target.height() * position;
        painter.drawLine(QPointF(target.left(), y), QPointF(target.right(), y));
    }
    for (const double position : m_verticalGuides) {
        const double x = target.left() + target.width() * position;
        painter.drawLine(QPointF(x, target.top()), QPointF(x, target.bottom()));
    }
    painter.setPen(QPen(QColor(255, 119, 189, 235), 1, Qt::DashLine));
    if (m_guideDragMode == GuideDragMode::Horizontal) {
        const double y = target.top() + target.height() * m_pendingGuide;
        painter.drawLine(QPointF(target.left(), y), QPointF(target.right(), y));
    } else if (m_guideDragMode == GuideDragMode::Vertical) {
        const double x = target.left() + target.width() * m_pendingGuide;
        painter.drawLine(QPointF(x, target.top()), QPointF(x, target.bottom()));
    }
    painter.restore();
}

void ImageCanvas::drawActiveLayerOverlay(QPainter &painter, const QRectF &target)
{
    if (!m_activeLayerOverlayVisible || target.isEmpty())
        return;
    painter.save();
    painter.setClipRect(target);
    if (m_activeLayerTint.isNull()) {
        painter.fillRect(target, QColor(76, 205, 240, 18));
        painter.setPen(QPen(QColor(110, 231, 249, 225), 1, Qt::DashLine));
        painter.drawRect(target.adjusted(0.5, 0.5, -0.5, -0.5));
    } else {
        painter.drawImage(target, m_activeLayerTint);
        painter.drawImage(target, m_activeLayerBoundary);
    }
    painter.restore();
}

QRectF ImageCanvas::watermarkRect(const QRectF &target) const
{
    const QSizeF size(target.width() * m_watermarkSize.width(),
                      target.height() * m_watermarkSize.height());
    const QPointF centre(target.left() + target.width() * m_watermarkPosition.x(),
                         target.top() + target.height() * m_watermarkPosition.y());
    return QRectF(centre.x() - size.width() / 2.0,
                  centre.y() - size.height() / 2.0,
                  size.width(), size.height());
}

void ImageCanvas::drawMovableWatermark(QPainter &painter, const QRectF &target)
{
    if (!m_watermarkVisible || target.isEmpty())
        return;
    const QRectF bounds = watermarkRect(target).intersected(target);
    painter.save();
    painter.fillRect(bounds, QColor(76, 205, 240, m_movingWatermark ? 28 : 14));
    painter.setPen(QPen(QColor(110, 231, 249, 240), 1, Qt::DashLine));
    painter.drawRect(bounds.adjusted(0.5, 0.5, -0.5, -0.5));
    const double handle = 5.0;
    painter.fillRect(QRectF(bounds.center().x() - handle / 2.0,
                            bounds.center().y() - handle / 2.0,
                            handle, handle), QColor(255, 255, 255, 220));
    painter.restore();
}

void ImageCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), QColor(QStringLiteral("#090b10")));

    if (m_original.isNull() && m_edited.isNull()) {
        painter.setPen(QColor(QStringLiteral("#9aa1af")));
        QFont titleFont = painter.font();
        titleFont.setPointSize(18);
        titleFont.setWeight(QFont::DemiBold);
        painter.setFont(titleFont);
        painter.drawText(rect().adjusted(20, -18, -20, 0), Qt::AlignCenter,
                         QStringLiteral("Bring your photographs to life"));
        QFont bodyFont = painter.font();
        bodyFont.setPointSize(11);
        bodyFont.setWeight(QFont::Normal);
        painter.setFont(bodyFont);
        painter.setPen(QColor(QStringLiteral("#6f7786")));
        painter.drawText(rect().adjusted(20, 32, -20, 0), Qt::AlignCenter,
                         QStringLiteral("Import a folder or add individual photos to begin"));
        return;
    }

    if (m_mode == CompareMode::SideBySide) {
        const QRectF left(0, 0, width() / 2.0, height());
        const QRectF right(width() / 2.0, 0, width() / 2.0, height());
        drawImage(painter, m_original, fittedRect(m_original.size(), left));
        const QRectF editedTarget = fittedRect(m_edited.size(), right);
        drawImage(painter, m_edited, editedTarget);
        drawActiveLayerOverlay(painter, editedTarget);
        painter.setPen(QPen(QColor(QStringLiteral("#6d5dfc")), 2));
        painter.drawLine(width() / 2, 0, width() / 2, height());
        painter.setPen(Qt::white);
        painter.drawText(QRectF(14, 14, 100, 24), QStringLiteral("BEFORE"));
        painter.drawText(QRectF(width() / 2.0 + 14, 14, 100, 24), QStringLiteral("AFTER"));
        drawGuides(painter, editedTarget);
    } else {
        const QRectF target = activeImageRect();
        if (m_mode == CompareMode::Original) {
            drawImage(painter, m_original, target);
        } else if (m_mode == CompareMode::Split) {
            drawImage(painter, m_original, target);
            painter.save();
            painter.setClipRect(QRectF(width() / 2.0, 0, width() / 2.0, height()));
            drawImage(painter, m_edited, target);
            painter.restore();
            painter.setPen(QPen(Qt::white, 2));
            painter.drawLine(width() / 2, target.top(), width() / 2, target.bottom());
        } else {
            drawImage(painter, m_edited, target);
        }
        if (m_mode != CompareMode::Original)
            drawActiveLayerOverlay(painter, target);
        drawGuides(painter, target);
        if (m_mode != CompareMode::Original)
            drawMovableWatermark(painter, target);
    }

    if (m_cropMode && !m_cropSelection.isNull()) {
        const QRectF imageRect = activeImageRect();
        QPainterPath shade;
        shade.addRect(imageRect);
        QPainterPath opening;
        opening.addRect(m_cropSelection.normalized().intersected(imageRect));
        painter.fillPath(shade.subtracted(opening), QColor(0, 0, 0, 150));
        painter.setPen(QPen(QColor(QStringLiteral("#f7f7fa")), 1));
        painter.drawRect(m_cropSelection.normalized().intersected(imageRect));
        painter.setPen(QPen(QColor(255, 255, 255, 110), 1));
        const QRectF selection = m_cropSelection.normalized().intersected(imageRect);
        painter.drawLine(selection.left() + selection.width() / 3.0, selection.top(),
                         selection.left() + selection.width() / 3.0, selection.bottom());
        painter.drawLine(selection.left() + selection.width() * 2.0 / 3.0, selection.top(),
                         selection.left() + selection.width() * 2.0 / 3.0, selection.bottom());
        painter.drawLine(selection.left(), selection.top() + selection.height() / 3.0,
                         selection.right(), selection.top() + selection.height() / 3.0);
        painter.drawLine(selection.left(), selection.top() + selection.height() * 2.0 / 3.0,
                         selection.right(), selection.top() + selection.height() * 2.0 / 3.0);
    }

    if (m_shapeMaskMode != ShapeMaskMode::None && !m_shapeMaskSelection.isNull()) {
        const QRectF imageRect = activeImageRect();
        const QRectF selection = m_shapeMaskSelection.normalized().intersected(imageRect);
        QPainterPath shade;
        shade.addRect(imageRect);
        QPainterPath opening;
        if (m_shapeMaskMode == ShapeMaskMode::Ellipse)
            opening.addEllipse(selection);
        else
            opening.addRect(selection);
        painter.fillPath(shade.subtracted(opening), QColor(0, 0, 0, 135));
        painter.setPen(QPen(QColor(QStringLiteral("#6ee7f9")), 2));
        if (m_shapeMaskMode == ShapeMaskMode::Ellipse)
            painter.drawEllipse(selection);
        else
            painter.drawRect(selection);
    }

    if (m_retouchMode == RetouchMode::CloneStamp && m_hasCloneSource
        && m_mode != CompareMode::Original && m_mode != CompareMode::SideBySide) {
        const QRectF imageRect = activeImageRect();
        const QPointF source(imageRect.left() + imageRect.width() * m_cloneSource.x(),
                             imageRect.top() + imageRect.height() * m_cloneSource.y());
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(QStringLiteral("#6ee7f9")), 2, Qt::DashLine));
        painter.setBrush(QColor(110, 231, 249, 24));
        painter.drawEllipse(source, 12.0, 12.0);
        painter.drawLine(source + QPointF(-16.0, 0.0), source + QPointF(16.0, 0.0));
        painter.drawLine(source + QPointF(0.0, -16.0), source + QPointF(0.0, 16.0));
        painter.restore();
    }
}

void ImageCanvas::resizeEvent(QResizeEvent *)
{
    if (m_zoom <= 0.0)
        update();
}

void ImageCanvas::wheelEvent(QWheelEvent *event)
{
    if (m_edited.isNull())
        return;
    if (m_zoom <= 0.0)
        m_zoom = static_cast<double>(activeImageRect().width()) / m_edited.width();
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    m_zoom = std::clamp(m_zoom * factor, 0.05, 8.0);
    emit zoomChanged(m_zoom * 100.0);
    update();
    event->accept();
}

void ImageCanvas::mousePressEvent(QMouseEvent *event)
{
    const QRectF imageRect = activeImageRect();
    const double rulerHeight = qMin(22.0, imageRect.height() * 0.12);
    const double rulerWidth = qMin(30.0, imageRect.width() * 0.12);
    if (m_rulersVisible && event->button() == Qt::LeftButton
        && imageRect.contains(event->position())
        && event->position().y() <= imageRect.top() + rulerHeight) {
        m_guideDragMode = GuideDragMode::Horizontal;
        m_pendingGuide = qBound(0.0,
            (event->position().y() - imageRect.top()) / imageRect.height(), 1.0);
        setCursor(Qt::SplitVCursor);
        event->accept();
        update();
        return;
    }
    if (m_retouchMode != RetouchMode::None
        && m_mode != CompareMode::Original && m_mode != CompareMode::SideBySide
        && event->button() == Qt::LeftButton && imageRect.contains(event->position())) {
        const QPointF normalized(
            qBound(0.0, (event->position().x() - imageRect.left()) / imageRect.width(), 1.0),
            qBound(0.0, (event->position().y() - imageRect.top()) / imageRect.height(), 1.0));
        if (m_retouchMode == RetouchMode::DustHeal) {
            emit dustRemovalRequested(normalized);
        } else if (!m_hasCloneSource) {
            m_cloneSource = normalized;
            m_hasCloneSource = true;
            emit cloneSourceSelected(normalized);
        } else {
            emit cloneRequested(m_cloneSource, normalized);
            m_hasCloneSource = false;
        }
        event->accept();
        update();
        return;
    }
    if (m_watermarkVisible && m_mode != CompareMode::Original
        && m_mode != CompareMode::SideBySide && event->button() == Qt::LeftButton
        && watermarkRect(imageRect).contains(event->position())) {
        m_movingWatermark = true;
        const QPointF centre(imageRect.left() + imageRect.width() * m_watermarkPosition.x(),
                             imageRect.top() + imageRect.height() * m_watermarkPosition.y());
        m_watermarkDragOffset = event->position() - centre;
        setCursor(Qt::SizeAllCursor);
        event->accept();
        update();
        return;
    }
    if (m_rulersVisible && event->button() == Qt::LeftButton
        && imageRect.contains(event->position())
        && event->position().x() <= imageRect.left() + rulerWidth) {
        m_guideDragMode = GuideDragMode::Vertical;
        m_pendingGuide = qBound(0.0,
            (event->position().x() - imageRect.left()) / imageRect.width(), 1.0);
        setCursor(Qt::SplitHCursor);
        event->accept();
        update();
        return;
    }
    if (m_shapeMaskMode != ShapeMaskMode::None
        && event->button() == Qt::LeftButton
        && imageRect.contains(event->position())) {
        m_selectingShapeMask = true;
        m_shapeMaskStart = event->position();
        m_shapeMaskSelection = QRectF(m_shapeMaskStart, m_shapeMaskStart);
    } else if (m_cropMode && event->button() == Qt::LeftButton
        && activeImageRect().contains(event->position())) {
        m_selectingCrop = true;
        m_cropStart = event->position();
        m_cropSelection = QRectF(m_cropStart, m_cropStart);
    } else if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastMouse = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_movingWatermark) {
        const QRectF imageRect = activeImageRect();
        const QPointF requested = event->position() - m_watermarkDragOffset;
        const double halfWidth = m_watermarkSize.width() / 2.0;
        const double halfHeight = m_watermarkSize.height() / 2.0;
        m_watermarkPosition = QPointF(
            qBound(halfWidth, (requested.x() - imageRect.left()) / imageRect.width(),
                   1.0 - halfWidth),
            qBound(halfHeight, (requested.y() - imageRect.top()) / imageRect.height(),
                   1.0 - halfHeight));
        update();
    } else if (m_guideDragMode != GuideDragMode::None) {
        const QRectF imageRect = activeImageRect();
        m_pendingGuide = m_guideDragMode == GuideDragMode::Horizontal
            ? qBound(0.0, (event->position().y() - imageRect.top()) / imageRect.height(), 1.0)
            : qBound(0.0, (event->position().x() - imageRect.left()) / imageRect.width(), 1.0);
        update();
    } else if (m_selectingShapeMask) {
        const QPointF bounded(qBound(activeImageRect().left(), event->position().x(), activeImageRect().right()),
                              qBound(activeImageRect().top(), event->position().y(), activeImageRect().bottom()));
        m_shapeMaskSelection = QRectF(m_shapeMaskStart, bounded);
        update();
    } else if (m_selectingCrop) {
        m_cropSelection = QRectF(m_cropStart, constrainedCropPoint(event->position()));
        update();
    } else if (m_panning) {
        const QPoint current = event->position().toPoint();
        m_pan += QPointF(current - m_lastMouse);
        m_lastMouse = current;
        update();
    } else if (m_rulersVisible && activeImageRect().contains(event->position())) {
        const QRectF imageRect = activeImageRect();
        const double rulerHeight = qMin(22.0, imageRect.height() * 0.12);
        const double rulerWidth = qMin(30.0, imageRect.width() * 0.12);
        if (event->position().y() <= imageRect.top() + rulerHeight)
            setCursor(Qt::SplitVCursor);
        else if (event->position().x() <= imageRect.left() + rulerWidth)
            setCursor(Qt::SplitHCursor);
        else
            setCursor(m_cropMode || m_shapeMaskMode != ShapeMaskMode::None
                || m_retouchMode != RetouchMode::None
                ? Qt::CrossCursor : Qt::ArrowCursor);
    }
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_movingWatermark && event->button() == Qt::LeftButton) {
        m_movingWatermark = false;
        setCursor(Qt::ArrowCursor);
        emit watermarkMoveRequested(m_watermarkPosition);
        event->accept();
        update();
        return;
    }
    if (m_guideDragMode != GuideDragMode::None && event->button() == Qt::LeftButton) {
        const QRectF imageRect = activeImageRect();
        if (imageRect.contains(event->position())) {
            QList<double> &guides = m_guideDragMode == GuideDragMode::Horizontal
                ? m_horizontalGuides : m_verticalGuides;
            const bool duplicate = std::any_of(guides.cbegin(), guides.cend(), [this](double value) {
                return std::abs(value - m_pendingGuide) < 0.004;
            });
            if (!duplicate)
                guides.append(m_pendingGuide);
        }
        m_guideDragMode = GuideDragMode::None;
        setCursor(Qt::ArrowCursor);
        event->accept();
        update();
        return;
    }
    if (m_selectingShapeMask && event->button() == Qt::LeftButton) {
        m_selectingShapeMask = false;
        const QRectF imageRect = activeImageRect();
        const QRectF selection = m_shapeMaskSelection.normalized().intersected(imageRect);
        const ShapeMaskMode completedMode = m_shapeMaskMode;
        if (selection.width() >= 20.0 && selection.height() >= 20.0) {
            emit shapeMaskRequested(QRectF(
                (selection.left() - imageRect.left()) / imageRect.width(),
                (selection.top() - imageRect.top()) / imageRect.height(),
                selection.width() / imageRect.width(),
                selection.height() / imageRect.height()), completedMode);
        }
        m_shapeMaskMode = ShapeMaskMode::None;
        setCursor(Qt::ArrowCursor);
        update();
    }
    if (m_selectingCrop && event->button() == Qt::LeftButton) {
        m_selectingCrop = false;
        const QRectF imageRect = activeImageRect();
        const QRectF selection = m_cropSelection.normalized().intersected(imageRect);
        if (selection.width() >= 20.0 && selection.height() >= 20.0) {
            emit cropRequested(QRectF(
                (selection.left() - imageRect.left()) / imageRect.width(),
                (selection.top() - imageRect.top()) / imageRect.height(),
                selection.width() / imageRect.width(),
                selection.height() / imageRect.height()));
        }
    }
    if (m_panning && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
        m_panning = false;
        setCursor(m_cropMode || m_shapeMaskMode != ShapeMaskMode::None
            || m_retouchMode != RetouchMode::None
            ? Qt::CrossCursor : Qt::ArrowCursor);
    }
}

} // namespace chroma
