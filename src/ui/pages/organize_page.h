#ifndef LOCALTOOLS_UI_PAGES_ORGANIZE_PAGE_H
#define LOCALTOOLS_UI_PAGES_ORGANIZE_PAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QProgressBar>
#include <QThread>

#include "../common/drop_zone.h"
#include "../common/file_card.h"
#include "../common/status_banner.h"
#include "../common/thumbnail_grid.h"
#include "../common/page_workspace.h"
#include "../../workers/pdf_worker.h"
#include "../../core/state/workspace_state.h"
#include "result_screen.h"
#include <QStackedWidget>

namespace LocalTools::UI {

class OrganizePage : public QWidget {
    Q_OBJECT

public:
    explicit OrganizePage(QWidget *parent = nullptr);
    ~OrganizePage() override;

    void loadPdfFiles(const QStringList& filePaths);
    void setActiveTab(int toolId);
    void clearWorkspace();
    /// Rebuilds icons and token-dependent local styling after a theme change.
    void refreshThemeIcons();

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();
    void updateControlsState(int selected, int total);
    void updateTabHeader(int tabId);
    void runWorkerOperation(Workers::PdfWorker* worker, const QString& expectedOutputFile);

    QLabel* m_titleLabel{nullptr};
    QLabel* m_descLabel{nullptr};

    DropZoneWidget* m_dropZone{nullptr};
    StatusBannerWidget* m_statusBanner{nullptr};
    QStackedWidget* m_stackedWidget{nullptr};
    PageWorkspace* m_pageWorkspace{nullptr};
    ThumbnailGridWidget* m_thumbnailGrid{nullptr};
    ResultScreenWidget* m_resultScreen{nullptr};
    QProgressBar* m_progressBar{nullptr};

    QLabel* m_selectionInfoLabel{nullptr};

    QButtonGroup* m_tabGroup{nullptr};
    QPushButton* m_mergeBtn{nullptr};
    QPushButton* m_splitBtn{nullptr};
    QPushButton* m_extractBtn{nullptr};
    QPushButton* m_deleteBtn{nullptr};
    QPushButton* m_reorderBtn{nullptr};
    QPushButton* m_rotateBtn{nullptr};
    QPushButton* m_selectAllBtn{nullptr};
    QPushButton* m_clearBtn{nullptr};
    QPushButton* m_moveLeftBtn{nullptr};
    QPushButton* m_moveRightBtn{nullptr};
    QPushButton* m_rotateLeftBtn{nullptr};
    QPushButton* m_rotateRightBtn{nullptr};
    QPushButton* m_rotate180Btn{nullptr};

    QWidget* m_splitOptionsWidget{nullptr};
    QRadioButton* m_splitAllRadio{nullptr};
    QRadioButton* m_splitNRadio{nullptr};
    QSpinBox* m_splitNSpin{nullptr};
    QRadioButton* m_splitRangeRadio{nullptr};
    QLineEdit* m_splitRangeEdit{nullptr};

    QPushButton* m_executeBtn{nullptr};

    QStringList m_loadedFiles;
    quint64 m_loadGeneration{0};
    QThread* m_workerThread{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_ORGANIZE_PAGE_H
