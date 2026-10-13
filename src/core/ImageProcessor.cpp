#include "core/ImageProcessor.h"

#include "core/SmartMaskEngine.h"

#include <QColorSpace>
#include <QFont>
#include <QFontMetricsF>
#include <QImageReader>
#include <QPainter>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace chroma {
namespace {

float clamp01(float value)
{
    return std::clamp(value, 0.0F, 1.0F);
}

float smoothstep(float edge0, float edge1, float x)
{
    const float t = clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0F - 2.0F * t);
}

QRect normalizedCropRect(const QSize &size, const QRectF &crop)
{
    const QRectF bounded = crop.normalized().intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    const int x = qBound(0, qRound(bounded.x() * size.width()), size.width() - 1);
    const int y = qBound(0, qRound(bounded.y() * size.height()), size.height() - 1);
    const int width = qBound(1, qRound(bounded.width() * size.width()), size.width() - x);
    const int height = qBound(1, qRound(bounded.height() * size.height()), size.height() - y);
    return QRect(x, y, width, height);
}

void hsvToRgb(float hue, float saturation, float value,
              float &red, float &green, float &blue)
{
    if (saturation <= 0.0001F || hue < 0.0F) {
        red = value;
        green = value;
        blue = value;
        return;
    }
    hue -= std::floor(hue);
    const float chroma = value * saturation;
    const float sector = hue * 6.0F;
    const float secondary = chroma * (1.0F - std::abs(std::fmod(sector, 2.0F) - 1.0F));
    float first = 0.0F;
    float second = 0.0F;
    float third = 0.0F;
    switch (static_cast<int>(sector) % 6) {
    case 0: first = chroma; second = secondary; break;
    case 1: first = secondary; second = chroma; break;
    case 2: second = chroma; third = secondary; break;
    case 3: second = secondary; third = chroma; break;
    case 4: first = secondary; third = chroma; break;
    case 5: first = chroma; third = secondary; break;
    }
    const float match = value - chroma;
    red = first + match;
    green = second + match;
    blue = third + match;
}

Qt::Alignment textAlignment(const QPointF &position)
{
    Qt::Alignment alignment;
    if (position.x() < 0.34)
        alignment |= Qt::AlignLeft;
    else if (position.x() > 0.66)
        alignment |= Qt::AlignRight;
    else
        alignment |= Qt::AlignHCenter;

    if (position.y() < 0.34)
        alignment |= Qt::AlignTop;
    else if (position.y() > 0.66)
        alignment |= Qt::AlignBottom;
    else
        alignment |= Qt::AlignVCenter;
    return alignment;
}

void drawTextOverlay(QImage &image, const TextOverlay &overlay, double opacity)
{
    if (image.isNull() || !overlay.isEnabled() || opacity <= 0.0)
        return;

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setOpacity(qBound(0.0, opacity, 1.0));
    QFont font(overlay.fontFamily);
    font.setPixelSize(qMax(8, qRound(image.height() * overlay.size)));
    font.setBold(overlay.bold);
    font.setItalic(overlay.italic);
    painter.setFont(font);

    const int margin = qMax(8, qRound(image.height() * 0.025));
    const QRectF textArea = QRectF(image.rect()).adjusted(margin, margin, -margin, -margin);
    const int flags = static_cast<int>(textAlignment(overlay.position))
        | Qt::TextWordWrap | Qt::TextDontClip;
    if (overlay.outline) {
        const int radius = qBound(1, qRound(image.height() * 0.0025), 8);
        painter.setPen(overlay.outlineColor);
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (x == 0 && y == 0)
                    continue;
                painter.drawText(textArea.translated(x, y), flags, overlay.text);
            }
        }
    }
    painter.setPen(overlay.color);
    painter.drawText(textArea, flags, overlay.text);
}

void drawWatermarkOverlay(QImage &image, const WatermarkOverlay &overlay,
                          double layerOpacity)
{
    if (image.isNull() || !overlay.isEnabled() || layerOpacity <= 0.0)
        return;
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setOpacity(qBound(0.0, overlay.opacity * layerOpacity, 1.0));

    if (overlay.type == WatermarkType::Image) {
        QImageReader reader(overlay.imagePath);
        reader.setAutoTransform(true);
        const QImage watermark = reader.read();
        if (watermark.isNull())
            return;
        const int targetWidth = qMax(1, qRound(image.width() * overlay.size));
        const QSize targetSize = watermark.size().scaled(
            targetWidth, qMax(1, image.height()), Qt::KeepAspectRatio);
        QPointF topLeft(image.width() * overlay.position.x() - targetSize.width() / 2.0,
                        image.height() * overlay.position.y() - targetSize.height() / 2.0);
        topLeft.setX(qBound(0.0, topLeft.x(),
            qMax(0.0, image.width() - static_cast<double>(targetSize.width()))));
        topLeft.setY(qBound(0.0, topLeft.y(),
            qMax(0.0, image.height() - static_cast<double>(targetSize.height()))));
        painter.drawImage(QRectF(topLeft, QSizeF(targetSize)), watermark);
        return;
    }

    QFont font(overlay.fontFamily);
    font.setPixelSize(qMax(8, qRound(image.height() * overlay.size)));
    font.setBold(overlay.bold);
    painter.setFont(font);
    const QFontMetricsF metrics(font);
    QRectF bounds = metrics.boundingRect(overlay.text);
    const double padding = qMax(3.0, image.height() * 0.004);
    bounds.adjust(-padding, -padding, padding, padding);
    QPointF topLeft(image.width() * overlay.position.x() - bounds.width() / 2.0,
                    image.height() * overlay.position.y() - bounds.height() / 2.0);
    topLeft.setX(qBound(0.0, topLeft.x(), qMax(0.0, image.width() - bounds.width())));
    topLeft.setY(qBound(0.0, topLeft.y(), qMax(0.0, image.height() - bounds.height())));
    const QRectF target(topLeft, bounds.size());
    const int flags = Qt::AlignCenter | Qt::TextSingleLine;
    if (overlay.outline) {
        const int radius = qBound(1, qRound(image.height() * 0.002), 7);
        painter.setPen(overlay.outlineColor);
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (x != 0 || y != 0)
                    painter.drawText(target.translated(x, y), flags, overlay.text);
            }
        }
    }
    painter.setPen(overlay.color);
    painter.drawText(target, flags, overlay.text);
}

QPoint pointForNormalizedPosition(const QPointF &position, const QSize &size)
{
    return QPoint(qBound(0, qRound(position.x() * (size.width() - 1)),
                          size.width() - 1),
                  qBound(0, qRound(position.y() * (size.height() - 1)),
                          size.height() - 1));
}

std::array<double, 3> ringMean(const QImage &image, const QPoint &center,
                               int innerRadius, int outerRadius)
{
    std::array<double, 3> mean{};
    int samples = 0;
    const int innerSquared = innerRadius * innerRadius;
    const int outerSquared = outerRadius * outerRadius;
    for (int y = -outerRadius; y <= outerRadius; ++y) {
        const int py = center.y() + y;
        if (py < 0 || py >= image.height())
            continue;
        const uchar *line = image.constScanLine(py);
        for (int x = -outerRadius; x <= outerRadius; ++x) {
            const int distanceSquared = x * x + y * y;
            if (distanceSquared < innerSquared || distanceSquared > outerSquared)
                continue;
            const int px = center.x() + x;
            if (px < 0 || px >= image.width())
                continue;
            const uchar *pixel = line + px * 4;
            mean[0] += pixel[0];
            mean[1] += pixel[1];
            mean[2] += pixel[2];
            ++samples;
        }
    }
    if (samples > 0) {
        for (double &channel : mean)
            channel /= samples;
    }
    return mean;
}

QPoint automaticHealSource(const QImage &image, const QPoint &target, int radius)
{
    const auto targetMean = ringMean(image, target, radius,
                                     qMax(radius + 1, qRound(radius * 1.45)));
    QPoint best = target;
    double bestScore = std::numeric_limits<double>::max();
    const int distance = qMax(radius * 3, 6);
    for (int step = 0; step < 16; ++step) {
        const double angle = static_cast<double>(step) * std::numbers::pi / 8.0;
        const QPoint candidate(target.x() + qRound(std::cos(angle) * distance),
                               target.y() + qRound(std::sin(angle) * distance));
        if (candidate.x() - radius < 0 || candidate.y() - radius < 0
            || candidate.x() + radius >= image.width()
            || candidate.y() + radius >= image.height())
            continue;
        const auto candidateMean = ringMean(image, candidate, 0, radius);
        double score = 0.0;
        for (int channel = 0; channel < 3; ++channel) {
            const double difference = targetMean[static_cast<size_t>(channel)]
                - candidateMean[static_cast<size_t>(channel)];
            score += difference * difference;
        }
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    // Very small images may not have a candidate at three radii. Prefer the
    // nearest valid patch to leaving the dust spot unchanged.
    if (best == target) {
        best.setX(qBound(radius, target.x() + radius * 2,
                         qMax(radius, image.width() - radius - 1)));
        best.setY(qBound(radius, target.y(),
                         qMax(radius, image.height() - radius - 1)));
    }
    return best;
}

QImage clonePatch(const QImage &input, const QPoint &sourceCenter,
                  const QPoint &targetCenter, int radius, bool matchColour)
{
    const QImage source = input.convertToFormat(QImage::Format_RGBA8888);
    QImage result = source.copy();
    const auto targetMean = ringMean(source, targetCenter, radius,
                                     qMax(radius + 1, qRound(radius * 1.45)));
    const auto sourceMean = ringMean(source, sourceCenter, 0, radius);
    const double inner = radius * 0.68;

    for (int offsetY = -radius; offsetY <= radius; ++offsetY) {
        const int targetY = targetCenter.y() + offsetY;
        const int sourceY = sourceCenter.y() + offsetY;
        if (targetY < 0 || targetY >= result.height()
            || sourceY < 0 || sourceY >= source.height())
            continue;
        uchar *targetLine = result.scanLine(targetY);
        const uchar *sourceLine = source.constScanLine(sourceY);
        for (int offsetX = -radius; offsetX <= radius; ++offsetX) {
            const double distance = std::hypot(static_cast<double>(offsetX),
                                               static_cast<double>(offsetY));
            if (distance > radius)
                continue;
            const int targetX = targetCenter.x() + offsetX;
            const int sourceX = sourceCenter.x() + offsetX;
            if (targetX < 0 || targetX >= result.width()
                || sourceX < 0 || sourceX >= source.width())
                continue;
            const float amount = distance <= inner ? 1.0F
                : 1.0F - smoothstep(static_cast<float>(inner),
                                    static_cast<float>(radius),
                                    static_cast<float>(distance));
            uchar *targetPixel = targetLine + targetX * 4;
            const uchar *sourcePixel = sourceLine + sourceX * 4;
            for (int channel = 0; channel < 4; ++channel) {
                double sample = sourcePixel[channel];
                if (matchColour && channel < 3) {
                    sample += targetMean[static_cast<size_t>(channel)]
                        - sourceMean[static_cast<size_t>(channel)];
                    sample = qBound(0.0, sample, 255.0);
                }
                targetPixel[channel] = static_cast<uchar>(std::lround(
                    targetPixel[channel] * (1.0F - amount) + sample * amount));
            }
        }
    }
    return result;
}

QImage applyRetouchOperation(const QImage &input,
                             const RetouchOperation &operation)
{
    if (input.isNull() || !operation.isEnabled())
        return input;
    if (operation.type == RetouchType::GeneratedImage) {
        QImageReader reader(operation.generatedImagePath);
        reader.setAutoTransform(true);
        QImage generated = reader.read();
        if (generated.isNull())
            return input;
        generated = generated.convertToFormat(QImage::Format_RGBA8888);
        if (generated.size() != input.size()) {
            generated = generated.scaled(input.size(), Qt::IgnoreAspectRatio,
                                         Qt::SmoothTransformation);
        }
        if (input.colorSpace().isValid())
            generated.setColorSpace(input.colorSpace());
        return generated;
    }

    const QImage image = input.convertToFormat(QImage::Format_RGBA8888);
    const int radius = qBound(2,
        qRound(qMin(image.width(), image.height()) * operation.radius),
        qMax(2, qMin(image.width(), image.height()) / 3));
    const QPoint target = pointForNormalizedPosition(operation.target, image.size());
    if (operation.type == RetouchType::HealSpot) {
        const QPoint source = automaticHealSource(image, target, radius);
        return clonePatch(image, source, target, radius, true);
    }
    const QPoint source = pointForNormalizedPosition(operation.source, image.size());
    return clonePatch(image, source, target, radius, false);
}

} // namespace

QImage ImageProcessor::orientedAndCropped(const QImage &source, const EditState &state,
                                           int maxEdge)
{
    if (source.isNull())
        return {};

    QTransform transform;
    if (state.rotation != 0)
        transform.rotate(state.rotation);
    transform.scale(state.flipHorizontal ? -1.0 : 1.0,
                    state.flipVertical ? -1.0 : 1.0);

    QImage image = transform.isIdentity()
        ? source
        : source.transformed(transform, Qt::SmoothTransformation);
    image = image.copy(normalizedCropRect(image.size(), state.crop));

    if (maxEdge > 0 && (image.width() > maxEdge || image.height() > maxEdge)) {
        image = image.scaled(maxEdge, maxEdge, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }
    return image;
}

QImage ImageProcessor::withBorder(const QImage &source, const EditState &state,
                                  int maxEdge)
{
    if (source.isNull() || state.borderSize <= 0.0001)
        return source;
    const int border = qMax(1, qRound(qMin(source.width(), source.height())
        * qBound(0.0, state.borderSize, 0.20)));
    QImage result(source.width() + border * 2, source.height() + border * 2,
                  QImage::Format_RGBA8888);
    result.fill(state.borderColor);
    QPainter painter(&result);
    painter.drawImage(QPoint(border, border), source);
    painter.end();
    if (source.colorSpace().isValid())
        result.setColorSpace(source.colorSpace());
    if (maxEdge > 0 && (result.width() > maxEdge || result.height() > maxEdge)) {
        result = result.scaled(maxEdge, maxEdge, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
    }
    return result;
}

QImage ImageProcessor::render(const QImage &source, const EditState &state, int maxEdge)
{
    QImage image = orientedAndCropped(source, state, maxEdge)
                       .convertToFormat(QImage::Format_RGBA8888);
    if (image.isNull())
        return {};

    const float exposureFactor = std::pow(2.0F, static_cast<float>(state.exposure));
    const float contrast = static_cast<float>(state.contrast / 100.0);
    const float temperature = static_cast<float>(state.temperature / 100.0);
    const float tint = static_cast<float>(state.tint / 100.0);
    const float saturation = static_cast<float>(state.saturation / 100.0);
    const float vibrance = static_cast<float>(state.vibrance / 100.0);
    const float clarity = static_cast<float>(state.clarity / 100.0);
    const float highlights = static_cast<float>(state.highlights / 100.0);
    const float shadows = static_cast<float>(state.shadows / 100.0);
    const float whites = static_cast<float>(state.whites / 100.0);
    const float blacks = static_cast<float>(state.blacks / 100.0);
    const float replacementAmount = state.replacementColor.isValid()
        ? static_cast<float>(qBound(0.0, state.replacementAmount / 100.0, 1.0))
        : 0.0F;
    float replacementHue = -1.0F;
    float replacementSaturation = 0.0F;
    float replacementValue = 0.0F;
    float replacementAlpha = 1.0F;
    if (replacementAmount > 0.0F) {
        state.replacementColor.getHsvF(&replacementHue, &replacementSaturation,
                                       &replacementValue, &replacementAlpha);
    }

    for (int y = 0; y < image.height(); ++y) {
        auto *line = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            auto *pixel = line + x * 4;
            float red = static_cast<float>(pixel[0]) / 255.0F;
            float green = static_cast<float>(pixel[1]) / 255.0F;
            float blue = static_cast<float>(pixel[2]) / 255.0F;

            // White balance. The coefficients are deliberately restrained so
            // the full slider range remains useful rather than destructive.
            red *= 1.0F + temperature * 0.18F + tint * 0.06F;
            green *= 1.0F - tint * 0.12F;
            blue *= 1.0F - temperature * 0.18F + tint * 0.06F;

            red *= exposureFactor;
            green *= exposureFactor;
            blue *= exposureFactor;

            float luminance = 0.2126F * red + 0.7152F * green + 0.0722F * blue;
            const float shadowMask = 1.0F - smoothstep(0.05F, 0.62F, luminance);
            const float highlightMask = smoothstep(0.38F, 0.95F, luminance);

            const float shadowDelta = shadows >= 0.0F
                ? shadows * 0.34F * shadowMask * (1.0F - luminance)
                : shadows * 0.26F * shadowMask * luminance;
            const float highlightDelta = highlights >= 0.0F
                ? highlights * 0.26F * highlightMask * (1.0F - luminance)
                : highlights * 0.34F * highlightMask * luminance;
            red += shadowDelta + highlightDelta;
            green += shadowDelta + highlightDelta;
            blue += shadowDelta + highlightDelta;

            const float whiteMask = smoothstep(0.70F, 1.0F, luminance);
            const float blackMask = 1.0F - smoothstep(0.0F, 0.30F, luminance);
            const float endDelta = whites * 0.20F * whiteMask
                + blacks * 0.18F * blackMask;
            red += endDelta;
            green += endDelta;
            blue += endDelta;

            // Contrast and clarity (midtone-selective contrast).
            const float contrastFactor = contrast >= 0.0F
                ? 1.0F + contrast * 1.7F
                : 1.0F + contrast * 0.75F;
            red = (red - 0.5F) * contrastFactor + 0.5F;
            green = (green - 0.5F) * contrastFactor + 0.5F;
            blue = (blue - 0.5F) * contrastFactor + 0.5F;

            luminance = 0.2126F * red + 0.7152F * green + 0.0722F * blue;
            const float midMask = 1.0F - std::abs(2.0F * clamp01(luminance) - 1.0F);
            const float clarityFactor = 1.0F + clarity * 0.55F * midMask;
            red = luminance + (red - luminance) * clarityFactor;
            green = luminance + (green - luminance) * clarityFactor;
            blue = luminance + (blue - luminance) * clarityFactor;

            luminance = 0.2126F * red + 0.7152F * green + 0.0722F * blue;
            const float maxChannel = std::max({red, green, blue});
            const float minChannel = std::min({red, green, blue});
            const float existingSaturation = clamp01(maxChannel - minChannel);
            const float saturationFactor = std::max(0.0F, 1.0F + saturation);
            const float vibranceFactor = std::max(0.0F, 1.0F + vibrance * (1.0F - existingSaturation));
            const float colorFactor = saturationFactor * vibranceFactor;
            red = luminance + (red - luminance) * colorFactor;
            green = luminance + (green - luminance) * colorFactor;
            blue = luminance + (blue - luminance) * colorFactor;

            if (replacementAmount > 0.0F) {
                float replacementRed = 0.0F;
                float replacementGreen = 0.0F;
                float replacementBlue = 0.0F;
                hsvToRgb(replacementHue, replacementSaturation,
                         clamp01(std::max({red, green, blue})),
                         replacementRed, replacementGreen, replacementBlue);
                const float keep = 1.0F - replacementAmount;
                red = red * keep + replacementRed * replacementAmount;
                green = green * keep + replacementGreen * replacementAmount;
                blue = blue * keep + replacementBlue * replacementAmount;
            }

            pixel[0] = static_cast<uchar>(std::lround(clamp01(red) * 255.0F));
            pixel[1] = static_cast<uchar>(std::lround(clamp01(green) * 255.0F));
            pixel[2] = static_cast<uchar>(std::lround(clamp01(blue) * 255.0F));
        }
    }

    if (state.blur > 0.01) {
        const QSize originalSize = image.size();
        const double divisor = 1.0 + state.blur / 7.5;
        const QSize reducedSize(qMax(1, qRound(image.width() / divisor)),
                                qMax(1, qRound(image.height() / divisor)));
        image = image.scaled(reducedSize, Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation)
                    .scaled(originalSize, Qt::IgnoreAspectRatio,
                            Qt::SmoothTransformation);
    }

    if (!image.colorSpace().isValid())
        image.setColorSpace(QColorSpace(QColorSpace::SRgb));
    return withBorder(image, state, maxEdge);
}

QImage ImageProcessor::renderLayers(const QImage &source, const LayerStack &layers,
                                    int maxEdge)
{
    if (source.isNull())
        return {};

    const AdjustmentLayer *base = layers.baseLayer();
    const EditState baseState = base ? base->edits : EditState{};
    QImage current = orientedAndCropped(source, baseState, maxEdge)
                         .convertToFormat(QImage::Format_RGBA8888);
    if (current.isNull())
        return {};

    for (const AdjustmentLayer &layer : layers.layers()) {
        if (!layer.visible || layer.opacity <= 0.0)
            continue;
        EditState adjustments = layer.edits;
        adjustments.rotation = 0;
        adjustments.flipHorizontal = false;
        adjustments.flipVertical = false;
        adjustments.crop = QRectF(0.0, 0.0, 1.0, 1.0);
        adjustments.borderSize = 0.0;
        const QImage retouched = applyRetouchOperation(
            current, layer.retouchOperation);
        const QImage adjusted = render(retouched, adjustments, 0);
        const QImage mask = layer.mask.isGlobal()
            ? QImage{} : SmartMaskEngine::generate(current, layer.mask);
        current = blend(current, adjusted, layer.opacity, mask);
        drawTextOverlay(current, layer.textOverlay, layer.opacity);
        drawWatermarkOverlay(current, layer.watermarkOverlay, layer.opacity);
    }
    return withBorder(current, baseState, maxEdge);
}

EditState ImageProcessor::suggestAutoEnhance(const QImage &source)
{
    EditState result;
    if (source.isNull())
        return result;

    const QImage sample = source.scaled(640, 640, Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation)
                              .convertToFormat(QImage::Format_RGB888);
    std::array<qsizetype, 256> histogram{};
    double luminanceTotal = 0.0;
    qsizetype count = 0;
    for (int y = 0; y < sample.height(); ++y) {
        const uchar *line = sample.constScanLine(y);
        for (int x = 0; x < sample.width(); ++x) {
            const uchar *pixel = line + x * 3;
            const int luminance = qBound(0, qRound(0.2126 * pixel[0]
                + 0.7152 * pixel[1] + 0.0722 * pixel[2]), 255);
            ++histogram[static_cast<size_t>(luminance)];
            luminanceTotal += static_cast<double>(luminance) / 255.0;
            ++count;
        }
    }
    if (count == 0)
        return result;

    const double mean = luminanceTotal / static_cast<double>(count);
    result.exposure = qBound(-1.5, std::log2(0.46 / qMax(0.03, mean)), 1.5);

    const auto percentile = [&histogram, count](double requested) {
        const qsizetype target = qRound64(static_cast<double>(count) * requested);
        qsizetype cumulative = 0;
        for (int value = 0; value < 256; ++value) {
            cumulative += histogram[static_cast<size_t>(value)];
            if (cumulative >= target)
                return static_cast<double>(value) / 255.0;
        }
        return 1.0;
    };
    const double low = percentile(0.05);
    const double high = percentile(0.95);
    const double range = high - low;
    result.contrast = qBound(6.0, (0.72 - range) * 55.0, 26.0);
    result.shadows = qBound(8.0, (0.20 - low) * 120.0, 32.0);
    result.highlights = qBound(-32.0, -(high - 0.78) * 100.0, -6.0);
    result.whites = 6.0;
    result.blacks = -5.0;
    result.vibrance = 16.0;
    result.clarity = 8.0;
    return result;
}

QImage ImageProcessor::blend(const QImage &bottom, const QImage &top, double opacity,
                             const QImage &mask)
{
    if (bottom.isNull())
        return top;
    if (top.isNull() || opacity <= 0.0)
        return bottom;
    if (opacity >= 1.0 && mask.isNull())
        return top;

    QImage result = bottom.convertToFormat(QImage::Format_RGBA8888);
    const QImage overlay = top.convertToFormat(QImage::Format_RGBA8888);
    if (result.size() != overlay.size())
        return result;
    const QImage coverage = mask.isNull()
        ? QImage{} : mask.convertToFormat(QImage::Format_Grayscale8);
    if (!coverage.isNull() && coverage.size() != result.size())
        return result;

    const float layerAmount = static_cast<float>(qBound(0.0, opacity, 1.0));
    for (int y = 0; y < result.height(); ++y) {
        uchar *target = result.scanLine(y);
        const uchar *source = overlay.constScanLine(y);
        const uchar *maskLine = coverage.isNull() ? nullptr : coverage.constScanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            const int offset = x * 4;
            const float amount = layerAmount
                * (maskLine ? static_cast<float>(maskLine[x]) / 255.0F : 1.0F);
            const float inverse = 1.0F - amount;
            for (int channel = 0; channel < 4; ++channel) {
                target[offset + channel] = static_cast<uchar>(std::lround(
                    target[offset + channel] * inverse
                    + source[offset + channel] * amount));
            }
        }
    }
    return result;
}

} // namespace chroma
