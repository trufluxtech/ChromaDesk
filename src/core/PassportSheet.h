#pragma once

#include <QImage>
#include <QSizeF>

namespace chroma {

struct PassportSheetOptions {
    QSizeF paperMillimetres = QSizeF(101.6, 152.4); // 4 x 6 inches.
    QSizeF photoMillimetres = QSizeF(35.0, 45.0);
    double marginMillimetres = 5.0;
    double gapMillimetres = 3.0;
    int dpi = 300;

    [[nodiscard]] static PassportSheetOptions fourBySix();
    [[nodiscard]] static PassportSheetOptions a4();
};

class PassportSheet final {
public:
    [[nodiscard]] static QImage create(const QImage &source,
                                       const PassportSheetOptions &options,
                                       int *copyCount = nullptr);
};

} // namespace chroma
