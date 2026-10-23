#ifndef LOCALTOOLS_UI_PAGES_QUICK_TOOLS_PAGE_H
#define LOCALTOOLS_UI_PAGES_QUICK_TOOLS_PAGE_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QButtonGroup>
#include <QProgressBar>
#include <QFrame>
#include <QTreeWidget>
#include <QTableWidget>

#include "../common/status_banner.h"
#include "../common/drop_zone.h"
#include "../common/icon_utils.h"
#include "result_screen.h"
#include "../../core/backends/quick_tools_backend.h"

namespace LocalTools::UI {

enum class QuickTabMode {
    QrCode = 0,
    QrReader = 1
};

class QuickToolsPage : public QWidget {
    Q_OBJECT

public:
    explicit QuickToolsPage(QWidget *parent = nullptr);
    ~QuickToolsPage() override = default;

    void setMode(QuickTabMode mode);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void updateQrPreview();
    void saveQrCode();
    void resetWorkspace();
    void onQrReaderFilesDropped(const QStringList& paths);
    void resetQrReader();
    void copyDecodedText();
    void openDecodedUrl();
    void executeDynamicQrAction();

private:
    void setupUi();
    void retranslateUi();
    QWidget* createTabHeader();
    QWidget* createQrPanel();
    QWidget* createQrReaderPanel();
    void applyQrReaderResult(const QString& imagePath,
                             const QImage& thumbnail,
                             const QSize& imageSize,
                             const Core::QrDecodeResult& decodeResult);

    // Tab buttons
    QPushButton* m_qrGenTabBtn{nullptr};
    QPushButton* m_qrReadTabBtn{nullptr};
    QButtonGroup* m_tabButtonGroup{nullptr};

    // Sub-panels
    QStackedWidget* m_panelStack{nullptr};
    QWidget* m_qrGenPanel{nullptr};
    QWidget* m_qrReadPanel{nullptr};

    // General
    QProgressBar* m_progressBar{nullptr};
    ResultScreenWidget* m_resultScreen{nullptr};
    QStackedWidget* m_pageStack{nullptr};
    StatusBannerWidget* m_statusBanner{nullptr};
    QImage m_currentQrImage;

    // --- QR Generator Controls ---
    QLineEdit* m_qrInputEdit{nullptr};
    QLabel* m_qrPreviewLabel{nullptr};
    QComboBox* m_qrSizeCombo{nullptr};
    QLabel* m_qrSizeLabel{nullptr};
    QPushButton* m_qrCopyBtn{nullptr};
    QPushButton* m_qrSaveBtn{nullptr};
    QLabel* m_qrInputLabel{nullptr};
    QLabel* m_qrPreviewTitleLabel{nullptr};

    // --- QR Reader Controls ---
    DropZoneWidget* m_qrReaderDropZone{nullptr};
    QFrame* m_qrReaderWorkspaceContainer{nullptr};
    QLabel* m_qrReaderThumbLabel{nullptr};
    QLabel* m_qrReaderFileNameLabel{nullptr};
    QLabel* m_qrReaderMetaLabel{nullptr};
    QLabel* m_qrReaderResultTitleLabel{nullptr};
    QPlainTextEdit* m_qrReaderResultEdit{nullptr};
    QPushButton* m_qrReaderCopyBtn{nullptr};
    QPushButton* m_qrReaderOpenUrlBtn{nullptr};
    QPushButton* m_qrReaderNewScanBtn{nullptr};
    QPushButton* m_qrReaderRemoveBtn{nullptr};
    QPushButton* m_qrReaderActionBtn{nullptr};
    QString m_decodedQrText;
    QString m_currentReaderImagePath;

    enum class QrActionType { None, CopyWifiPassword, SaveVCard };
    QrActionType m_dynamicQrActionType = QrActionType::None;
    QString m_dynamicQrActionData;
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_QUICK_TOOLS_PAGE_H
