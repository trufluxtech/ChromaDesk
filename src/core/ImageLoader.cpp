#include "core/ImageLoader.h"

#include <QByteArray>
#include <QColorSpace>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>

#ifdef CHROMADESK_HAS_LIBRAW
#include <libraw/libraw.h>
#endif

namespace chroma {
namespace {

const QStringList &rawExtensions()
{
    static const QStringList extensions = {
        QStringLiteral("3fr"), QStringLiteral("arw"), QStringLiteral("cr2"),
        QStringLiteral("cr3"), QStringLiteral("dng"), QStringLiteral("erf"),
        QStringLiteral("fff"), QStringLiteral("iiq"), QStringLiteral("kdc"),
        QStringLiteral("mef"), QStringLiteral("mos"), QStringLiteral("mrw"),
        QStringLiteral("nef"), QStringLiteral("nrw"), QStringLiteral("orf"),
        QStringLiteral("pef"), QStringLiteral("raf"), QStringLiteral("raw"),
        QStringLiteral("rw2"), QStringLiteral("rwl"), QStringLiteral("sr2"),
        QStringLiteral("srf"), QStringLiteral("srw"), QStringLiteral("x3f")
    };
    return extensions;
}

ImageLoadResult loadStandard(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull())
        return {{}, reader.errorString(), false};

    if (!image.colorSpace().isValid()) {
        image.setColorSpace(QColorSpace(QColorSpace::SRgb));
    } else if (image.colorSpace() != QColorSpace(QColorSpace::SRgb)) {
        const QImage converted = image.convertedToColorSpace(QColorSpace(QColorSpace::SRgb));
        if (!converted.isNull())
            image = converted;
    }
    return {image, {}, false};
}

#ifdef CHROMADESK_HAS_LIBRAW
ImageLoadResult loadRaw(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {{}, file.errorString(), true};
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty())
        return {{}, QStringLiteral("The RAW file is empty."), true};

    LibRaw decoder;
    decoder.imgdata.params.output_bps = 8;
    decoder.imgdata.params.use_camera_wb = 1;
    decoder.imgdata.params.no_auto_bright = 1;
    decoder.imgdata.params.output_color = 1; // sRGB

    int result = decoder.open_buffer(const_cast<char *>(bytes.constData()),
                                     static_cast<size_t>(bytes.size()));
    if (result != LIBRAW_SUCCESS)
        return {{}, QString::fromLatin1(libraw_strerror(result)), true};
    result = decoder.unpack();
    if (result != LIBRAW_SUCCESS)
        return {{}, QString::fromLatin1(libraw_strerror(result)), true};
    result = decoder.dcraw_process();
    if (result != LIBRAW_SUCCESS)
        return {{}, QString::fromLatin1(libraw_strerror(result)), true};

    int memoryError = LIBRAW_SUCCESS;
    libraw_processed_image_t *processed = decoder.dcraw_make_mem_image(&memoryError);
    if (!processed || memoryError != LIBRAW_SUCCESS) {
        if (processed)
            LibRaw::dcraw_clear_mem(processed);
        return {{}, QString::fromLatin1(libraw_strerror(memoryError)), true};
    }

    QImage image;
    if (processed->type == LIBRAW_IMAGE_BITMAP && processed->colors == 3 && processed->bits == 8) {
        const QImage view(processed->data, static_cast<int>(processed->width),
                          static_cast<int>(processed->height),
                          static_cast<int>(processed->width * processed->colors),
                          QImage::Format_RGB888);
        image = view.copy();
        image.setColorSpace(QColorSpace(QColorSpace::SRgb));
    }
    LibRaw::dcraw_clear_mem(processed);

    if (image.isNull())
        return {{}, QStringLiteral("LibRaw returned an unsupported pixel layout."), true};
    return {image, {}, true};
}
#endif

} // namespace

ImageLoadResult ImageLoader::load(const QString &path)
{
    if (isRawPath(path)) {
#ifdef CHROMADESK_HAS_LIBRAW
        return loadRaw(path);
#else
        return {{}, QStringLiteral("Camera RAW support requires LibRaw. Rebuild with CHROMADESK_ENABLE_RAW=ON after installing LibRaw."), true};
#endif
    }
    return loadStandard(path);
}

QImage ImageLoader::thumbnail(const QString &path, int edge)
{
    if (isRawPath(path)) {
        const auto result = load(path);
        return result ? result.image.scaled(edge, edge, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation) : QImage{};
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize original = reader.size();
    if (original.isValid())
        reader.setScaledSize(original.scaled(edge, edge, Qt::KeepAspectRatio));
    return reader.read();
}

QStringList ImageLoader::supportedNameFilters()
{
    QStringList extensions;
    for (const QByteArray &format : QImageReader::supportedImageFormats())
        extensions << QStringLiteral("*.%1").arg(QString::fromLatin1(format).toLower());
#ifdef CHROMADESK_HAS_LIBRAW
    for (const QString &extension : rawExtensions())
        extensions << QStringLiteral("*.%1").arg(extension);
#endif
    extensions.removeDuplicates();
    extensions.sort(Qt::CaseInsensitive);
    return extensions;
}

bool ImageLoader::isRawPath(const QString &path)
{
    return rawExtensions().contains(QFileInfo(path).suffix().toLower());
}

bool ImageLoader::rawSupportAvailable()
{
#ifdef CHROMADESK_HAS_LIBRAW
    return true;
#else
    return false;
#endif
}

} // namespace chroma
