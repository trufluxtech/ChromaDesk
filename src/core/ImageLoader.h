#pragma once

#include <QImage>
#include <QString>
#include <QStringList>

namespace chroma {

struct ImageLoadResult {
    QImage image;
    QString error;
    bool fromRaw = false;

    [[nodiscard]] explicit operator bool() const { return !image.isNull(); }
};

class ImageLoader final {
public:
    [[nodiscard]] static ImageLoadResult load(const QString &path);
    [[nodiscard]] static QImage thumbnail(const QString &path, int edge = 280);
    [[nodiscard]] static QStringList supportedNameFilters();
    [[nodiscard]] static bool isRawPath(const QString &path);
    [[nodiscard]] static bool rawSupportAvailable();
};

} // namespace chroma
