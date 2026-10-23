#ifndef LOCALTOOLS_UI_COMMON_VIDEO_RANGE_SLIDER_H
#define LOCALTOOLS_UI_COMMON_VIDEO_RANGE_SLIDER_H

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

namespace LocalTools::UI {

class VideoRangeSlider : public QWidget {
    Q_OBJECT

public:
    explicit VideoRangeSlider(QWidget *parent = nullptr);

    double minimum() const { return m_min; }
    double maximum() const { return m_max; }
    double startValue() const { return m_start; }
    double endValue() const { return m_end; }
    double playheadPosition() const { return m_playhead; }

    void setRange(double minVal, double maxVal);
    void setStartValue(double startVal);
    void setEndValue(double endVal);
    void setPlayheadPosition(double pos);

signals:
    void rangeChanged(double start, double end);
    void startValueChanged(double start);
    void endValueChanged(double end);
    void seekRequested(double position);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QSize sizeHint() const override;

private:
    double posToValue(double x) const;
    double valueToPos(double val) const;
    QString formatTime(double seconds) const;

    enum class DragTarget {
        None,
        StartHandle,
        EndHandle,
        MiddleRange,
        Playhead
    };

    double m_min{0.0};
    double m_max{100.0};
    double m_start{0.0};
    double m_end{100.0};
    double m_playhead{0.0};

    DragTarget m_dragTarget{DragTarget::None};
    double m_dragStartOffset{0.0};
    double m_dragInitialStart{0.0};
    double m_dragInitialEnd{0.0};

    static constexpr double HandleWidth = 14.0;
    static constexpr double Margin = 16.0;
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_VIDEO_RANGE_SLIDER_H
