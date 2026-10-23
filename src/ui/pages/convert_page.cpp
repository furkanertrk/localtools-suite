#include "convert_page.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <algorithm>
#include "../../core/backend/pdfium_backend.h"
#include "../common/icon_utils.h"
#include "../common/async_task.h"
#include "../common/output_file_dialog.h"
#include "../theme.h"

namespace LocalTools::UI {

namespace {
struct PdfPreviewResult {
    int pageCount{0};
    QImage firstPage;
    bool opened{false};
};
}

ConvertPage::ConvertPage(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

ConvertPage::~ConvertPage() {
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void ConvertPage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 20, 28, 20);
    mainLayout->setSpacing(14);

    // 1. Mode Selector Tabs at top
    auto *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);

    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);

    auto createModeBtn = [this](const QString& text, IconType iconType, int id) -> QPushButton* {
        auto *btn = new QPushButton(text, this);
        btn->setObjectName("ToolTabButton");
        btn->setCheckable(true);
        btn->setFocusPolicy(Qt::StrongFocus);
        btn->setIcon(IconUtils::getIcon(iconType, Theme::color(ColorToken::TextSecondary), 16));
        btn->setIconSize(QSize(16, 16));
        btn->setCursor(Qt::PointingHandCursor);
        m_modeGroup->addButton(btn, id);
        return btn;
    };

    m_imgToPdfBtn = createModeBtn(tr("Images to PDF"), IconType::ImagesToPdf, static_cast<int>(ConvertMode::ImagesToPdf));
    m_pdfToImgBtn = createModeBtn(tr("PDF to Images"), IconType::PdfToImages, static_cast<int>(ConvertMode::PdfToImages));
    m_pdfToTextBtn = createModeBtn(tr("PDF to Text"), IconType::PdfToText, static_cast<int>(ConvertMode::PdfToText));

    m_imgToPdfBtn->setChecked(true);

    toolbarLayout->addWidget(m_imgToPdfBtn);
    toolbarLayout->addWidget(m_pdfToImgBtn);
    toolbarLayout->addWidget(m_pdfToTextBtn);
    toolbarLayout->addStretch();

    mainLayout->addLayout(toolbarLayout);

    // 2. Tab Description Subtitle
    m_descLabel = new QLabel(tr("Convert JPG, PNG, WEBP, and BMP images into a single PDF document."), this);
    m_descLabel->setStyleSheet("font-size: 13px; color: palette(text); opacity: 0.75; margin-bottom: 2px;");
    m_descLabel->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_descLabel);

    connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        setMode(static_cast<ConvertMode>(id));
    });

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

    // Stacked Widget: Main Convert Workspace vs Result Screen
    m_stackedWidget = new QStackedWidget(this);

    auto *workspaceContainer = new QWidget(this);
    auto *workspaceLayout = new QVBoxLayout(workspaceContainer);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(12);

    // DropZone
    m_dropZone = new DropZoneWidget(workspaceContainer);
    connect(m_dropZone, &DropZoneWidget::filesDropped, this, &ConvertPage::loadFiles);
    workspaceLayout->addWidget(m_dropZone);

    // Centered Container for PDF File Card
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

    m_pdfBadgeLabel = new QLabel(m_pdfFileCard);
    m_pdfBadgeLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #2563eb; background-color: rgba(37, 99, 235, 0.1); border-radius: 4px; padding: 2px 6px;");

    auto *detailsRow = new QHBoxLayout();
    detailsRow->setSpacing(8);
    detailsRow->addWidget(m_pdfFileDetailsLabel);
    detailsRow->addWidget(m_pdfBadgeLabel);
    detailsRow->addStretch();

    cardTextLayout->addWidget(m_pdfFileNameLabel);
    cardTextLayout->addLayout(detailsRow);
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
            loadFiles(QStringList() << path);
        }
    });
    connect(m_removePdfBtn, &QPushButton::clicked, this, &ConvertPage::clearSelectedFiles);

    // Multi-Image List Workspace
    m_imageListContainer = new QWidget(workspaceContainer);
    auto *imageListLayout = new QVBoxLayout(m_imageListContainer);
    imageListLayout->setContentsMargins(0, 0, 0, 0);
    imageListLayout->setSpacing(8);

    auto *imageListHeader = new QHBoxLayout();
    m_imageListTitleLabel = new QLabel(tr("Images (0 images)"), m_imageListContainer);
    m_imageListTitleLabel->setStyleSheet("font-size: 13px; font-weight: bold;");
    imageListHeader->addWidget(m_imageListTitleLabel);
    imageListHeader->addStretch();

    m_addImagesBtn = new QPushButton(tr("+ Add Image"), m_imageListContainer);
    m_addImagesBtn->setObjectName("ToolTabButton");
    m_addImagesBtn->setCursor(Qt::PointingHandCursor);
    imageListHeader->addWidget(m_addImagesBtn);

    m_clearImagesBtn = new QPushButton(tr("Clear All"), m_imageListContainer);
    m_clearImagesBtn->setObjectName("ToolTabButton");
    m_clearImagesBtn->setCursor(Qt::PointingHandCursor);
    imageListHeader->addWidget(m_clearImagesBtn);

    imageListLayout->addLayout(imageListHeader);

    auto *imageListRow = new QHBoxLayout();
    m_imageList = new QListWidget(m_imageListContainer);
    m_imageList->setMinimumHeight(180);
    m_imageList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_imageList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_imageList->setStyleSheet(R"(
        QListWidget {
            border: 1px solid palette(mid);
            background-color: palette(base);
            border-radius: 8px;
            padding: 4px;
        }
        QListWidget::item {
            border-radius: 6px;
            margin-bottom: 2px;
        }
        QListWidget::item:hover {
            background-color: palette(alternate-base);
        }
        QListWidget::item:selected {
            background-color: rgba(37, 99, 235, 0.12);
            color: palette(text);
        }
    )");
    imageListRow->addWidget(m_imageList, 1);
    imageListLayout->addLayout(imageListRow);
    workspaceLayout->addWidget(m_imageListContainer, 1);

    connect(m_addImagesBtn, &QPushButton::clicked, this, [this]() {
        QStringList paths = QFileDialog::getOpenFileNames(this, tr("Select Image"), QString(), tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (!paths.isEmpty()) {
            loadFiles(paths);
        }
    });
    connect(m_clearImagesBtn, &QPushButton::clicked, this, &ConvertPage::clearSelectedFiles);

    // Options Bar (Format / DPI for PDF -> Images)
    m_optionsContainer = new QWidget(workspaceContainer);
    auto *optionsLayout = new QHBoxLayout(m_optionsContainer);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->setSpacing(12);

    auto *fmtLbl = new QLabel(tr("Image Format:"), m_optionsContainer);
    fmtLbl->setStyleSheet("font-size: 13px; font-weight: bold;");
    optionsLayout->addWidget(fmtLbl);

    m_formatCombo = new QComboBox(m_optionsContainer);
    m_formatCombo->addItem("PNG (.png)", "png");
    m_formatCombo->addItem("JPEG (.jpg)", "jpg");
    optionsLayout->addWidget(m_formatCombo);

    auto *dpiLbl = new QLabel(tr("Resolution (DPI):"), m_optionsContainer);
    dpiLbl->setStyleSheet("font-size: 13px; font-weight: bold; margin-left: 12px;");
    optionsLayout->addWidget(dpiLbl);

    m_dpiCombo = new QComboBox(m_optionsContainer);
    m_dpiCombo->addItem(tr("150 DPI (Default)"), 150);
    m_dpiCombo->addItem(tr("300 DPI (High Quality)"), 300);
    m_dpiCombo->addItem(tr("72 DPI (Web)"), 72);
    m_dpiCombo->setCurrentIndex(0);
    optionsLayout->addWidget(m_dpiCombo);

    optionsLayout->addStretch();
    m_optionsContainer->hide();

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

    footerLayout->addWidget(m_optionsContainer, 1);

    m_executeBtn = new QPushButton(tr("Convert"), footerBar);
    m_executeBtn->setObjectName("PrimaryButton");
    // Minimum, not fixed: Turkish labels run up to ~1.6x the English width.
    m_executeBtn->setMinimumSize(180, 40);
    m_executeBtn->setCursor(Qt::PointingHandCursor);
    footerLayout->addWidget(m_executeBtn, 0);

    connect(m_executeBtn, &QPushButton::clicked, this, &ConvertPage::startConversion);
    workspaceLayout->addWidget(footerBar);

    m_stackedWidget->addWidget(workspaceContainer);

    // Result Screen
    m_resultScreen = new ResultScreenWidget(this);
    m_stackedWidget->addWidget(m_resultScreen);

    connect(m_resultScreen, &ResultScreenWidget::backToWorkspaceRequested, this, [this]() {
        clearSelectedFiles();
        m_stackedWidget->setCurrentIndex(0);
        m_statusBanner->hideBanner();
        m_progressBar->hide();
    });

    mainLayout->addWidget(m_stackedWidget, 1);
    mainLayout->addStretch(1);
    updateModeHeader(ConvertMode::ImagesToPdf);
    retranslateUi();
}

void ConvertPage::updateInputState() {
    const bool hasFiles = !m_selectedFiles.isEmpty();
    QWidget *cardWrapper = m_pdfFileCard ? m_pdfFileCard->parentWidget() : nullptr;

    if (m_currentMode == ConvertMode::ImagesToPdf) {
        if (cardWrapper) cardWrapper->hide();
        m_optionsContainer->hide();

        if (hasFiles) {
            m_dropZone->hide();
            m_imageListContainer->show();
        } else {
            m_dropZone->show();
            m_imageListContainer->hide();
        }
    } else {
        // PdfToImages or PdfToText
        m_imageListContainer->hide();
        if (m_currentMode == ConvertMode::PdfToImages) {
            m_optionsContainer->show();
        } else {
            m_optionsContainer->hide();
        }

        if (hasFiles) {
            m_dropZone->hide();
            if (cardWrapper) cardWrapper->show();
        } else {
            m_dropZone->show();
            if (cardWrapper) cardWrapper->hide();
        }
    }
}

void ConvertPage::updateModeHeader(ConvertMode mode) {
    ++m_loadGeneration;
    m_currentMode = mode;
    m_selectedFiles.clear();
    m_statusBanner->hideBanner();

    switch (mode) {
        case ConvertMode::ImagesToPdf:
            if (m_descLabel) m_descLabel->setText(tr("Convert JPG, PNG, WEBP, and BMP images into a single PDF document."));
            m_dropZone->setInstruction(tr("Drop Images Here"), tr("Supported: PNG, JPG, JPEG, BMP, WEBP"));
            m_dropZone->setAcceptedFileExtensions({"png", "jpg", "jpeg", "bmp", "webp"}, tr("Select Image Files"), tr("This operation only accepts PNG, JPG, JPEG, BMP, and WEBP images."));
            break;
        case ConvertMode::PdfToImages:
            if (m_descLabel) m_descLabel->setText(tr("Convert your PDF pages to high-quality PNG or JPEG images."));
            m_dropZone->setInstruction(tr("Drag PDF File Here"), tr("Pages will be exported to images at selected resolution"));
            m_dropZone->setAcceptedFileExtensions({"pdf"}, tr("Select PDF File"), tr("This operation only accepts PDF files."));
            break;
        case ConvertMode::PdfToText:
            if (m_descLabel) m_descLabel->setText(tr("Extract all text content from PDF as clean Unicode text (.txt) file."));
            m_dropZone->setInstruction(tr("Drag PDF File Here"), tr("Text layer will be exported sequentially to .txt file"));
            m_dropZone->setAcceptedFileExtensions({"pdf"}, tr("Select PDF File"), tr("This operation only accepts PDF files."));
            break;
    }

    updateInputState();
}

void ConvertPage::setMode(ConvertMode mode) {
    if (QAbstractButton* btn = m_modeGroup->button(static_cast<int>(mode))) {
        btn->setChecked(true);
    }
    updateModeHeader(mode);
}

void ConvertPage::loadFiles(const QStringList& files) {
    if (files.isEmpty()) return;

    if (m_currentMode == ConvertMode::ImagesToPdf) {
        bool hadPdf = false;
        QStringList validImages;
        for (const auto& f : files) {
            const QFileInfo imageInfo(f);
            const QString ext = imageInfo.suffix().toLower();
            if (ext == "pdf") {
                hadPdf = true;
            }
            const bool supported = ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "bmp";
            if (imageInfo.isFile() && supported && !m_selectedFiles.contains(imageInfo.absoluteFilePath(), Qt::CaseInsensitive)) {
                validImages.append(imageInfo.absoluteFilePath());
            }
        }
        if (hadPdf && validImages.isEmpty()) {
            m_statusBanner->showBanner(BannerType::Warning, tr("This operation only accepts PNG, JPG, JPEG, BMP, and WEBP images. PDF files can be used in 'PDF to Images' or 'PDF to Text' tabs."));
            return;
        }
        if (validImages.isEmpty()) {
            m_statusBanner->showBanner(BannerType::Warning, tr("This operation only accepts PNG, JPG, JPEG, BMP, and WEBP images."));
            return;
        }
        m_selectedFiles.append(validImages);
        updateImageList();
        m_statusBanner->hideBanner();
    } else {
        // PDF mode: take first valid PDF
        QString pdfFile;
        for (const auto& f : files) {
            if (f.endsWith(".pdf", Qt::CaseInsensitive)) {
                pdfFile = f;
                break;
            }
        }
        if (pdfFile.isEmpty()) {
            m_statusBanner->showBanner(BannerType::Warning, tr("This operation only accepts PDF files. Switch to 'Images to PDF' tab to add images."));
            return;
        }
        m_selectedFiles = QStringList() << pdfFile;

        const QFileInfo fi(pdfFile);
        const double sizeMb = fi.size() / (1024.0 * 1024.0);
        const quint64 generation = ++m_loadGeneration;

        m_pdfFileNameLabel->setText(fi.fileName());
        m_pdfFileDetailsLabel->setText(tr("Loading..."));
        m_pdfThumbnailLabel->setPixmap(IconUtils::getPixmap(
            (m_currentMode == ConvertMode::PdfToImages ? IconType::PdfToImages : IconType::PdfToText),
            Theme::color(ColorToken::Accent), 32));

        if (m_currentMode == ConvertMode::PdfToImages) {
            m_pdfBadgeLabel->setText(tr("PDF -> Images"));
        } else {
            m_pdfBadgeLabel->setText(tr("PDF -> Metin (.txt)"));
        }

        updateInputState();
        m_statusBanner->hideBanner();

        AsyncTask::run<PdfPreviewResult>(
            this,
            [pdfFile](AsyncTask::CancelCheck cancelled) {
                PdfPreviewResult result;
                if (cancelled()) return result;
                Core::PdfiumBackend backend;
                if (!backend.openDocument(pdfFile.toStdString())) return result;
                result.opened = true;
                result.pageCount = backend.getPageCount();
                if (result.pageCount > 0 && !cancelled()) {
                    result.firstPage = backend.renderPage(0, 120, 160);
                }
                backend.closeDocument();
                return result;
            },
            [this, generation, pdfFile, sizeMb](PdfPreviewResult result) {
                if (generation != m_loadGeneration || m_selectedFiles.value(0) != pdfFile) return;
                if (!result.firstPage.isNull()) {
                    m_pdfThumbnailLabel->setPixmap(QPixmap::fromImage(result.firstPage).scaled(
                        60, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                if (result.pageCount > 0) {
                    m_pdfFileDetailsLabel->setText(tr("%1 pages · %2 MB")
                        .arg(result.pageCount).arg(QString::number(sizeMb, 'f', 2)));
                } else {
                    m_pdfFileDetailsLabel->setText(tr("%1 MB").arg(QString::number(sizeMb, 'f', 2)));
                }
                if (!result.opened) {
                    m_statusBanner->showBanner(BannerType::Error,
                        tr("The PDF could not be opened. It may be corrupted or password protected."));
                }
            });
    }
}

void ConvertPage::clearSelectedFiles() {
    ++m_loadGeneration;
    m_selectedFiles.clear();
    updateImageList();
    updateInputState();
}

void ConvertPage::updateImageList() {
    if (!m_imageList) {
        return;
    }

    m_imageList->clear();
    for (qsizetype index = 0; index < m_selectedFiles.size(); ++index) {
        const QString path = m_selectedFiles.at(index);
        const QFileInfo fileInfo(path);
        double sizeMb = fileInfo.size() / (1024.0 * 1024.0);

        auto *item = new QListWidgetItem(m_imageList);
        item->setSizeHint(QSize(0, 52));

        auto *rowWidget = new QWidget(m_imageList);
        rowWidget->setObjectName("ImageRowWidget");

        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(6, 4, 8, 4);
        rowLayout->setSpacing(10);

        // Sequence Badge
        auto *seqLabel = new QLabel(QString("%1").arg(index + 1, 2, 10, QChar('0')), rowWidget);
        seqLabel->setFixedWidth(22);
        seqLabel->setStyleSheet("font-size: 12px; font-weight: bold; opacity: 0.65;");
        rowLayout->addWidget(seqLabel);

        // Thumbnail Label (40x40 smooth scaled preview)
        auto *thumbLabel = new QLabel(rowWidget);
        thumbLabel->setFixedSize(40, 40);
        thumbLabel->setStyleSheet("border: 1px solid palette(mid); border-radius: 4px; background: palette(base);");
        thumbLabel->setAlignment(Qt::AlignCenter);

        QPixmap pix(path);
        if (!pix.isNull()) {
            QPixmap scaledPix = pix.scaled(40, 40, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            int x = (scaledPix.width() - 40) / 2;
            int y = (scaledPix.height() - 40) / 2;
            thumbLabel->setPixmap(scaledPix.copy(x, y, 40, 40));
        } else {
            thumbLabel->setPixmap(IconUtils::getPixmap(IconType::ImagesToPdf, QColor("#94a3b8"), 20));
        }
        rowLayout->addWidget(thumbLabel);

        // Text Column (Filename + Size)
        auto *textCol = new QVBoxLayout();
        textCol->setSpacing(2);
        textCol->setContentsMargins(0, 0, 0, 0);

        auto *nameLabel = new QLabel(fileInfo.fileName(), rowWidget);
        QFont font = nameLabel->font();
        font.setPointSize(12);
        font.setBold(true);
        nameLabel->setFont(font);

        auto *sizeLabel = new QLabel(tr("%1 MB").arg(QString::number(sizeMb, 'f', 2)), rowWidget);
        sizeLabel->setStyleSheet("font-size: 11px; opacity: 0.7;");

        textCol->addWidget(nameLabel);
        textCol->addWidget(sizeLabel);
        rowLayout->addLayout(textCol, 1);

        // Compact Action Buttons
        auto *rowUpBtn = new QPushButton(rowWidget);
        rowUpBtn->setIcon(IconUtils::getIcon(IconType::ArrowUp, QColor("#64748b"), 14));
        rowUpBtn->setToolTip(tr("Move Up"));
        rowUpBtn->setAccessibleName(rowUpBtn->toolTip());
        rowUpBtn->setFixedSize(26, 26);
        rowUpBtn->setObjectName("ToolTabButton");
        rowUpBtn->setCursor(Qt::PointingHandCursor);
        rowUpBtn->setEnabled(index > 0);

        auto *rowDownBtn = new QPushButton(rowWidget);
        rowDownBtn->setIcon(IconUtils::getIcon(IconType::ArrowDown, QColor("#64748b"), 14));
        rowDownBtn->setToolTip(tr("Move Down"));
        rowDownBtn->setAccessibleName(rowDownBtn->toolTip());
        rowDownBtn->setFixedSize(26, 26);
        rowDownBtn->setObjectName("ToolTabButton");
        rowDownBtn->setCursor(Qt::PointingHandCursor);
        rowDownBtn->setEnabled(index < m_selectedFiles.size() - 1);

        auto *rowDelBtn = new QPushButton(rowWidget);
        rowDelBtn->setIcon(IconUtils::getIcon(IconType::Close, QColor("#e11d48"), 14));
        rowDelBtn->setToolTip(tr("Remove"));
        rowDelBtn->setAccessibleName(rowDelBtn->toolTip());
        rowDelBtn->setFixedSize(26, 26);
        rowDelBtn->setObjectName("ToolTabButton");
        rowDelBtn->setCursor(Qt::PointingHandCursor);

        rowLayout->addWidget(rowUpBtn);
        rowLayout->addWidget(rowDownBtn);
        rowLayout->addWidget(rowDelBtn);

        connect(rowUpBtn, &QPushButton::clicked, this, [this, index]() {
            moveSelectedImageIndex(index, -1);
        });
        connect(rowDownBtn, &QPushButton::clicked, this, [this, index]() {
            moveSelectedImageIndex(index, 1);
        });
        connect(rowDelBtn, &QPushButton::clicked, this, [this, index]() {
            removeImageAtIndex(index);
        });

        m_imageList->setItemWidget(item, rowWidget);
    }

    if (m_imageListTitleLabel) {
        m_imageListTitleLabel->setText(tr("Images (%1 images)").arg(m_selectedFiles.size()));
    }

    updateInputState();
}

void ConvertPage::moveSelectedImageIndex(qsizetype index, int offset) {
    const qsizetype targetIndex = index + offset;
    if (index < 0 || targetIndex < 0 || targetIndex >= m_selectedFiles.size()) {
        return;
    }

    m_selectedFiles.move(index, targetIndex);
    updateImageList();
}

void ConvertPage::removeImageAtIndex(qsizetype index) {
    if (index < 0 || index >= m_selectedFiles.size()) {
        return;
    }

    m_selectedFiles.removeAt(index);
    updateImageList();
}

void ConvertPage::startConversion() {
    if (m_selectedFiles.isEmpty()) {
        m_statusBanner->showBanner(BannerType::Warning, tr("Please upload file(s) to process first."));
        return;
    }

    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/LocalTools";
    QDir().mkpath(defaultDir);

    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }

    m_workerThread = new QThread(this);
    auto *worker = new Workers::PdfWorker();
    worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, worker, &Workers::PdfWorker::process);
    connect(worker, &Workers::PdfWorker::progressChanged, m_progressBar, &QProgressBar::setValue);
    connect(worker, &Workers::PdfWorker::statusMessage, this, [this](const QString& msg) {
        m_statusBanner->showBanner(BannerType::Info, msg);
    });

    m_progressBar->setValue(0);
    m_progressBar->show();
    m_executeBtn->setEnabled(false);

    if (m_currentMode == ConvertMode::ImagesToPdf) {
        QString targetFile = OutputFileDialog::getSaveFileName(this, tr("Save Converted PDF"), defaultDir + "/converted_images.pdf", tr("PDF Files (*.pdf)"), m_selectedFiles);
        if (targetFile.isEmpty()) {
            m_progressBar->hide();
            m_executeBtn->setEnabled(true);
            m_workerThread->deleteLater();
            return;
        }
        worker->prepareImagesToPdf(m_selectedFiles, targetFile);

        connect(worker, &Workers::PdfWorker::finished, this, [this, worker, targetFile](const Core::Result<void>& result) {
            m_executeBtn->setEnabled(true);
            m_progressBar->hide();
            if (result.hasValue()) {
                m_statusBanner->hideBanner();
                m_resultScreen->showResult(targetFile, tr("Images successfully converted to PDF!"));
                m_stackedWidget->setCurrentWidget(m_resultScreen);
            } else {
                m_statusBanner->showBanner(BannerType::Error, tr("Conversion failed: ") + QString::fromStdString(result.error().message));
            }
            m_workerThread->quit();
            worker->deleteLater();
        });

    } else if (m_currentMode == ConvertMode::PdfToImages) {
        QString targetDir = QFileDialog::getExistingDirectory(this, tr("Select Folder to Save Images"), defaultDir);
        if (targetDir.isEmpty()) {
            m_progressBar->hide();
            m_executeBtn->setEnabled(true);
            m_workerThread->deleteLater();
            return;
        }
        QString fmt = m_formatCombo->currentData().toString();
        int dpi = m_dpiCombo->currentData().toInt();

        worker->preparePdfToImages(m_selectedFiles.first(), targetDir, fmt, dpi);

        connect(worker, &Workers::PdfWorker::finished, this, [this, worker, targetDir](const Core::Result<void>& result) {
            m_executeBtn->setEnabled(true);
            m_progressBar->hide();
            if (result.hasValue()) {
                m_statusBanner->hideBanner();
                m_resultScreen->showResult(targetDir, tr("PDF pages successfully converted to images!"));
                m_stackedWidget->setCurrentWidget(m_resultScreen);
            } else {
                m_statusBanner->showBanner(BannerType::Error, tr("Image conversion failed: ") + QString::fromStdString(result.error().message));
            }
            m_workerThread->quit();
            worker->deleteLater();
        });

    } else if (m_currentMode == ConvertMode::PdfToText) {
        QString targetFile = OutputFileDialog::getSaveFileName(this, tr("Save Text File"), defaultDir + "/extracted_text.txt", tr("Text Files (*.txt)"), m_selectedFiles);
        if (targetFile.isEmpty()) {
            m_progressBar->hide();
            m_executeBtn->setEnabled(true);
            m_workerThread->deleteLater();
            return;
        }
        worker->preparePdfToText(m_selectedFiles.first(), targetFile);

        connect(worker, &Workers::PdfWorker::finished, this, [this, worker, targetFile](const Core::Result<void>& result) {
            m_executeBtn->setEnabled(true);
            m_progressBar->hide();
            if (result.hasValue()) {
                m_statusBanner->hideBanner();
                m_resultScreen->showResult(targetFile, tr("PDF text content successfully extracted!"));
                m_stackedWidget->setCurrentWidget(m_resultScreen);
            } else {
                m_statusBanner->showBanner(BannerType::Error, tr("Text extraction failed: ") + QString::fromStdString(result.error().message));
            }
            m_workerThread->quit();
            worker->deleteLater();
        });
    }

    m_workerThread->start();
}

void ConvertPage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ConvertPage::retranslateUi() {
    if (m_imgToPdfBtn) m_imgToPdfBtn->setText(tr("Images to PDF"));
    if (m_pdfToImgBtn) m_pdfToImgBtn->setText(tr("PDF to Images"));
    if (m_pdfToTextBtn) m_pdfToTextBtn->setText(tr("PDF to Text"));
    if (m_addImagesBtn) m_addImagesBtn->setText(tr("Add Images"));
    if (m_clearImagesBtn) m_clearImagesBtn->setText(tr("Clear All"));
    if (m_executeBtn) m_executeBtn->setText(tr("Convert"));

    if (m_dpiCombo && m_dpiCombo->count() >= 3) {
        m_dpiCombo->setItemText(0, tr("150 DPI (Default)"));
        m_dpiCombo->setItemText(1, tr("300 DPI (High Quality)"));
        m_dpiCombo->setItemText(2, tr("72 DPI (Web)"));
    }

    updateModeHeader(m_currentMode);
}

} // namespace LocalTools::UI
