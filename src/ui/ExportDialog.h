#pragma once

#include <QByteArray>
#include <QDialog>
#include <QString>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace chroma {

enum class ExportMode {
    SaveCopy,
    Export,
    Convert
};

struct ExportOptions {
    QString path;
    QByteArray format;
    int quality = 92;
    int maxEdge = 0;
};

class ExportDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ExportDialog(const QString &sourcePath, ExportMode mode,
                          QWidget *parent = nullptr);
    [[nodiscard]] ExportOptions options() const;

private slots:
    void browse();
    void updateExtension();
    void validateAndAccept();

private:
    QLineEdit *m_path;
    QComboBox *m_format;
    QSpinBox *m_quality;
    QSpinBox *m_maxEdge;
    QString m_sourcePath;
};

} // namespace chroma
