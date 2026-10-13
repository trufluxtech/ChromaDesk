#pragma once

#include <QImage>
#include <QWidget>

#include <array>

namespace chroma {

class HistogramWidget final : public QWidget {
    Q_OBJECT

public:
    explicit HistogramWidget(QWidget *parent = nullptr);
    void setImage(const QImage &image);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::array<int, 256> m_red{};
    std::array<int, 256> m_green{};
    std::array<int, 256> m_blue{};
    int m_peak = 1;
};

} // namespace chroma
