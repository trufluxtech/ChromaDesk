#include "ui/ThemeManager.h"

#include <QApplication>
#include <QFile>

namespace chroma {

const QList<UiThemeOption> &ThemeManager::options()
{
    static const QList<UiThemeOption> result = {
        {UiTheme::Gray, QStringLiteral("gray"), QStringLiteral("Gray"),
         QStringLiteral("Balanced neutral dark workspace")},
        {UiTheme::HighContrast, QStringLiteral("high-contrast"),
         QStringLiteral("High Contrast"),
         QStringLiteral("Black workspace, white text and yellow focus accents")},
        {UiTheme::Colorful, QStringLiteral("colorful"), QStringLiteral("Colorful"),
         QStringLiteral("Deep blue workspace with cyan and violet accents")}
    };
    return result;
}

UiTheme ThemeManager::fromKey(const QString &key)
{
    for (const auto &option : options()) {
        if (option.key == key)
            return option.theme;
    }
    return UiTheme::Gray;
}

QString ThemeManager::key(UiTheme theme)
{
    for (const auto &option : options()) {
        if (option.theme == theme)
            return option.key;
    }
    return QStringLiteral("gray");
}

QString ThemeManager::styleSheet(UiTheme theme)
{
    QFile file(QStringLiteral(":/chromadesk/theme.qss"));
    QString style;
    if (file.open(QIODevice::ReadOnly))
        style = QString::fromUtf8(file.readAll());

    if (theme == UiTheme::HighContrast) {
        style += QStringLiteral(R"(
QMainWindow, QDialog, QWidget { background: #000000; color: #ffffff; }
QToolBar, QMenuBar, QMenu, QStatusBar, QDockWidget::title { background: #050505; color: #ffffff; border-color: #ffffff; }
QToolBar QToolButton, QPushButton { background: #000000; color: #ffffff; border: 2px solid #ffffff; }
QToolBar QToolButton:hover, QPushButton:hover, QMenu::item:selected { background: #ffff00; color: #000000; }
QToolBar QToolButton:checked, QPushButton:checked, QPushButton#primaryButton { background: #ffff00; border-color: #ffff00; color: #000000; }
QScrollArea, QListWidget, QTreeView, QTabWidget::pane { background: #000000; border-color: #ffffff; }
QTabBar::tab { background: #050505; color: #ffffff; border-color: #ffffff; }
QTabBar::tab:selected { background: #ffff00; color: #000000; border-color: #ffff00; }
QListWidget::item:selected { background: #242400; border: 2px solid #ffff00; }
QListWidget#layerList::item:selected { background: #242400; border: 1px dashed #ffff00; }
QGroupBox { border-color: #ffffff; }
QLabel#mutedLabel, QLabel#sectionLabel { color: #ffffff; }
QSlider::groove:horizontal { background: #ffffff; }
QSlider::sub-page:horizontal, QSlider::handle:horizontal { background: #ffff00; }
QDoubleSpinBox, QSpinBox, QComboBox, QLineEdit, QPlainTextEdit { background: #000000; color: #ffffff; border: 2px solid #ffffff; selection-background-color: #ffff00; selection-color: #000000; }
QToolTip { background: #000000; color: #ffffff; border: 2px solid #ffff00; }
)");
    } else if (theme == UiTheme::Colorful) {
        style += QStringLiteral(R"(
QMainWindow, QDialog, QWidget { background: #0c1022; color: #f2f5ff; }
QToolBar, QMenuBar, QMenu, QStatusBar, QDockWidget::title { background: #151a35; color: #f2f5ff; border-color: #303a70; }
QToolBar QToolButton, QPushButton { background: #20284d; color: #f5f7ff; border-color: #45549a; }
QToolBar QToolButton:hover, QPushButton:hover { background: #304178; border-color: #55d9ff; }
QToolBar QToolButton:checked, QPushButton:checked, QPushButton#primaryButton { background: #7447e8; border-color: #a887ff; color: #ffffff; }
QMenu::item:selected { background: #7447e8; }
QScrollArea, QListWidget, QTreeView, QTabWidget::pane { background: #111631; border-color: #303a70; }
QTabBar::tab { background: #171d3b; color: #aeb9e7; border-color: #303a70; }
QTabBar::tab:selected { background: #37265f; color: #ffffff; border-color: #55d9ff; }
QListWidget::item:selected { background: #293668; border: 1px solid #55d9ff; }
QListWidget#layerList::item:selected { background: #24345c; border: 1px dashed #55d9ff; }
QGroupBox { border-color: #303a70; }
QLabel#mutedLabel { color: #aeb9d8; }
QLabel#sectionLabel { color: #8ee8ff; }
QSlider::groove:horizontal { background: #35406d; }
QSlider::sub-page:horizontal { background: #38c8f2; }
QSlider::handle:horizontal { background: #ff77bd; }
QDoubleSpinBox, QSpinBox, QComboBox, QLineEdit, QPlainTextEdit { background: #171d3b; color: #f2f5ff; border-color: #45549a; selection-background-color: #7447e8; }
QToolTip { background: #20284d; color: #ffffff; border-color: #55d9ff; }
)");
    }
    return style;
}

void ThemeManager::apply(QApplication &application, UiTheme theme)
{
    application.setStyleSheet(styleSheet(theme));
}

} // namespace chroma
