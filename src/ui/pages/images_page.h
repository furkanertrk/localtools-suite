#ifndef LOCALTOOLS_UI_PAGES_IMAGES_PAGE_H
#define LOCALTOOLS_UI_PAGES_IMAGES_PAGE_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QProgressBar>
#include <QStringList>
#include <vector>

#include "../common/drop_zone.h"
#include "../common/status_banner.h"
#include "../common/icon_utils.h"
#include "result_screen.h"

namespace LocalTools::UI {

enum class ImageTabMode {
    CompressMode = 0,
    Convert = 1
};

class ImagesPage : public QWidget {
    Q_OBJECT

public:
    explicit ImagesPage(QWidget *parent = nullptr);
    ~ImagesPage() override = default;

    void setMode(ImageTabMode mode);
    void loadImages(const QStringList& filePaths);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void handleFilesDropped(const QStringList& files);
    void processCurrentImageAction();
    void resetWorkspace();

private:
    void setupUi();
    void retranslateUi();
    QWidget* createTabHeader();
    QWidget* createCompressPanel();
    QWidget* createConvertPanel();
    void updateFileDetails();
    void updateTabDescription(int id);

    // Tab buttons & Descriptions
    QPushButton* m_compressTabBtn{nullptr};
    QPushButton* m_convertTabBtn{nullptr};
    QButtonGroup* m_tabButtonGroup{nullptr};
    QLabel* m_tabDescriptionLabel{nullptr};

    // Sub-panels
    QStackedWidget* m_panelStack{nullptr};
    QWidget* m_compressPanel{nullptr};
    QWidget* m_convertPanel{nullptr};

    // File Workspace & Dropzone
    DropZoneWidget* m_dropZone{nullptr};
    QWidget* m_workspaceContainer{nullptr};
    QLabel* m_selectedFilesLabel{nullptr};
    QLabel* m_fileInfoLabel{nullptr};
    QLabel* m_imagePreviewLabel{nullptr};
    QPushButton* m_changeFileBtn{nullptr};
    QStringList m_loadedImages;

    // Original Image Dimensions & Stats
    int m_origWidth{0};
    int m_origHeight{0};
    qint64 m_origTotalBytes{0};
    bool m_updatingDimensions{false};

    // Compress controls
    QSlider* m_qualitySlider{nullptr};
    QLabel* m_qualityValueLabel{nullptr};
    QRadioButton* m_qualityRadio{nullptr};
    QRadioButton* m_targetSizeRadio{nullptr};
    QComboBox* m_targetSizeCombo{nullptr};

    // Convert controls
    QComboBox* m_formatCombo{nullptr};
    QWidget* m_convertQualityContainer{nullptr};
    QSlider* m_convertQualitySlider{nullptr};
    QLabel* m_convertQualityValueLabel{nullptr};
    QLabel* m_convertFormatNoteLabel{nullptr};

    // Execution & Results
    QPushButton* m_actionBtn{nullptr};
    QProgressBar* m_progressBar{nullptr};
    StatusBannerWidget* m_statusBanner{nullptr};
    ResultScreenWidget* m_resultScreen{nullptr};
    QStackedWidget* m_pageStack{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_IMAGES_PAGE_H
