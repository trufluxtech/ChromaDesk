#include "core/EditState.h"

#include <QtGlobal>
#include <QStringList>

#include <cmath>

namespace chroma {
namespace {

constexpr double epsilon = 0.0001;

bool closeEnough(double a, double b)
{
    return std::abs(a - b) < epsilon;
}

double bounded(const QJsonObject &json, const QString &key, double fallback,
               double low, double high)
{
    return qBound(low, json.value(key).toDouble(fallback), high);
}

} // namespace

bool operator==(const EditState &lhs, const EditState &rhs)
{
    return closeEnough(lhs.exposure, rhs.exposure)
        && closeEnough(lhs.contrast, rhs.contrast)
        && closeEnough(lhs.highlights, rhs.highlights)
        && closeEnough(lhs.shadows, rhs.shadows)
        && closeEnough(lhs.whites, rhs.whites)
        && closeEnough(lhs.blacks, rhs.blacks)
        && closeEnough(lhs.temperature, rhs.temperature)
        && closeEnough(lhs.tint, rhs.tint)
        && closeEnough(lhs.saturation, rhs.saturation)
        && closeEnough(lhs.vibrance, rhs.vibrance)
        && closeEnough(lhs.clarity, rhs.clarity)
        && closeEnough(lhs.blur, rhs.blur)
        && lhs.replacementColor == rhs.replacementColor
        && closeEnough(lhs.replacementAmount, rhs.replacementAmount)
        && closeEnough(lhs.borderSize, rhs.borderSize)
        && lhs.borderColor == rhs.borderColor
        && lhs.rotation == rhs.rotation
        && lhs.flipHorizontal == rhs.flipHorizontal
        && lhs.flipVertical == rhs.flipVertical
        && closeEnough(lhs.crop.x(), rhs.crop.x())
        && closeEnough(lhs.crop.y(), rhs.crop.y())
        && closeEnough(lhs.crop.width(), rhs.crop.width())
        && closeEnough(lhs.crop.height(), rhs.crop.height());
}

bool EditState::isDefault() const
{
    return *this == EditState{};
}

QJsonObject EditState::toJson() const
{
    return {
        {QStringLiteral("exposure"), exposure},
        {QStringLiteral("contrast"), contrast},
        {QStringLiteral("highlights"), highlights},
        {QStringLiteral("shadows"), shadows},
        {QStringLiteral("whites"), whites},
        {QStringLiteral("blacks"), blacks},
        {QStringLiteral("temperature"), temperature},
        {QStringLiteral("tint"), tint},
        {QStringLiteral("saturation"), saturation},
        {QStringLiteral("vibrance"), vibrance},
        {QStringLiteral("clarity"), clarity},
        {QStringLiteral("blur"), blur},
        {QStringLiteral("replacementColor"), replacementColor.isValid()
            ? replacementColor.name(QColor::HexRgb) : QString{}},
        {QStringLiteral("replacementAmount"), replacementAmount},
        {QStringLiteral("borderSize"), borderSize},
        {QStringLiteral("borderColor"), borderColor.name(QColor::HexArgb)},
        {QStringLiteral("rotation"), rotation},
        {QStringLiteral("flipHorizontal"), flipHorizontal},
        {QStringLiteral("flipVertical"), flipVertical},
        {QStringLiteral("cropX"), crop.x()},
        {QStringLiteral("cropY"), crop.y()},
        {QStringLiteral("cropW"), crop.width()},
        {QStringLiteral("cropH"), crop.height()}
    };
}

EditState EditState::fromJson(const QJsonObject &json)
{
    EditState state;
    state.exposure = bounded(json, QStringLiteral("exposure"), 0.0, -5.0, 5.0);
    state.contrast = bounded(json, QStringLiteral("contrast"), 0.0, -100.0, 100.0);
    state.highlights = bounded(json, QStringLiteral("highlights"), 0.0, -100.0, 100.0);
    state.shadows = bounded(json, QStringLiteral("shadows"), 0.0, -100.0, 100.0);
    state.whites = bounded(json, QStringLiteral("whites"), 0.0, -100.0, 100.0);
    state.blacks = bounded(json, QStringLiteral("blacks"), 0.0, -100.0, 100.0);
    state.temperature = bounded(json, QStringLiteral("temperature"), 0.0, -100.0, 100.0);
    state.tint = bounded(json, QStringLiteral("tint"), 0.0, -100.0, 100.0);
    state.saturation = bounded(json, QStringLiteral("saturation"), 0.0, -100.0, 100.0);
    state.vibrance = bounded(json, QStringLiteral("vibrance"), 0.0, -100.0, 100.0);
    state.clarity = bounded(json, QStringLiteral("clarity"), 0.0, -100.0, 100.0);
    state.blur = bounded(json, QStringLiteral("blur"), 0.0, 0.0, 100.0);
    const QColor requestedReplacement(
        json.value(QStringLiteral("replacementColor")).toString());
    if (requestedReplacement.isValid())
        state.replacementColor = requestedReplacement;
    state.replacementAmount = bounded(json, QStringLiteral("replacementAmount"),
                                      0.0, 0.0, 100.0);
    state.borderSize = bounded(json, QStringLiteral("borderSize"), 0.0, 0.0, 0.20);
    const QColor requestedBorder(json.value(QStringLiteral("borderColor")).toString());
    if (requestedBorder.isValid())
        state.borderColor = requestedBorder;

    const int requestedRotation = json.value(QStringLiteral("rotation")).toInt(0);
    state.rotation = ((requestedRotation % 360) + 360) % 360;
    state.rotation = (state.rotation / 90) * 90;
    state.flipHorizontal = json.value(QStringLiteral("flipHorizontal")).toBool(false);
    state.flipVertical = json.value(QStringLiteral("flipVertical")).toBool(false);

    const QRectF requestedCrop(
        bounded(json, QStringLiteral("cropX"), 0.0, 0.0, 1.0),
        bounded(json, QStringLiteral("cropY"), 0.0, 0.0, 1.0),
        bounded(json, QStringLiteral("cropW"), 1.0, 0.001, 1.0),
        bounded(json, QStringLiteral("cropH"), 1.0, 0.001, 1.0));
    state.crop = requestedCrop.intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (state.crop.width() < 0.001 || state.crop.height() < 0.001)
        state.crop = QRectF(0.0, 0.0, 1.0, 1.0);
    return state;
}

QString editStateSummary(const EditState &state)
{
    QStringList changes;
    if (!closeEnough(state.exposure, 0.0))
        changes << QStringLiteral("Exposure %1").arg(state.exposure, 0, 'f', 1);
    if (!closeEnough(state.contrast, 0.0)) changes << QStringLiteral("Contrast");
    if (!closeEnough(state.temperature, 0.0) || !closeEnough(state.tint, 0.0))
        changes << QStringLiteral("White balance");
    if (!closeEnough(state.saturation, 0.0) || !closeEnough(state.vibrance, 0.0))
        changes << QStringLiteral("Colour");
    if (!closeEnough(state.blur, 0.0)) changes << QStringLiteral("Blur");
    if (state.replacementColor.isValid()
        && !closeEnough(state.replacementAmount, 0.0)) {
        changes << QStringLiteral("Replace colour");
    }
    if (!closeEnough(state.borderSize, 0.0)) changes << QStringLiteral("Border");
    if (state.rotation != 0) changes << QStringLiteral("Rotation");
    if (state.crop != QRectF(0.0, 0.0, 1.0, 1.0)) changes << QStringLiteral("Crop");
    return changes.isEmpty() ? QStringLiteral("Original") : changes.join(QStringLiteral(" · "));
}

} // namespace chroma
