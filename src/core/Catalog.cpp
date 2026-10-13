#include "core/Catalog.h"

#include "core/ImageLoader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

namespace chroma {

Catalog::Catalog(QObject *parent)
    : QObject(parent)
{
}

QString Catalog::catalogPath()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(directory);
    return QDir(directory).filePath(QStringLiteral("catalog.json"));
}

CatalogEntry *Catalog::find(const QString &path)
{
    const QString canonical = QFileInfo(path).absoluteFilePath();
    for (auto &entry : m_entries) {
        if (QFileInfo(entry.path).absoluteFilePath() == canonical)
            return &entry;
    }
    return nullptr;
}

const CatalogEntry *Catalog::find(const QString &path) const
{
    const QString canonical = QFileInfo(path).absoluteFilePath();
    for (const auto &entry : m_entries) {
        if (QFileInfo(entry.path).absoluteFilePath() == canonical)
            return &entry;
    }
    return nullptr;
}

int Catalog::addFiles(const QStringList &paths)
{
    int added = 0;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.isFile() || find(info.absoluteFilePath()))
            continue;
        const bool recognized = ImageLoader::supportedNameFilters().contains(
            QStringLiteral("*.%1").arg(info.suffix().toLower()));
        if (!recognized)
            continue;
        m_entries.prepend({info.absoluteFilePath(), {}, 0, false,
                           QDateTime::currentDateTimeUtc()});
        ++added;
    }
    if (added > 0) {
        save();
        emit changed();
    }
    return added;
}

void Catalog::updateLayers(const QString &path, const LayerStack &layers)
{
    if (auto *entry = find(path)) {
        entry->layers = layers;
        save();
        emit changed();
    }
}

void Catalog::setRating(const QString &path, int rating)
{
    if (auto *entry = find(path)) {
        entry->rating = qBound(0, rating, 5);
        save();
        emit changed();
    }
}

void Catalog::setPicked(const QString &path, bool picked)
{
    if (auto *entry = find(path)) {
        entry->picked = picked;
        save();
        emit changed();
    }
}

bool Catalog::save(QString *error) const
{
    QJsonArray entriesJson;
    for (const auto &entry : m_entries) {
        entriesJson.append(QJsonObject{
            {QStringLiteral("path"), entry.path},
            {QStringLiteral("layers"), entry.layers.toJson()},
            {QStringLiteral("rating"), entry.rating},
            {QStringLiteral("picked"), entry.picked},
            {QStringLiteral("importedAt"), entry.importedAt.toString(Qt::ISODateWithMs)}
        });
    }

    QSaveFile file(catalogPath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(QJsonObject{
        {QStringLiteral("schemaVersion"), 7},
        {QStringLiteral("entries"), entriesJson}
    }).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool Catalog::load(QString *error)
{
    QFile file(catalogPath());
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return false;
    }

    QList<CatalogEntry> loaded;
    const QJsonArray entriesJson = document.object().value(QStringLiteral("entries")).toArray();
    for (const QJsonValue &value : entriesJson) {
        const QJsonObject object = value.toObject();
        const QString path = object.value(QStringLiteral("path")).toString();
        if (path.isEmpty())
            continue;
        const LayerStack layers = object.value(QStringLiteral("layers")).isArray()
            ? LayerStack::fromJson(object.value(QStringLiteral("layers")).toArray())
            : LayerStack::fromLegacyEdits(
                  EditState::fromJson(object.value(QStringLiteral("edits")).toObject()));
        loaded.append({
            path,
            layers,
            qBound(0, object.value(QStringLiteral("rating")).toInt(), 5),
            object.value(QStringLiteral("picked")).toBool(false),
            QDateTime::fromString(object.value(QStringLiteral("importedAt")).toString(),
                                  Qt::ISODateWithMs)
        });
    }
    m_entries = std::move(loaded);
    emit changed();
    return true;
}

} // namespace chroma
