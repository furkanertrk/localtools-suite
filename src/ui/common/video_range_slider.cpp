#include "video_range_slider.h"

#include <algorithm>
#include <cmath>

namespace LocalTools::UI {

VideoRangeSlider::VideoRangeSlider(QWidget *parent)
    : QWidget(parent) {
    setFixedHeight(48);
    setMinimumWidth(320);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void VideoRangeSlider::setRange(double minVal, double maxVal) {
    m_min = minVal;
    m_max = std::max(minVal + 0.1, maxVal);
    m_start = std::clamp(m_start, m_min, m_max);
    m_end = std::clamp(m_end, m_start + 0.1, m_max);
    m_playhead = std::clamp(m_playhead, m_min, m_max);
    update();
}

void VideoRangeSlider::setStartValue(double startVal) {
    m_start = std::clamp(startVal, m_min, m_end - 0.1);
    update();
    emit startValueChanged(m_start);
    emit rangeChanged(m_start, m_end);
}

void VideoRangeSlider::setEndValue(double endVal) {
    m_end = std::clamp(endVal, m_start + 0.1, m_max);
    update();
    emit endValueChanged(m_end);
    emit rangeChanged(m_start, m_end);
}

void VideoRangeSlider::setPlayheadPosition(double pos) {
    m_playhead = std::clamp(pos, m_min, m_max);
    update();
}

double VideoRangeSlider::posToValue(double x) const {
    double trackW = width() - 2.0 * Margin;
    if (trackW <= 0.0) return m_min;
    double ratio = (x - Margin) / trackW;
    return m_min + ratio * (m_max - m_min);
}

double VideoRangeSlider::valueToPos(double val) const {
    double trackW = width() - 2.0 * Margin;
    if (m_max <= m_min) return Margin;
    double ratio = (val - m_min) / (m_max - m_min);
    return Margin + ratio * trackW;
}

QString VideoRangeSlider::formatTime(double seconds) const {
    int totalSec = static_cast<int>(seconds);
    int m = totalSec / 60;
    int s = totalSec % 60;
    int ms = static_cast<int>((seconds - totalSec) * 10.0);
    return QString("%1:%2.%3")
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(ms);
}

QSize VideoRangeSlider::sizeHint() const {
    return QSize(450, 48);
}

void VideoRangeSlider::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;

    double mouseX = event->pos().x();
    double startX = valueToPos(m_start);
    double endX = valueToPos(m_end);
    double playheadX = valueToPos(m_playhead);

    if (std::abs(mouseX - playheadX) <= 10.0) {
        m_dragTarget = DragTarget::Playhead;
        emit seekRequested(m_playhead);
    } else if (std::abs(mouseX - startX) <= HandleWidth) {
        m_dragTarget = DragTarget::StartHandle;
    } else if (std::abs(mouseX - endX) <= HandleWidth) {
        m_dragTarget = DragTarget::EndHandle;
    } else {
        // Direct click on timeline moves playhead and seeks video
        double clickVal = posToValue(mouseX);
        setPlayheadPosition(clickVal);
        m_dragTarget = DragTarget::Playhead;
        emit seekRequested(clickVal);
    }
}

void VideoRangeSlider::mouseMoveEvent(QMouseEvent *event) {
    double mouseX = event->pos().x();
    double curVal = posToValue(mouseX);

    if (m_dragTarget == DragTarget::Playhead) {
        setPlayheadPosition(curVal);
        emit seekRequested(m_playhead);
    } else if (m_dragTarget == DragTarget::StartHandle) {
        m_start = std::clamp(curVal, m_min, m_end - 0.2);
        update();
        emit startValueChanged(m_start);
        emit rangeChanged(m_start, m_end);
        emit seekRequested(m_start);
    } else if (m_dragTarget == DragTarget::EndHandle) {
        m_end = std::clamp(curVal, m_start + 0.2, m_max);
        update();
        emit endValueChanged(m_end);
        emit rangeChanged(m_start, m_end);
        emit seekRequested(m_end);
    } else if (m_dragTarget == DragTarget::MiddleRange) {
        double delta = curVal - m_dragStartOffset;
        double rangeLen = m_dragInitialEnd - m_dragInitialStart;
        double newStart = std::clamp(m_dragInitialStart + delta, m_min, m_max - rangeLen);
        m_start = newStart;
        m_end = newStart + rangeLen;
        update();
        emit rangeChanged(m_start, m_end);
        emit startValueChanged(m_start);
        emit endValueChanged(m_end);
    } else {
        // Update mouse cursor
        double startX = valueToPos(m_start);
        double endX = valueToPos(m_end);
        double playheadX = valueToPos(m_playhead);

        if (std::abs(mouseX - playheadX) <= 8.0) {
            setCursor(Qt::SplitHCursor);
        } else if (std::abs(mouseX - startX) <= HandleWidth || std::abs(mouseX - endX) <= HandleWidth) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::PointingHandCursor);
        }
    }
}

void VideoRangeSlider::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragTarget = DragTarget::None;
        setCursor(Qt::PointingHandCursor);
    }
}

void VideoRangeSlider::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();

    double trackH = 26.0;
    double trackY = (h - trackH) / 2.0;
    double trackW = w - 2.0 * Margin;

    bool isDark = (palette().window().color().lightness() < 128);

    // 1. Inactive Background Track
    QPainterPath bgPath;
    bgPath.addRoundedRect(QRectF(Margin, trackY, trackW, trackH), 6.0, 6.0);
    painter.fillPath(bgPath, isDark ? QColor(255, 255, 255, 25) : QColor(226, 232, 240));

    // 2. Active Cut Region
    double startX = valueToPos(m_start);
    double endX = valueToPos(m_end);
    double activeW = std::max(4.0, endX - startX);

    QRectF activeRect(startX, trackY, activeW, trackH);
    QPainterPath activePath;
    activePath.addRect(activeRect);
    QPainterPath clippedActive = activePath.intersected(bgPath);

    QLinearGradient grad(startX, trackY, endX, trackY);
    grad.setColorAt(0.0, QColor("#2563eb"));
    grad.setColorAt(1.0, QColor("#3b82f6"));
    painter.fillPath(clippedActive, grad);

    // Subtle stripe tick marks inside track
    painter.setPen(isDark ? QColor(255, 255, 255, 40) : QColor(0, 0, 0, 30));
    for (int i = 1; i < 20; ++i) {
        double tickX = Margin + (i / 20.0) * trackW;
        painter.drawLine(QPointF(tickX, trackY + 4), QPointF(tickX, trackY + trackH - 4));
    }

    // 3. Start Handle (Left)
    double handleH = trackH + 10.0;
    double handleY = (h - handleH) / 2.0;

    QRectF startHandleRect(startX - HandleWidth / 2.0, handleY, HandleWidth, handleH);
    QPainterPath startHandlePath;
    startHandlePath.addRoundedRect(startHandleRect, 4.0, 4.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 70));
    painter.drawRoundedRect(startHandleRect.translated(0, 1.5), 4.0, 4.0); // shadow

    painter.setBrush(QColor("#ffffff"));
    painter.drawPath(startHandlePath);

    // Grip lines inside start handle
    painter.setPen(QColor("#2563eb"));
    painter.drawLine(QPointF(startX - 2, handleY + 6), QPointF(startX - 2, handleY + handleH - 6));
    painter.drawLine(QPointF(startX + 2, handleY + 6), QPointF(startX + 2, handleY + handleH - 6));

    // 4. End Handle (Right)
    QRectF endHandleRect(endX - HandleWidth / 2.0, handleY, HandleWidth, handleH);
    QPainterPath endHandlePath;
    endHandlePath.addRoundedRect(endHandleRect, 4.0, 4.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 70));
    painter.drawRoundedRect(endHandleRect.translated(0, 1.5), 4.0, 4.0); // shadow

    painter.setBrush(QColor("#ffffff"));
    painter.drawPath(endHandlePath);

    // Grip lines inside end handle
    painter.setPen(QColor("#2563eb"));
    painter.drawLine(QPointF(endX - 2, handleY + 6), QPointF(endX - 2, handleY + handleH - 6));
    painter.drawLine(QPointF(endX + 2, handleY + 6), QPointF(endX + 2, handleY + handleH - 6));

    // 5. Interactive Playhead Needle & Indicator
    if (m_playhead >= m_min && m_playhead <= m_max) {
        double playheadX = valueToPos(m_playhead);

        // Needle line
        painter.setPen(QPen(QColor("#ef4444"), 2.0));
        painter.drawLine(QPointF(playheadX, trackY - 5), QPointF(playheadX, trackY + trackH + 5));

        // Top triangular / circular cursor head
        painter.setBrush(QColor("#ef4444"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(playheadX, trackY - 4), 5.0, 5.0);
        painter.setBrush(QColor("#ffffff"));
        painter.drawEllipse(QPointF(playheadX, trackY - 4), 2.0, 2.0);
    }
}

} // namespace LocalTools::UI
