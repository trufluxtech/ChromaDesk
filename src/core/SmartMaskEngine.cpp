#include "core/SmartMaskEngine.h"
#include "core/AppleVisionMaskEngine.h"

#include <QByteArrayView>
#include <QCache>
#include <QCryptographicHash>
#include <QMutex>
#include <QMutexLocker>
#include <QPoint>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <vector>

namespace chroma {
namespace {

float clamp01(float value)
{
    return std::clamp(value, 0.0F, 1.0F);
}

float smoothstep(float edge0, float edge1, float value)
{
    if (std::abs(edge1 - edge0) < 0.00001F)
        return value >= edge1 ? 1.0F : 0.0F;
    const float amount = clamp01((value - edge0) / (edge1 - edge0));
    return amount * amount * (3.0F - 2.0F * amount);
}

struct RgbMean {
    float red = 0.0F;
    float green = 0.0F;
    float blue = 0.0F;
};

RgbMean patchMean(const QImage &image, const QRect &area)
{
    RgbMean mean;
    const QRect bounded = area.intersected(image.rect());
    if (bounded.isEmpty())
        return mean;
    qsizetype count = 0;
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        const uchar *line = image.constScanLine(y);
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            const uchar *pixel = line + x * 3;
            mean.red += pixel[0];
            mean.green += pixel[1];
            mean.blue += pixel[2];
            ++count;
        }
    }
    if (count > 0) {
        mean.red /= static_cast<float>(count);
        mean.green /= static_cast<float>(count);
        mean.blue /= static_cast<float>(count);
    }
    return mean;
}

float colorDistance(const uchar *pixel, const RgbMean &mean)
{
    const float red = static_cast<float>(pixel[0]) - mean.red;
    const float green = static_cast<float>(pixel[1]) - mean.green;
    const float blue = static_cast<float>(pixel[2]) - mean.blue;
    return std::sqrt((red * red + green * green + blue * blue) / 3.0F) / 255.0F;
}

QImage boxBlur(const QImage &input, int radius)
{
    if (input.isNull() || radius <= 0)
        return input;
    QImage horizontal(input.size(), QImage::Format_Grayscale8);
    QImage result(input.size(), QImage::Format_Grayscale8);

    for (int y = 0; y < input.height(); ++y) {
        const uchar *source = input.constScanLine(y);
        uchar *target = horizontal.scanLine(y);
        int total = 0;
        for (int x = -radius; x <= radius; ++x)
            total += source[qBound(0, x, input.width() - 1)];
        for (int x = 0; x < input.width(); ++x) {
            target[x] = static_cast<uchar>(total / (radius * 2 + 1));
            total -= source[qBound(0, x - radius, input.width() - 1)];
            total += source[qBound(0, x + radius + 1, input.width() - 1)];
        }
    }

    for (int x = 0; x < input.width(); ++x) {
        int total = 0;
        for (int y = -radius; y <= radius; ++y)
            total += horizontal.constScanLine(qBound(0, y, input.height() - 1))[x];
        for (int y = 0; y < input.height(); ++y) {
            result.scanLine(y)[x] = static_cast<uchar>(total / (radius * 2 + 1));
            total -= horizontal.constScanLine(qBound(0, y - radius, input.height() - 1))[x];
            total += horizontal.constScanLine(qBound(0, y + radius + 1,
                                                     input.height() - 1))[x];
        }
    }
    return result;
}

QString imageFingerprint(const QImage &reference)
{
    const QImage sample = reference.scaled(96, 96, Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (int y = 0; y < sample.height(); ++y) {
        hash.addData(QByteArrayView(reinterpret_cast<const char *>(
            sample.constScanLine(y)), sample.width() * 3));
    }
    return QStringLiteral("%1x%2:%3").arg(reference.width()).arg(reference.height())
        .arg(QString::fromLatin1(hash.result().toHex()));
}

QImage cachedVisionForegroundMask(const QImage &reference)
{
    if (!AppleVisionMaskEngine::available())
        return {};
    static QCache<QString, QImage> cache(256 * 1024); // Cost is in KiB.
    static QMutex cacheMutex;
    const QString key = imageFingerprint(reference);
    {
        const QMutexLocker lock(&cacheMutex);
        if (const QImage *cached = cache.object(key))
            return *cached;
    }
    const QImage generated = AppleVisionMaskEngine::foregroundMask(reference);
    if (generated.isNull())
        return {};
    const int cost = qMax(1, static_cast<int>(qMin<qsizetype>(
        generated.sizeInBytes() / 1024, 256 * 1024)));
    {
        const QMutexLocker lock(&cacheMutex);
        cache.insert(key, new QImage(generated), cost);
    }
    return generated;
}

QImage subjectMask(const QImage &reference, double feather)
{
    const QImage small = reference.scaled(512, 512, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_RGB888);
    if (small.isNull())
        return {};

    const int patch = qMax(4, qRound(qMin(small.width(), small.height()) * 0.10));
    const std::array<RgbMean, 4> backgrounds = {
        patchMean(small, QRect(0, 0, patch, patch)),
        patchMean(small, QRect(small.width() - patch, 0, patch, patch)),
        patchMean(small, QRect(0, small.height() - patch, patch, patch)),
        patchMean(small, QRect(small.width() - patch, small.height() - patch,
                               patch, patch))
    };

    const int count = small.width() * small.height();
    std::vector<float> scores(static_cast<size_t>(count));
    double total = 0.0;
    double totalSquared = 0.0;
    for (int y = 0; y < small.height(); ++y) {
        const uchar *line = small.constScanLine(y);
        const float normalizedY = (static_cast<float>(y) + 0.5F) / small.height();
        for (int x = 0; x < small.width(); ++x) {
            const uchar *pixel = line + x * 3;
            float distance = 1.0F;
            for (const RgbMean &background : backgrounds)
                distance = std::min(distance, colorDistance(pixel, background));
            const float normalizedX = (static_cast<float>(x) + 0.5F) / small.width();
            const float radial = std::sqrt(
                std::pow((normalizedX - 0.5F) / 0.7071F, 2.0F)
                + std::pow((normalizedY - 0.5F) / 0.7071F, 2.0F));
            const float centrePrior = clamp01(1.0F - radial);
            const float score = clamp01(distance * 1.85F) * 0.78F
                + centrePrior * 0.22F;
            scores[static_cast<size_t>(y * small.width() + x)] = score;
            total += score;
            totalSquared += score * score;
        }
    }

    const float mean = static_cast<float>(total / count);
    const float variance = static_cast<float>(qMax(0.0,
        totalSquared / count - static_cast<double>(mean * mean)));
    const float deviation = std::sqrt(variance);
    const float threshold = std::clamp(mean + deviation * 0.12F, 0.20F, 0.62F);
    const float softness = 0.055F + static_cast<float>(feather) * 0.24F;

    QImage mask(small.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < small.height(); ++y) {
        uchar *line = mask.scanLine(y);
        for (int x = 0; x < small.width(); ++x) {
            const float score = scores[static_cast<size_t>(y * small.width() + x)];
            line[x] = static_cast<uchar>(std::lround(
                smoothstep(threshold - softness, threshold + softness, score) * 255.0F));
        }
    }

    mask = boxBlur(mask, qBound(1, qRound(2.0 + feather * 18.0), 12));
    return mask.scaled(reference.size(), Qt::IgnoreAspectRatio,
                       Qt::SmoothTransformation);
}

QImage accurateSubjectMask(const QImage &reference, double feather)
{
    QImage result = cachedVisionForegroundMask(reference);
    if (result.isNull())
        return subjectMask(reference, feather);
    result = result.convertToFormat(QImage::Format_Grayscale8);
    const int radius = qBound(0, qRound(feather * 12.0), 7);
    return boxBlur(result, radius);
}

QImage skyMask(const QImage &reference, double feather)
{
    const QImage small = reference.scaled(512, 512, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_RGB888);
    if (small.isNull())
        return {};

    const int width = small.width();
    const int height = small.height();
    const int pixelCount = width * height;
    std::vector<float> scores(static_cast<size_t>(pixelCount), 0.0F);

    // Estimate a sky likelihood from cool colour, bright neutral cloud tones,
    // and position. A connected-region pass then keeps only regions reachable
    // from the upper image boundary, which prevents similarly coloured objects
    // near the ground from being selected.
    for (int y = 0; y < height; ++y) {
        const uchar *line = small.constScanLine(y);
        const float normalizedY = (static_cast<float>(y) + 0.5F) / height;
        const float upperPrior = 1.0F - smoothstep(0.42F, 0.98F, normalizedY);
        for (int x = 0; x < width; ++x) {
            const uchar *pixel = line + x * 3;
            const float red = static_cast<float>(pixel[0]) / 255.0F;
            const float green = static_cast<float>(pixel[1]) / 255.0F;
            const float blue = static_cast<float>(pixel[2]) / 255.0F;
            const float maximum = std::max({red, green, blue});
            const float minimum = std::min({red, green, blue});
            const float saturation = maximum - minimum;
            const float luminance = 0.2126F * red + 0.7152F * green + 0.0722F * blue;

            const float coolSky = smoothstep(-0.04F, 0.16F, blue - red)
                * smoothstep(-0.10F, 0.10F, blue - green)
                * smoothstep(0.16F, 0.70F, luminance);
            const float brightCloud = smoothstep(0.55F, 0.92F, luminance)
                * (1.0F - smoothstep(0.06F, 0.32F, saturation));
            const float likelihood = std::max(coolSky, brightCloud * 0.72F)
                * (0.58F + upperPrior * 0.42F);
            scores[static_cast<size_t>(y * width + x)] = clamp01(likelihood);
        }
    }

    std::vector<uchar> connected(static_cast<size_t>(pixelCount), 0);
    std::queue<int> pending;
    const int seedRows = qMax(1, qRound(height * 0.08));
    for (int y = 0; y < seedRows; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = y * width + x;
            if (scores[static_cast<size_t>(index)] >= 0.32F) {
                connected[static_cast<size_t>(index)] = 1;
                pending.push(index);
            }
        }
    }

    const std::array<QPoint, 4> neighbours = {
        QPoint(-1, 0), QPoint(1, 0), QPoint(0, -1), QPoint(0, 1)
    };
    while (!pending.empty()) {
        const int index = pending.front();
        pending.pop();
        const int x = index % width;
        const int y = index / width;
        for (const QPoint &offset : neighbours) {
            const int nextX = x + offset.x();
            const int nextY = y + offset.y();
            if (nextX < 0 || nextX >= width || nextY < 0 || nextY >= height)
                continue;
            const int next = nextY * width + nextX;
            if (connected[static_cast<size_t>(next)] != 0
                || scores[static_cast<size_t>(next)] < 0.23F) {
                continue;
            }
            connected[static_cast<size_t>(next)] = 1;
            pending.push(next);
        }
    }

    QImage result(small.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        uchar *target = result.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const int index = y * width + x;
            const float amount = connected[static_cast<size_t>(index)] == 0
                ? 0.0F
                : smoothstep(0.20F, 0.62F, scores[static_cast<size_t>(index)]);
            target[x] = static_cast<uchar>(std::lround(amount * 255.0F));
        }
    }

    result = boxBlur(result, qBound(1, qRound(1.5 + feather * 20.0), 12));
    return result.scaled(reference.size(), Qt::IgnoreAspectRatio,
                         Qt::SmoothTransformation);
}

QImage colorRangeMask(const QImage &reference, const LayerMask &mask)
{
    const QImage image = reference.convertToFormat(QImage::Format_RGB888);
    QImage result(image.size(), QImage::Format_Grayscale8);
    const float targetRed = mask.targetColor.redF();
    const float targetGreen = mask.targetColor.greenF();
    const float targetBlue = mask.targetColor.blueF();
    const float tolerance = static_cast<float>(mask.colorTolerance);
    const float softness = qMax(0.005F, static_cast<float>(mask.feather) * 0.45F);
    for (int y = 0; y < image.height(); ++y) {
        const uchar *source = image.constScanLine(y);
        uchar *target = result.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            const uchar *pixel = source + x * 3;
            const float red = static_cast<float>(pixel[0]) / 255.0F - targetRed;
            const float green = static_cast<float>(pixel[1]) / 255.0F - targetGreen;
            const float blue = static_cast<float>(pixel[2]) / 255.0F - targetBlue;
            const float distance = std::sqrt((red * red + green * green + blue * blue) / 3.0F);
            const float amount = 1.0F - smoothstep(tolerance, tolerance + softness, distance);
            target[x] = static_cast<uchar>(std::lround(amount * 255.0F));
        }
    }
    return result;
}

QImage luminanceRangeMask(const QImage &reference, const LayerMask &mask)
{
    const QImage image = reference.convertToFormat(QImage::Format_RGB888);
    QImage result(image.size(), QImage::Format_Grayscale8);
    const float minimum = static_cast<float>(mask.luminanceMinimum);
    const float maximum = static_cast<float>(mask.luminanceMaximum);
    const float softness = qMax(0.002F, static_cast<float>(mask.feather));
    for (int y = 0; y < image.height(); ++y) {
        const uchar *source = image.constScanLine(y);
        uchar *target = result.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            const uchar *pixel = source + x * 3;
            const float luminance = (0.2126F * pixel[0] + 0.7152F * pixel[1]
                                     + 0.0722F * pixel[2]) / 255.0F;
            const float amount = smoothstep(minimum - softness, minimum, luminance)
                * (1.0F - smoothstep(maximum, maximum + softness, luminance));
            target[x] = static_cast<uchar>(std::lround(amount * 255.0F));
        }
    }
    return result;
}

QImage shapeMask(const QSize &size, const LayerMask &mask)
{
    QImage result(size, QImage::Format_Grayscale8);
    const QRectF bounds = mask.bounds.normalized();
    const float feather = qMax(0.00001F, static_cast<float>(mask.feather));
    for (int y = 0; y < size.height(); ++y) {
        uchar *target = result.scanLine(y);
        const float normalizedY = (static_cast<float>(y) + 0.5F) / size.height();
        for (int x = 0; x < size.width(); ++x) {
            const float normalizedX = (static_cast<float>(x) + 0.5F) / size.width();
            float signedDistance = 0.0F;
            if (mask.type == LayerMaskType::Ellipse) {
                const float radiusX = qMax(0.0001F, static_cast<float>(bounds.width() / 2.0));
                const float radiusY = qMax(0.0001F, static_cast<float>(bounds.height() / 2.0));
                const float dx = (normalizedX - static_cast<float>(bounds.center().x())) / radiusX;
                const float dy = (normalizedY - static_cast<float>(bounds.center().y())) / radiusY;
                signedDistance = (1.0F - std::sqrt(dx * dx + dy * dy))
                    * std::min(radiusX, radiusY);
            } else {
                const float outsideX = std::max({static_cast<float>(bounds.left()) - normalizedX,
                    0.0F, normalizedX - static_cast<float>(bounds.right())});
                const float outsideY = std::max({static_cast<float>(bounds.top()) - normalizedY,
                    0.0F, normalizedY - static_cast<float>(bounds.bottom())});
                if (outsideX > 0.0F || outsideY > 0.0F) {
                    signedDistance = -std::sqrt(outsideX * outsideX + outsideY * outsideY);
                } else {
                    signedDistance = std::min({normalizedX - static_cast<float>(bounds.left()),
                        static_cast<float>(bounds.right()) - normalizedX,
                        normalizedY - static_cast<float>(bounds.top()),
                        static_cast<float>(bounds.bottom()) - normalizedY});
                }
            }
            const float amount = mask.feather <= 0.0
                ? (signedDistance >= 0.0F ? 1.0F : 0.0F)
                : smoothstep(-feather, feather, signedDistance);
            target[x] = static_cast<uchar>(std::lround(amount * 255.0F));
        }
    }
    return result;
}

} // namespace

QImage SmartMaskEngine::generate(const QImage &reference, const LayerMask &mask)
{
    if (reference.isNull())
        return {};
    switch (mask.type) {
    case LayerMaskType::AiSubject:
        return accurateSubjectMask(reference, mask.feather);
    case LayerMaskType::AiBackground: {
        QImage result = accurateSubjectMask(reference, mask.feather);
        for (int y = 0; y < result.height(); ++y) {
            uchar *line = result.scanLine(y);
            for (int x = 0; x < result.width(); ++x)
                line[x] = static_cast<uchar>(255 - line[x]);
        }
        return result;
    }
    case LayerMaskType::AiSky: {
        QImage result = skyMask(reference, mask.feather);
        const QImage foreground = cachedVisionForegroundMask(reference);
        if (!foreground.isNull()) {
            const QImage scaledForeground = foreground.size() == result.size()
                ? foreground.convertToFormat(QImage::Format_Grayscale8)
                : foreground.scaled(result.size(), Qt::IgnoreAspectRatio,
                    Qt::SmoothTransformation).convertToFormat(QImage::Format_Grayscale8);
            for (int y = 0; y < result.height(); ++y) {
                uchar *sky = result.scanLine(y);
                const uchar *subject = scaledForeground.constScanLine(y);
                for (int x = 0; x < result.width(); ++x) {
                    sky[x] = static_cast<uchar>((static_cast<int>(sky[x])
                        * (255 - static_cast<int>(subject[x]))) / 255);
                }
            }
        }
        return result;
    }
    case LayerMaskType::ColorRange:
        return colorRangeMask(reference, mask);
    case LayerMaskType::LuminanceRange:
        return luminanceRangeMask(reference, mask);
    case LayerMaskType::Rectangle:
    case LayerMaskType::Ellipse:
        return shapeMask(reference.size(), mask);
    case LayerMaskType::None:
        break;
    }
    QImage result(reference.size(), QImage::Format_Grayscale8);
    result.fill(255);
    return result;
}

QString SmartMaskEngine::engineName(LayerMaskType type)
{
    switch (type) {
    case LayerMaskType::AiSubject:
    case LayerMaskType::AiBackground:
        return highAccuracyAvailable()
            ? QStringLiteral("Apple Vision · accurate foreground segmentation")
            : QStringLiteral("Local saliency fallback");
    case LayerMaskType::AiSky:
        return highAccuracyAvailable()
            ? QStringLiteral("Sky classifier · Vision foreground exclusion")
            : QStringLiteral("Local connected-sky classifier");
    default:
        return QStringLiteral("Local deterministic mask");
    }
}

bool SmartMaskEngine::highAccuracyAvailable()
{
    return AppleVisionMaskEngine::available();
}

} // namespace chroma
