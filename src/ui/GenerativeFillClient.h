#pragma once

#include <QImage>
#include <QObject>

class QNetworkAccessManager;

namespace chroma {

class GenerativeFillClient final : public QObject {
    Q_OBJECT

public:
    explicit GenerativeFillClient(QObject *parent = nullptr);
    void requestFill(const QString &apiKey, const QImage &image,
                     const QImage &mask, const QString &prompt);

signals:
    void completed(const QImage &image, const QString &errorMessage);

private:
    QNetworkAccessManager *m_network = nullptr;
};

} // namespace chroma

