#pragma once

#include <QIcon>
#include <QPixmap>

namespace chroma::branding {

[[nodiscard]] QPixmap logoPixmap(int size);
[[nodiscard]] QIcon applicationIcon();
[[nodiscard]] QPixmap splashPixmap();

} // namespace chroma::branding
