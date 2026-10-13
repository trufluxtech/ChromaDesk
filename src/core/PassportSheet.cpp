#include "core/PassportSheet.h"

#include <QColorSpace>
#include <QPainter>
#include <QPen>
#include <QtGlobal>

namespace chroma {
namespace {

int millimetresToPixels(double millimetres, int dpi)
{
    return qMax(1, qRound(millimetres * static_cast<double>(dpi) / 25.4));
}

QImage centerCrop(const QImage &source, double targetAspect)
{
    if (source.isNull() || targetAspect <= 0.0)
        return {};
    const double sourceAspect = static_cast<double>(source.width()) / source.height();
    QRect crop = source.rect();
    if (sourceAspect > targetAspect) {
        const int width = qMax(1, qRound(source.height() * targetAspect));
        crop.setLeft((source.width() - width) / 2);
        crop.setWidth(width);
    } else if (sourceAspect < targetAspect) {
        const int height = qMax(1, qRound(source.width() / targetAspect));
        crop.setTop((source.height() - height) / 2);
        crop.setHeight(height);
    }
    return source.copy(crop.intersected(source.rect()));
}

} // namespace

PassportSheetOptions PassportSheetOptions::fourBySix()
{
    return {};
}

PassportSheetOptions PassportSheetOptions::a4()
{
    PassportSheetOptions options;
    options.paperMillimetres = QSizeF(210.0, 297.0);
    options.marginMillimetres = 8.0;
    return options;
}

QImage PassportSheet::create(const QImage &source,
                             const PassportSheetOptions &options,
                             int *copyCount)
{
    if (copyCount)
        *copyCount = 0;
    if (source.isNull() || options.dpi <= 0
        || options.paperMillimetres.isEmpty()
        || options.photoMillimetres.isEmpty()) {
        return {};
    }

    const QSize sheetSize(
        millimetresToPixels(options.paperMillimetres.width(), options.dpi),
        millimetresToPixels(options.paperMillimetres.height(), options.dpi));
    const QSize photoSize(
        millimetresToPixels(options.photoMillimetres.width(), options.dpi),
        millimetresToPixels(options.photoMillimetres.height(), options.dpi));
    const int margin = millimetresToPixels(options.marginMillimetres, options.dpi);
    const int gap = millimetresToPixels(options.gapMillimetres, options.dpi);
    const int columns = (sheetSize.width() - margin * 2 + gap)
        / (photoSize.width() + gap);
    const int rows = (sheetSize.height() - margin * 2 + gap)
        / (photoSize.height() + gap);
    if (columns <= 0 || rows <= 0)
        return {};

    const double aspect = options.photoMillimetres.width()
        / options.photoMillimetres.height();
    const QImage portrait = centerCrop(source, aspect)
        .scaled(photoSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (portrait.isNull())
        return {};

    QImage sheet(sheetSize, QImage::Format_RGB32);
    sheet.fill(Qt::white);
    sheet.setDotsPerMeterX(qRound(options.dpi / 0.0254));
    sheet.setDotsPerMeterY(qRound(options.dpi / 0.0254));
    sheet.setColorSpace(QColorSpace(QColorSpace::SRgb));

    const int gridWidth = columns * photoSize.width() + (columns - 1) * gap;
    const int gridHeight = rows * photoSize.height() + (rows - 1) * gap;
    const int startX = (sheetSize.width() - gridWidth) / 2;
    const int startY = (sheetSize.height() - gridHeight) / 2;
    QPainter painter(&sheet);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setPen(QPen(QColor(QStringLiteral("#b8bcc5")), 1));
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const QRect target(startX + column * (photoSize.width() + gap),
                               startY + row * (photoSize.height() + gap),
                               photoSize.width(), photoSize.height());
            painter.drawImage(target, portrait);
            painter.drawRect(target.adjusted(0, 0, -1, -1));
        }
    }
    if (copyCount)
        *copyCount = rows * columns;
    return sheet;
}

} // namespace chroma
