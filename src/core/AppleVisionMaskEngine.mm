#include "core/AppleVisionMaskEngine.h"

#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <ImageIO/ImageIO.h>
#import <Vision/Vision.h>

#include <QImage>

#include <algorithm>
#include <cmath>

namespace chroma {
namespace {

CGImageRef makeCgImage(const QImage &source)
{
    const QImage rgba = source.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.isNull())
        return nullptr;
    const qsizetype byteCount = rgba.sizeInBytes();
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, rgba.constBits(), byteCount);
    if (!data)
        return nullptr;
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CFRelease(data);
    if (!provider)
        return nullptr;
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    const CGBitmapInfo bitmapInfo = static_cast<CGBitmapInfo>(
        kCGImageAlphaLast | kCGBitmapByteOrderDefault);
    CGImageRef image = CGImageCreate(rgba.width(), rgba.height(), 8, 32,
        rgba.bytesPerLine(), colorSpace, bitmapInfo, provider, nullptr,
        false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
    return image;
}

QImage imageFromMaskBuffer(CVPixelBufferRef buffer)
{
    if (!buffer || CVPixelBufferLockBaseAddress(buffer,
            kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) {
        return {};
    }

    const int width = static_cast<int>(CVPixelBufferGetWidth(buffer));
    const int height = static_cast<int>(CVPixelBufferGetHeight(buffer));
    const size_t stride = CVPixelBufferGetBytesPerRow(buffer);
    const OSType format = CVPixelBufferGetPixelFormatType(buffer);
    const auto *base = static_cast<const uchar *>(CVPixelBufferGetBaseAddress(buffer));
    QImage result(width, height, QImage::Format_Grayscale8);

    if (format == kCVPixelFormatType_OneComponent8) {
        uchar maximum = 0;
        for (int y = 0; y < height; ++y) {
            std::copy_n(base + static_cast<size_t>(y) * stride, width,
                        result.scanLine(y));
            maximum = std::max(maximum, *std::max_element(
                result.constScanLine(y), result.constScanLine(y) + width));
        }
        // Some OS revisions expose a binary 0/1 buffer rather than 0/255.
        if (maximum == 1) {
            for (int y = 0; y < height; ++y) {
                uchar *line = result.scanLine(y);
                for (int x = 0; x < width; ++x)
                    line[x] = line[x] == 0 ? 0 : 255;
            }
        }
    } else if (format == kCVPixelFormatType_OneComponent32Float) {
        for (int y = 0; y < height; ++y) {
            const auto *source = reinterpret_cast<const float *>(
                base + static_cast<size_t>(y) * stride);
            uchar *target = result.scanLine(y);
            for (int x = 0; x < width; ++x) {
                target[x] = static_cast<uchar>(std::lround(
                    std::clamp(source[x], 0.0F, 1.0F) * 255.0F));
            }
        }
    } else {
        result = {};
    }

    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    return result;
}

} // namespace

bool AppleVisionMaskEngine::available()
{
    if (@available(macOS 14.0, *))
        return true;
    return false;
}

QImage AppleVisionMaskEngine::foregroundMask(const QImage &reference)
{
    if (reference.isNull() || !available())
        return {};

    @autoreleasepool {
        CGImageRef image = makeCgImage(reference);
        if (!image)
            return {};

        VNImageRequestHandler *handler = [[VNImageRequestHandler alloc]
            initWithCGImage:image
            orientation:kCGImagePropertyOrientationUp
            options:@{}];
        CGImageRelease(image);
        VNGenerateForegroundInstanceMaskRequest *request =
            [[VNGenerateForegroundInstanceMaskRequest alloc] init];
        NSError *error = nil;
        const BOOL succeeded = [handler performRequests:@[request] error:&error];
        if (!succeeded || error || request.results.count == 0)
            return {};

        VNInstanceMaskObservation *observation = request.results.firstObject;
        if (observation.allInstances.count == 0)
            return {};
        CVPixelBufferRef buffer = [observation
            generateScaledMaskForImageForInstances:observation.allInstances
            fromRequestHandler:handler
            error:&error];
        if (!buffer || error)
            return {};

        QImage result = imageFromMaskBuffer(buffer);
        if (!result.isNull() && result.size() != reference.size()) {
            result = result.scaled(reference.size(), Qt::IgnoreAspectRatio,
                                   Qt::SmoothTransformation);
        }
        return result;
    }
}

} // namespace chroma
