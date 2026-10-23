#include "settings_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QEvent>
#include <QFile>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>

#include "../../services/settings_service.h"
#include "../../services/file_service.h"
#include "../../services/localization_service.h"
#include "../common/icon_utils.h"

namespace LocalTools::UI {

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    loadSettings();
}

void SettingsPage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 20, 28, 20);
    mainLayout->setSpacing(14);

    m_titleLabel = new QLabel(tr("Application Settings"), this);
    QFont font = m_titleLabel->font();
    font.setPointSize(18);
    font.setBold(true);
    m_titleLabel->setFont(font);
    mainLayout->addWidget(m_titleLabel);

    // Appearance Settings Group
    m_appearanceGroup = new QGroupBox(tr("Appearance & Theme"), this);
    auto *appLayout = new QFormLayout(m_appearanceGroup);
    appLayout->setSpacing(12);

    m_themeCombo = new QComboBox(this);
    m_themeLabel = new QLabel(tr("Theme Mode:"), this);
    appLayout->addRow(m_themeLabel, m_themeCombo);

    m_langCombo = new QComboBox(this);
    m_langLabel = new QLabel(tr("Language:"), this);
    appLayout->addRow(m_langLabel, m_langCombo);

    mainLayout->addWidget(m_appearanceGroup);

    // Output Options Group
    m_outputGroup = new QGroupBox(tr("Output Files & Save Location"), this);
    auto *outLayout = new QVBoxLayout(m_outputGroup);
    outLayout->setSpacing(10);

    m_confirmOverwriteCheck = new QCheckBox(tr("Confirm before overwriting existing files"), this);
    outLayout->addWidget(m_confirmOverwriteCheck);

    m_autoOpenCheck = new QCheckBox(tr("Automatically open output file when finished"), this);
    outLayout->addWidget(m_autoOpenCheck);

    mainLayout->addWidget(m_outputGroup);

    // Privacy & Storage Maintenance
    m_storageGroup = new QGroupBox(tr("Local Data Maintenance & Privacy"), this);
    auto *storageLayout = new QVBoxLayout(m_storageGroup);
    storageLayout->setSpacing(14);

    // Row 1: Temp Cache Files
    auto *tempRow = new QHBoxLayout();
    tempRow->setSpacing(12);
    auto *tempTextBox = new QVBoxLayout();
    tempTextBox->setSpacing(2);
    m_tempTitle = new QLabel(tr("Temporary Cache Files"), this);
    m_tempTitle->setStyleSheet("font-weight: 600; font-size: 13px;");
    m_tempDesc = new QLabel(tr("Deletes temporary working files created during conversions and operations to free up disk space."), this);
    m_tempDesc->setStyleSheet("font-size: 12px; color: palette(text); opacity: 0.7;");
    m_tempDesc->setWordWrap(true);
    tempTextBox->addWidget(m_tempTitle);
    tempTextBox->addWidget(m_tempDesc);
    tempRow->addLayout(tempTextBox, 1);

    m_autoCleanTempCheck = new QCheckBox(tr("Clean automatically when the app starts and closes"), this);
    tempRow->addWidget(m_autoCleanTempCheck);

    m_clearTempButton = new QPushButton(tr("Clear Cache"), this);
    m_clearTempButton->setObjectName("MaintenanceButton");
    m_clearTempButton->setIcon(IconUtils::getIcon(IconType::Delete, QColor("#e11d48"), 16));
    m_clearTempButton->setIconSize(QSize(16, 16));
    m_clearTempButton->setCursor(Qt::PointingHandCursor);
    m_clearTempButton->setFixedSize(160, 36);
    tempRow->addWidget(m_clearTempButton);
    storageLayout->addLayout(tempRow);

    // Separator line
    auto *sepLine = new QFrame(this);
    sepLine->setFrameShape(QFrame::HLine);
    sepLine->setFrameShadow(QFrame::Sunken);
    sepLine->setStyleSheet("color: palette(mid); margin: 4px 0;");
    storageLayout->addWidget(sepLine);

    // Row 2: Recent Files History
    auto *recentRow = new QHBoxLayout();
    recentRow->setSpacing(12);
    auto *recentTextBox = new QVBoxLayout();
    recentTextBox->setSpacing(2);
    m_recentTitle = new QLabel(tr("Recent Files History"), this);
    m_recentTitle->setStyleSheet("font-weight: 600; font-size: 13px;");
    m_recentDesc = new QLabel(tr("Clears recent file records listed on the home page."), this);
    m_recentDesc->setStyleSheet("font-size: 12px; color: palette(text); opacity: 0.7;");
    m_recentDesc->setWordWrap(true);
    recentTextBox->addWidget(m_recentTitle);
    recentTextBox->addWidget(m_recentDesc);
    recentRow->addLayout(recentTextBox, 1);

    m_keepRecentFilesCheck = new QCheckBox(tr("Keep recent file history"), this);
    recentRow->addWidget(m_keepRecentFilesCheck);

    m_clearRecentButton = new QPushButton(tr("Clear History"), this);
    m_clearRecentButton->setObjectName("MaintenanceButton");
    m_clearRecentButton->setIcon(IconUtils::getIcon(IconType::Delete, QColor("#64748b"), 16));
    m_clearRecentButton->setIconSize(QSize(16, 16));
    m_clearRecentButton->setCursor(Qt::PointingHandCursor);
    m_clearRecentButton->setFixedSize(160, 36);
    recentRow->addWidget(m_clearRecentButton);
    storageLayout->addLayout(recentRow);

    mainLayout->addWidget(m_storageGroup);

    m_legalGroup = new QGroupBox(tr("Privacy & Open-Source Licenses"), this);
    auto *legalLayout = new QHBoxLayout(m_legalGroup);
    legalLayout->setSpacing(12);
    m_legalDesc = new QLabel(
        tr("Review how local data is handled and the licenses of bundled components."),
        m_legalGroup);
    m_legalDesc->setWordWrap(true);
    legalLayout->addWidget(m_legalDesc, 1);

    m_privacyButton = new QPushButton(tr("Privacy Policy"), m_legalGroup);
    m_privacyButton->setAccessibleDescription(
        tr("Opens the LocalTools Suite privacy policy inside the application."));
    legalLayout->addWidget(m_privacyButton);

    m_noticesButton = new QPushButton(tr("Third-Party Notices"), m_legalGroup);
    m_noticesButton->setAccessibleDescription(
        tr("Opens licenses and attributions for bundled components."));
    legalLayout->addWidget(m_noticesButton);

    m_relinkingButton = new QPushButton(tr("Qt LGPL Instructions"), m_legalGroup);
    m_relinkingButton->setAccessibleDescription(
        tr("Opens instructions for replacing the LGPL-covered Qt libraries."));
    legalLayout->addWidget(m_relinkingButton);

    mainLayout->addWidget(m_legalGroup);
    mainLayout->addStretch();

    // Connect Theme Combo
    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_isUpdatingLang) return;
        auto theme = static_cast<Services::AppTheme>(m_themeCombo->currentData().toInt());
        Services::SettingsService::instance().setTheme(theme);
        emit themeChanged(theme);
    });

    // Connect Language Combo
    connect(m_langCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_isUpdatingLang) return;
        QString langCode = m_langCombo->currentData().toString();
        Services::LocalizationService::instance().setLanguageCode(langCode);
    });

    // Connect Checkboxes
    connect(m_confirmOverwriteCheck, &QCheckBox::toggled, this, [](bool checked) {
        Services::SettingsService::instance().setConfirmOverwrite(checked);
    });

    connect(m_autoOpenCheck, &QCheckBox::toggled, this, [](bool checked) {
        Services::SettingsService::instance().setAutoOpenOutput(checked);
    });

    connect(m_autoCleanTempCheck, &QCheckBox::toggled, this, [](bool checked) {
        Services::SettingsService::instance().setAutoCleanTemp(checked);
    });

    connect(m_keepRecentFilesCheck, &QCheckBox::toggled, this, [this](bool checked) {
        Services::SettingsService::instance().setKeepRecentFiles(checked);
        if (!checked) emit recentFilesCleared();
    });

    // Connect Maintenance Actions
    connect(m_clearTempButton, &QPushButton::clicked, this, [this]() {
        Services::FileService::instance().cleanTempDirectory();
        QMessageBox::information(this, tr("Information"), tr("Temporary working cache files cleared successfully."));
    });

    connect(m_clearRecentButton, &QPushButton::clicked, this, [this]() {
        Services::SettingsService::instance().clearRecentFiles();
        emit recentFilesCleared();
        QMessageBox::information(this, tr("Information"), tr("Recent files history cleared successfully."));
    });

    connect(m_privacyButton, &QPushButton::clicked, this, [this]() {
        const QString language = Services::LocalizationService::instance().currentLanguageCode();
        showLegalDocument(language == "tr" ? ":/legal/privacy_tr.md" : ":/legal/privacy_en.md",
                          tr("Privacy Policy"));
    });
    connect(m_noticesButton, &QPushButton::clicked, this, [this]() {
        showLegalDocument(":/legal/third_party.txt", tr("Third-Party Notices"));
    });
    connect(m_relinkingButton, &QPushButton::clicked, this, [this]() {
        showLegalDocument(":/legal/qt_relinking.md", tr("Qt LGPL Instructions"));
    });
    retranslateUi();
}

void SettingsPage::showLegalDocument(const QString& resourcePath, const QString& title) {
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Information"), tr("The requested document could not be opened."));
        return;
    }

    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(title);
    dialog->resize(820, 620);

    auto *layout = new QVBoxLayout(dialog);
    auto *browser = new QTextBrowser(dialog);
    browser->setOpenExternalLinks(false);
    browser->setReadOnly(true);
    const QString content = QString::fromUtf8(file.readAll());
    if (resourcePath.endsWith(".md")) {
        browser->setMarkdown(content);
    } else {
        browser->setPlainText(content);
    }
    browser->setAccessibleName(title);
    layout->addWidget(browser, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);
    dialog->open();
}

void SettingsPage::loadSettings() {
    m_isUpdatingLang = true;

    // Theme items
    m_themeCombo->clear();
    m_themeCombo->addItem(tr("System Default (Windows 11)"), static_cast<int>(Services::AppTheme::System));
    m_themeCombo->addItem(tr("Light Theme (Light Mode)"), static_cast<int>(Services::AppTheme::Light));
    m_themeCombo->addItem(tr("Dark Theme (Dark Mode)"), static_cast<int>(Services::AppTheme::Dark));

    auto currentTheme = Services::SettingsService::instance().getTheme();
    int themeIndex = m_themeCombo->findData(static_cast<int>(currentTheme));
    if (themeIndex >= 0) {
        m_themeCombo->setCurrentIndex(themeIndex);
    }

    // Language items
    m_langCombo->clear();
    m_langCombo->addItem("Türkçe (Turkish)", "tr");
    m_langCombo->addItem("English", "en");

    QString currentLang = Services::LocalizationService::instance().currentLanguageCode();
    int langIndex = m_langCombo->findData(currentLang);
    if (langIndex >= 0) {
        m_langCombo->setCurrentIndex(langIndex);
    }

    m_confirmOverwriteCheck->setChecked(Services::SettingsService::instance().getConfirmOverwrite());
    m_autoOpenCheck->setChecked(Services::SettingsService::instance().getAutoOpenOutput());
    m_autoCleanTempCheck->setChecked(Services::SettingsService::instance().getAutoCleanTemp());
    m_keepRecentFilesCheck->setChecked(Services::SettingsService::instance().getKeepRecentFiles());

    m_isUpdatingLang = false;
}

void SettingsPage::syncThemeSelection(Services::AppTheme theme) {
    if (!m_themeCombo) return;

    const int index = m_themeCombo->findData(static_cast<int>(theme));
    if (index < 0 || index == m_themeCombo->currentIndex()) return;

    // Guard against the currentIndexChanged handler re-emitting themeChanged,
    // which would bounce the update back to MainWindow.
    m_isUpdatingLang = true;
    m_themeCombo->setCurrentIndex(index);
    m_isUpdatingLang = false;
}

void SettingsPage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void SettingsPage::retranslateUi() {
    m_isUpdatingLang = true;

    m_titleLabel->setText(tr("Application Settings"));
    m_appearanceGroup->setTitle(tr("Appearance & Theme"));
    m_themeLabel->setText(tr("Theme Mode:"));
    m_langLabel->setText(tr("Language:"));

    int curTheme = m_themeCombo->currentData().toInt();
    m_themeCombo->clear();
    m_themeCombo->addItem(tr("System Default (Windows 11)"), static_cast<int>(Services::AppTheme::System));
    m_themeCombo->addItem(tr("Light Theme (Light Mode)"), static_cast<int>(Services::AppTheme::Light));
    m_themeCombo->addItem(tr("Dark Theme (Dark Mode)"), static_cast<int>(Services::AppTheme::Dark));
    int tIdx = m_themeCombo->findData(curTheme);
    if (tIdx >= 0) m_themeCombo->setCurrentIndex(tIdx);

    QString curLang = m_langCombo->currentData().toString();
    m_langCombo->clear();
    m_langCombo->addItem("Türkçe (Turkish)", "tr");
    m_langCombo->addItem("English", "en");
    int lIdx = m_langCombo->findData(curLang);
    if (lIdx >= 0) m_langCombo->setCurrentIndex(lIdx);

    m_outputGroup->setTitle(tr("Output Files & Save Location"));
    m_confirmOverwriteCheck->setText(tr("Confirm before overwriting existing files"));
    m_autoOpenCheck->setText(tr("Automatically open output file when finished"));

    m_storageGroup->setTitle(tr("Local Data Maintenance & Privacy"));
    m_tempTitle->setText(tr("Temporary Cache Files"));
    m_tempDesc->setText(tr("Deletes temporary working files created during conversions and operations to free up disk space."));
    m_autoCleanTempCheck->setText(tr("Clean automatically when the app starts and closes"));
    m_clearTempButton->setText(tr("Clear Cache"));

    m_recentTitle->setText(tr("Recent Files History"));
    m_recentDesc->setText(tr("Clears recent file records listed on the home page."));
    m_keepRecentFilesCheck->setText(tr("Keep recent file history"));
    m_clearRecentButton->setText(tr("Clear History"));

    m_legalGroup->setTitle(tr("Privacy & Open-Source Licenses"));
    m_legalDesc->setText(
        tr("Review how local data is handled and the licenses of bundled components."));
    m_privacyButton->setText(tr("Privacy Policy"));
    m_privacyButton->setAccessibleDescription(
        tr("Opens the LocalTools Suite privacy policy inside the application."));
    m_noticesButton->setText(tr("Third-Party Notices"));
    m_noticesButton->setAccessibleDescription(
        tr("Opens licenses and attributions for bundled components."));
    m_relinkingButton->setText(tr("Qt LGPL Instructions"));
    m_relinkingButton->setAccessibleDescription(
        tr("Opens instructions for replacing the LGPL-covered Qt libraries."));

    m_isUpdatingLang = false;
}

} // namespace LocalTools::UI
