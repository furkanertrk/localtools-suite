#ifndef LOCALTOOLS_UI_PAGES_DOCUMENTS_PAGE_H
#define LOCALTOOLS_UI_PAGES_DOCUMENTS_PAGE_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QProgressBar>

#include "../common/drop_zone.h"
#include "../common/status_banner.h"
#include "../common/icon_utils.h"
#include "result_screen.h"

namespace LocalTools::UI {

enum class DocumentTabMode {
    IdCopy = 0,
    PassportPhoto = 1
};

class DocumentsPage : public QWidget {
    Q_OBJECT

public:
    explicit DocumentsPage(QWidget *parent = nullptr);
    ~DocumentsPage() override = default;

    void setMode(DocumentTabMode mode);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void processCurrentAction();
    void resetWorkspace();

private:
    void setupUi();
    void retranslateUi();
    QWidget* createTabHeader();
    QWidget* createIdCopyPanel();
    QWidget* createPassportPhotoPanel();

    // Tab buttons & Description
    QPushButton* m_idCopyTabBtn{nullptr};
    QPushButton* m_passportTabBtn{nullptr};
    QButtonGroup* m_tabButtonGroup{nullptr};
    QLabel* m_tabDescriptionLabel{nullptr};
    void updateTabDescription(int id);

    // Sub-panels
    QStackedWidget* m_panelStack{nullptr};
    QWidget* m_idCopyPanel{nullptr};
    QWidget* m_passportPanel{nullptr};

    // --- ID Copy Controls ---
    QString m_frontImagePath;
    QString m_backImagePath;
    QLabel* m_frontStatusLabel{nullptr};
    QLabel* m_backStatusLabel{nullptr};
    QLabel* m_frontPreview{nullptr};
    QLabel* m_backPreview{nullptr};
    QPushButton* m_frontBtn{nullptr};
    QPushButton* m_backBtn{nullptr};
    QComboBox* m_idFormatCombo{nullptr};

    // --- Passport Photo Controls ---
    QString m_passportPhotoPath;
    QLabel* m_passportPhotoPreview{nullptr};
    QPushButton* m_passportSelectBtn{nullptr};
    QLabel* m_passportDescLabel{nullptr};
    QLabel* m_photoStandardLabel{nullptr};
    QLabel* m_photoCountLabel{nullptr};
    QLabel* m_printPaperLabel{nullptr};
    QLabel* m_outFormatLabel{nullptr};
    QComboBox* m_passportPresetCombo{nullptr};
    QComboBox* m_passportCountCombo{nullptr};
    QComboBox* m_passportPaperCombo{nullptr};
    QComboBox* m_passportOutFormatCombo{nullptr};

    // Execution & Status
    QPushButton* m_actionBtn{nullptr};
    QProgressBar* m_progressBar{nullptr};
    StatusBannerWidget* m_statusBanner{nullptr};
    ResultScreenWidget* m_resultScreen{nullptr};
    QStackedWidget* m_pageStack{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_DOCUMENTS_PAGE_H
