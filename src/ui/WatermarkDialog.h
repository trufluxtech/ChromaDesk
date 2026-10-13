#pragma once

#include "core/WatermarkOverlay.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;

namespace chroma {

class WatermarkDialog final : public QDialog {
    Q_OBJECT

public:
    explicit WatermarkDialog(const WatermarkOverlay &initial,
                             QWidget *parent = nullptr);
    [[nodiscard]] WatermarkOverlay overlay() const;

private slots:
    void browseImage();
    void chooseTextColor();
    void validateAndAccept();
    void updateTypePresentation();

private:
    void updateColorButton();

    QComboBox *m_type;
    QStackedWidget *m_contentStack;
    QLineEdit *m_text;
    QLineEdit *m_imagePath;
    QFontComboBox *m_font;
    QSpinBox *m_size;
    QSpinBox *m_transparency;
    QCheckBox *m_bold;
    QCheckBox *m_outline;
    QPushButton *m_colorButton;
    QColor m_color;
    QPointF m_position;
};

} // namespace chroma
