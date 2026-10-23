#ifndef LOCALTOOLS_UI_COMMON_PILL_SLIDER_H
#define LOCALTOOLS_UI_COMMON_PILL_SLIDER_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <vector>

namespace LocalTools::UI {

struct SliderStepInfo {
    QString title;
    QString description;
    int crfValue;
};

class PillSliderCanvas : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double handlePos READ handlePos WRITE setHandlePos)

public:
    explicit PillSliderCanvas(int stepCount = 4, QWidget *parent = nullptr);

    int stepCount() const { return m_stepCount; }
    int currentStep() const { return m_currentStep; }
    void setCurrentStep(int step, bool animated = true);

    double handlePos() const { return m_handlePos; }
    void setHandlePos(double pos);

signals:
    void stepChanged(int step);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    QSize sizeHint() const override;

private:
    int stepFromX(int mouseX) const;
    double xForStep(int step) const;

    int m_stepCount{4};
    int m_currentStep{1};
    double m_handlePos{1.0}; // from 0.0 to (m_stepCount - 1)
    bool m_isDragging{false};
    bool m_isHovered{false};
    QPropertyAnimation* m_animation{nullptr};
};

class PillSliderWidget : public QWidget {
    Q_OBJECT

public:
    explicit PillSliderWidget(QWidget *parent = nullptr);

    int currentStep() const;
    int currentCrf() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setStep(int step);

signals:
    void stepChanged(int step);
    void crfChanged(int crf);

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();
    void updateLabels(int step);

    PillSliderCanvas* m_canvas{nullptr};
    QLabel* m_leftHeaderLabel{nullptr};
    QLabel* m_rightHeaderLabel{nullptr};
    QLabel* m_badgeLabel{nullptr};
    QLabel* m_descLabel{nullptr};

    std::vector<SliderStepInfo> m_steps;
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_PILL_SLIDER_H
