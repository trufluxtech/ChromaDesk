#pragma once

#include "core/TextOverlay.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace chroma {

class TextOverlayDialog final : public QDialog {
    Q_OBJECT

public:
    explicit TextOverlayDialog(const TextOverlay &initial, QWidget *parent = nullptr);
    [[nodiscard]] TextOverlay overlay() const;

private slots:
    void chooseTextColor();
    void chooseOutlineColor();
    void validateAndAccept();

private:
    void updateColorButton(QPushButton *button, const QColor &color);

    QPlainTextEdit *m_text;
    QFontComboBox *m_font;
    QSpinBox *m_size;
    QComboBox *m_position;
    QCheckBox *m_bold;
    QCheckBox *m_italic;
    QCheckBox *m_outline;
    QPushButton *m_textColorButton;
    QPushButton *m_outlineColorButton;
    QColor m_textColor;
    QColor m_outlineColor;
};

} // namespace chroma
