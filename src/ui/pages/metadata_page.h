#ifndef LOCALTOOLS_UI_PAGES_METADATA_PAGE_H
#define LOCALTOOLS_UI_PAGES_METADATA_PAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QTableWidget>
#include <QProgressBar>
#include <QFrame>

#include "../common/drop_zone.h"
#include "../common/status_banner.h"
#include "result_screen.h"
#include "../../core/backend/pdfium_backend.h"

namespace LocalTools::UI {

/**
 * Every metadata operation in the product lives on this page.
 *
 * View covers read-only metadata for images and PDFs; Edit covers PDF document
 * properties; Inspect & Clean removes EXIF/GPS/device metadata. The latter used to be
 * a third tab inside Quick Tools, which split "metadata" across two unrelated
 * sections of the app.
 */
enum class MetadataMode {
    View = 1,
    Edit = 2,
    Clean = 3
};

class MetadataPage : public QWidget {
    Q_OBJECT

public:
    explicit MetadataPage(QWidget *parent = nullptr);
    ~MetadataPage() override = default;

    void loadPdfFile(const QStringList& filePaths);
    /** Loads any file into the Inspect &amp; Clean tab, switching to it first. */
    void loadMetadataFile(const QString& filePath);
    void setMode(MetadataMode mode);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void onMetadataFilesDropped(const QStringList& paths);
    void cleanCurrentMetadata();
    void resetMetadataWorkspace();

private:
    void setupUi();
    void retranslateUi();
    void clearSelectedFile();
    void saveMetadata();
    QWidget* createPdfPanel();
    QWidget* createCleanPanel();
    void refreshTabIcons();

    MetadataMode m_currentMode{MetadataMode::View};
    QString m_loadedFile;
    QLabel* m_titleLabel{nullptr};
    QLabel* m_descLabel{nullptr};

    QButtonGroup* m_modeGroup{nullptr};
    QPushButton* m_viewBtn{nullptr};
    QPushButton* m_editBtn{nullptr};
    QPushButton* m_cleanBtn{nullptr};

    /** 0 = PDF properties (View/Edit), 1 = Inspect &amp; Clean. */
    QStackedWidget* m_panelStack{nullptr};

    DropZoneWidget* m_dropZone{nullptr};
    StatusBannerWidget* m_statusBanner{nullptr};

    QWidget* m_formContainer{nullptr};
    
    // Centered PDF File Card Header
    QWidget* m_pdfFileCard{nullptr};
    QLabel* m_pdfThumbnailLabel{nullptr};
    QLabel* m_pdfFileNameLabel{nullptr};
    QLabel* m_pdfFileDetailsLabel{nullptr};
    QPushButton* m_changePdfBtn{nullptr};
    QPushButton* m_removePdfBtn{nullptr};

    QLineEdit* m_titleEdit{nullptr};
    QLineEdit* m_authorEdit{nullptr};
    QLineEdit* m_subjectEdit{nullptr};
    QLineEdit* m_keywordsEdit{nullptr};
    QLineEdit* m_creatorEdit{nullptr};
    QLineEdit* m_producerEdit{nullptr};

    QPushButton* m_saveButton{nullptr};

    QStackedWidget* m_stackedWidget{nullptr};
    ResultScreenWidget* m_resultScreen{nullptr};
    QProgressBar* m_progressBar{nullptr};

    // --- Inspect & Clean tab ---
    DropZoneWidget* m_metaDropZone{nullptr};
    QFrame* m_metaWorkspaceContainer{nullptr};
    QLabel* m_metaThumbLabel{nullptr};
    QLabel* m_metaFileNameLabel{nullptr};
    QLabel* m_metaFileSizeLabel{nullptr};
    QLabel* m_metaStatusBadge{nullptr};
    QTableWidget* m_metaTable{nullptr};
    QPushButton* m_metaCleanBtn{nullptr};
    QPushButton* m_metaChangeFileBtn{nullptr};
    QLabel* m_metaPrivacyNoteLabel{nullptr};
    QString m_currentMetaFilePath;
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_METADATA_PAGE_H
