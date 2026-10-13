#pragma once

#include <QList>
#include <QString>

class QApplication;

namespace chroma {

enum class UiTheme {
    Gray,
    HighContrast,
    Colorful
};

struct UiThemeOption {
    UiTheme theme;
    QString key;
    QString label;
    QString description;
};

class ThemeManager final {
public:
    [[nodiscard]] static const QList<UiThemeOption> &options();
    [[nodiscard]] static UiTheme fromKey(const QString &key);
    [[nodiscard]] static QString key(UiTheme theme);
    [[nodiscard]] static QString styleSheet(UiTheme theme);
    static void apply(QApplication &application, UiTheme theme);
};

} // namespace chroma
