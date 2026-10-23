#include "metadata_page.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QHeaderView>
#include <QStyle>
#include <QImage>
#include <optional>

#include "../common/icon_utils.h"
#include "../common/output_file_dialog.h"
#include "../common/async_task.h"
#include "../theme.h"
#include "../../services/settings_service.h"
#include "../../services/file_service.h"
#include "../../core/backends/quick_tools_backend.h"

namespace LocalTools::UI {

namespace {

struct MetadataInspectionOutcome {
    std::optional<Core::FileMetadataReport> report;
    Core::PdfError error;
    QImage thumbnail;
};

struct PdfInspectionOutcome {
    bool opened{false};
    int pages{0};
    QImage firstPageImage;
    Core::PdfiumBackend::PdfMetadata metadata;
};

void fitModeButtonToText(QPushButton* button) {
    if (!button) return;
    const int iconWidth = button->icon().isNull() ? 0 : button->iconSize().width() + 8;
    // Account for the themed horizontal padding, border and a small DPI safety
    // margin. Qt's default size hint can otherwise clip longer translations.
    button->setMinimumWidth(button->fontMetrics().horizontalAdvance(button->text())
                            + iconWidth + 42);
}

} // namespace

MetadataPage::MetadataPage(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

void MetadataPage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 20, 28, 20);
    mainLayout->setSpacing(14);

    // 1. Mode Selector Toolbar: View vs Edit
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
        fitModeButtonToText(btn);
        return btn;
    };

    m_viewBtn = createModeBtn(tr("View Metadata"), IconType::View, static_cast<int>(MetadataMode::View));
    m_editBtn = createModeBtn(tr("Edit Metadata"), IconType::Edit, static_cast<int>(MetadataMode::Edit));
    m_cleanBtn = createModeBtn(tr("Inspect & Clean"), IconType::Delete, static_cast<int>(MetadataMode::Clean));

    m_viewBtn->setChecked(true);

    toolbarLayout->addWidget(m_viewBtn);
    toolbarLayout->addWidget(m_editBtn);
    toolbarLayout->addWidget(m_cleanBtn);
    toolbarLayout->addStretch();

    mainLayout->addLayout(toolbarLayout);

    // 2. Tab Description Subtitle
    m_descLabel = new QLabel(tr("Inspect and update standard metadata such as Title, Author, Subject, and Keywords in PDF."), this);
    m_descLabel->setStyleSheet("font-size: 13px; color: palette(text); opacity: 0.75; margin-bottom: 2px;");
    m_descLabel->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_descLabel);

    connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        setMode(static_cast<MetadataMode>(id));
    });

    // Status Banner
    m_statusBanner = new StatusBannerWidget(this);
    m_statusBanner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    mainLayout->addWidget(m_statusBanner);

    // Stacked Widget: Workspace vs Result Screen
    m_stackedWidget = new QStackedWidget(this);

    auto *workspaceContainer = new QWidget(this);
    auto *workspaceLayout = new QVBoxLayout(workspaceContainer);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(8);

    // DropZone
    m_dropZone = new DropZoneWidget(workspaceContainer);
    m_dropZone->setInstruction(tr("Drop PDF to Edit Metadata"),
                               tr("PDF title, author, subject, and keywords can be updated"));
    m_dropZone->setAcceptedFileExtensions({"pdf"}, tr("Select PDF File"));
    connect(m_dropZone, &DropZoneWidget::filesDropped, this, &MetadataPage::loadPdfFile);
    workspaceLayout->addWidget(m_dropZone);

    // Form Container (Centered 2-Column Grid Layout)
    m_formContainer = new QWidget(workspaceContainer);
    auto *formWrapperLayout = new QHBoxLayout(m_formContainer);
    formWrapperLayout->setContentsMargins(0, 0, 0, 0);
    formWrapperLayout->setAlignment(Qt::AlignCenter);

    auto *formBox = new QWidget(m_formContainer);
    formBox->setMaximumWidth(680);
    auto *formBoxLayout = new QVBoxLayout(formBox);
    formBoxLayout->setContentsMargins(0, 0, 0, 0);
    formBoxLayout->setSpacing(12);

    // Centered PDF File Card Header
    m_pdfFileCard = new QWidget(formBox);
    m_pdfFileCard->setObjectName("PdfFileCard");
    // Styled by the application stylesheet via its objectName. A local sheet
    // would override the app one for this subtree, and its QLabel rule carried
    // no colour - which is what rendered these labels black in dark mode.

    auto *cardLayout = new QHBoxLayout(m_pdfFileCard);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(14);
    cardLayout->setAlignment(Qt::AlignVCenter);

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

    formBoxLayout->addWidget(m_pdfFileCard);

    connect(m_changePdfBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Select PDF Document"), QString(), tr("PDF Files (*.pdf)"));
        if (!path.isEmpty()) {
            loadPdfFile(QStringList() << path);
        }
    });
    connect(m_removePdfBtn, &QPushButton::clicked, this, &MetadataPage::clearSelectedFile);

    // 2-Column Grid Layout for Metadata Fields
    auto *gridLayout = new QGridLayout();
    gridLayout->setHorizontalSpacing(16);
    gridLayout->setVerticalSpacing(10);

    auto createGridFormField = [this](QGridLayout* grid, int row, int col, const QString& labelText, QLineEdit*& editPtr, const QString& placeholder) {
        auto *fieldBox = new QVBoxLayout();
        fieldBox->setSpacing(4);

        auto *lbl = new QLabel(labelText, m_formContainer);
        lbl->setStyleSheet("font-size: 12px; font-weight: bold;");
        fieldBox->addWidget(lbl);

        editPtr = new QLineEdit(m_formContainer);
        editPtr->setPlaceholderText(placeholder);
        editPtr->setFixedHeight(36);
        editPtr->setStyleSheet("QLineEdit { border-radius: 6px; padding: 0 10px; }"
                               "QLineEdit:focus { border: 1px solid #2563eb; }");
        fieldBox->addWidget(editPtr);

        grid->addLayout(fieldBox, row, col);
    };

    // Left Column (Col 0)
    createGridFormField(gridLayout, 0, 0, tr("Document Title:"), m_titleEdit, tr("e.g. Annual Activity Report"));
    createGridFormField(gridLayout, 1, 0, tr("Author:"), m_authorEdit, tr("e.g. John Doe"));
    createGridFormField(gridLayout, 2, 0, tr("Subject:"), m_subjectEdit, tr("e.g. Financial Analysis"));

    // Right Column (Col 1)
    createGridFormField(gridLayout, 0, 1, tr("Keywords:"), m_keywordsEdit, tr("e.g. report, finance, 2026"));
    createGridFormField(gridLayout, 1, 1, tr("Creator Software:"), m_creatorEdit, tr("e.g. Microsoft Word"));
    createGridFormField(gridLayout, 2, 1, tr("Producer Application:"), m_producerEdit, tr("e.g. LocalTools Engine"));

    formBoxLayout->addLayout(gridLayout);

    auto *bottomLayout = new QHBoxLayout();
    
    auto *clearAllBtn = new QPushButton(tr("Clear All Metadata"), m_formContainer);
    clearAllBtn->setIcon(IconUtils::getIcon(IconType::Delete, QColor("#64748b"), 16));
    clearAllBtn->setIconSize(QSize(16, 16));
    clearAllBtn->setCursor(Qt::PointingHandCursor);
    clearAllBtn->setObjectName("ToolTabButton");
    connect(clearAllBtn, &QPushButton::clicked, this, [this]() {
        m_titleEdit->clear();
        m_authorEdit->clear();
        m_subjectEdit->clear();
        m_keywordsEdit->clear();
        m_creatorEdit->clear();
        m_producerEdit->clear();
    });
    bottomLayout->addWidget(clearAllBtn);

    bottomLayout->addStretch();

    m_saveButton = new QPushButton(tr("Save Metadata"), m_formContainer);
    m_saveButton->setIcon(IconUtils::getIcon(IconType::Edit, Qt::white, 16));
    m_saveButton->setIconSize(QSize(16, 16));
    m_saveButton->setObjectName("PrimaryButton");
    // Minimum, not fixed: Turkish labels run up to ~1.6x the English width.
    m_saveButton->setMinimumSize(180, 40);
    m_saveButton->setCursor(Qt::PointingHandCursor);
    bottomLayout->addWidget(m_saveButton);

    connect(m_saveButton, &QPushButton::clicked, this, &MetadataPage::saveMetadata);
    formBoxLayout->addLayout(bottomLayout);

    formWrapperLayout->addWidget(formBox);
    m_formContainer->hide();
    workspaceLayout->addWidget(m_formContainer, 1);

    // The PDF form and the Inspect & Clean workspace are siblings; the mode
    // buttons pick between them.
    m_panelStack = new QStackedWidget(this);
    m_panelStack->addWidget(workspaceContainer);   // 0 - View / Edit
    m_panelStack->addWidget(createCleanPanel());   // 1 - Clean

    auto *panelHost = new QWidget(this);
    auto *panelHostLayout = new QVBoxLayout(panelHost);
    panelHostLayout->setContentsMargins(0, 0, 0, 0);
    panelHostLayout->setSpacing(Theme::space(Space::XS));
    panelHostLayout->addWidget(m_panelStack, 1);

    m_progressBar = new QProgressBar(panelHost);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->hide();
    panelHostLayout->addWidget(m_progressBar);

    m_stackedWidget->addWidget(panelHost);

    // Result Screen
    m_resultScreen = new ResultScreenWidget(this);
    m_stackedWidget->addWidget(m_resultScreen);

    connect(m_resultScreen, &ResultScreenWidget::backToWorkspaceRequested, this, [this]() {
        clearSelectedFile();
        m_stackedWidget->setCurrentIndex(0);
        m_statusBanner->hideBanner();
    });

    // Keep the metadata card comfortably tall without letting it consume the
    // whole page. The previous 1:1 split was too short; no trailing stretch was
    // too tall. A 5:1 ratio matches the intended card boundary while leaving
    // enough table height for ordinary metadata reports without a scrollbar.
    mainLayout->addWidget(m_stackedWidget, 5);
    mainLayout->addStretch(1);
    setMode(MetadataMode::View);
    retranslateUi();
}

QWidget* MetadataPage::createCleanPanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Theme::space(Space::S));

    m_metaDropZone = new DropZoneWidget(panel);
    m_metaDropZone->setInstruction(tr("Drop File to Read and Clean Metadata"),
                                   tr("Images (JPG, PNG, WEBP, BMP, TIFF) or PDF documents supported"));
    m_metaDropZone->setAcceptedFileExtensions({"jpg", "jpeg", "png", "webp", "bmp", "tiff", "pdf"},
                                              tr("Select File"));
    m_metaDropZone->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_metaDropZone, &DropZoneWidget::filesDropped, this, &MetadataPage::onMetadataFilesDropped);
    layout->addWidget(m_metaDropZone);

    m_metaWorkspaceContainer = new QFrame(panel);
    m_metaWorkspaceContainer->setObjectName("CardFrame");
    m_metaWorkspaceContainer->hide();

    auto *wsLayout = new QVBoxLayout(m_metaWorkspaceContainer);
    const int pad = Theme::space(Space::M);
    wsLayout->setContentsMargins(pad, pad, pad, pad);
    wsLayout->setSpacing(Theme::space(Space::S));

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(Theme::space(Space::S));

    m_metaThumbLabel = new QLabel(m_metaWorkspaceContainer);
    m_metaThumbLabel->setObjectName("ThumbTile");
    m_metaThumbLabel->setFixedSize(52, 52);
    m_metaThumbLabel->setAlignment(Qt::AlignCenter);
    topRow->addWidget(m_metaThumbLabel);

    auto *nameBox = new QVBoxLayout();
    nameBox->setSpacing(2);
    m_metaFileNameLabel = new QLabel(m_metaWorkspaceContainer);
    m_metaFileNameLabel->setObjectName("ToolCardName");
    m_metaFileSizeLabel = new QLabel(m_metaWorkspaceContainer);
    m_metaFileSizeLabel->setObjectName("CaptionLabel");
    nameBox->addWidget(m_metaFileNameLabel);
    nameBox->addWidget(m_metaFileSizeLabel);
    topRow->addLayout(nameBox, 1);

    m_metaStatusBadge = new QLabel(m_metaWorkspaceContainer);
    m_metaStatusBadge->setObjectName("StatusChip");
    topRow->addWidget(m_metaStatusBadge, 0, Qt::AlignRight | Qt::AlignVCenter);

    m_metaChangeFileBtn = new QPushButton(tr("Select Different File"), m_metaWorkspaceContainer);
    m_metaChangeFileBtn->setObjectName("SecondaryButton");
    m_metaChangeFileBtn->setMinimumHeight(34);
    m_metaChangeFileBtn->setCursor(Qt::PointingHandCursor);
    connect(m_metaChangeFileBtn, &QPushButton::clicked, this, &MetadataPage::resetMetadataWorkspace);
    topRow->addWidget(m_metaChangeFileBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

    wsLayout->addLayout(topRow);

    m_metaTable = new QTableWidget(m_metaWorkspaceContainer);
    m_metaTable->setColumnCount(3);
    m_metaTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_metaTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_metaTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_metaTable->verticalHeader()->setVisible(false);
    m_metaTable->setAlternatingRowColors(true);
    m_metaTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_metaTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_metaTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    wsLayout->addWidget(m_metaTable, 1);

    auto *footerRow = new QHBoxLayout();
    footerRow->setSpacing(Theme::space(Space::S));

    m_metaPrivacyNoteLabel = new QLabel(m_metaWorkspaceContainer);
    m_metaPrivacyNoteLabel->setObjectName("CaptionLabel");
    m_metaPrivacyNoteLabel->setWordWrap(true);
    footerRow->addWidget(m_metaPrivacyNoteLabel, 1);

    m_metaCleanBtn = new QPushButton(m_metaWorkspaceContainer);
    m_metaCleanBtn->setObjectName("PrimaryButton");
    m_metaCleanBtn->setMinimumHeight(40);
    m_metaCleanBtn->setCursor(Qt::PointingHandCursor);
    connect(m_metaCleanBtn, &QPushButton::clicked, this, &MetadataPage::cleanCurrentMetadata);
    footerRow->addWidget(m_metaCleanBtn);

    wsLayout->addLayout(footerRow);
    layout->addWidget(m_metaWorkspaceContainer, 1);

    return panel;
}

void MetadataPage::loadMetadataFile(const QString& filePath) {
    setMode(MetadataMode::Clean);
    onMetadataFilesDropped({filePath});
}

void MetadataPage::onMetadataFilesDropped(const QStringList& paths) {
    if (paths.isEmpty()) return;

    const QString filePath = paths.first();
    m_currentMetaFilePath = filePath;
    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_metaDropZone->setEnabled(false);
    AsyncTask::run<MetadataInspectionOutcome>(
        this,
        [filePath](AsyncTask::CancelCheck cancel) {
            MetadataInspectionOutcome outcome;
            if (cancel()) {
                outcome.error = Core::PdfError::create(
                    Core::PdfErrorCategory::Cancelled, "Operation was cancelled.");
                return outcome;
            }
            auto result = Core::QuickToolsBackend::inspectMetadata(filePath);
            if (result.hasError()) {
                outcome.error = result.error();
                return outcome;
            }
            outcome.report = result.value();
            if (QFileInfo(filePath).suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) != 0) {
                outcome.thumbnail = QImage(filePath);
            }
            return outcome;
        },
        [this, filePath](MetadataInspectionOutcome outcome) {
            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            m_metaDropZone->setEnabled(true);
            if (filePath != m_currentMetaFilePath) return;
            if (!outcome.report.has_value()) {
                m_statusBanner->showBanner(BannerType::Error, tr("Could not read file metadata."));
                return;
            }

            const auto& report = *outcome.report;
            m_metaFileNameLabel->setText(report.fileName);
            m_metaFileSizeLabel->setText(
                report.items.size() > 1
                    ? QStringLiteral("%1 • %2").arg(report.fileType, report.items[1].value)
                    : report.fileType);
            if (QFileInfo(filePath).suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) == 0) {
                m_metaThumbLabel->setPixmap(
                    IconUtils::getPixmap(IconType::Metadata, Theme::color(ColorToken::Accent), 26));
            } else if (!outcome.thumbnail.isNull()) {
                m_metaThumbLabel->setPixmap(QPixmap::fromImage(outcome.thumbnail).scaled(
                    46, 46, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                m_metaThumbLabel->setPixmap(
                    IconUtils::getPixmap(IconType::Image, Theme::color(ColorToken::Accent), 26));
            }

            m_metaStatusBadge->setText(report.hasMetadata ? tr("Personal / EXIF Data Detected")
                                                          : tr("No Privacy Risk Found / Clean"));
            m_metaStatusBadge->setProperty("chipState", report.hasMetadata ? "risk" : "clean");
            m_metaStatusBadge->style()->unpolish(m_metaStatusBadge);
            m_metaStatusBadge->style()->polish(m_metaStatusBadge);

            m_metaTable->setHorizontalHeaderLabels({tr("Category"), tr("Property"), tr("Value")});
            m_metaTable->setRowCount(static_cast<int>(report.items.size()));
            for (int row = 0; row < static_cast<int>(report.items.size()); ++row) {
                const auto& item = report.items[static_cast<size_t>(row)];
                auto* keyItem = new QTableWidgetItem(item.key);
                QFont keyFont = keyItem->font();
                keyFont.setWeight(QFont::DemiBold);
                keyItem->setFont(keyFont);
                m_metaTable->setItem(row, 0, new QTableWidgetItem(item.category));
                m_metaTable->setItem(row, 1, keyItem);
                m_metaTable->setItem(row, 2, new QTableWidgetItem(item.value));
            }
            m_metaDropZone->hide();
            m_metaWorkspaceContainer->show();
            m_statusBanner->showBanner(BannerType::Success, tr("File metadata listed successfully."));
        });
}

void MetadataPage::cleanCurrentMetadata() {
    if (m_currentMetaFilePath.isEmpty()) return;

    const QFileInfo fi(m_currentMetaFilePath);
    const QString outDir = Services::SettingsService::instance().getDefaultOutputDir();
    QDir().mkpath(outDir);
    const QString outPath = Services::FileService::instance().generateUniqueOutputPath(
        outDir + "/" + fi.completeBaseName() + "_clean." + fi.suffix());

    const QString inputPath = m_currentMetaFilePath;
    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_metaCleanBtn->setEnabled(false);
    AsyncTask::run<Core::Result<void>>(
        this,
        [inputPath, outPath](AsyncTask::CancelCheck cancel) {
            if (cancel()) {
                return Core::Result<void>::fail(
                    Core::PdfErrorCategory::Cancelled, "Operation was cancelled.");
            }
            return Core::QuickToolsBackend::cleanMetadata(inputPath, outPath);
        },
        [this, outPath](Core::Result<void> result) {
            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            m_metaCleanBtn->setEnabled(true);
            if (result.hasValue()) {
                Services::SettingsService::instance().addRecentFile(outPath);
                m_resultScreen->showResult(outPath, tr("Metadata cleaned successfully!"));
                m_stackedWidget->setCurrentIndex(1);
            } else {
                m_statusBanner->showBanner(
                    BannerType::Error,
                    tr("Error cleaning metadata: ") +
                        QString::fromStdString(result.error().toUserFriendlyMessage()));
            }
        });
}

void MetadataPage::resetMetadataWorkspace() {
    m_metaWorkspaceContainer->hide();
    m_metaDropZone->show();
    m_currentMetaFilePath.clear();
    m_metaTable->setRowCount(0);
    m_statusBanner->hideBanner();
}

void MetadataPage::clearSelectedFile() {
    m_loadedFile.clear();
    m_formContainer->hide();
    m_dropZone->show();
}

void MetadataPage::setMode(MetadataMode mode) {
    m_currentMode = mode;

    if (auto* btn = m_modeGroup->button(static_cast<int>(mode))) {
        btn->setChecked(true);
    }

    const bool isEdit = (mode == MetadataMode::Edit);
    const bool isClean = (mode == MetadataMode::Clean);
    if (m_panelStack) {
        // View and Clean share the rich, read-only metadata report. Edit keeps
        // the PDF document-property form and never silently changes mode.
        m_panelStack->setCurrentIndex(isEdit ? 0 : 1);
    }
    if (m_stackedWidget) m_stackedWidget->setCurrentIndex(0);

    m_titleEdit->setReadOnly(!isEdit);
    m_authorEdit->setReadOnly(!isEdit);
    m_subjectEdit->setReadOnly(!isEdit);
    m_keywordsEdit->setReadOnly(!isEdit);
    m_creatorEdit->setReadOnly(!isEdit);
    m_producerEdit->setReadOnly(!isEdit);

    m_saveButton->setVisible(isEdit);
    if (m_metaCleanBtn) m_metaCleanBtn->setVisible(isClean);
    if (m_metaPrivacyNoteLabel) m_metaPrivacyNoteLabel->setVisible(isClean);

    if (m_metaDropZone) {
        if (isClean) {
            m_metaDropZone->setInstruction(
                tr("Drop File to Read and Clean Metadata"),
                tr("Images (JPG, PNG, WEBP, BMP, TIFF) or PDF documents supported"));
        } else if (!isEdit) {
            m_metaDropZone->setInstruction(
                tr("Drop File to View Metadata"),
                tr("View image and PDF metadata without modifying the file"));
        }
    }

    if (m_descLabel) {
        if (isClean) {
            m_descLabel->setText(tr("Read and remove EXIF, GPS, and device metadata from images and PDF documents."));
        } else if (!isEdit) {
            m_descLabel->setText(tr("View file, EXIF, GPS, device, and PDF document metadata without changing the file."));
        } else {
            m_descLabel->setText(tr("Update metadata fields of the PDF document and save as a new file."));
        }
    }
}

void MetadataPage::loadPdfFile(const QStringList& filePaths) {
    if (filePaths.isEmpty()) return;

    if (m_currentMode == MetadataMode::View || m_currentMode == MetadataMode::Clean) {
        onMetadataFilesDropped(filePaths);
        return;
    }

    QString pdfPath = filePaths.first();
    QFileInfo fi(pdfPath);
    QString ext = fi.suffix().toLower();

    if (ext != "pdf") {
        m_statusBanner->showBanner(
            BannerType::Warning,
            tr("Metadata editing is supported for PDF documents. Use View Metadata to inspect image metadata."));
        return;
    }

    if (pdfPath.isEmpty()) {
        m_statusBanner->showBanner(BannerType::Error, tr("Please select a valid PDF file."));
        return;
    }

    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_dropZone->setEnabled(false);
    AsyncTask::run<PdfInspectionOutcome>(
        this,
        [pdfPath](AsyncTask::CancelCheck cancel) {
            PdfInspectionOutcome outcome;
            if (cancel()) return outcome;
            Core::PdfiumBackend backend;
            if (!backend.openDocument(pdfPath.toStdString())) return outcome;
            outcome.opened = true;
            outcome.pages = backend.getPageCount();
            if (outcome.pages > 0) {
                outcome.firstPageImage = backend.renderPage(0, 120, 160);
            }
            outcome.metadata = backend.getMetadata();
            backend.closeDocument();
            return outcome;
        },
        [this, pdfPath](PdfInspectionOutcome outcome) {
            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            m_dropZone->setEnabled(true);
            if (!outcome.opened) {
                m_statusBanner->showBanner(
                    BannerType::Error,
                    tr("PDF document could not be opened or is password-protected/corrupted."));
                return;
            }

            const QFileInfo info(pdfPath);
            if (!outcome.firstPageImage.isNull()) {
                m_pdfThumbnailLabel->setPixmap(QPixmap::fromImage(outcome.firstPageImage).scaled(
                    60, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                m_pdfThumbnailLabel->setPixmap(
                    IconUtils::getPixmap(IconType::Metadata, QColor("#2563eb"), 32));
            }
            m_pdfFileNameLabel->setText(info.fileName());
            const double sizeMb = info.size() / (1024.0 * 1024.0);
            m_pdfFileDetailsLabel->setText(
                outcome.pages > 0
                    ? tr("%1 pages · %2 MB").arg(outcome.pages).arg(QString::number(sizeMb, 'f', 2))
                    : tr("%1 MB").arg(QString::number(sizeMb, 'f', 2)));
            const auto& metadata = outcome.metadata;
            m_titleEdit->setText(metadata.title.isEmpty() ? info.completeBaseName() : metadata.title);
            m_authorEdit->setText(metadata.author);
            m_subjectEdit->setText(metadata.subject);
            m_keywordsEdit->setText(metadata.keywords);
            m_creatorEdit->setText(metadata.creator);
            m_producerEdit->setText(metadata.producer);
            m_loadedFile = pdfPath;
            m_dropZone->hide();
            m_formContainer->show();
            m_statusBanner->hideBanner();
        });
}

void MetadataPage::saveMetadata() {
    if (m_loadedFile.isEmpty()) {
        m_statusBanner->showBanner(BannerType::Warning, tr("Please upload a PDF document first."));
        return;
    }

    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/LocalTools";
    QDir().mkpath(defaultDir);

    QFileInfo fi(m_loadedFile);
    QString suggestedName = defaultDir + "/" + fi.completeBaseName() + "_updated.pdf";

    QString targetFile = OutputFileDialog::getSaveFileName(this, tr("Save PDF with Updated Metadata"), suggestedName, tr("PDF Files (*.pdf)"), {m_loadedFile});
    if (targetFile.isEmpty()) return;

    const QString inputFile = m_loadedFile;
    const QString title = m_titleEdit->text();
    const QString author = m_authorEdit->text();
    const QString subject = m_subjectEdit->text();
    const QString keywords = m_keywordsEdit->text();
    const QString creator = m_creatorEdit->text();
    const QString producer = m_producerEdit->text();
    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_saveButton->setEnabled(false);
    AsyncTask::run<Core::Result<void>>(
        this,
        [=](AsyncTask::CancelCheck cancel) {
            if (cancel()) {
                return Core::Result<void>::fail(
                    Core::PdfErrorCategory::Cancelled, "Operation was cancelled.");
            }
            return Core::PdfiumBackend::saveWithMetadata(
                inputFile.toStdString(), targetFile.toStdString(),
                title, author, subject, keywords, creator, producer);
        },
        [this, targetFile](Core::Result<void> result) {
            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            m_saveButton->setEnabled(true);
            if (result.hasValue()) {
                Services::SettingsService::instance().addRecentFile(targetFile);
                m_statusBanner->hideBanner();
                m_resultScreen->showResult(targetFile, tr("Metadata saved successfully!"));
                m_stackedWidget->setCurrentWidget(m_resultScreen);
            } else {
                m_statusBanner->showBanner(
                    BannerType::Error,
                    tr("Failed to save metadata: ") +
                        QString::fromStdString(result.error().toUserFriendlyMessage()));
            }
        });
}

void MetadataPage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void MetadataPage::retranslateUi() {
    if (m_viewBtn) m_viewBtn->setText(tr("View Metadata"));
    if (m_editBtn) m_editBtn->setText(tr("Edit Metadata"));
    if (m_cleanBtn) m_cleanBtn->setText(tr("Inspect & Clean"));
    fitModeButtonToText(m_viewBtn);
    fitModeButtonToText(m_editBtn);
    fitModeButtonToText(m_cleanBtn);
    if (m_saveButton) m_saveButton->setText(tr("Save Metadata"));
    if (m_dropZone) {
        m_dropZone->setInstruction(tr("Drop PDF to Edit Metadata"),
                                   tr("PDF title, author, subject, and keywords can be updated"));
    }

    // Shared View / Inspect & Clean panel
    if (m_metaDropZone) {
        m_metaDropZone->setInstruction(tr("Drop File to Read and Clean Metadata"),
                                       tr("Images (JPG, PNG, WEBP, BMP, TIFF) or PDF documents supported"));
    }
    if (m_metaChangeFileBtn) m_metaChangeFileBtn->setText(tr("Select Different File"));
    if (m_metaCleanBtn) m_metaCleanBtn->setText(tr("Clean Metadata (Privacy)"));
    if (m_metaPrivacyNoteLabel) {
        m_metaPrivacyNoteLabel->setText(
            tr("Metadata cleaning removes device info, GPS, and EXIF tags to protect your privacy."));
    }
    if (m_metaTable) {
        m_metaTable->setHorizontalHeaderLabels({tr("Category"), tr("Property"), tr("Value")});
    }

    setMode(m_currentMode);
}

} // namespace LocalTools::UI
