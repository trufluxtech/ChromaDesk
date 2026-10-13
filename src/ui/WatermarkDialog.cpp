#include "ui/WatermarkDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace chroma {

WatermarkDialog::WatermarkDialog(const WatermarkOverlay &initial, QWidget *parent)
    : QDialog(parent), m_type(new QComboBox(this)),
      m_contentStack(new QStackedWidget(this)), m_text(new QLineEdit(this)),
      m_imagePath(new QLineEdit(this)), m_font(new QFontComboBox(this)),
      m_size(new QSpinBox(this)), m_transparency(new QSpinBox(this)),
      m_bold(new QCheckBox(QStringLiteral("Bold"), this)),
      m_outline(new QCheckBox(QStringLiteral("Outline"), this)),
      m_colorButton(new QPushButton(this)), m_color(initial.color),
      m_position(initial.position)
{
    setWindowTitle(initial.isEnabled() ? QStringLiteral("Edit Watermark")
                                       : QStringLiteral("Add Watermark"));
    setMinimumWidth(520);
    m_type->addItem(QStringLiteral("Text Watermark"),
                    static_cast<int>(WatermarkType::Text));
    m_type->addItem(QStringLiteral("Image Watermark"),
                    static_cast<int>(WatermarkType::Image));
    m_type->setCurrentIndex(initial.type == WatermarkType::Image ? 1 : 0);

    auto *textPage = new QWidget(m_contentStack);
    auto *textForm = new QFormLayout(textPage);
    m_text->setText(initial.text);
    m_text->setPlaceholderText(QStringLiteral("Copyright, studio name, or caption"));
    m_font->setCurrentFont(QFont(initial.fontFamily));
    m_bold->setChecked(initial.bold);
    m_outline->setChecked(initial.outline);
    auto *textStyle = new QWidget(textPage);
    auto *textStyleLayout = new QHBoxLayout(textStyle);
    textStyleLayout->setContentsMargins(0, 0, 0, 0);
    textStyleLayout->addWidget(m_bold);
    textStyleLayout->addWidget(m_outline);
    textStyleLayout->addStretch();
    textForm->addRow(QStringLiteral("Text"), m_text);
    textForm->addRow(QStringLiteral("Font"), m_font);
    textForm->addRow(QStringLiteral("Style"), textStyle);
    textForm->addRow(QStringLiteral("Colour"), m_colorButton);

    auto *imagePage = new QWidget(m_contentStack);
    auto *imageLayout = new QHBoxLayout(imagePage);
    m_imagePath->setText(initial.imagePath);
    m_imagePath->setPlaceholderText(QStringLiteral("Choose a PNG, JPEG, TIFF, or WebP image"));
    auto *browse = new QPushButton(QStringLiteral("Browse…"), imagePage);
    connect(browse, &QPushButton::clicked, this, &WatermarkDialog::browseImage);
    imageLayout->addWidget(m_imagePath, 1);
    imageLayout->addWidget(browse);
    m_contentStack->addWidget(textPage);
    m_contentStack->addWidget(imagePage);

    m_size->setRange(1, 50);
    m_size->setValue(qRound(initial.size * 100.0));
    m_size->setSuffix(QStringLiteral("%"));
    m_transparency->setRange(0, 100);
    m_transparency->setValue(qRound((1.0 - initial.opacity) * 100.0));
    m_transparency->setSuffix(QStringLiteral("% transparent"));
    updateColorButton();

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Type"), m_type);
    form->addRow(QStringLiteral("Content"), m_contentStack);
    form->addRow(QStringLiteral("Size"), m_size);
    form->addRow(QStringLiteral("Transparency"), m_transparency);

    auto *note = new QLabel(QStringLiteral(
        "After saving, select the watermark layer and drag it freely across the photo. Layer opacity, visibility, order, and undo/redo remain available."), this);
    note->setWordWrap(true);
    note->setObjectName(QStringLiteral("mutedLabel"));
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Save, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &WatermarkDialog::validateAndAccept);
    connect(m_type, &QComboBox::currentIndexChanged,
            this, &WatermarkDialog::updateTypePresentation);
    connect(m_colorButton, &QPushButton::clicked,
            this, &WatermarkDialog::chooseTextColor);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(note);
    layout->addWidget(buttons);
    updateTypePresentation();
}

WatermarkOverlay WatermarkDialog::overlay() const
{
    WatermarkOverlay result;
    result.type = static_cast<WatermarkType>(m_type->currentData().toInt());
    result.text = m_text->text().trimmed();
    const QString imagePath = m_imagePath->text().trimmed();
    result.imagePath = imagePath.isEmpty() ? QString()
                                           : QFileInfo(imagePath).absoluteFilePath();
    result.fontFamily = m_font->currentFont().family();
    result.color = m_color;
    result.position = m_position;
    result.size = static_cast<double>(m_size->value()) / 100.0;
    result.opacity = 1.0 - static_cast<double>(m_transparency->value()) / 100.0;
    result.bold = m_bold->isChecked();
    result.outline = m_outline->isChecked();
    return result;
}

void WatermarkDialog::browseImage()
{
    const QString path = QFileDialog::getOpenFileName(this,
        QStringLiteral("Choose Watermark Image"), m_imagePath->text(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.tif *.tiff *.webp *.bmp);;All files (*)"));
    if (!path.isEmpty())
        m_imagePath->setText(path);
}

void WatermarkDialog::chooseTextColor()
{
    const QColor selected = QColorDialog::getColor(m_color, this,
        QStringLiteral("Choose Watermark Colour"), QColorDialog::ShowAlphaChannel);
    if (selected.isValid()) {
        m_color = selected;
        updateColorButton();
    }
}

void WatermarkDialog::validateAndAccept()
{
    const auto type = static_cast<WatermarkType>(m_type->currentData().toInt());
    if (type == WatermarkType::Text && m_text->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Watermark"),
                             QStringLiteral("Enter watermark text."));
        return;
    }
    if (type == WatermarkType::Image) {
        QImageReader reader(m_imagePath->text().trimmed());
        if (!QFileInfo::exists(m_imagePath->text().trimmed()) || !reader.canRead()) {
            QMessageBox::warning(this, QStringLiteral("Watermark"),
                                 QStringLiteral("Choose a readable image file."));
            return;
        }
    }
    accept();
}

void WatermarkDialog::updateTypePresentation()
{
    const bool image = static_cast<WatermarkType>(m_type->currentData().toInt())
        == WatermarkType::Image;
    m_contentStack->setCurrentIndex(image ? 1 : 0);
    m_size->setToolTip(image
        ? QStringLiteral("Width as a percentage of the photo width")
        : QStringLiteral("Text height as a percentage of the photo height"));
}

void WatermarkDialog::updateColorButton()
{
    m_colorButton->setText(m_color.name(QColor::HexArgb).toUpper());
    const QColor foreground = m_color.lightnessF() < 0.5 ? Qt::white : Qt::black;
    m_colorButton->setStyleSheet(QStringLiteral("background:%1;color:%2;")
        .arg(m_color.name(QColor::HexArgb), foreground.name(QColor::HexRgb)));
}

} // namespace chroma
