#include "ui/TextOverlayDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <array>
#include <limits>

namespace chroma {
namespace {

const std::array<QPointF, 9> positions = {
    QPointF(0.1, 0.1), QPointF(0.5, 0.1), QPointF(0.9, 0.1),
    QPointF(0.1, 0.5), QPointF(0.5, 0.5), QPointF(0.9, 0.5),
    QPointF(0.1, 0.9), QPointF(0.5, 0.9), QPointF(0.9, 0.9)
};

int nearestPositionIndex(const QPointF &requested)
{
    int bestIndex = 0;
    double bestDistance = std::numeric_limits<double>::max();
    for (int index = 0; index < static_cast<int>(positions.size()); ++index) {
        const QPointF delta = requested - positions.at(static_cast<size_t>(index));
        const double distance = delta.x() * delta.x() + delta.y() * delta.y();
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
        }
    }
    return bestIndex;
}

} // namespace

TextOverlayDialog::TextOverlayDialog(const TextOverlay &initial, QWidget *parent)
    : QDialog(parent), m_text(new QPlainTextEdit(this)),
      m_font(new QFontComboBox(this)), m_size(new QSpinBox(this)),
      m_position(new QComboBox(this)), m_bold(new QCheckBox(QStringLiteral("Bold"), this)),
      m_italic(new QCheckBox(QStringLiteral("Italic"), this)),
      m_outline(new QCheckBox(QStringLiteral("Outline"), this)),
      m_textColorButton(new QPushButton(this)),
      m_outlineColorButton(new QPushButton(this)), m_textColor(initial.color),
      m_outlineColor(initial.outlineColor)
{
    setWindowTitle(initial.isEnabled() ? QStringLiteral("Edit Text Layer")
                                       : QStringLiteral("Add Text Layer"));
    setMinimumWidth(480);

    m_text->setPlainText(initial.text);
    m_text->setPlaceholderText(QStringLiteral("Type the text to place on the photo"));
    m_text->setMaximumHeight(105);
    m_font->setCurrentFont(QFont(initial.fontFamily));
    m_size->setRange(1, 30);
    m_size->setValue(qRound(initial.size * 100.0));
    m_size->setSuffix(QStringLiteral("% of image height"));
    m_position->addItems({
        QStringLiteral("Top left"), QStringLiteral("Top centre"),
        QStringLiteral("Top right"), QStringLiteral("Middle left"),
        QStringLiteral("Centre"), QStringLiteral("Middle right"),
        QStringLiteral("Bottom left"), QStringLiteral("Bottom centre"),
        QStringLiteral("Bottom right")
    });
    m_position->setCurrentIndex(nearestPositionIndex(initial.position));
    m_bold->setChecked(initial.bold);
    m_italic->setChecked(initial.italic);
    m_outline->setChecked(initial.outline);
    updateColorButton(m_textColorButton, m_textColor);
    updateColorButton(m_outlineColorButton, m_outlineColor);
    connect(m_textColorButton, &QPushButton::clicked,
            this, &TextOverlayDialog::chooseTextColor);
    connect(m_outlineColorButton, &QPushButton::clicked,
            this, &TextOverlayDialog::chooseOutlineColor);
    connect(m_outline, &QCheckBox::toggled,
            m_outlineColorButton, &QPushButton::setEnabled);
    m_outlineColorButton->setEnabled(initial.outline);

    auto *styleRow = new QWidget(this);
    auto *styleLayout = new QHBoxLayout(styleRow);
    styleLayout->setContentsMargins(0, 0, 0, 0);
    styleLayout->addWidget(m_bold);
    styleLayout->addWidget(m_italic);
    styleLayout->addWidget(m_outline);
    styleLayout->addStretch();

    auto *colorRow = new QWidget(this);
    auto *colorLayout = new QHBoxLayout(colorRow);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->addWidget(new QLabel(QStringLiteral("Text"), colorRow));
    colorLayout->addWidget(m_textColorButton, 1);
    colorLayout->addWidget(new QLabel(QStringLiteral("Outline"), colorRow));
    colorLayout->addWidget(m_outlineColorButton, 1);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Text"), m_text);
    form->addRow(QStringLiteral("Font"), m_font);
    form->addRow(QStringLiteral("Size"), m_size);
    form->addRow(QStringLiteral("Position"), m_position);
    form->addRow(QStringLiteral("Style"), styleRow);
    form->addRow(QStringLiteral("Colours"), colorRow);

    auto *note = new QLabel(QStringLiteral(
        "Text is stored as a nondestructive layer. Use the Layers tab to change its opacity, order, visibility, or name."),
        this);
    note->setWordWrap(true);
    note->setObjectName(QStringLiteral("mutedLabel"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save,
                                         this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &TextOverlayDialog::validateAndAccept);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(note);
    layout->addWidget(buttons);
}

TextOverlay TextOverlayDialog::overlay() const
{
    TextOverlay result;
    result.text = m_text->toPlainText().trimmed();
    result.fontFamily = m_font->currentFont().family();
    result.color = m_textColor;
    result.outlineColor = m_outlineColor;
    result.size = static_cast<double>(m_size->value()) / 100.0;
    result.position = positions.at(static_cast<size_t>(m_position->currentIndex()));
    result.bold = m_bold->isChecked();
    result.italic = m_italic->isChecked();
    result.outline = m_outline->isChecked();
    return result;
}

void TextOverlayDialog::chooseTextColor()
{
    const QColor selected = QColorDialog::getColor(m_textColor, this,
        QStringLiteral("Choose text colour"), QColorDialog::ShowAlphaChannel);
    if (selected.isValid()) {
        m_textColor = selected;
        updateColorButton(m_textColorButton, selected);
    }
}

void TextOverlayDialog::chooseOutlineColor()
{
    const QColor selected = QColorDialog::getColor(m_outlineColor, this,
        QStringLiteral("Choose outline colour"), QColorDialog::ShowAlphaChannel);
    if (selected.isValid()) {
        m_outlineColor = selected;
        updateColorButton(m_outlineColorButton, selected);
    }
}

void TextOverlayDialog::validateAndAccept()
{
    if (m_text->toPlainText().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Text Layer"),
                             QStringLiteral("Enter text before saving the layer."));
        return;
    }
    accept();
}

void TextOverlayDialog::updateColorButton(QPushButton *button, const QColor &color)
{
    button->setText(color.name(QColor::HexArgb).toUpper());
    const QColor foreground = color.lightnessF() < 0.5 ? Qt::white : Qt::black;
    button->setStyleSheet(QStringLiteral("background:%1;color:%2;")
        .arg(color.name(QColor::HexArgb), foreground.name(QColor::HexRgb)));
}

} // namespace chroma
