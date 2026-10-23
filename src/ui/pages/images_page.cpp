#include "images_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QFileInfo>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QPainter>
#include <QDir>
#include <QApplication>
#include <cmath>
#include <algorithm>

#include "../../core/backends/image_backend.h"
#include "../../services/settings_service.h"
#include "../../services/file_service.h"
#include "../../services/logging_service.h"
#include "../common/async_task.h"
#include "../theme.h"

namespace LocalTools::UI {

namespace {

struct ImageBatchOutcome {
    int successCount{0};
    qint64 totalNewBytes{0};
    QString lastOutputPath;
    QStringList outputPaths;
    QString firstFailedMessage;
    bool cancelled{false};
};

struct ImageDetails {
    Core::ImageBackend::ImageInfo info;
    QImage thumbnail;
};

} // namespace

ImagesPage::ImagesPage(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

void ImagesPage::setupUi() {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_pageStack = new QStackedWidget(this);

    auto *mainContainer = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(mainContainer);
    mainLayout->setContentsMargins(28, 20, 28, 20);
    mainLayout->setSpacing(14);

    // 1. Top Sub-navigation Tabs (Compress, Convert)
    mainLayout->addWidget(createTabHeader());

    // Tab Subtitle / Purpose Description
    m_tabDescriptionLabel = new QLabel(this);
    m_tabDescriptionLabel->setStyleSheet("font-size: 13px; color: palette(text); opacity: 0.7; margin-bottom: 2px;");
    updateTabDescription(0);
    mainLayout->addWidget(m_tabDescriptionLabel);

    // 2. Inline Status Banner
    m_statusBanner = new StatusBannerWidget(this);
    mainLayout->addWidget(m_statusBanner);

    // 3. Drop Zone for Images
    m_dropZone = new DropZoneWidget(this);
    m_dropZone->setInstruction(tr("Drag Image Files Here"),
                               tr("or click to select files (JPG, PNG, WEBP, BMP, TIFF)"));
    m_dropZone->setAcceptedFileExtensions({"jpg", "jpeg", "png", "webp", "bmp", "tiff", "tif"},
                                          tr("Select Image Files"),
                                          tr("Only supported image files are accepted (JPG, PNG, WEBP, BMP, TIFF)"));
    connect(m_dropZone, &DropZoneWidget::filesDropped, this, &ImagesPage::handleFilesDropped);
    mainLayout->addWidget(m_dropZone);

    // 4. Selected Files Workspace Container
    m_workspaceContainer = new QFrame(this);
    m_workspaceContainer->setObjectName("CardFrame");
    m_workspaceContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_workspaceContainer->setFixedHeight(300);
    auto *wsLayout = new QVBoxLayout(m_workspaceContainer);
    wsLayout->setContentsMargins(18, 12, 18, 12);
    wsLayout->setSpacing(8);

    // Header info row
    auto *infoRow = new QHBoxLayout();
    m_selectedFilesLabel = new QLabel(m_workspaceContainer);
    QFont bFont = m_selectedFilesLabel->font();
    bFont.setPointSize(13);
    bFont.setBold(true);
    m_selectedFilesLabel->setFont(bFont);
    infoRow->addWidget(m_selectedFilesLabel);
    infoRow->addStretch();

    m_changeFileBtn = new QPushButton(tr("Change Images"), m_workspaceContainer);
    m_changeFileBtn->setObjectName("ToolTabButton");
    m_changeFileBtn->setIcon(IconUtils::getIcon(IconType::Folder, Theme::color(ColorToken::TextSecondary), 14));
    m_changeFileBtn->setCursor(Qt::PointingHandCursor);
    connect(m_changeFileBtn, &QPushButton::clicked, this, &ImagesPage::resetWorkspace);
    infoRow->addWidget(m_changeFileBtn);
    wsLayout->addLayout(infoRow);

    // Image details row (Thumbnail + Dimensions/Size)
    auto *previewRow = new QHBoxLayout();
    previewRow->setSpacing(14);

    m_imagePreviewLabel = new QLabel(m_workspaceContainer);
    m_imagePreviewLabel->setFixedSize(80, 56);
    m_imagePreviewLabel->setStyleSheet("background-color: palette(alternate-base); border-radius: 6px; border: 1px solid palette(mid);");
    m_imagePreviewLabel->setAlignment(Qt::AlignCenter);
    previewRow->addWidget(m_imagePreviewLabel);

    m_fileInfoLabel = new QLabel(m_workspaceContainer);
    m_fileInfoLabel->setStyleSheet("font-size: 12px; color: palette(text);");
    previewRow->addWidget(m_fileInfoLabel, 1);
    wsLayout->addLayout(previewRow);

    // 5. Stack of Sub-Panels (Compress, Convert)
    m_panelStack = new QStackedWidget(m_workspaceContainer);
    m_compressPanel = createCompressPanel();
    m_convertPanel = createConvertPanel();

    m_panelStack->addWidget(m_compressPanel); // 0
    m_panelStack->addWidget(m_convertPanel);  // 1
    wsLayout->addWidget(m_panelStack, 1);

    // 6. Action Button & Progress Bar
    auto *bottomRow = new QHBoxLayout();
    m_progressBar = new QProgressBar(m_workspaceContainer);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    bottomRow->addWidget(m_progressBar, 1);

    m_actionBtn = new QPushButton(tr("Compress Image"), m_workspaceContainer);
    m_actionBtn->setObjectName("PrimaryButton");
    m_actionBtn->setMinimumWidth(170);
    m_actionBtn->setFixedHeight(38);
    m_actionBtn->setCursor(Qt::PointingHandCursor);
    connect(m_actionBtn, &QPushButton::clicked, this, &ImagesPage::processCurrentImageAction);
    bottomRow->addWidget(m_actionBtn);
    wsLayout->addLayout(bottomRow);

    m_workspaceContainer->hide();
    mainLayout->addWidget(m_workspaceContainer, 0, Qt::AlignTop);
    mainLayout->addStretch(1);

    m_pageStack->addWidget(mainContainer);

    // 7. Result Screen
    m_resultScreen = new ResultScreenWidget(this);
    connect(m_resultScreen, &ResultScreenWidget::backToWorkspaceRequested, this, &ImagesPage::resetWorkspace);
    m_pageStack->addWidget(m_resultScreen);

    rootLayout->addWidget(m_pageStack);
    retranslateUi();
}

QWidget* ImagesPage::createTabHeader() {
    auto *headerWidget = new QWidget(this);
    auto *layout = new QHBoxLayout(headerWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_tabButtonGroup = new QButtonGroup(this);
    m_tabButtonGroup->setExclusive(true);

    auto createTabBtn = [this, layout](const QString& text, IconType iconType, int id) {
        auto *btn = new QPushButton(text, this);
        btn->setObjectName("ToolTabButton");
        btn->setCheckable(true);
        btn->setIcon(IconUtils::getIcon(iconType, Theme::color(ColorToken::TextSecondary), 16));
        btn->setIconSize(QSize(16, 16));
        btn->setCursor(Qt::PointingHandCursor);
        m_tabButtonGroup->addButton(btn, id);
        layout->addWidget(btn);
        return btn;
    };

    m_compressTabBtn = createTabBtn(tr("Compress"), IconType::Compress, 0);
    m_convertTabBtn = createTabBtn(tr("Convert"), IconType::Convert, 1);

    m_compressTabBtn->setChecked(true);
    layout->addStretch();

    connect(m_tabButtonGroup, &QButtonGroup::idClicked, this, [this](int id) {
        setMode(static_cast<ImageTabMode>(id));
    });

    return headerWidget;
}

void ImagesPage::updateTabDescription(int id) {
    if (!m_tabDescriptionLabel) return;
    switch (id) {
        case 0:
            m_tabDescriptionLabel->setText(tr("Reduce image file size while maintaining quality."));
            break;
        case 1:
            m_tabDescriptionLabel->setText(tr("Convert images to JPG, PNG, WEBP, BMP, or TIFF formats."));
            break;
        default:
            m_tabDescriptionLabel->setText(QString());
            break;
    }
}

QWidget* ImagesPage::createCompressPanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 2, 0, 0);
    layout->setSpacing(6);

    auto *optGroup = new QButtonGroup(panel);

    // Option A: Quality Slider
    auto *radioA = new QHBoxLayout();
    m_qualityRadio = new QRadioButton(tr("Quality Level:"), panel);
    m_qualityRadio->setChecked(true);
    optGroup->addButton(m_qualityRadio);
    radioA->addWidget(m_qualityRadio);

    m_qualitySlider = new QSlider(Qt::Horizontal, panel);
    m_qualitySlider->setObjectName("ImageQualitySlider");
    m_qualitySlider->setRange(10, 100);
    m_qualitySlider->setValue(80);
    radioA->addWidget(m_qualitySlider, 1);

    m_qualityValueLabel = new QLabel(tr("%80 (Recommended)"), panel);
    m_qualityValueLabel->setFixedWidth(180);
    m_qualityValueLabel->setObjectName("AccentValueLabel");
    radioA->addWidget(m_qualityValueLabel);
    layout->addLayout(radioA);

    connect(m_qualitySlider, &QSlider::valueChanged, this, [this](int val) {
        QString text = QString("%%1").arg(val);
        if (val >= 80) text += tr(" (Recommended - High Quality)");
        else if (val >= 50) text += tr(" (Balanced)");
        else text += tr(" (Small Size)");
        m_qualityValueLabel->setText(text);
    });

    // Option B: Target Size
    auto *radioB = new QHBoxLayout();
    m_targetSizeRadio = new QRadioButton(tr("Target Maximum File Size:"), panel);
    optGroup->addButton(m_targetSizeRadio);
    radioB->addWidget(m_targetSizeRadio);

    m_targetSizeCombo = new QComboBox(panel);
    m_targetSizeCombo->addItem(tr("200 KB"), 200 * 1024);
    m_targetSizeCombo->addItem(tr("300 KB"), 300 * 1024);
    m_targetSizeCombo->addItem(tr("500 KB"), 500 * 1024);
    m_targetSizeCombo->addItem(tr("1 MB"), 1024 * 1024);
    m_targetSizeCombo->addItem(tr("2 MB"), 2 * 1024 * 1024);
    m_targetSizeCombo->addItem(tr("5 MB"), 5 * 1024 * 1024);
    m_targetSizeCombo->setCurrentIndex(2); // 500 KB
    radioB->addWidget(m_targetSizeCombo);
    radioB->addStretch();
    layout->addLayout(radioB);

    return panel;
}

QWidget* ImagesPage::createConvertPanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 2, 0, 0);
    layout->setSpacing(6);

    auto *row = new QHBoxLayout();
    auto *lbl = new QLabel(tr("Target Output Format:"), panel);
    lbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    row->addWidget(lbl);

    m_formatCombo = new QComboBox(panel);
    QStringList writeFormats = Core::ImageBackend::supportedWriteFormats();
    if (writeFormats.contains("jpg") || writeFormats.contains("jpeg")) {
        m_formatCombo->addItem("JPEG (.jpg)", "jpg");
    }
    if (writeFormats.contains("png")) {
        m_formatCombo->addItem("PNG (.png)", "png");
    }
    if (writeFormats.contains("webp")) {
        m_formatCombo->addItem("WEBP (.webp)", "webp");
    }
    if (writeFormats.contains("bmp")) {
        m_formatCombo->addItem("BMP (.bmp)", "bmp");
    }
    if (writeFormats.contains("tiff") || writeFormats.contains("tif")) {
        m_formatCombo->addItem("TIFF (.tiff)", "tiff");
    }
    if (m_formatCombo->count() == 0) {
        m_formatCombo->addItem("JPEG (.jpg)", "jpg");
        m_formatCombo->addItem("PNG (.png)", "png");
        m_formatCombo->addItem("BMP (.bmp)", "bmp");
    }
    row->addWidget(m_formatCombo);
    row->addStretch();
    layout->addLayout(row);

    // Quality slider container for lossy formats (JPEG/WEBP)
    m_convertQualityContainer = new QWidget(panel);
    auto *qLayout = new QHBoxLayout(m_convertQualityContainer);
    qLayout->setContentsMargins(0, 0, 0, 0);
    qLayout->setSpacing(10);

    auto *qLabel = new QLabel(tr("Conversion Quality:"), m_convertQualityContainer);
    qLayout->addWidget(qLabel);

    m_convertQualitySlider = new QSlider(Qt::Horizontal, m_convertQualityContainer);
    m_convertQualitySlider->setObjectName("ImageQualitySlider");
    m_convertQualitySlider->setRange(10, 100);
    m_convertQualitySlider->setValue(90);
    qLayout->addWidget(m_convertQualitySlider, 1);

    m_convertQualityValueLabel = new QLabel("%90", m_convertQualityContainer);
    m_convertQualityValueLabel->setFixedWidth(60);
    m_convertQualityValueLabel->setObjectName("AccentValueLabel");
    qLayout->addWidget(m_convertQualityValueLabel);
    layout->addWidget(m_convertQualityContainer);

    connect(m_convertQualitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_convertQualityValueLabel->setText(QString("%%1").arg(val));
    });

    // Format informational note
    m_convertFormatNoteLabel = new QLabel(m_convertQualityContainer);
    m_convertFormatNoteLabel->setStyleSheet("font-size: 11px; color: palette(text); opacity: 0.75;");
    m_convertFormatNoteLabel->setText(tr("JPEG format does not support transparency; transparent backgrounds will automatically be filled with white."));
    layout->addWidget(m_convertFormatNoteLabel);

    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        QString fmt = m_formatCombo->currentData().toString();
        bool isLossy = (fmt == "jpg" || fmt == "webp");
        m_convertQualityContainer->setVisible(isLossy);
        
        if (fmt == "jpg") {
            m_convertFormatNoteLabel->setText(tr("Perfect for photos. Transparent backgrounds are saved as white."));
        } else if (fmt == "png") {
            m_convertFormatNoteLabel->setText(tr("PNG is a lossless format; pixels and transparency are fully preserved."));
        } else if (fmt == "webp") {
            m_convertFormatNoteLabel->setText(tr("WEBP is a modern and highly efficient web format; supports transparency."));
        } else if (fmt == "bmp") {
            m_convertFormatNoteLabel->setText(tr("BMP is an uncompressed standard Windows bitmap image format."));
        } else if (fmt == "tiff") {
            m_convertFormatNoteLabel->setText(tr("TIFF is a professional print and high quality archiving format."));
        }
    });

    return panel;
}

void ImagesPage::setMode(ImageTabMode mode) {
    int id = static_cast<int>(mode);
    if (auto *btn = m_tabButtonGroup->button(id)) {
        btn->setChecked(true);
    }
    m_panelStack->setCurrentIndex(id);
    updateTabDescription(id);

    m_workspaceContainer->setFixedHeight(300);
    m_workspaceContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_workspaceContainer->updateGeometry();
    if (this->layout()) this->layout()->activate();

    bool isMulti = (m_loadedImages.size() > 1);
    if (id == 0) m_actionBtn->setText(isMulti ? tr("Compress %1 Images").arg(m_loadedImages.size()) : tr("Compress Image"));
    else if (id == 1) m_actionBtn->setText(isMulti ? tr("Convert %1 Images").arg(m_loadedImages.size()) : tr("Convert Format"));
}

void ImagesPage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ImagesPage::retranslateUi() {
    if (m_compressTabBtn) m_compressTabBtn->setText(tr("Compress"));
    if (m_convertTabBtn) m_convertTabBtn->setText(tr("Convert"));

    updateTabDescription(m_panelStack->currentIndex());

    if (m_dropZone) {
        m_dropZone->setInstruction(tr("Drag Image Files Here"),
                                   tr("or click to select files (JPG, PNG, WEBP, BMP, TIFF)"));
    }
    if (m_changeFileBtn) m_changeFileBtn->setText(tr("Change Images"));
    if (m_qualityRadio) m_qualityRadio->setText(tr("Quality Level:"));
    if (m_targetSizeRadio) m_targetSizeRadio->setText(tr("Target Maximum File Size:"));

    if (m_targetSizeCombo && m_targetSizeCombo->count() >= 5) {
        m_targetSizeCombo->setItemText(0, tr("Max 50 KB (Avatar / Icon)"));
        m_targetSizeCombo->setItemText(1, tr("Max 100 KB (Fast Web Loading)"));
        m_targetSizeCombo->setItemText(2, tr("Max 200 KB (Standard Quality)"));
        m_targetSizeCombo->setItemText(3, tr("Max 500 KB (High Quality Web)"));
        m_targetSizeCombo->setItemText(4, tr("Max 1 MB (Email / Document)"));
    }

    setMode(static_cast<ImageTabMode>(m_panelStack->currentIndex()));
}

void ImagesPage::handleFilesDropped(const QStringList& files) {
    loadImages(files);
}

void ImagesPage::loadImages(const QStringList& filePaths) {
    if (filePaths.isEmpty()) return;
    m_loadedImages = filePaths;
    updateFileDetails();
    m_dropZone->hide();
    m_workspaceContainer->show();
    m_statusBanner->hideBanner();
    setMode(static_cast<ImageTabMode>(m_panelStack->currentIndex()));
}

void ImagesPage::updateFileDetails() {
    if (m_loadedImages.isEmpty()) return;

    QString firstPath = m_loadedImages.first();
    QFileInfo fi(firstPath);
    m_origTotalBytes = 0;

    for (const QString& path : m_loadedImages) {
        m_origTotalBytes += QFileInfo(path).size();
    }

    if (m_loadedImages.size() == 1) {
        m_selectedFilesLabel->setText(fi.fileName());
    } else {
        m_selectedFilesLabel->setText(tr("%1 Image Files Selected (Batch)").arg(m_loadedImages.size()));
    }

    m_fileInfoLabel->setText(tr("Analyzing..."));
    m_imagePreviewLabel->clear();
    m_actionBtn->setEnabled(false);

    const QStringList inputPaths = m_loadedImages;
    const qint64 totalBytes = m_origTotalBytes;
    AsyncTask::run<Core::Result<ImageDetails>>(
        this,
        [firstPath](AsyncTask::CancelCheck cancel) -> Core::Result<ImageDetails> {
            if (cancel()) {
                return Core::Result<ImageDetails>::fail(
                    Core::PdfErrorCategory::Cancelled, "Image analysis was cancelled");
            }

            auto infoRes = Core::ImageBackend::getInfo(firstPath);
            if (!infoRes.hasValue()) {
                return infoRes.error();
            }

            QImage image(firstPath);
            if (image.isNull()) {
                return Core::Result<ImageDetails>::fail(
                    Core::PdfErrorCategory::InvalidInput, "Image file could not be decoded");
            }

            ImageDetails details;
            details.info = infoRes.value();
            details.thumbnail = image.scaled(
                120, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            return details;
        },
        [this, inputPaths, totalBytes](Core::Result<ImageDetails> infoRes) {
            if (m_loadedImages != inputPaths) {
                return;
            }

            m_actionBtn->setEnabled(true);
            if (!infoRes.hasValue()) {
                m_fileInfoLabel->setText(tr("Image details could not be read."));
                m_statusBanner->showBanner(
                    BannerType::Error,
                    QString::fromStdString(infoRes.error().toUserFriendlyMessage()));
                return;
            }

        const auto& info = infoRes.value().info;
        m_origWidth = info.width;
        m_origHeight = info.height;

        double sizeMb = info.fileSize / (1024.0 * 1024.0);
        double totalMb = totalBytes / (1024.0 * 1024.0);

        QString details;
        if (m_loadedImages.size() == 1) {
            details = tr("<b>Format:</b> %1 · <b>Resolution:</b> %2 × %3 px · <b>Size:</b> %4 MB")
                .arg(info.format.toUpper())
                .arg(info.width)
                .arg(info.height)
                .arg(QString::number(sizeMb, 'f', 2));
        } else {
            details = tr("<b>First File:</b> %1 (%2 × %3 px) · <b>Total Size:</b> %4 MB (%5 File(s))")
                .arg(info.format.toUpper())
                .arg(info.width)
                .arg(info.height)
                .arg(QString::number(totalMb, 'f', 2))
                .arg(m_loadedImages.size());
        }
        m_fileInfoLabel->setText(details);

            m_imagePreviewLabel->setPixmap(QPixmap::fromImage(infoRes.value().thumbnail));
        });

    // Refresh action button text
    int curTab = m_panelStack->currentIndex();
    bool isMulti = (m_loadedImages.size() > 1);
    if (curTab == 0) m_actionBtn->setText(isMulti ? tr("Compress %1 Images").arg(m_loadedImages.size()) : tr("Compress Image"));
    else if (curTab == 1) m_actionBtn->setText(isMulti ? tr("Convert %1 Images").arg(m_loadedImages.size()) : tr("Convert Format"));
}

void ImagesPage::resetWorkspace() {
    m_loadedImages.clear();
    m_origWidth = 0;
    m_origHeight = 0;
    m_origTotalBytes = 0;
    m_workspaceContainer->hide();
    m_dropZone->show();
    m_pageStack->setCurrentIndex(0);
}

void ImagesPage::processCurrentImageAction() {
    if (m_loadedImages.isEmpty()) {
        m_statusBanner->showBanner(BannerType::Warning, tr("Please select at least one image file."));
        return;
    }

    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_actionBtn->setEnabled(false);

    const QString defaultDir = Services::SettingsService::instance().getDefaultOutputDir();
    QDir().mkpath(defaultDir);
    const int currentTab = m_panelStack->currentIndex();
    const QStringList inputPaths = m_loadedImages;
    const bool useTargetSize = m_targetSizeRadio->isChecked();
    const qint64 targetBytes = m_targetSizeCombo->currentData().toLongLong();
    const int compressionQuality = m_qualitySlider->value();
    const QString targetExtension = m_formatCombo->currentData().toString();
    const int conversionQuality = (targetExtension == "jpg" || targetExtension == "webp")
        ? m_convertQualitySlider->value()
        : -1;
    const qint64 originalBytes = m_origTotalBytes;

    AsyncTask::run<ImageBatchOutcome>(
        this,
        [=](AsyncTask::CancelCheck cancel) {
            ImageBatchOutcome outcome;
            for (const QString& inputPath : inputPaths) {
                if (cancel()) {
                    outcome.cancelled = true;
                    break;
                }

                const QFileInfo inputInfo(inputPath);
                QString outputPath;
                Core::Result<void> result = Core::Result<void>::ok();
                if (currentTab == 0) {
                    outputPath = Services::FileService::instance().generateUniqueOutputPath(
                        defaultDir + "/" + inputInfo.completeBaseName() + "_compressed.jpg");
                    result = useTargetSize
                        ? Core::ImageBackend::compressToTargetSize(inputPath, outputPath, targetBytes)
                        : Core::ImageBackend::compress(inputPath, outputPath, compressionQuality);
                } else {
                    outputPath = Services::FileService::instance().generateUniqueOutputPath(
                        defaultDir + "/" + inputInfo.completeBaseName() + "_converted." + targetExtension);
                    result = Core::ImageBackend::convertFormat(
                        inputPath, outputPath, targetExtension, conversionQuality);
                }

                if (result.hasValue()) {
                    ++outcome.successCount;
                    outcome.lastOutputPath = outputPath;
                    outcome.outputPaths.append(outputPath);
                    outcome.totalNewBytes += QFileInfo(outputPath).size();
                } else if (outcome.firstFailedMessage.isEmpty()) {
                    outcome.firstFailedMessage =
                        QString::fromStdString(result.error().toUserFriendlyMessage());
                }
            }
            return outcome;
        },
        [=, this](ImageBatchOutcome outcome) {
            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            m_actionBtn->setEnabled(true);
            for (const QString& outputPath : outcome.outputPaths) {
                Services::SettingsService::instance().addRecentFile(outputPath);
            }

            if (outcome.cancelled) {
                m_statusBanner->showBanner(BannerType::Warning, tr("Operation was cancelled."));
                return;
            }
            if (outcome.successCount != inputPaths.size()) {
                m_statusBanner->showBanner(
                    BannerType::Error,
                    outcome.firstFailedMessage.isEmpty()
                        ? tr("Some errors occurred during the operation.")
                        : outcome.firstFailedMessage);
                return;
            }

            const double originalMb = originalBytes / (1024.0 * 1024.0);
            const double newMb = outcome.totalNewBytes / (1024.0 * 1024.0);
            const double savedPercent = (originalBytes > 0 && outcome.totalNewBytes < originalBytes)
                ? (static_cast<double>(originalBytes - outcome.totalNewBytes) / originalBytes) * 100.0
                : 0.0;
            QString summaryMessage;
            if (inputPaths.size() == 1 && currentTab == 0) {
                summaryMessage = tr("Image compressed successfully!\nOriginal: %1 MB · New: %2 MB · %%3 Savings")
                    .arg(QString::number(originalMb, 'f', 2))
                    .arg(QString::number(newMb, 'f', 2))
                    .arg(QString::number(savedPercent, 'f', 1));
            } else if (inputPaths.size() == 1) {
                summaryMessage = tr("Image successfully converted to %1 format!")
                    .arg(targetExtension.toUpper());
            } else {
                summaryMessage = tr("%1 image(s) processed successfully!\nTotal Original: %2 MB · New: %3 MB · %%4 Savings")
                    .arg(inputPaths.size())
                    .arg(QString::number(originalMb, 'f', 2))
                    .arg(QString::number(newMb, 'f', 2))
                    .arg(QString::number(savedPercent, 'f', 1));
            }
            m_resultScreen->showResult(outcome.lastOutputPath, summaryMessage);
            m_pageStack->setCurrentIndex(1);
        });
}

} // namespace LocalTools::UI
