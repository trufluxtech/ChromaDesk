#include "ui/GenerativeFillClient.h"

#include <QBuffer>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace chroma {
namespace {

QHttpPart textPart(const QByteArray &name, const QString &value)
{
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
        QStringLiteral("form-data; name=\"%1\"")
            .arg(QString::fromLatin1(name)));
    part.setBody(value.toUtf8());
    return part;
}

QHttpPart pngPart(const QByteArray &name, const QString &fileName,
                  const QImage &image, QHttpMultiPart *owner)
{
    auto *buffer = new QBuffer(owner);
    buffer->open(QIODevice::ReadWrite);
    image.save(buffer, "PNG");
    buffer->seek(0);

    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("image/png"));
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
        QStringLiteral("form-data; name=\"%1\"; filename=\"%2\"")
            .arg(QString::fromLatin1(name), fileName));
    part.setBodyDevice(buffer);
    return part;
}

QString apiErrorMessage(const QJsonObject &root, const QString &fallback)
{
    const QString message = root.value(QStringLiteral("error")).toObject()
        .value(QStringLiteral("message")).toString();
    return message.trimmed().isEmpty() ? fallback : message;
}

} // namespace

GenerativeFillClient::GenerativeFillClient(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this))
{
}

void GenerativeFillClient::requestFill(const QString &apiKey, const QImage &image,
                                       const QImage &mask, const QString &prompt)
{
    if (apiKey.trimmed().isEmpty()) {
        emit completed({}, QStringLiteral("An OpenAI API key is required."));
        return;
    }
    if (image.isNull() || mask.isNull() || image.size() != mask.size()) {
        emit completed({}, QStringLiteral(
            "The source image and transparent edit mask must have matching dimensions."));
        return;
    }

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    multiPart->append(textPart("model", QStringLiteral("gpt-image-2")));
    multiPart->append(textPart("prompt", prompt));
    multiPart->append(textPart("quality", QStringLiteral("high")));
    multiPart->append(textPart("input_fidelity", QStringLiteral("high")));
    multiPart->append(textPart("output_format", QStringLiteral("png")));
    multiPart->append(textPart("size", QStringLiteral("auto")));
    multiPart->append(pngPart("image[]", QStringLiteral("chromadesk-source.png"),
                              image, multiPart));
    multiPart->append(pngPart("mask", QStringLiteral("chromadesk-mask.png"),
                              mask, multiPart));

    QNetworkRequest request(QUrl(QStringLiteral("https://api.openai.com/v1/images/edits")));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.trimmed().toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(180000);
    QNetworkReply *reply = m_network->post(request, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QByteArray payload = reply->readAll();
        const QJsonDocument document = QJsonDocument::fromJson(payload);
        const QJsonObject root = document.object();
        if (reply->error() != QNetworkReply::NoError) {
            const QString fallback = reply->errorString().isEmpty()
                ? QStringLiteral("Generative Fill request failed.")
                : reply->errorString();
            emit completed({}, apiErrorMessage(root, fallback));
            reply->deleteLater();
            return;
        }

        const QJsonArray data = root.value(QStringLiteral("data")).toArray();
        const QString encoded = data.isEmpty() ? QString{}
            : data.at(0).toObject().value(QStringLiteral("b64_json")).toString();
        QImage generated;
        generated.loadFromData(QByteArray::fromBase64(encoded.toLatin1()), "PNG");
        if (generated.isNull()) {
            emit completed({}, apiErrorMessage(root,
                QStringLiteral("OpenAI returned no readable generated image.")));
        } else {
            emit completed(generated, {});
        }
        reply->deleteLater();
    });
}

} // namespace chroma
