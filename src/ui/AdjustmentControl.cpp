#include "ui/AdjustmentControl.h"

#include <QDoubleSpinBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QSlider>

#include <cmath>

namespace chroma {

AdjustmentControl::AdjustmentControl(const QString &label, double minimum,
                                     double maximum, double step, int decimals,
                                     int mergeId, QWidget *parent)
    : QWidget(parent), m_label(new QLabel(label, this)),
      m_slider(new QSlider(Qt::Horizontal, this)),
      m_spin(new QDoubleSpinBox(this)), m_minimum(minimum), m_step(step),
      m_mergeId(mergeId)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(7);
    m_label->setMinimumWidth(72);
    m_label->setToolTip(QStringLiteral("Double-click to reset"));
    m_label->installEventFilter(this);

    m_slider->setRange(0, qRound((maximum - minimum) / step));
    m_slider->setSingleStep(1);
    m_slider->setPageStep(qMax(1, qRound(5.0 / step)));

    m_spin->setRange(minimum, maximum);
    m_spin->setSingleStep(step);
    m_spin->setDecimals(decimals);
    m_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_spin->setAlignment(Qt::AlignRight);
    m_spin->setFixedWidth(58);

    layout->addWidget(m_label);
    layout->addWidget(m_slider, 1);
    layout->addWidget(m_spin);

    connect(m_slider, &QSlider::valueChanged, this, [this](int sliderValue) {
        const double newValue = fromSlider(sliderValue);
        const QSignalBlocker blocker(m_spin);
        m_spin->setValue(newValue);
        emit valueChanged(newValue, m_mergeId);
    });
    connect(m_spin, &QDoubleSpinBox::valueChanged, this, [this](double newValue) {
        const QSignalBlocker blocker(m_slider);
        m_slider->setValue(toSlider(newValue));
        emit valueChanged(newValue, m_mergeId);
    });
    setValue(0.0);
}

double AdjustmentControl::value() const
{
    return m_spin->value();
}

void AdjustmentControl::setValue(double value)
{
    const QSignalBlocker spinBlocker(m_spin);
    const QSignalBlocker sliderBlocker(m_slider);
    m_spin->setValue(value);
    m_slider->setValue(toSlider(value));
}

bool AdjustmentControl::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_label && event->type() == QEvent::MouseButtonDblClick) {
        setValue(0.0);
        emit valueChanged(0.0, m_mergeId);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

int AdjustmentControl::toSlider(double value) const
{
    return qRound((value - m_minimum) / m_step);
}

double AdjustmentControl::fromSlider(int value) const
{
    return m_minimum + static_cast<double>(value) * m_step;
}

} // namespace chroma
