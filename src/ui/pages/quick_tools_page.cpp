#include "quick_tools_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QClipboard>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QHeaderView>
#include <QFileInfo>
#include <QMessageBox>
#include <QEvent>

#include "../../core/backends/quick_tools_backend.h"
#include "../../services/settings_service.h"
#include "../../services/file_service.h"
#include "../theme.h"
#include "../common/async_task.h"
#include "../common/output_file_dialog.h"

namespace LocalTools::UI {

namespace {

struct QrReaderLoad {
    QImage thumbnail;
    QSize imageSize;
    Core::QrDecodeResult decoded;
};

} // namespace

QuickToolsPage::QuickToolsPage(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

void QuickToolsPage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 20, 28, 20);
    mainLayout->setSpacing(14);

    // 1. Tab Header Bar
    mainLayout->addWidget(createTabHeader());

    // 2. Status Banner & Progress Bar
    m_statusBanner = new StatusBannerWidget(this);
    m_statusBanner->hide();
    mainLayout->addWidget(m_statusBanner);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    // 3. Page Stack (Workspaces vs Result Screen)
    m_pageStack = new QStackedWidget(this);

    // Workspace Stack (Panel 0: QR Gen, Panel 1: QR Reader).
    // Metadata used to be a third panel here; it now lives on the Metadata page
    // so every metadata operation sits in one place.
    m_panelStack = new QStackedWidget(m_pageStack);
    m_qrGenPanel = createQrPanel();
    m_qrReadPanel = createQrReaderPanel();

    m_panelStack->addWidget(m_qrGenPanel);
    m_panelStack->addWidget(m_qrReadPanel);

    m_pageStack->addWidget(m_panelStack); // Index 0

    // Result Screen Widget (Index 1)
    m_resultScreen = new ResultScreenWidget(m_pageStack);
    connect(m_resultScreen, &ResultScreenWidget::backToWorkspaceRequested, this, &QuickToolsPage::resetWorkspace);
    m_pageStack->addWidget(m_resultScreen);

    mainLayout->addWidget(m_pageStack);
    mainLayout->addStretch(1);

    setMode(QuickTabMode::QrCode);
    retranslateUi();
}

QWidget* QuickToolsPage::createTabHeader() {
    auto *headerWidget = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    m_tabButtonGroup = new QButtonGroup(this);
    m_tabButtonGroup->setExclusive(true);

    auto createTabBtn = [this](const QString& text, IconType iconType, int id) -> QPushButton* {
        auto *btn = new QPushButton(text, this);
        btn->setObjectName("QuickModeTabButton");
        btn->setCheckable(true);
        btn->setFocusPolicy(Qt::StrongFocus);
        btn->setIcon(IconUtils::getIcon(iconType, Theme::color(ColorToken::TextSecondary), 16));
        btn->setIconSize(QSize(16, 16));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumWidth(220);
        btn->setMinimumHeight(44);
        m_tabButtonGroup->addButton(btn, id);
        return btn;
    };

    m_qrGenTabBtn = createTabBtn(tr("QR Code Generator"), IconType::ImagesToPdf, static_cast<int>(QuickTabMode::QrCode));
    m_qrReadTabBtn = createTabBtn(tr("QR Code Reader"), IconType::View, static_cast<int>(QuickTabMode::QrReader));

    m_qrGenTabBtn->setAccessibleName(tr("QR Code Generator"));
    m_qrGenTabBtn->setAccessibleDescription(tr("Switch to QR code generator mode"));
    m_qrGenTabBtn->setToolTip(tr("Switch to QR code generator mode"));
    m_qrReadTabBtn->setAccessibleName(tr("QR Code Reader"));
    m_qrReadTabBtn->setAccessibleDescription(tr("Switch to QR code reader mode"));
    m_qrReadTabBtn->setToolTip(tr("Switch to QR code reader mode"));

    m_qrGenTabBtn->setChecked(true);

    headerLayout->addWidget(m_qrGenTabBtn);
    headerLayout->addWidget(m_qrReadTabBtn);
    headerLayout->addStretch();

    connect(m_tabButtonGroup, &QButtonGroup::idClicked, this, [this](int id) {
        setMode(static_cast<QuickTabMode>(id));
    });

    return headerWidget;
}

void QuickToolsPage::setMode(QuickTabMode mode) {
    int id = static_cast<int>(mode);
    if (auto *btn = m_tabButtonGroup->button(id)) {
        btn->setChecked(true);
    }
    m_panelStack->setCurrentIndex(id);
    m_pageStack->setCurrentIndex(0);

    const QString checkMark = QString(QChar(0x2713));
    m_qrGenTabBtn->setText(mode == QuickTabMode::QrCode
        ? QStringLiteral("%1  %2").arg(checkMark, tr("QR Code Generator"))
        : tr("QR Code Generator"));
    m_qrReadTabBtn->setText(mode == QuickTabMode::QrReader
        ? QStringLiteral("%1  %2").arg(checkMark, tr("QR Code Reader"))
        : tr("QR Code Reader"));
    m_qrGenTabBtn->setIcon(IconUtils::getIcon(
        IconType::ImagesToPdf,
        Theme::color(mode == QuickTabMode::QrCode ? ColorToken::OnAccent : ColorToken::TextSecondary), 16));
    m_qrReadTabBtn->setIcon(IconUtils::getIcon(
        IconType::View,
        Theme::color(mode == QuickTabMode::QrReader ? ColorToken::OnAccent : ColorToken::TextSecondary), 16));

    if (mode == QuickTabMode::QrCode) {
        setAccessibleName(tr("QR Code Generator"));
        m_qrGenPanel->setAccessibleName(tr("QR Code Generator workspace"));
        updateQrPreview();
    } else {
        setAccessibleName(tr("QR Code Reader"));
        m_qrReadPanel->setAccessibleName(tr("QR Code Reader workspace"));
    }
}

QWidget* QuickToolsPage::createQrPanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(24);

    // Left Column: Configuration Controls
    auto *leftBox = new QWidget(panel);
    auto *leftLayout = new QVBoxLayout(leftBox);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(14);

    m_qrInputLabel = new QLabel(tr("QR Code Text or URL:"), leftBox);
    m_qrInputLabel->setStyleSheet("font-weight: 600; font-size: 13px;");
    leftLayout->addWidget(m_qrInputLabel);

    m_qrInputEdit = new QLineEdit(leftBox);
    m_qrInputEdit->setPlaceholderText(tr("Enter text, website, or Wi-Fi connection..."));
    m_qrInputEdit->setFixedHeight(40);
    m_qrInputEdit->setText("");
    connect(m_qrInputEdit, &QLineEdit::textChanged, this, &QuickToolsPage::updateQrPreview);
    leftLayout->addWidget(m_qrInputEdit);

    auto *sizeRow = new QHBoxLayout();
    m_qrSizeLabel = new QLabel(tr("Resolution Size:"), leftBox);
    m_qrSizeLabel->setStyleSheet("font-size: 13px;");
    m_qrSizeCombo = new QComboBox(leftBox);
    m_qrSizeCombo->addItem("256 × 256 px", 256);
    m_qrSizeCombo->addItem(tr("512 × 512 px (Standard)"), 512);
    m_qrSizeCombo->addItem(tr("1024 × 1024 px (High Quality)"), 1024);
    m_qrSizeCombo->setCurrentIndex(1);
    connect(m_qrSizeCombo, &QComboBox::currentIndexChanged, this, &QuickToolsPage::updateQrPreview);

    sizeRow->addWidget(m_qrSizeLabel);
    sizeRow->addWidget(m_qrSizeCombo, 1);
    leftLayout->addLayout(sizeRow);

    leftLayout->addSpacing(10);

    // Action Buttons
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);

    m_qrCopyBtn = new QPushButton(tr("Copy QR Code"), leftBox);
    m_qrCopyBtn->setObjectName("SecondaryButton");
    m_qrCopyBtn->setIcon(IconUtils::getIcon(IconType::Convert, QColor("#2563eb"), 16));
    m_qrCopyBtn->setFixedHeight(42);
    connect(m_qrCopyBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentQrImage.isNull()) {
            QGuiApplication::clipboard()->setImage(m_currentQrImage);
            m_statusBanner->showBanner(BannerType::Success, tr("QR code copied to clipboard!"));
        }
    });

    m_qrSaveBtn = new QPushButton(tr("Save QR Code"), leftBox);
    m_qrSaveBtn->setObjectName("PrimaryActionButton");
    m_qrSaveBtn->setIcon(IconUtils::getIcon(IconType::ImagesToPdf, Qt::white, 16));
    m_qrSaveBtn->setFixedHeight(42);
    connect(m_qrSaveBtn, &QPushButton::clicked, this, &QuickToolsPage::saveQrCode);

    btnRow->addWidget(m_qrCopyBtn);
    btnRow->addWidget(m_qrSaveBtn);
    leftLayout->addLayout(btnRow);

    leftLayout->addStretch();
    layout->addWidget(leftBox, 1);

    // Right Column: Live QR Preview Card
    auto *previewCard = new QFrame(panel);
    previewCard->setObjectName("FileCard");
    previewCard->setStyleSheet("QFrame#FileCard { background-color: palette(base); border: 1px solid palette(mid); border-radius: 12px; }");
    previewCard->setFixedWidth(340);
    auto *previewLayout = new QVBoxLayout(previewCard);
    previewLayout->setContentsMargins(16, 16, 16, 16);
    previewLayout->setSpacing(12);
    previewLayout->setAlignment(Qt::AlignCenter);

    m_qrPreviewTitleLabel = new QLabel(tr("Live Preview"), previewCard);
    m_qrPreviewTitleLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: palette(text);");
    m_qrPreviewTitleLabel->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_qrPreviewTitleLabel);

    m_qrPreviewLabel = new QLabel(previewCard);
    m_qrPreviewLabel->setFixedSize(260, 260);
    m_qrPreviewLabel->setAlignment(Qt::AlignCenter);
    m_qrPreviewLabel->setStyleSheet("background-color: white; border: 1px solid #e2e8f0; border-radius: 8px;");
    previewLayout->addWidget(m_qrPreviewLabel, 0, Qt::AlignCenter);

    layout->addWidget(previewCard);
    return panel;
}

void QuickToolsPage::updateQrPreview() {
    QString text = m_qrInputEdit->text().trimmed();
    if (text.isEmpty()) {
        m_qrPreviewLabel->clear();
        m_qrPreviewLabel->setText(tr("Enter text for preview"));
        m_qrCopyBtn->setEnabled(false);
        m_qrSaveBtn->setEnabled(false);
        return;
    }

    int size = m_qrSizeCombo->currentData().toInt();
    Core::QrOptions options;
    options.size = size;
    options.margin = 4;

    auto result = Core::QuickToolsBackend::generateQrImage(text, options);
    if (result.hasValue()) {
        m_currentQrImage = result.value();
        m_qrPreviewLabel->setPixmap(QPixmap::fromImage(m_currentQrImage).scaled(240, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_qrCopyBtn->setEnabled(true);
        m_qrSaveBtn->setEnabled(true);
    } else {
        m_qrPreviewLabel->setText(tr("Failed to generate QR code"));
        m_qrCopyBtn->setEnabled(false);
        m_qrSaveBtn->setEnabled(false);
    }
}

void QuickToolsPage::saveQrCode() {
    if (m_currentQrImage.isNull()) return;

    QString defaultPath = Services::SettingsService::instance().getDefaultOutputDir() + "/qrcode.png";
    QString savePath = OutputFileDialog::getSaveFileName(this, tr("Save QR Code"), defaultPath, "PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp)");
    if (savePath.isEmpty()) return;

    if (m_currentQrImage.save(savePath)) {
        m_resultScreen->showResult(savePath, tr("QR code saved successfully!"));
        m_pageStack->setCurrentIndex(1);
    } else {
        m_statusBanner->showBanner(BannerType::Error, tr("Error occurred while saving image."));
    }
}

QWidget* QuickToolsPage::createQrReaderPanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(12);

    // Dropzone
    m_qrReaderDropZone = new DropZoneWidget(panel);
    m_qrReaderDropZone->setInstruction(tr("Drop QR Code Image to Read"),
                                       tr("Drag QR image in JPG, PNG, WEBP, or BMP format here"));
    m_qrReaderDropZone->setAcceptedFileExtensions({"png", "jpg", "jpeg", "webp", "bmp"}, tr("Select Image File"));
    connect(m_qrReaderDropZone, &DropZoneWidget::filesDropped, this, &QuickToolsPage::onQrReaderFilesDropped);
    layout->addWidget(m_qrReaderDropZone);

    // Workspace Container (Hidden initially)
    m_qrReaderWorkspaceContainer = new QFrame(panel);
    m_qrReaderWorkspaceContainer->setObjectName("QrReaderWorkspace");
    m_qrReaderWorkspaceContainer->setStyleSheet("QFrame#QrReaderWorkspace { background-color: palette(base); border: 1px solid palette(mid); border-radius: 12px; }");
    m_qrReaderWorkspaceContainer->hide();

    auto *wsLayout = new QHBoxLayout(m_qrReaderWorkspaceContainer);
    wsLayout->setContentsMargins(20, 20, 20, 20);
    wsLayout->setSpacing(20);

    // Left Preview Column
    auto *leftCol = new QVBoxLayout();
    leftCol->setSpacing(10);
    leftCol->setAlignment(Qt::AlignTop);

    m_qrReaderThumbLabel = new QLabel(m_qrReaderWorkspaceContainer);
    m_qrReaderThumbLabel->setFixedSize(220, 220);
    m_qrReaderThumbLabel->setStyleSheet("background-color: palette(alternate-base); border: 1px solid palette(mid); border-radius: 8px;");
    m_qrReaderThumbLabel->setAlignment(Qt::AlignCenter);
    leftCol->addWidget(m_qrReaderThumbLabel, 0, Qt::AlignCenter);

    m_qrReaderFileNameLabel = new QLabel(m_qrReaderWorkspaceContainer);
    m_qrReaderFileNameLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: palette(text);");
    m_qrReaderFileNameLabel->setAlignment(Qt::AlignCenter);
    m_qrReaderFileNameLabel->setWordWrap(true);
    leftCol->addWidget(m_qrReaderFileNameLabel);

    m_qrReaderMetaLabel = new QLabel(m_qrReaderWorkspaceContainer);
    m_qrReaderMetaLabel->setStyleSheet("font-size: 11px; color: palette(text); opacity: 0.7;");
    m_qrReaderMetaLabel->setAlignment(Qt::AlignCenter);
    leftCol->addWidget(m_qrReaderMetaLabel);

    wsLayout->addLayout(leftCol, 0);

    // Right Column: Result Text & Action Buttons
    auto *rightCol = new QVBoxLayout();
    rightCol->setSpacing(12);

    m_qrReaderResultTitleLabel = new QLabel(tr("Detected QR Code Content:"), m_qrReaderWorkspaceContainer);
    m_qrReaderResultTitleLabel->setStyleSheet("font-weight: 600; font-size: 14px;");
    rightCol->addWidget(m_qrReaderResultTitleLabel);

    m_qrReaderResultEdit = new QPlainTextEdit(m_qrReaderWorkspaceContainer);
    m_qrReaderResultEdit->setReadOnly(true);
    m_qrReaderResultEdit->setStyleSheet(R"(
        QPlainTextEdit {
            background-color: palette(alternate-base);
            border: 1px solid palette(mid);
            border-radius: 8px;
            padding: 12px;
            font-family: monospace;
            font-size: 13px;
        }
    )");
    rightCol->addWidget(m_qrReaderResultEdit, 1);

    // Action buttons row
    auto *actRow = new QHBoxLayout();
    actRow->setSpacing(10);

    m_qrReaderCopyBtn = new QPushButton(tr("Copy Text"), m_qrReaderWorkspaceContainer);
    m_qrReaderCopyBtn->setObjectName("SecondaryButton");
    m_qrReaderCopyBtn->setIcon(IconUtils::getIcon(IconType::Convert, QColor("#2563eb"), 16));
    m_qrReaderCopyBtn->setFixedHeight(40);
    connect(m_qrReaderCopyBtn, &QPushButton::clicked, this, &QuickToolsPage::copyDecodedText);

    m_qrReaderOpenUrlBtn = new QPushButton(tr("Open Link"), m_qrReaderWorkspaceContainer);
    m_qrReaderOpenUrlBtn->setObjectName("SecondaryButton");
    m_qrReaderOpenUrlBtn->setIcon(IconUtils::getIcon(IconType::View, QColor("#16a34a"), 16));
    m_qrReaderOpenUrlBtn->setFixedHeight(40);
    m_qrReaderOpenUrlBtn->hide();
    connect(m_qrReaderOpenUrlBtn, &QPushButton::clicked, this, &QuickToolsPage::openDecodedUrl);

    m_qrReaderActionBtn = new QPushButton("", m_qrReaderWorkspaceContainer);
    m_qrReaderActionBtn->setObjectName("PrimaryActionButton");
    m_qrReaderActionBtn->setFixedHeight(40);
    m_qrReaderActionBtn->hide();
    connect(m_qrReaderActionBtn, &QPushButton::clicked, this, &QuickToolsPage::executeDynamicQrAction);

    m_qrReaderNewScanBtn = new QPushButton(tr("Scan New QR"), m_qrReaderWorkspaceContainer);
    m_qrReaderNewScanBtn->setObjectName("PrimaryActionButton");
    m_qrReaderNewScanBtn->setIcon(IconUtils::getIcon(IconType::ImagesToPdf, Qt::white, 16));
    m_qrReaderNewScanBtn->setFixedHeight(40);
    connect(m_qrReaderNewScanBtn, &QPushButton::clicked, this, &QuickToolsPage::resetQrReader);

    actRow->addWidget(m_qrReaderCopyBtn);
    actRow->addWidget(m_qrReaderOpenUrlBtn);
    actRow->addWidget(m_qrReaderActionBtn);
    actRow->addWidget(m_qrReaderNewScanBtn);
    rightCol->addLayout(actRow);

    wsLayout->addLayout(rightCol, 1);
    layout->addWidget(m_qrReaderWorkspaceContainer, 1);

    return panel;
}

void QuickToolsPage::onQrReaderFilesDropped(const QStringList& paths) {
    if (paths.isEmpty()) return;

    const QString imagePath = paths.first();
    m_currentReaderImagePath = imagePath;
    m_qrReaderDropZone->setEnabled(false);
    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_statusBanner->hideBanner();

    AsyncTask::run<Core::Result<QrReaderLoad>>(
        this,
        [imagePath](AsyncTask::CancelCheck cancel) -> Core::Result<QrReaderLoad> {
            QImage image(imagePath);
            if (image.isNull()) {
                return Core::Result<QrReaderLoad>::fail(
                    Core::PdfErrorCategory::InvalidInput, "Image file could not be loaded");
            }
            if (cancel()) {
                return Core::Result<QrReaderLoad>::fail(
                    Core::PdfErrorCategory::Cancelled, "QR analysis was cancelled");
            }

            auto decoded = Core::QuickToolsBackend::decodeQrImage(image);
            if (!decoded.hasValue()) {
                return decoded.error();
            }

            QrReaderLoad load;
            load.imageSize = image.size();
            load.thumbnail = image.scaled(
                200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            load.decoded = decoded.value();
            return load;
        },
        [this, imagePath](Core::Result<QrReaderLoad> result) {
            if (m_currentReaderImagePath != imagePath) {
                return;
            }

            m_progressBar->hide();
            m_progressBar->setRange(0, 100);
            m_qrReaderDropZone->setEnabled(true);
            if (!result.hasValue()) {
                m_statusBanner->showBanner(
                    BannerType::Error,
                    QString::fromStdString(result.error().toUserFriendlyMessage()));
                return;
            }

            applyQrReaderResult(
                imagePath, result.value().thumbnail, result.value().imageSize, result.value().decoded);
        });
}

void QuickToolsPage::applyQrReaderResult(const QString& imagePath,
                                         const QImage& thumbnail,
                                         const QSize& imageSize,
                                         const Core::QrDecodeResult& decodeResult) {
    const QFileInfo fi(imagePath);
    m_qrReaderThumbLabel->setPixmap(QPixmap::fromImage(thumbnail));
    m_qrReaderFileNameLabel->setText(fi.fileName());
    m_qrReaderMetaLabel->setText(QString("%1 × %2 px | %3 KB")
        .arg(imageSize.width()).arg(imageSize.height()).arg(fi.size() / 1024));

    if (!decodeResult.text.isEmpty()) {
        m_decodedQrText = decodeResult.text;

        QString displayText = m_decodedQrText;

        m_qrReaderOpenUrlBtn->hide();
        if (m_qrReaderActionBtn) m_qrReaderActionBtn->hide();
        m_dynamicQrActionData.clear();
        m_dynamicQrActionType = QrActionType::None;

        // Parse WIFI
        if (m_decodedQrText.startsWith("WIFI:", Qt::CaseInsensitive)) {
            QString ssid, type, pass;
            QStringList parts = m_decodedQrText.mid(5).split(';');
            for (const QString& p : parts) {
                if (p.startsWith("S:")) ssid = p.mid(2);
                else if (p.startsWith("T:")) type = p.mid(2);
                else if (p.startsWith("P:")) pass = p.mid(2);
            }
            displayText = tr("Wi-Fi Network (SSID): %1\nEncryption: %2\nPassword: %3").arg(ssid).arg(type).arg(pass);

            m_dynamicQrActionType = QrActionType::CopyWifiPassword;
            m_dynamicQrActionData = pass;
            if (m_qrReaderActionBtn) {
                m_qrReaderActionBtn->setText(tr("Copy Password"));
                m_qrReaderActionBtn->setIcon(IconUtils::getIcon(IconType::Convert, Qt::white, 16));
                m_qrReaderActionBtn->setVisible(true);
            }
        }
        // Parse MECARD
        else if (m_decodedQrText.startsWith("MECARD:", Qt::CaseInsensitive)) {
            QString name, tel, email;
            QStringList parts = m_decodedQrText.mid(7).split(';');
            for (const QString& p : parts) {
                if (p.startsWith("N:")) name = p.mid(2);
                else if (p.startsWith("TEL:")) tel = p.mid(4);
                else if (p.startsWith("EMAIL:")) email = p.mid(6);
            }
            displayText = tr("Contact Card (MECARD)\nName: %1\nPhone: %2\nEmail: %3").arg(name).arg(tel).arg(email);

            m_dynamicQrActionType = QrActionType::SaveVCard;
            m_dynamicQrActionData = QString("BEGIN:VCARD\nVERSION:3.0\nFN:%1\nTEL:%2\nEMAIL:%3\nEND:VCARD").arg(name, tel, email);
            if (m_qrReaderActionBtn) {
                m_qrReaderActionBtn->setText(tr("Save Contact (.vcf)"));
                m_qrReaderActionBtn->setIcon(IconUtils::getIcon(IconType::ImagesToPdf, Qt::white, 16));
                m_qrReaderActionBtn->setVisible(true);
            }
        }
        // Parse vCard
        else if (m_decodedQrText.startsWith("BEGIN:VCARD", Qt::CaseInsensitive)) {
            QString name, tel, email;
            QStringList lines = m_decodedQrText.split('\n');
            for (QString line : lines) {
                line = line.trimmed();
                if (line.startsWith("FN:")) name = line.mid(3);
                else if (line.startsWith("TEL")) {
                    int idx = line.indexOf(':');
                    if (idx != -1) tel = line.mid(idx+1);
                }
                else if (line.startsWith("EMAIL")) {
                    int idx = line.indexOf(':');
                    if (idx != -1) email = line.mid(idx+1);
                }
            }
            displayText = tr("Contact Card (vCard)\nName: %1\nPhone: %2\nEmail: %3").arg(name).arg(tel).arg(email);

            m_dynamicQrActionType = QrActionType::SaveVCard;
            m_dynamicQrActionData = m_decodedQrText;
            if (m_qrReaderActionBtn) {
                m_qrReaderActionBtn->setText(tr("Save Contact (.vcf)"));
                m_qrReaderActionBtn->setIcon(IconUtils::getIcon(IconType::ImagesToPdf, Qt::white, 16));
                m_qrReaderActionBtn->setVisible(true);
            }
        }

        m_qrReaderResultEdit->setPlainText(displayText);

        bool isUrl = m_decodedQrText.startsWith("http://", Qt::CaseInsensitive) ||
                     m_decodedQrText.startsWith("https://", Qt::CaseInsensitive);
        m_qrReaderOpenUrlBtn->setVisible(isUrl);
        m_statusBanner->showBanner(BannerType::Success, tr("QR code read successfully!"));
    } else {
        m_decodedQrText = "";
        m_qrReaderResultEdit->setPlainText(tr("No readable QR code found in image."));
        m_qrReaderOpenUrlBtn->hide();
        m_statusBanner->showBanner(BannerType::Warning, tr("Could not detect a valid QR code on the image."));
    }

    m_qrReaderDropZone->hide();
    m_qrReaderWorkspaceContainer->show();
}

void QuickToolsPage::resetQrReader() {
    m_qrReaderWorkspaceContainer->hide();
    m_qrReaderDropZone->show();
    m_qrReaderDropZone->setEnabled(true);
    m_progressBar->hide();
    m_progressBar->setRange(0, 100);
    m_decodedQrText = "";
    m_currentReaderImagePath = "";
    m_qrReaderResultEdit->clear();
    m_statusBanner->hideBanner();
}

void QuickToolsPage::copyDecodedText() {
    if (!m_decodedQrText.isEmpty()) {
        QGuiApplication::clipboard()->setText(m_decodedQrText);
        m_statusBanner->showBanner(BannerType::Success, tr("Text copied to clipboard!"));
    }
}

void QuickToolsPage::openDecodedUrl() {
    if (m_decodedQrText.startsWith("http://", Qt::CaseInsensitive) ||
        m_decodedQrText.startsWith("https://", Qt::CaseInsensitive)) {
        QDesktopServices::openUrl(QUrl(m_decodedQrText));
    }
}

void QuickToolsPage::executeDynamicQrAction() {
    if (m_dynamicQrActionType == QrActionType::CopyWifiPassword) {
        if (!m_dynamicQrActionData.isEmpty()) {
            QGuiApplication::clipboard()->setText(m_dynamicQrActionData);
            m_statusBanner->showBanner(BannerType::Success, tr("Wi-Fi password copied to clipboard!"));
        }
    } else if (m_dynamicQrActionType == QrActionType::SaveVCard) {
        QString defaultPath = Services::SettingsService::instance().getDefaultOutputDir() + "/contact.vcf";
        QString savePath = OutputFileDialog::getSaveFileName(this, tr("Save Contact"), defaultPath, "vCard (*.vcf)");
        if (!savePath.isEmpty()) {
            QFile file(savePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(m_dynamicQrActionData.toUtf8());
                file.close();
                m_statusBanner->showBanner(BannerType::Success, tr("Contact saved successfully!"));
            } else {
                m_statusBanner->showBanner(BannerType::Error, tr("Error saving contact."));
            }
        }
    }
}

void QuickToolsPage::resetWorkspace() {
    m_pageStack->setCurrentIndex(0);
    resetQrReader();
    m_statusBanner->hideBanner();
}

void QuickToolsPage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void QuickToolsPage::retranslateUi() {
    m_qrGenTabBtn->setAccessibleName(tr("QR Code Generator"));
    m_qrGenTabBtn->setAccessibleDescription(tr("Switch to QR code generator mode"));
    m_qrGenTabBtn->setToolTip(tr("Switch to QR code generator mode"));
    m_qrReadTabBtn->setAccessibleName(tr("QR Code Reader"));
    m_qrReadTabBtn->setAccessibleDescription(tr("Switch to QR code reader mode"));
    m_qrReadTabBtn->setToolTip(tr("Switch to QR code reader mode"));

    QuickTabMode mode = static_cast<QuickTabMode>(m_panelStack->currentIndex());
    const QString checkMark = QString(QChar(0x2713));
    m_qrGenTabBtn->setText(mode == QuickTabMode::QrCode
        ? QStringLiteral("%1  %2").arg(checkMark, tr("QR Code Generator"))
        : tr("QR Code Generator"));
    m_qrReadTabBtn->setText(mode == QuickTabMode::QrReader
        ? QStringLiteral("%1  %2").arg(checkMark, tr("QR Code Reader"))
        : tr("QR Code Reader"));

    // QR Gen
    if (m_qrInputLabel) m_qrInputLabel->setText(tr("QR Code Text or URL:"));
    if (m_qrInputEdit) m_qrInputEdit->setPlaceholderText(tr("Enter text, website, or Wi-Fi connection..."));
    if (m_qrSizeLabel) m_qrSizeLabel->setText(tr("Resolution Size:"));
    if (m_qrSizeCombo && m_qrSizeCombo->count() >= 3) {
        m_qrSizeCombo->setItemText(0, "256 × 256 px");
        m_qrSizeCombo->setItemText(1, tr("512 × 512 px (Standard)"));
        m_qrSizeCombo->setItemText(2, tr("1024 × 1024 px (High Quality)"));
    }
    if (m_qrCopyBtn) m_qrCopyBtn->setText(tr("Copy QR Code"));
    if (m_qrSaveBtn) m_qrSaveBtn->setText(tr("Save QR Code"));
    if (m_qrPreviewTitleLabel) m_qrPreviewTitleLabel->setText(tr("Live Preview"));

    // QR Reader
    m_qrReaderDropZone->setInstruction(tr("Drop QR Code Image to Read"),
                                       tr("Drag QR image in JPG, PNG, WEBP, or BMP format here"));
    m_qrReaderResultTitleLabel->setText(tr("Detected QR Code Content:"));
    m_qrReaderCopyBtn->setText(tr("Copy Text"));
    m_qrReaderOpenUrlBtn->setText(tr("Open Link"));
    m_qrReaderNewScanBtn->setText(tr("Scan New QR"));
}

} // namespace LocalTools::UI
