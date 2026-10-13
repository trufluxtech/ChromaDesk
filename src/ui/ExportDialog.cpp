#include "ui/ExportDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace chroma {
namespace {

QByteArray normalizedFormat(const QString &suffix)
{
    const QByteArray format = suffix.toLower().toLatin1();
    if (format == QByteArray("jpeg"))
        return QByteArray("jpg");
    if (format == QByteArray("tif"))
        return QByteArray("tiff");
    if (format == QByteArray("jpg") || format == QByteArray("png")
        || format == QByteArray("tiff")) {
        return format;
    }
    return QByteArray("jpg");
}

QString formatFilter(const QByteArray &format)
{
    if (format == QByteArray("jpg"))
        return QStringLiteral("JPEG images (*.jpg *.jpeg)");
    if (format == QByteArray("tiff"))
        return QStringLiteral("TIFF images (*.tif *.tiff)");
    return QStringLiteral("PNG images (*.png)");
}

Qt::CaseSensitivity pathCaseSensitivity()
{
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

} // namespace

ExportDialog::ExportDialog(const QString &sourcePath, ExportMode mode, QWidget *parent)
    : QDialog(parent), m_path(new QLineEdit(this)), m_format(new QComboBox(this)),
      m_quality(new QSpinBox(this)), m_maxEdge(new QSpinBox(this)),
      m_sourcePath(QFileInfo(sourcePath).absoluteFilePath())
{
    const QString title = mode == ExportMode::SaveCopy
        ? QStringLiteral("Save File")
        : mode == ExportMode::Convert ? QStringLiteral("Convert Format")
                                      : QStringLiteral("Export Photo");
    setWindowTitle(title);
    setMinimumWidth(480);

    const QFileInfo source(sourcePath);
    auto *browseButton = new QPushButton(QStringLiteral("Browse…"), this);
    connect(browseButton, &QPushButton::clicked, this, &ExportDialog::browse);

    auto *pathRow = new QWidget(this);
    auto *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->addWidget(m_path, 1);
    pathLayout->addWidget(browseButton);

    m_format->addItem(QStringLiteral("JPEG"), QByteArray("jpg"));
    m_format->addItem(QStringLiteral("PNG"), QByteArray("png"));
    m_format->addItem(QStringLiteral("TIFF"), QByteArray("tiff"));
    QByteArray initialFormat = normalizedFormat(source.suffix());
    QString nameSuffix = QStringLiteral("-edited");
    if (mode == ExportMode::Export) {
        initialFormat = QByteArray("jpg");
        nameSuffix = QStringLiteral("-export");
    } else if (mode == ExportMode::Convert) {
        initialFormat = initialFormat == QByteArray("png")
            ? QByteArray("jpg") : QByteArray("png");
        nameSuffix = QStringLiteral("-converted");
    }
    const int initialIndex = m_format->findData(initialFormat);
    m_format->setCurrentIndex(qMax(0, initialIndex));
    m_path->setText(source.dir().filePath(source.completeBaseName() + nameSuffix
        + QLatin1Char('.') + QString::fromLatin1(initialFormat)));
    connect(m_format, &QComboBox::currentIndexChanged,
            this, &ExportDialog::updateExtension);

    m_quality->setRange(1, 100);
    m_quality->setValue(92);
    m_quality->setSuffix(QStringLiteral("%"));
    m_maxEdge->setRange(0, 30000);
    m_maxEdge->setValue(0);
    m_maxEdge->setSpecialValueText(QStringLiteral("Original size"));
    m_maxEdge->setSuffix(QStringLiteral(" px"));

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Save as"), pathRow);
    form->addRow(QStringLiteral("Format"), m_format);
    form->addRow(QStringLiteral("Quality"), m_quality);
    form->addRow(QStringLiteral("Long edge"), m_maxEdge);

    auto *note = new QLabel(QStringLiteral(
        "The current layers are rendered to an sRGB image. The source photo is never changed."),
        this);
    note->setWordWrap(true);
    note->setObjectName(QStringLiteral("mutedLabel"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save,
                                         this);
    if (QPushButton *saveButton = buttons->button(QDialogButtonBox::Save))
        saveButton->setText(mode == ExportMode::Convert ? QStringLiteral("Convert")
                                                        : QStringLiteral("Save"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &ExportDialog::validateAndAccept);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(note);
    layout->addSpacing(8);
    layout->addWidget(buttons);
}

ExportOptions ExportDialog::options() const
{
    return {m_path->text().trimmed(), m_format->currentData().toByteArray(),
            m_quality->value(), m_maxEdge->value()};
}

void ExportDialog::browse()
{
    const QByteArray format = m_format->currentData().toByteArray();
    const QString selected = QFileDialog::getSaveFileName(
        this, windowTitle(), m_path->text(), formatFilter(format));
    if (!selected.isEmpty())
        m_path->setText(selected);
}

void ExportDialog::updateExtension()
{
    QFileInfo info(m_path->text());
    const QString extension = QString::fromLatin1(m_format->currentData().toByteArray());
    m_path->setText(info.dir().filePath(info.completeBaseName() + QLatin1Char('.') + extension));
    m_quality->setEnabled(extension == QStringLiteral("jpg"));
}

void ExportDialog::validateAndAccept()
{
    if (m_path->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Export Photo"),
                             QStringLiteral("Choose an export file name."));
        return;
    }
    const QFileInfo target(m_path->text().trimmed());
    if (!target.dir().exists()) {
        QMessageBox::warning(this, QStringLiteral("Export Photo"),
                             QStringLiteral("The selected destination folder does not exist."));
        return;
    }
    if (target.absoluteFilePath().compare(m_sourcePath, pathCaseSensitivity()) == 0) {
        QMessageBox::warning(this, QStringLiteral("Protect Source Photo"),
            QStringLiteral("Choose a different file name. ChromaDesk never overwrites the source photo."));
        return;
    }
    if (target.exists()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Replace File?"),
            QStringLiteral("%1 already exists. Replace it?").arg(target.fileName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    accept();
}

} // namespace chroma
