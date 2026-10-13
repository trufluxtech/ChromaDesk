#pragma once

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QSlider;

namespace chroma {

class AdjustmentControl final : public QWidget {
    Q_OBJECT

public:
    AdjustmentControl(const QString &label, double minimum, double maximum,
                      double step, int decimals, int mergeId,
                      QWidget *parent = nullptr);

    [[nodiscard]] double value() const;
    [[nodiscard]] int mergeId() const { return m_mergeId; }
    void setValue(double value);

signals:
    void valueChanged(double value, int mergeId);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    [[nodiscard]] int toSlider(double value) const;
    [[nodiscard]] double fromSlider(int value) const;

    QLabel *m_label;
    QSlider *m_slider;
    QDoubleSpinBox *m_spin;
    double m_minimum;
    double m_step;
    int m_mergeId;
};

} // namespace chroma
