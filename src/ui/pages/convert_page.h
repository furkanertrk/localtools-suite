#ifndef LOCALTOOLS_UI_PAGES_CONVERT_PAGE_H
#define LOCALTOOLS_UI_PAGES_CONVERT_PAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QThread>
#include <QListWidget>

#include "../common/drop_zone.h"
#include "../common/status_banner.h"
#include "result_screen.h"
#include "../../workers/pdf_worker.h"

namespace LocalTools::UI {

enum class ConvertMode {
    ImagesToPdf = 1,
    PdfToImages = 2,
    PdfToText = 3
};

enum class InputState {
    Empty,
    Selected
};

class ConvertPage : public QWidget {
    Q_OBJECT

public:
    explicit ConvertPage(QWidget *parent = nullptr);
    ~ConvertPage() override;

    void setMode(ConvertMode mode);
    void loadFiles(const QStringList& files);

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();
    void updateModeHeader(ConvertMode mode);
    void updateInputState();
    void updateImageList();
    void moveSelectedImageIndex(qsizetype index, int offset);
    void removeImageAtIndex(qsizetype index);
    void clearSelectedFiles();
    void startConversion();

    ConvertMode m_currentMode{ConvertMode::ImagesToPdf};
    QStringList m_selectedFiles;

    QLabel* m_titleLabel{nullptr};
    QLabel* m_descLabel{nullptr};

    QButtonGroup* m_modeGroup{nullptr};
    QPushButton* m_imgToPdfBtn{nullptr};
    QPushButton* m_pdfToImgBtn{nullptr};
    QPushButton* m_pdfToTextBtn{nullptr};

    DropZoneWidget* m_dropZone{nullptr};
    
    // Compact PDF Input File Card
    QWidget* m_pdfFileCard{nullptr};
    QLabel* m_pdfThumbnailLabel{nullptr};
    QLabel* m_pdfFileIconLabel{nullptr};
    QLabel* m_pdfFileNameLabel{nullptr};
    QLabel* m_pdfFileDetailsLabel{nullptr};
    QLabel* m_pdfBadgeLabel{nullptr};
    QPushButton* m_changePdfBtn{nullptr};
    QPushButton* m_removePdfBtn{nullptr};

    QWidget* m_imageListContainer{nullptr};
    QLabel* m_imageListTitleLabel{nullptr};
    QListWidget* m_imageList{nullptr};
    QPushButton* m_addImagesBtn{nullptr};
    QPushButton* m_clearImagesBtn{nullptr};

    QWidget* m_optionsContainer{nullptr};
    QComboBox* m_formatCombo{nullptr};
    QComboBox* m_dpiCombo{nullptr};

    StatusBannerWidget* m_statusBanner{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QPushButton* m_executeBtn{nullptr};

    QStackedWidget* m_stackedWidget{nullptr};
    ResultScreenWidget* m_resultScreen{nullptr};

    quint64 m_loadGeneration{0};
    QThread* m_workerThread{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_CONVERT_PAGE_H
