#ifndef LOCALTOOLS_UI_PAGES_OPTIMIZE_PAGE_H
#define LOCALTOOLS_UI_PAGES_OPTIMIZE_PAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QThread>

#include "../common/drop_zone.h"
#include "../common/status_banner.h"
#include "result_screen.h"
#include "../../workers/pdf_worker.h"

namespace LocalTools::UI {

class OptimizePage : public QWidget {
    Q_OBJECT

public:
    explicit OptimizePage(QWidget *parent = nullptr);
    ~OptimizePage() override;

    void loadPdfFile(const QStringList& filePaths);

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();
    void clearSelectedFile();
    void startCompression();

    QPushButton* m_tabBtn{nullptr};
    QLabel* m_titleLabel{nullptr};
    QLabel* m_descLabel{nullptr};
    QLabel* m_levelLabel{nullptr};

    DropZoneWidget* m_dropZone{nullptr};
    
    // Centered PDF File Card
    QWidget* m_pdfFileCard{nullptr};
    QLabel* m_pdfThumbnailLabel{nullptr};
    QLabel* m_pdfFileNameLabel{nullptr};
    QLabel* m_pdfFileDetailsLabel{nullptr};
    QPushButton* m_changePdfBtn{nullptr};
    QPushButton* m_removePdfBtn{nullptr};

    QComboBox* m_levelCombo{nullptr};
    StatusBannerWidget* m_statusBanner{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QPushButton* m_executeBtn{nullptr};

    QStackedWidget* m_stackedWidget{nullptr};
    ResultScreenWidget* m_resultScreen{nullptr};

    QString m_loadedFile;
    quint64 m_loadGeneration{0};
    QThread* m_workerThread{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_OPTIMIZE_PAGE_H
