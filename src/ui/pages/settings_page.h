#ifndef LOCALTOOLS_UI_PAGES_SETTINGS_PAGE_H
#define LOCALTOOLS_UI_PAGES_SETTINGS_PAGE_H

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>

#include "../../services/settings_service.h"

namespace LocalTools::UI {

class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage() override = default;

    /**
     * Reflects a theme change made elsewhere (e.g. the Home toggle) without
     * re-emitting themeChanged.
     */
    void syncThemeSelection(Services::AppTheme theme);

signals:
    void themeChanged(Services::AppTheme newTheme);
    void recentFilesCleared();

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void loadSettings();
    void retranslateUi();
    void showLegalDocument(const QString& resourcePath, const QString& title);

    QLabel* m_titleLabel{nullptr};

    // Appearance
    QGroupBox* m_appearanceGroup{nullptr};
    QLabel* m_themeLabel{nullptr};
    QComboBox* m_themeCombo{nullptr};
    QLabel* m_langLabel{nullptr};
    QComboBox* m_langCombo{nullptr};

    // Output
    QGroupBox* m_outputGroup{nullptr};
    QCheckBox* m_confirmOverwriteCheck{nullptr};
    QCheckBox* m_autoOpenCheck{nullptr};

    // Maintenance
    QGroupBox* m_storageGroup{nullptr};
    QLabel* m_tempTitle{nullptr};
    QLabel* m_tempDesc{nullptr};
    QCheckBox* m_autoCleanTempCheck{nullptr};
    QPushButton* m_clearTempButton{nullptr};
    QLabel* m_recentTitle{nullptr};
    QLabel* m_recentDesc{nullptr};
    QCheckBox* m_keepRecentFilesCheck{nullptr};
    QPushButton* m_clearRecentButton{nullptr};

    // Legal & privacy
    QGroupBox* m_legalGroup{nullptr};
    QLabel* m_legalDesc{nullptr};
    QPushButton* m_privacyButton{nullptr};
    QPushButton* m_noticesButton{nullptr};
    QPushButton* m_relinkingButton{nullptr};

    bool m_isUpdatingLang{false};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_SETTINGS_PAGE_H
