#include "ui/HistogramWidget.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace chroma {

HistogramWidget::HistogramWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(118);
}

QSize HistogramWidget::sizeHint() const
{
    return {280, 132};
}

void HistogramWidget::setImage(const QImage &source)
{
    m_red.fill(0);
    m_green.fill(0);
    m_blue.fill(0);
    m_peak = 1;
    if (source.isNull()) {
        update();
        return;
    }

    const QImage image = source.scaled(360, 240, Qt::KeepAspectRatio,
                                       Qt::FastTransformation)
                             .convertToFormat(QImage::Format_RGB888);
    for (int y = 0; y < image.height(); ++y) {
        const uchar *line = image.constScanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            ++m_red[line[x * 3]];
            ++m_green[line[x * 3 + 1]];
            ++m_blue[line[x * 3 + 2]];
        }
    }
    for (int i = 1; i < 255; ++i)
        m_peak = std::max({m_peak, m_red[i], m_green[i], m_blue[i]});
    update();
}

void HistogramWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(QStringLiteral("#0d0f15")));
    painter.setPen(QColor(QStringLiteral("#252a35")));
    for (int i = 1; i < 4; ++i)
        painter.drawLine(width() * i / 4, 0, width() * i / 4, height());

    painter.setCompositionMode(QPainter::CompositionMode_Plus);
    const auto drawChannel = [this, &painter](const std::array<int, 256> &bins,
                                              const QColor &color) {
        QPainterPath path;
        path.moveTo(0.0, height());
        for (int i = 0; i < 256; ++i) {
            const qreal x = static_cast<qreal>(i) * width() / 255.0;
            const qreal normalized = static_cast<qreal>(bins[i]) / m_peak;
            path.lineTo(x, height() * (1.0 - normalized));
        }
        path.lineTo(width(), height());
        path.closeSubpath();
        painter.fillPath(path, color);
    };
    drawChannel(m_red, QColor(255, 55, 78, 105));
    drawChannel(m_green, QColor(35, 226, 135, 105));
    drawChannel(m_blue, QColor(55, 132, 255, 105));
}

} // namespace chroma
