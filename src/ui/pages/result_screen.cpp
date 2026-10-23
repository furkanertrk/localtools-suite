#include "result_screen.h"

#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QEvent>

#include "../common/icon_utils.h"
#include "../../services/settings_service.h"

namespace LocalTools::UI {

ResultScreenWidget::ResultScreenWidget(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

void ResultScreenWidget::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);
    mainLayout->setSpacing(16);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(56, 56);
    m_iconLabel->setPixmap(IconUtils::getPixmap(IconType::Success, QColor("#2563eb"), 48));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_iconLabel, 0, Qt::AlignCenter);

    m_messageLabel = new QLabel(tr("Operation Completed Successfully!"), this);
    QFont msgFont = m_messageLabel->font();
    msgFont.setPointSize(18);
    msgFont.setBold(true);
    m_messageLabel->setFont(msgFont);
    m_messageLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_messageLabel);

    m_filePathLabel = new QLabel(this);
    m_filePathLabel->setStyleSheet("font-size: 13px; opacity: 0.7;");
    m_filePathLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_filePathLabel);

    mainLayout->addSpacing(16);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->setAlignment(Qt::AlignCenter);
    btnLayout->setSpacing(12);

    m_downloadBtn = new QPushButton(tr("Open File"), this);
    m_downloadBtn->setObjectName("PrimaryButton");
    m_downloadBtn->setIcon(IconUtils::getIcon(IconType::View, QColor("#ffffff"), 16));
    m_downloadBtn->setIconSize(QSize(16, 16));
    m_downloadBtn->setCursor(Qt::PointingHandCursor);
    m_downloadBtn->setFixedSize(140, 38);
    
    m_backBtn = new QPushButton(tr("New Task"), this);
    m_backBtn->setObjectName("ToolTabButton");
    m_backBtn->setIcon(IconUtils::getIcon(IconType::Organize, QColor("#64748b"), 16));
    m_backBtn->setIconSize(QSize(16, 16));
    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_backBtn->setFixedSize(140, 38);

    btnLayout->addWidget(m_downloadBtn);
    btnLayout->addWidget(m_backBtn);
    
    mainLayout->addLayout(btnLayout);

    connect(m_downloadBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentFilePath.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentFilePath));
        }
    });

    connect(m_backBtn, &QPushButton::clicked, this, &ResultScreenWidget::backToWorkspaceRequested);
}

void ResultScreenWidget::showResult(const QString& filePath, const QString& message) {
    m_currentFilePath = filePath;
    m_rawMessageKey = message;
    m_messageLabel->setText(tr(message.toUtf8().constData()));
    m_filePathLabel->setText(filePath);

    if (Services::SettingsService::instance().getAutoOpenOutput() && !filePath.isEmpty() && QFile::exists(filePath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    }
}

void ResultScreenWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ResultScreenWidget::retranslateUi() {
    if (m_messageLabel) m_messageLabel->setText(tr(m_rawMessageKey.toUtf8().constData()));
    if (m_downloadBtn) m_downloadBtn->setText(tr("Open File"));
    if (m_backBtn) m_backBtn->setText(tr("New Task"));
}

} // namespace LocalTools::UI
