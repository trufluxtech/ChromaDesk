#pragma once

#include <QColor>
#include <QJsonObject>
#include <QRectF>
#include <QString>

namespace chroma {

struct EditState {
    double exposure = 0.0;      // stops, -5..+5
    double contrast = 0.0;      // -100..+100
    double highlights = 0.0;    // -100..+100
    double shadows = 0.0;       // -100..+100
    double whites = 0.0;        // -100..+100
    double blacks = 0.0;        // -100..+100
    double temperature = 0.0;   // -100..+100
    double tint = 0.0;          // -100..+100
    double saturation = 0.0;    // -100..+100
    double vibrance = 0.0;      // -100..+100
    double clarity = 0.0;       // -100..+100
    double blur = 0.0;          // 0..100, primarily for masked backgrounds
    QColor replacementColor;    // Invalid when color replacement is disabled.
    double replacementAmount = 0.0; // 0..100
    double borderSize = 0.0;    // Fraction of shortest edge, 0.0..0.20.
    QColor borderColor = QColor(Qt::white);
    int rotation = 0;           // 0, 90, 180, 270 clockwise
    bool flipHorizontal = false;
    bool flipVertical = false;
    QRectF crop = QRectF(0.0, 0.0, 1.0, 1.0); // normalized after orientation

    [[nodiscard]] bool isDefault() const;
    [[nodiscard]] QJsonObject toJson() const;
    static EditState fromJson(const QJsonObject &json);

    friend bool operator==(const EditState &lhs, const EditState &rhs);
    friend bool operator!=(const EditState &lhs, const EditState &rhs) { return !(lhs == rhs); }
};

[[nodiscard]] QString editStateSummary(const EditState &state);

} // namespace chroma
