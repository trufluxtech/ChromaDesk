#pragma once

#include "core/EditState.h"

#include <QList>
#include <QString>

namespace chroma {

struct Preset {
    QString name;
    QString description;
    EditState edits;
};

struct CreativeFilter {
    QString category;
    QString name;
    QString description;
    EditState edits;
};

class PresetRepository final {
public:
    [[nodiscard]] static const QList<Preset> &builtIns();
    [[nodiscard]] static const QList<CreativeFilter> &creativeFilters();
};

} // namespace chroma
