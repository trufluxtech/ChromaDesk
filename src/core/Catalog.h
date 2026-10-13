#pragma once

#include "core/LayerStack.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

namespace chroma {

struct CatalogEntry {
    QString path;
    LayerStack layers;
    int rating = 0;
    bool picked = false;
    QDateTime importedAt;
};

class Catalog final : public QObject {
    Q_OBJECT

public:
    explicit Catalog(QObject *parent = nullptr);

    [[nodiscard]] const QList<CatalogEntry> &entries() const { return m_entries; }
    [[nodiscard]] CatalogEntry *find(const QString &path);
    [[nodiscard]] const CatalogEntry *find(const QString &path) const;
    int addFiles(const QStringList &paths);
    void updateLayers(const QString &path, const LayerStack &layers);
    void setRating(const QString &path, int rating);
    void setPicked(const QString &path, bool picked);
    bool save(QString *error = nullptr) const;
    bool load(QString *error = nullptr);

signals:
    void changed();

private:
    [[nodiscard]] static QString catalogPath();
    QList<CatalogEntry> m_entries;
};

} // namespace chroma
