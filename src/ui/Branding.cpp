#include "ui/Branding.h"

#include <QColor>
#include <QConicalGradient>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>

namespace chroma::branding {

QPixmap logoPixmap(int size)
{
    const int boundedSize = qMax(32, size);
    QPixmap pixmap(boundedSize, boundedSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(static_cast<double>(boundedSize) / 128.0,
                  static_cast<double>(boundedSize) / 128.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#11131a")));
    painter.drawRoundedRect(QRectF(0.0, 0.0, 128.0, 128.0), 30.0, 30.0);

    QConicalGradient ring(64.0, 64.0, -90.0);
    ring.setColorAt(0.0, QColor(QStringLiteral("#6ee7f9")));
    ring.setColorAt(0.52, QColor(QStringLiteral("#7c6cff")));
    ring.setColorAt(1.0, QColor(QStringLiteral("#ff5fa2")));
    painter.setBrush(ring);
    painter.drawEllipse(QRectF(15.0, 15.0, 98.0, 98.0));
    painter.setBrush(QColor(QStringLiteral("#11131a")));
    painter.drawEllipse(QRectF(30.0, 30.0, 68.0, 68.0));
    painter.setBrush(QColor(QStringLiteral("#f8fafc")));
    painter.drawEllipse(QRectF(52.0, 52.0, 24.0, 24.0));
    return pixmap;
}

QIcon applicationIcon()
{
    QIcon icon;
    for (const int size : {32, 64, 128, 256})
        icon.addPixmap(logoPixmap(size));
    return icon;
}

QPixmap splashPixmap()
{
    QPixmap pixmap(620, 360);
    pixmap.fill(QColor(QStringLiteral("#0b0d12")));

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(246, 54, logoPixmap(128));

    QFont titleFont = painter.font();
    titleFont.setPointSize(30);
    titleFont.setWeight(QFont::DemiBold);
    painter.setFont(titleFont);
    painter.setPen(QColor(QStringLiteral("#f8fafc")));
    painter.drawText(QRect(0, 196, pixmap.width(), 52),
                     Qt::AlignHCenter | Qt::AlignVCenter,
                     QStringLiteral("ChromaDesk"));

    QFont subtitleFont = painter.font();
    subtitleFont.setPointSize(12);
    subtitleFont.setWeight(QFont::Normal);
    painter.setFont(subtitleFont);
    painter.setPen(QColor(QStringLiteral("#aeb5c5")));
    painter.drawText(QRect(0, 247, pixmap.width(), 30),
                     Qt::AlignHCenter | Qt::AlignVCenter,
                     QStringLiteral("Local AI photo editing · Windows + macOS"));

    QLinearGradient accent(150.0, 0.0, 470.0, 0.0);
    accent.setColorAt(0.0, QColor(QStringLiteral("#6ee7f9")));
    accent.setColorAt(0.52, QColor(QStringLiteral("#7c6cff")));
    accent.setColorAt(1.0, QColor(QStringLiteral("#ff5fa2")));
    painter.setPen(QPen(accent, 4.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(150.0, 312.0), QPointF(470.0, 312.0));
    return pixmap;
}

} // namespace chroma::branding
