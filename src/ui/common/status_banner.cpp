#include "status_banner.h"

#include <QHBoxLayout>

#include "icon_utils.h"
#include "../theme.h"

namespace LocalTools::UI {

StatusBannerWidget::StatusBannerWidget(QWidget *parent)
    : QFrame(parent) {
    setupUi();
    hide();
}

void StatusBannerWidget::setupUi() {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(20, 20);
    layout->addWidget(m_iconLabel);

    m_messageLabel = new QLabel(this);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setStyleSheet("font-size: 13px; font-weight: 500;");
    layout->addWidget(m_messageLabel, 1);

    m_closeButton = new QPushButton("✕", this);
    m_closeButton->setFixedSize(20, 20);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setStyleSheet("border: none; background: transparent; font-weight: bold;");
    layout->addWidget(m_closeButton);

    connect(m_closeButton, &QPushButton::clicked, this, &StatusBannerWidget::hideBanner);
}

void StatusBannerWidget::showBanner(BannerType type, const QString& message) {
    m_messageLabel->setText(message);

    bool isDark = (palette().window().color().lightness() < 128);

    if (isDark) {
        switch (type) {
            case BannerType::Success:
                m_iconLabel->setPixmap(IconUtils::getPixmap(IconType::Success, QColor(Theme::SuccessTextDark), 18));
                setStyleSheet(QString("QFrame { background-color: %1; border: none; border-radius: %2; color: %3; padding: 2px 4px; }")
                                .arg(Theme::SuccessBgDark).arg(Theme::RadiusMedium).arg(Theme::SuccessTextDark));
                break;
            case BannerType::Warning:
                m_iconLabel->setPixmap(IconUtils::getPixmap(IconType::Warning, QColor(Theme::WarningTextDark), 18));
                setStyleSheet(QString("QFrame { background-color: %1; border: none; border-radius: %2; color: %3; padding: 2px 4px; }")
                                .arg(Theme::WarningBgDark).arg(Theme::RadiusMedium).arg(Theme::WarningTextDark));
                break;
            case BannerType::Error:
                m_iconLabel->setPixmap(IconUtils::getPixmap(IconType::Error, QColor(Theme::ErrorTextDark), 18));
                setStyleSheet(QString("QFrame { background-color: %1; border: none; border-radius: %2; color: %3; padding: 2px 4px; }")
                                .arg(Theme::ErrorBgDark).arg(Theme::RadiusMedium).arg(Theme::ErrorTextDark));
                break;
            case BannerType::Info:
            default:
                m_iconLabel->setPixmap(IconUtils::getPixmap(IconType::Info, QColor(Theme::InfoTextDark), 18));
                setStyleSheet(QString("QFrame { background-color: %1; border: none; border-radius: %2; color: %3; padding: 2px 4px; }")
                                .arg(Theme::InfoBgDark).arg(Theme::RadiusMedium).arg(Theme::InfoTextDark));
                break;
        }
    } else {
        switch (type) {
            case BannerType::Success:
                m_iconLabel->setPixmap(IconUtils::getPixmap(IconType::Success, QColor(Theme::SuccessTextLight), 18));
                setStyleSheet(QString("QFrame { background-color: %1; border: none; border-radius: %2; color: %3; padding: 2px 4px; }")
                                .arg(Theme::SuccessBgLight).arg(Theme::RadiusMedium).arg(Theme::SuccessTextLight));
                break;
            case BannerType::Warning:
                m_iconLabel->setPixmap(IconUtils::getPixmap(IconType::Warning, QColor(Theme::WarningTextLight), 18));
                setStyleSheet(QString("QFrame { background-color: %1; border: none; border-radius: %2; color: %3; padding: 2px 4px; }")
                                .arg(Theme::WarningBgLight).arg(Theme::RadiusMedium).arg(Theme::WarningTextLight));
                break;
            case BannerType::Error:
                m_iconLabel->setPixmap(IconUtils::getPixmap(IconType::Error, QColor(Theme::ErrorTextLight), 18));
                setStyleSheet(QString("QFrame { background-color: %1; border: none; border-radius: %2; color: %3; padding: 2px 4px; }")
                                .arg(Theme::ErrorBgLight).arg(Theme::RadiusMedium).arg(Theme::ErrorTextLight));
                break;
            case BannerType::Info:
            default:
                m_iconLabel->setPixmap(IconUtils::getPixmap(IconType::Info, QColor(Theme::InfoTextLight), 18));
                setStyleSheet(QString("QFrame { background-color: %1; border: none; border-radius: %2; color: %3; padding: 2px 4px; }")
                                .arg(Theme::InfoBgLight).arg(Theme::RadiusMedium).arg(Theme::InfoTextLight));
                break;
        }
    }

    show();
}

void StatusBannerWidget::hideBanner() {
    hide();
}

} // namespace LocalTools::UI
