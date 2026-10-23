#include "optimize_page.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QFrame>

#include "../common/icon_utils.h"
#include "../common/async_task.h"
#include "../common/output_file_dialog.h"
#include "../theme.h"
#include "../../core/backend/pdfium_backend.h"

namespace LocalTools::UI {

namespace {
struct OptimizePreviewResult {
    int pageCount{0};
    QImage firstPage;
    bool opened{false};
};
}

static QString formatFileSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    return QString("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 2));
}

OptimizePage::OptimizePage(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

OptimizePage::~OptimizePage() {
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void OptimizePage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 20, 28, 20);
    mainLayout->setSpacing(14);

    // 1. Top Tab Button
    auto *topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    m_tabBtn = new QPushButton(tr("Compress PDF"), this);
    m_tabBtn->setObjectName("ToolTabButton");
    m_tabBtn->setChecked(true);
    m_tabBtn->setCheckable(true);
    m_tabBtn->setFocusPolicy(Qt::StrongFocus);
    m_tabBtn->setIcon(IconUtils::getIcon(IconType::Compress, Theme::color(ColorToken::TextSecondary), 16));
    m_tabBtn->setIconSize(QSize(16, 16));
    m_tabBtn->setCursor(Qt::PointingHandCursor);
    topRow->addWidget(m_tabBtn);
    topRow->addStretch();
    mainLayout->addLayout(topRow);

    // 2. Tab Description Subtitle
    m_descLabel = new QLabel(tr("Reduce PDF file size locally without sacrificing visual quality."), this);
    m_descLabel->setStyleSheet("font-size: 13px; color: palette(text); opacity: 0.75; margin-bottom: 2px;");
    m_descLabel->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_descLabel);

    // Status Banner
    m_statusBanner = new StatusBannerWidget(this);
    m_statusBanner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    mainLayout->addWidget(m_statusBanner);

    // Progress Bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    // Stacked Widget: Workspace vs Result Screen
    m_stackedWidget = new QStackedWidget(this);

    auto *workspaceContainer = new QWidget(this);
    auto *workspaceLayout = new QVBoxLayout(workspaceContainer);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(12);

    // DropZone
    m_dropZone = new DropZoneWidget(workspaceContainer);
    m_dropZone->setInstruction(tr("Drop PDF File to Compress Here"), tr("Your original file will be preserved and not overwritten"));
    m_dropZone->setAcceptedFileExtensions(QStringList() << "pdf", tr("Select PDF File"));
    connect(m_dropZone, &DropZoneWidget::filesDropped, this, &OptimizePage::loadPdfFile);
    workspaceLayout->addWidget(m_dropZone);

    // Centered PDF File Card
    auto *cardCenterWrapper = new QWidget(workspaceContainer);
    auto *wrapperLayout = new QHBoxLayout(cardCenterWrapper);
    wrapperLayout->setContentsMargins(0, 16, 0, 16);
    wrapperLayout->setAlignment(Qt::AlignCenter);

    m_pdfFileCard = new QWidget(cardCenterWrapper);
    m_pdfFileCard->setObjectName("PdfFileCard");
    m_pdfFileCard->setMinimumWidth(260);
    m_pdfFileCard->setMaximumWidth(680);
    // Styled by the application stylesheet via its objectName. A local sheet
    // would override the app one for this subtree, and its QLabel rule carried
    // no colour - which is what rendered these labels black in dark mode.

    auto *cardLayout = new QHBoxLayout(m_pdfFileCard);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(14);
    cardLayout->setAlignment(Qt::AlignVCenter);

    // First-page PDF thumbnail (60x80px)
    m_pdfThumbnailLabel = new QLabel(m_pdfFileCard);
    m_pdfThumbnailLabel->setFixedSize(60, 80);
    m_pdfThumbnailLabel->setStyleSheet("border: 1px solid palette(mid); border-radius: 4px; background: palette(base);");
    m_pdfThumbnailLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_pdfThumbnailLabel, 0, Qt::AlignVCenter);

    auto *cardTextLayout = new QVBoxLayout();
    cardTextLayout->setSpacing(4);
    cardTextLayout->setContentsMargins(0, 0, 0, 0);

    m_pdfFileNameLabel = new QLabel(m_pdfFileCard);
    QFont fnFont = m_pdfFileNameLabel->font();
    fnFont.setPointSize(13);
    fnFont.setBold(true);
    m_pdfFileNameLabel->setFont(fnFont);

    m_pdfFileDetailsLabel = new QLabel(m_pdfFileCard);
    m_pdfFileDetailsLabel->setStyleSheet("font-size: 12px; opacity: 0.75;");

    cardTextLayout->addWidget(m_pdfFileNameLabel);
    cardTextLayout->addWidget(m_pdfFileDetailsLabel);
    cardLayout->addLayout(cardTextLayout, 1);

    auto *cardActionsLayout = new QHBoxLayout();
    cardActionsLayout->setSpacing(8);

    m_changePdfBtn = new QPushButton(tr("Change"), m_pdfFileCard);
    m_changePdfBtn->setObjectName("ToolTabButton");
    m_changePdfBtn->setCursor(Qt::PointingHandCursor);

    m_removePdfBtn = new QPushButton(tr("Remove"), m_pdfFileCard);
    m_removePdfBtn->setObjectName("ToolTabButton");
    m_removePdfBtn->setCursor(Qt::PointingHandCursor);

    cardActionsLayout->addWidget(m_changePdfBtn);
    cardActionsLayout->addWidget(m_removePdfBtn);
    cardLayout->addLayout(cardActionsLayout, 0);

    wrapperLayout->addWidget(m_pdfFileCard);
    cardCenterWrapper->hide();
    workspaceLayout->addWidget(cardCenterWrapper, 1);

    connect(m_changePdfBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Select PDF Document"), QString(), tr("PDF Files (*.pdf)"));
        if (!path.isEmpty()) {
            loadPdfFile(QStringList() << path);
        }
    });
    connect(m_removePdfBtn, &QPushButton::clicked, this, &OptimizePage::clearSelectedFile);

    // Integrated Footer Action Bar
    auto *footerBar = new QFrame(workspaceContainer);
    footerBar->setObjectName("FooterBar");
    footerBar->setStyleSheet(R"(
        QFrame#FooterBar {
            border-top: 1px solid palette(midlight);
            background-color: transparent;
        }
    )");
    auto *footerLayout = new QHBoxLayout(footerBar);
    footerLayout->setContentsMargins(0, 8, 0, 0);
    footerLayout->setSpacing(12);

    m_levelLabel = new QLabel(tr("Compression Level:"), footerBar);
    m_levelLabel->setStyleSheet("font-size: 13px; font-weight: bold;");
    footerLayout->addWidget(m_levelLabel);

    m_levelCombo = new QComboBox(footerBar);
    m_levelCombo->addItem(tr("Balanced (Standard Quality & Size)"), 1);
    m_levelCombo->addItem(tr("High Quality (Low Compression)"), 0);
    m_levelCombo->addItem(tr("Smallest Size (High Compression)"), 2);
    m_levelCombo->setCurrentIndex(0);
    footerLayout->addWidget(m_levelCombo, 1);

    m_executeBtn = new QPushButton(tr("Compress"), footerBar);
    m_executeBtn->setObjectName("PrimaryButton");
    // Minimum, not fixed: Turkish labels run up to ~1.6x the English width.
    m_executeBtn->setMinimumSize(180, 40);
    m_executeBtn->setCursor(Qt::PointingHandCursor);
    footerLayout->addWidget(m_executeBtn, 0);

    connect(m_executeBtn, &QPushButton::clicked, this, &OptimizePage::startCompression);
    workspaceLayout->addWidget(footerBar);

    m_stackedWidget->addWidget(workspaceContainer);

    // Result Screen
    m_resultScreen = new ResultScreenWidget(this);
    m_stackedWidget->addWidget(m_resultScreen);

    connect(m_resultScreen, &ResultScreenWidget::backToWorkspaceRequested, this, [this]() {
        clearSelectedFile();
        m_stackedWidget->setCurrentIndex(0);
        m_statusBanner->hideBanner();
        m_progressBar->hide();
    });

    mainLayout->addWidget(m_stackedWidget, 1);
    mainLayout->addStretch(1);
    retranslateUi();
}

void OptimizePage::clearSelectedFile() {
    ++m_loadGeneration;
    m_loadedFile.clear();
    if (m_pdfFileCard && m_pdfFileCard->parentWidget()) {
        m_pdfFileCard->parentWidget()->hide();
    }
    if (m_dropZone) {
        m_dropZone->show();
    }
}

void OptimizePage::loadPdfFile(const QStringList& filePaths) {
    if (filePaths.isEmpty()) return;

    QString pdfPath;
    for (const auto& path : filePaths) {
        if (path.endsWith(".pdf", Qt::CaseInsensitive)) {
            pdfPath = path;
            break;
        }
    }

    if (pdfPath.isEmpty()) {
        m_statusBanner->showBanner(BannerType::Error, tr("Please select a valid PDF file."));
        return;
    }

    m_loadedFile = pdfPath;
    const QFileInfo fi(pdfPath);
    const qint64 size = fi.size();
    const quint64 generation = ++m_loadGeneration;

    m_pdfThumbnailLabel->setPixmap(IconUtils::getPixmap(
        IconType::Compress, Theme::color(ColorToken::Accent), 32));
    m_pdfFileNameLabel->setText(fi.fileName());
    m_pdfFileDetailsLabel->setText(tr("Loading..."));

    m_dropZone->hide();
    if (m_pdfFileCard && m_pdfFileCard->parentWidget()) {
        m_pdfFileCard->parentWidget()->show();
    }
    m_statusBanner->hideBanner();

    AsyncTask::run<OptimizePreviewResult>(
        this,
        [pdfPath](AsyncTask::CancelCheck cancelled) {
            OptimizePreviewResult result;
            if (cancelled()) return result;
            Core::PdfiumBackend backend;
            if (!backend.openDocument(pdfPath.toStdString())) return result;
            result.opened = true;
            result.pageCount = backend.getPageCount();
            if (result.pageCount > 0 && !cancelled()) {
                result.firstPage = backend.renderPage(0, 120, 160);
            }
            backend.closeDocument();
            return result;
        },
        [this, generation, pdfPath, size](OptimizePreviewResult result) {
            if (generation != m_loadGeneration || m_loadedFile != pdfPath) return;
            if (!result.firstPage.isNull()) {
                m_pdfThumbnailLabel->setPixmap(QPixmap::fromImage(result.firstPage).scaled(
                    60, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            if (result.pageCount > 0) {
                m_pdfFileDetailsLabel->setText(
                    tr("%1 pages · %2").arg(result.pageCount).arg(formatFileSize(size)));
            } else {
                m_pdfFileDetailsLabel->setText(formatFileSize(size));
            }
            if (!result.opened) {
                m_statusBanner->showBanner(BannerType::Error,
                    tr("The PDF could not be opened. It may be corrupted or password protected."));
            }
        });
}

void OptimizePage::startCompression() {
    if (m_loadedFile.isEmpty()) {
        m_statusBanner->showBanner(BannerType::Warning, tr("Please upload a PDF file to compress first."));
        return;
    }

    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/LocalTools";
    QDir().mkpath(defaultDir);

    QFileInfo fi(m_loadedFile);
    QString suggestedName = defaultDir + "/" + fi.completeBaseName() + "_compressed.pdf";

    QString targetFile = OutputFileDialog::getSaveFileName(this, tr("Save Compressed PDF"), suggestedName, tr("PDF Files (*.pdf)"), {m_loadedFile});
    if (targetFile.isEmpty()) return;

    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }

    m_workerThread = new QThread(this);
    auto *worker = new Workers::PdfWorker();
    worker->moveToThread(m_workerThread);

    int level = m_levelCombo->currentData().toInt();
    worker->prepareCompress(m_loadedFile, targetFile, level);

    connect(m_workerThread, &QThread::started, worker, &Workers::PdfWorker::process);
    connect(worker, &Workers::PdfWorker::progressChanged, m_progressBar, &QProgressBar::setValue);
    connect(worker, &Workers::PdfWorker::statusMessage, this, [this](const QString& msg) {
        m_statusBanner->showBanner(BannerType::Info, msg);
    });

    m_progressBar->setValue(0);
    m_progressBar->show();
    m_executeBtn->setEnabled(false);

    connect(worker, &Workers::PdfWorker::finished, this, [this, worker, targetFile](const Core::Result<void>& result) {
        m_executeBtn->setEnabled(true);
        m_progressBar->hide();
        if (result.hasValue()) {
            m_statusBanner->hideBanner();
            QFileInfo origFi(m_loadedFile);
            QFileInfo compFi(targetFile);
            qint64 origSize = origFi.size();
            qint64 compSize = compFi.size();
            int reduction = 0;
            if (origSize > 0 && compSize < origSize) {
                reduction = static_cast<int>((1.0 - (double)compSize / origSize) * 100.0);
            }

            QString msg = tr("PDF compressed successfully! (%1 ➔ %2, %%3 savings)").arg(formatFileSize(origSize)).arg(formatFileSize(compSize)).arg(reduction);
            m_resultScreen->showResult(targetFile, msg);
            m_stackedWidget->setCurrentWidget(m_resultScreen);
        } else {
            m_statusBanner->showBanner(BannerType::Error, tr("Compression failed: ") + QString::fromStdString(result.error().message));
        }
        m_workerThread->quit();
        worker->deleteLater();
    });

    m_workerThread->start();
}

void OptimizePage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void OptimizePage::retranslateUi() {
    if (m_tabBtn) m_tabBtn->setText(tr("Compress PDF"));
    if (m_descLabel) m_descLabel->setText(tr("Reduce PDF file size locally without sacrificing visual quality."));
    if (m_levelLabel) m_levelLabel->setText(tr("Compression Level:"));
    if (m_executeBtn) m_executeBtn->setText(tr("Compress"));
    if (m_dropZone) {
        m_dropZone->setInstruction(tr("Drop PDF File to Compress Here"),
                                   tr("Your original file will be preserved and not overwritten"));
    }
    if (m_levelCombo && m_levelCombo->count() >= 3) {
        m_levelCombo->setItemText(0, tr("Balanced (Standard Quality & Size)"));
        m_levelCombo->setItemText(1, tr("High Quality (Low Compression)"));
        m_levelCombo->setItemText(2, tr("Smallest Size (High Compression)"));
    }
}

} // namespace LocalTools::UI
