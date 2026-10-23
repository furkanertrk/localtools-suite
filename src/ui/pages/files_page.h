#ifndef LOCALTOOLS_UI_PAGES_FILES_PAGE_H
#define LOCALTOOLS_UI_PAGES_FILES_PAGE_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QProgressBar>
#include <QButtonGroup>

#include "../common/drop_zone.h"
#include "../common/status_banner.h"
#include "../common/icon_utils.h"
#include "result_screen.h"
#include "../../core/backends/file_utils_backend.h"

namespace LocalTools::UI {

enum class FilesTabMode {
    HashCheck = 0
};

class FilesPage : public QWidget {
    Q_OBJECT

public:
    explicit FilesPage(QWidget *parent = nullptr);
    ~FilesPage() override = default;

    void setMode(FilesTabMode mode);

    void handleHashFileDropped(const QStringList& files);
    void checkHashMatch();
    void resetWorkspace();

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();
    QWidget* createTabHeader();
    QWidget* createHashPanel();

    // Tab buttons
    QPushButton* m_hashTabBtn{nullptr};
    QButtonGroup* m_tabButtonGroup{nullptr};
    QLabel* m_tabDescriptionLabel{nullptr};

    // Sub-panels
    QStackedWidget* m_panelStack{nullptr};
    QWidget* m_hashPanel{nullptr};


    DropZoneWidget* m_hashDropZone{nullptr};
    QString m_hashedFilePath;
    QLabel* m_hashFileNameLabel{nullptr};
    QLineEdit* m_md5Edit{nullptr};
    QLineEdit* m_sha1Edit{nullptr};
    QLineEdit* m_sha256Edit{nullptr};
    QLineEdit* m_sha512Edit{nullptr};
    QLineEdit* m_verifyHashInput{nullptr};
    QLabel* m_verifyResultLabel{nullptr};
    QWidget* m_hashResultContainer{nullptr};

    // Status Banner & Stack
    StatusBannerWidget* m_statusBanner{nullptr};
    QProgressBar* m_progressBar{nullptr};
    ResultScreenWidget* m_resultScreen{nullptr};
    QStackedWidget* m_pageStack{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_FILES_PAGE_H
