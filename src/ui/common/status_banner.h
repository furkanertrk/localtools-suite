#ifndef LOCALTOOLS_UI_COMMON_STATUS_BANNER_H
#define LOCALTOOLS_UI_COMMON_STATUS_BANNER_H

#include <QFrame>
#include <QLabel>
#include <QPushButton>

namespace LocalTools::UI {

enum class BannerType {
    Info,
    Success,
    Warning,
    Error
};

class StatusBannerWidget : public QFrame {
    Q_OBJECT

public:
    explicit StatusBannerWidget(QWidget *parent = nullptr);
    ~StatusBannerWidget() override = default;

    void showBanner(BannerType type, const QString& message);
    void hideBanner();

private:
    void setupUi();

    QLabel* m_iconLabel{nullptr};
    QLabel* m_messageLabel{nullptr};
    QPushButton* m_closeButton{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_STATUS_BANNER_H
