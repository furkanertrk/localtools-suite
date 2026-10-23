#include "pill_slider.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QEvent>
#include <QEasingCurve>
#include <QFrame>
#include <cmath>

namespace LocalTools::UI {

PillSliderCanvas::PillSliderCanvas(int stepCount, QWidget *parent)
    : QWidget(parent), m_stepCount(stepCount) {
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(36);
    setMinimumWidth(260);

    m_animation = new QPropertyAnimation(this, "handlePos", this);
    m_animation->setDuration(220);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
}

void PillSliderCanvas::setHandlePos(double pos) {
    m_handlePos = std::clamp(pos, 0.0, static_cast<double>(m_stepCount - 1));
    update();
}

void PillSliderCanvas::setCurrentStep(int step, bool animated) {
    step = std::clamp(step, 0, m_stepCount - 1);
    if (m_currentStep == step && !animated) return;

    m_currentStep = step;
    if (animated) {
        m_animation->stop();
        m_animation->setStartValue(m_handlePos);
        m_animation->setEndValue(static_cast<double>(step));
        m_animation->start();
    } else {
        setHandlePos(static_cast<double>(step));
    }
    emit stepChanged(m_currentStep);
}

int PillSliderCanvas::stepFromX(int mouseX) const {
    double trackMargin = 14.0;
    double trackH = 20.0;
    double radius = trackH / 2.0;
    double startX = trackMargin + radius;
    double endX = width() - trackMargin - radius;
    double travelW = endX - startX;
    if (travelW <= 0) return 0;

    double ratio = (mouseX - startX) / travelW;
    int step = static_cast<int>(std::round(ratio * (m_stepCount - 1)));
    return std::clamp(step, 0, m_stepCount - 1);
}

double PillSliderCanvas::xForStep(int step) const {
    double trackMargin = 14.0;
    double trackH = 20.0;
    double radius = trackH / 2.0;
    double startX = trackMargin + radius;
    double endX = width() - trackMargin - radius;
    double travelW = endX - startX;
    if (m_stepCount <= 1 || travelW <= 0) return startX;
    return startX + (static_cast<double>(step) / (m_stepCount - 1)) * travelW;
}

QSize PillSliderCanvas::sizeHint() const {
    return QSize(360, 36);
}

void PillSliderCanvas::enterEvent(QEnterEvent*) {
    m_isHovered = true;
    update();
}

void PillSliderCanvas::leaveEvent(QEvent*) {
    m_isHovered = false;
    update();
}

void PillSliderCanvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_animation->stop();
        int step = stepFromX(event->pos().x());
        setCurrentStep(step, true);
    }
}

void PillSliderCanvas::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDragging) {
        double trackMargin = 14.0;
        double trackH = 20.0;
        double radius = trackH / 2.0;
        double startX = trackMargin + radius;
        double endX = width() - trackMargin - radius;
        double travelW = endX - startX;
        if (travelW > 0) {
            double pos = (event->pos().x() - startX) / travelW * (m_stepCount - 1);
            setHandlePos(pos);
            int step = std::clamp(static_cast<int>(std::round(m_handlePos)), 0, m_stepCount - 1);
            if (step != m_currentStep) {
                m_currentStep = step;
                emit stepChanged(m_currentStep);
            }
        }
    }
}

void PillSliderCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        int step = stepFromX(event->pos().x());
        setCurrentStep(step, true);
    }
}

void PillSliderCanvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();

    double trackMargin = 14.0;
    double trackH = 20.0;
    double trackY = (h - trackH) / 2.0;
    double trackW = w - 2.0 * trackMargin;
    double radius = trackH / 2.0; // 10.0px

    double startX = trackMargin + radius;
    double endX = w - trackMargin - radius;
    double travelW = endX - startX;

    bool isDark = (palette().window().color().lightness() < 128);

    // 1. Draw Inactive Background Pill Track
    QPainterPath bgPath;
    bgPath.addRoundedRect(QRectF(trackMargin, trackY, trackW, trackH), radius, radius);
    QColor trackBg = isDark ? QColor(255, 255, 255, 30) : QColor(226, 232, 240);
    painter.fillPath(bgPath, trackBg);

    // 2. Calculate Precise Handle Center Position
    double handleX = (travelW > 0)
        ? (startX + (m_handlePos / (m_stepCount - 1)) * travelW)
        : startX;

    // 3. Draw Active Blue Pill Segment
    double activeW = handleX - trackMargin;
    if (activeW > 0.1) {
        QPainterPath activePath;
        activePath.addRoundedRect(QRectF(trackMargin, trackY, activeW + radius, trackH), radius, radius);

        // Intersect with bgPath to guarantee perfect rounded caps without any overflow
        QPainterPath clippedActive = activePath.intersected(bgPath);
        QLinearGradient grad(trackMargin, trackY, endX + radius, trackY);
        grad.setColorAt(0.0, QColor("#3b82f6"));
        grad.setColorAt(1.0, QColor("#2563eb"));
        painter.fillPath(clippedActive, grad);
    }

    // 4. Draw Step Indicator Dots
    for (int i = 0; i < m_stepCount; ++i) {
        double dotX = (travelW > 0) ? (startX + (static_cast<double>(i) / (m_stepCount - 1)) * travelW) : startX;
        double dotY = h / 2.0;
        double dotRadius = 2.0;

        // Skip drawing dot if it is covered by the handle
        if (std::abs(dotX - handleX) < 8.0) {
            continue;
        }

        painter.setPen(Qt::NoPen);
        if (i <= m_currentStep) {
            painter.setBrush(QColor(255, 255, 255, 220));
        } else {
            painter.setBrush(isDark ? QColor(255, 255, 255, 80) : QColor(148, 163, 184, 160));
        }
        painter.drawEllipse(QPointF(dotX, dotY), dotRadius, dotRadius);
    }

    // 5. Draw Thumb Knob (White Circle with subtle drop shadow)
    double knobRadius = 10.5;
    double knobY = h / 2.0;

    // Drop shadow
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, isDark ? 80 : 35));
    painter.drawEllipse(QPointF(handleX, knobY + 1.0), knobRadius + 0.5, knobRadius + 0.5);

    // White circle knob
    painter.setBrush(QColor("#ffffff"));
    if (!isDark) {
        painter.setPen(QPen(QColor("#cbd5e1"), 1.0));
    } else {
        painter.setPen(QPen(QColor(255, 255, 255, 60), 0.5));
    }
    painter.drawEllipse(QPointF(handleX, knobY), knobRadius, knobRadius);

    // Subtle inner highlight if hovered or dragged
    if (m_isHovered || m_isDragging) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(37, 99, 235, 35));
        painter.drawEllipse(QPointF(handleX, knobY), knobRadius, knobRadius);
    }
}

// -------------------------------------------------------------
// PillSliderWidget Implementation
// -------------------------------------------------------------

PillSliderWidget::PillSliderWidget(QWidget *parent)
    : QWidget(parent) {
    m_steps = {
        {tr("Smaller Size"), tr("High compression, fast sharing and minimum file size (~75% reduction)"), 30},
        {tr("Balanced (Recommended)"), tr("Significantly reduces file size while maintaining clarity (~55% reduction)"), 26},
        {tr("High Quality"), tr("Light compression for high-res and detailed videos (~35% reduction)"), 22},
        {tr("Ultra Quality"), tr("Closest to original quality, minimum lossy processing (~15% reduction)"), 18}
    };

    setupUi();
    updateLabels(1); // Default to Balanced (step 1)
}

void PillSliderWidget::setupUi() {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(5);

    // Top Header Row: Exact alignment matching track ends (trackMargin = 14)
    auto *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(14, 0, 14, 0);
    headerRow->setSpacing(0);

    const QString labelStyle = "font-size: 12px; font-weight: 600; color: palette(text); border: none; background: transparent;";

    m_leftHeaderLabel = new QLabel(tr("Smaller size"), this);
    m_leftHeaderLabel->setStyleSheet(labelStyle);
    m_leftHeaderLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_rightHeaderLabel = new QLabel(tr("Higher quality"), this);
    m_rightHeaderLabel->setStyleSheet(labelStyle);
    m_rightHeaderLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    headerRow->addWidget(m_leftHeaderLabel, 0, Qt::AlignVCenter);
    headerRow->addStretch(1);
    headerRow->addWidget(m_rightHeaderLabel, 0, Qt::AlignVCenter);
    mainLayout->addLayout(headerRow);

    // Middle Canvas Slider
    m_canvas = new PillSliderCanvas(static_cast<int>(m_steps.size()), this);
    m_canvas->setFixedHeight(38);
    mainLayout->addWidget(m_canvas);

    // Bottom Structured Preset Info Card
    auto *infoCard = new QFrame(this);
    infoCard->setObjectName("PresetInfoCard");
    infoCard->setFixedHeight(48);
    infoCard->setStyleSheet(R"(
        QFrame#PresetInfoCard {
            background-color: rgba(37, 99, 235, 0.08);
            border: 1px solid rgba(37, 99, 235, 0.25);
            border-radius: 8px;
            padding: 4px;
        }
    )");

    auto *descBox = new QHBoxLayout(infoCard);
    descBox->setContentsMargins(12, 6, 14, 6);
    descBox->setSpacing(12);

    m_badgeLabel = new QLabel(infoCard);
    m_badgeLabel->setStyleSheet(R"(
        QLabel {
            background-color: #2563eb;
            color: #ffffff;
            border-radius: 6px;
            padding: 5px 12px;
            font-size: 12.5px;
            font-weight: 700;
        }
    )");

    m_descLabel = new QLabel(infoCard);
    m_descLabel->setStyleSheet("font-size: 13px; color: palette(text); font-weight: 500;");
    m_descLabel->setWordWrap(true);

    descBox->addWidget(m_badgeLabel, 0, Qt::AlignVCenter);
    descBox->addWidget(m_descLabel, 1, Qt::AlignVCenter);
    mainLayout->addWidget(infoCard);

    connect(m_canvas, &PillSliderCanvas::stepChanged, this, [this](int step) {
        updateLabels(step);
        emit stepChanged(step);
        if (step >= 0 && step < static_cast<int>(m_steps.size())) {
            emit crfChanged(m_steps[step].crfValue);
        }
    });
}

QSize PillSliderWidget::sizeHint() const {
    return QSize(500, 126);
}

QSize PillSliderWidget::minimumSizeHint() const {
    return QSize(280, 126);
}

void PillSliderWidget::updateLabels(int step) {
    if (step < 0 || step >= static_cast<int>(m_steps.size())) return;

    const auto& info = m_steps[step];
    m_badgeLabel->setText(info.title);
    m_descLabel->setText(info.description);
}

int PillSliderWidget::currentStep() const {
    return m_canvas ? m_canvas->currentStep() : 1;
}

int PillSliderWidget::currentCrf() const {
    int step = currentStep();
    if (step >= 0 && step < static_cast<int>(m_steps.size())) {
        return m_steps[step].crfValue;
    }
    return 26;
}

void PillSliderWidget::setStep(int step) {
    if (m_canvas) {
        m_canvas->setCurrentStep(step, true);
    }
}

void PillSliderWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void PillSliderWidget::retranslateUi() {
    m_steps = {
        {tr("Smaller Size"), tr("High compression, fast sharing and minimum file size (~75% reduction)"), 30},
        {tr("Balanced (Recommended)"), tr("Significantly reduces file size while maintaining clarity (~55% reduction)"), 26},
        {tr("High Quality"), tr("Light compression for high-res and detailed videos (~35% reduction)"), 22},
        {tr("Ultra Quality"), tr("Closest to original quality, minimum lossy processing (~15% reduction)"), 18}
    };

    if (m_leftHeaderLabel) m_leftHeaderLabel->setText(tr("Smaller size"));
    if (m_rightHeaderLabel) m_rightHeaderLabel->setText(tr("Higher quality"));

    updateLabels(currentStep());
}

} // namespace LocalTools::UI
