#include "documents_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileInfo>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>

#include "../../core/backends/document_backend.h"
#include "../../core/backends/native_vision_backend.h"
#include "../../core/backends/image_backend.h"
#include "../../services/settings_service.h"
#include "../../services/file_service.h"
#include "../../services/logging_service.h"
#include "../common/async_task.h"
#include "../theme.h"

namespace LocalTools::UI {

namespace {

struct DocumentTaskOutcome {
    Core::Result<void> result{Core::Result<void>::ok()};
    QString outputPath;
    QString validationMessage;
};

void setDocumentTabText(QPushButton *button, const QString &visibleText) {
    if (!button) return;

    // QPushButton interprets a single '&' as a keyboard mnemonic.  These tab
    // titles use it as visible copy, so escape it before assigning the text.
    QString buttonText = visibleText;
    buttonText.replace('&', "&&");
    button->setText(buttonText);

    // Leave explicit room for the icon, the style's horizontal padding and a
    // little DPI/semibold rounding slack.  This avoids the final glyph touching
    // or being clipped by the focus border at fractional Windows scale factors.
    const int safeWidth = button->fontMetrics().horizontalAdvance(visibleText)
        + 16  // icon
        + 8   // icon/text spacing
        + 48; // QSS padding + focus/DPI safety
    button->setMinimumWidth(safeWidth);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

} // namespace

DocumentsPage::DocumentsPage(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

void DocumentsPage::setupUi() {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_pageStack = new QStackedWidget(this);

    auto *mainContainer = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(mainContainer);
    mainLayout->setContentsMargins(28, 20, 28, 20);
    mainLayout->setSpacing(14);

    // 1. Header Tabs
    mainLayout->addWidget(createTabHeader());

    // 2. Tab Description Subtitle
    m_tabDescriptionLabel = new QLabel(tr("Place front and back ID or driver license photos on standard A4 print template."), this);
    m_tabDescriptionLabel->setStyleSheet("font-size: 13px; color: palette(text); opacity: 0.75; margin-bottom: 2px;");
    mainLayout->addWidget(m_tabDescriptionLabel);

    // 3. Status Banner
    m_statusBanner = new StatusBannerWidget(this);
    mainLayout->addWidget(m_statusBanner);

    // 4. Panels Stack (Fixed 300px Height)
    m_panelStack = new QStackedWidget(this);
    m_panelStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_panelStack->setFixedHeight(300);
    m_idCopyPanel = createIdCopyPanel();
    m_passportPanel = createPassportPhotoPanel();

    m_panelStack->addWidget(m_idCopyPanel);
    m_panelStack->addWidget(m_passportPanel);
    mainLayout->addWidget(m_panelStack, 0, Qt::AlignTop);

    // 4. Bottom Action Row
    auto *bottomRow = new QHBoxLayout();
    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    bottomRow->addWidget(m_progressBar, 1);

    m_actionBtn = new QPushButton(tr("Create A4 Copy"), this);
    m_actionBtn->setObjectName("PrimaryButton");
    m_actionBtn->setMinimumWidth(200);
    m_actionBtn->setFixedHeight(38);
    m_actionBtn->setCursor(Qt::PointingHandCursor);
    connect(m_actionBtn, &QPushButton::clicked, this, &DocumentsPage::processCurrentAction);
    bottomRow->addWidget(m_actionBtn);
    mainLayout->addLayout(bottomRow);

    mainLayout->addStretch(1);
    m_pageStack->addWidget(mainContainer);

    // 5. Result Screen
    m_resultScreen = new ResultScreenWidget(this);
    connect(m_resultScreen, &ResultScreenWidget::backToWorkspaceRequested, this, &DocumentsPage::resetWorkspace);
    m_pageStack->addWidget(m_resultScreen);

    rootLayout->addWidget(m_pageStack);
    retranslateUi();
}

QWidget* DocumentsPage::createTabHeader() {
    auto *headerWidget = new QWidget(this);
    auto *layout = new QHBoxLayout(headerWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_tabButtonGroup = new QButtonGroup(this);
    m_tabButtonGroup->setExclusive(true);

    auto createTabBtn = [this, layout](const QString& text, IconType iconType, int id) {
        auto *btn = new QPushButton(this);
        btn->setObjectName("ToolTabButton");
        btn->setCheckable(true);
        btn->setIcon(IconUtils::getIcon(iconType, QColor("#64748b"), 16));
        btn->setIconSize(QSize(16, 16));
        setDocumentTabText(btn, text);
        btn->setCursor(Qt::PointingHandCursor);
        m_tabButtonGroup->addButton(btn, id);
        layout->addWidget(btn);
        return btn;
    };

    m_idCopyTabBtn = createTabBtn(tr("ID Card Copy (A4)"), IconType::ImagesToPdf, 0);
    m_passportTabBtn = createTabBtn(tr("Passport & Biometric Photo"), IconType::Organize, 1);

    m_idCopyTabBtn->setChecked(true);
    layout->addStretch();

    connect(m_tabButtonGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_panelStack->setCurrentIndex(id);
        updateTabDescription(id);
        if (id == 0) m_actionBtn->setText(tr("Create A4 Copy"));
        else if (id == 1) m_actionBtn->setText(tr("Create Print Template"));
    });

    return headerWidget;
}

void DocumentsPage::updateTabDescription(int id) {
    if (!m_tabDescriptionLabel) return;
    if (id == 0) {
        m_tabDescriptionLabel->setText(tr("Place front and back ID or driver license photos on standard A4 print template."));
    } else if (id == 1) {
        m_tabDescriptionLabel->setText(tr("Multiply your photos into A4 print sheets per official biometric and passport standards."));
    }
}

QWidget* DocumentsPage::createIdCopyPanel() {
    auto *card = new QFrame(this);
    card->setObjectName("CardFrame");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setFixedHeight(300);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 12, 18, 12);
    layout->setSpacing(10);

    // Front / Back Pickers
    auto *cardsRow = new QHBoxLayout();
    cardsRow->setSpacing(20);

    const auto setEmptyPreview = [](QLabel *preview) {
        preview->clear();
        preview->setPixmap(IconUtils::getPixmap(
            IconType::Image, Theme::color(ColorToken::Accent), 30));
    };

    // Front box
    auto *frontBox = new QFrame(card);
    frontBox->setObjectName("DocumentUploadCard");
    frontBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *frontLayout = new QVBoxLayout(frontBox);
    frontLayout->setContentsMargins(16, 12, 16, 12);
    frontLayout->setSpacing(8);
    frontLayout->setAlignment(Qt::AlignCenter);

    m_frontStatusLabel = new QLabel(tr("Front Side"), frontBox);
    m_frontStatusLabel->setObjectName("DocumentUploadTitle");
    m_frontStatusLabel->setAlignment(Qt::AlignCenter);
    frontLayout->addWidget(m_frontStatusLabel);

    m_frontPreview = new QLabel(frontBox);
    m_frontPreview->setObjectName("DocumentPreviewTile");
    m_frontPreview->setFixedSize(176, 96);
    m_frontPreview->setAlignment(Qt::AlignCenter);
    m_frontPreview->setAccessibleName(tr("Front Side"));
    setEmptyPreview(m_frontPreview);
    frontLayout->addWidget(m_frontPreview);

    m_frontBtn = new QPushButton(tr("Select Front Photo"), frontBox);
    m_frontBtn->setObjectName("SecondaryButton");
    m_frontBtn->setIcon(IconUtils::getIcon(
        IconType::Folder, Theme::color(ColorToken::Accent), 16));
    m_frontBtn->setIconSize(QSize(16, 16));
    m_frontBtn->setMinimumWidth(210);
    m_frontBtn->setCursor(Qt::PointingHandCursor);
    connect(m_frontBtn, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, tr("Select Front ID Photo"), QString(), tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (!file.isEmpty()) {
            m_frontImagePath = file;
            QImage img(file);
            if (!img.isNull()) {
                m_frontPreview->setPixmap(QPixmap::fromImage(
                    img.scaled(m_frontPreview->size(), Qt::KeepAspectRatio,
                               Qt::SmoothTransformation)));
            }
        }
    });
    frontLayout->addWidget(m_frontBtn);
    cardsRow->addWidget(frontBox, 1);

    // Back box
    auto *backBox = new QFrame(card);
    backBox->setObjectName("DocumentUploadCard");
    backBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *backLayout = new QVBoxLayout(backBox);
    backLayout->setContentsMargins(16, 12, 16, 12);
    backLayout->setSpacing(8);
    backLayout->setAlignment(Qt::AlignCenter);

    m_backStatusLabel = new QLabel(tr("Back Side (Optional)"), backBox);
    m_backStatusLabel->setObjectName("DocumentUploadTitle");
    m_backStatusLabel->setAlignment(Qt::AlignCenter);
    backLayout->addWidget(m_backStatusLabel);

    m_backPreview = new QLabel(backBox);
    m_backPreview->setObjectName("DocumentPreviewTile");
    m_backPreview->setFixedSize(176, 96);
    m_backPreview->setAlignment(Qt::AlignCenter);
    m_backPreview->setAccessibleName(tr("Back Side (Optional)"));
    setEmptyPreview(m_backPreview);
    backLayout->addWidget(m_backPreview);

    m_backBtn = new QPushButton(tr("Select Back Photo"), backBox);
    m_backBtn->setObjectName("SecondaryButton");
    m_backBtn->setIcon(IconUtils::getIcon(
        IconType::Folder, Theme::color(ColorToken::Accent), 16));
    m_backBtn->setIconSize(QSize(16, 16));
    m_backBtn->setMinimumWidth(210);
    m_backBtn->setCursor(Qt::PointingHandCursor);
    connect(m_backBtn, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, tr("Select Back ID Photo"), QString(), tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (!file.isEmpty()) {
            m_backImagePath = file;
            QImage img(file);
            if (!img.isNull()) {
                m_backPreview->setPixmap(QPixmap::fromImage(
                    img.scaled(m_backPreview->size(), Qt::KeepAspectRatio,
                               Qt::SmoothTransformation)));
            }
        }
    });
    backLayout->addWidget(m_backBtn);
    cardsRow->addWidget(backBox, 1);

    layout->addLayout(cardsRow);

    // Options Row
    auto *optRow = new QHBoxLayout();

    m_idFormatCombo = new QComboBox(card);
    m_idFormatCombo->addItem(tr("Print-Ready PDF (.pdf)"), 0);
    m_idFormatCombo->addItem(tr("High Quality Image (.png)"), 1);
    optRow->addWidget(m_idFormatCombo);

    optRow->addStretch();
    layout->addLayout(optRow);

    return card;
}

QWidget* DocumentsPage::createPassportPhotoPanel() {
    auto *card = new QFrame(this);
    card->setObjectName("CardFrame");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setFixedHeight(300);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 12, 18, 12);
    layout->setSpacing(10);

    m_passportDescLabel = new QLabel(tr("Create a multi-print template (10x15 cm or A4) according to official passport, visa, and ID standards."), card);
    m_passportDescLabel->setStyleSheet("font-size: 12px; color: palette(text); opacity: 0.8;");
    m_passportDescLabel->setWordWrap(true);
    layout->addWidget(m_passportDescLabel);

    auto *pickerRow = new QHBoxLayout();
    m_passportPhotoPreview = new QLabel(card);
    m_passportPhotoPreview->setFixedSize(110, 140);
    m_passportPhotoPreview->setStyleSheet("background: palette(alternate-base); border-radius: 6px; border: 1px solid palette(mid);");
    m_passportPhotoPreview->setAlignment(Qt::AlignCenter);
    m_passportPhotoPreview->setText(tr("Passport\nPreview"));
    pickerRow->addWidget(m_passportPhotoPreview);

    auto *rightCol = new QVBoxLayout();
    rightCol->setSpacing(8);
    m_passportSelectBtn = new QPushButton(tr("Select Portrait / Passport Photo"), card);
    m_passportSelectBtn->setObjectName("ToolTabButton");
    m_passportSelectBtn->setIcon(IconUtils::getIcon(IconType::Folder, QColor("#2563eb"), 16));
    m_passportSelectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_passportSelectBtn, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, tr("Select Passport Photo"), QString(), tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (!file.isEmpty()) {
            m_passportPhotoPath = file;
            QImage img(file);
            if (!img.isNull()) {
                m_passportPhotoPreview->setPixmap(QPixmap::fromImage(img.scaled(110, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            }
        }
    });
    rightCol->addWidget(m_passportSelectBtn);

    // Presets
    auto *presetRow = new QHBoxLayout();
    m_photoStandardLabel = new QLabel(tr("Photo Standard:"), card);
    m_photoStandardLabel->setStyleSheet("font-weight: 600; font-size: 13px;");
    presetRow->addWidget(m_photoStandardLabel);

    m_passportPresetCombo = new QComboBox(card);
    m_passportPresetCombo->addItem(tr("35 × 45 mm (TR Passport, Visa & ID)"), 0);
    m_passportPresetCombo->addItem(tr("50 × 50 mm (US Visa & Biometric)"), 1);
    m_passportPresetCombo->addItem(tr("30 × 40 mm (Classic Passport)"), 2);
    presetRow->addWidget(m_passportPresetCombo);
    rightCol->addLayout(presetRow);

    // Count & Paper
    auto *optRow = new QHBoxLayout();
    m_photoCountLabel = new QLabel(tr("Count:"), card);
    m_photoCountLabel->setStyleSheet("font-weight: 600; font-size: 13px;");
    optRow->addWidget(m_photoCountLabel);

    m_passportCountCombo = new QComboBox(card);
    m_passportCountCombo->addItem(tr("4 Photos"), 4);
    m_passportCountCombo->addItem(tr("6 Photos"), 6);
    m_passportCountCombo->addItem(tr("8 Photos"), 8);
    m_passportCountCombo->setCurrentIndex(1); // 6 photos
    optRow->addWidget(m_passportCountCombo);

    m_printPaperLabel = new QLabel(tr("Print Paper:"), card);
    m_printPaperLabel->setStyleSheet("font-weight: 600; font-size: 13px; margin-left: 12px;");
    optRow->addWidget(m_printPaperLabel);

    m_passportPaperCombo = new QComboBox(card);
    m_passportPaperCombo->addItem(tr("10 × 15 cm Standard Photo Card"), false);
    m_passportPaperCombo->addItem(tr("A4 Document Sheet"), true);
    optRow->addWidget(m_passportPaperCombo);

    m_outFormatLabel = new QLabel(tr("Format:"), card);
    m_outFormatLabel->setStyleSheet("font-weight: 600; font-size: 13px; margin-left: 12px;");
    optRow->addWidget(m_outFormatLabel);

    m_passportOutFormatCombo = new QComboBox(card);
    m_passportOutFormatCombo->addItem("PDF (.pdf)", "pdf");
    m_passportOutFormatCombo->addItem("JPEG (.jpg)", "jpg");
    optRow->addWidget(m_passportOutFormatCombo);

    optRow->addStretch();
    rightCol->addLayout(optRow);

    pickerRow->addLayout(rightCol, 1);
    layout->addLayout(pickerRow);

    return card;
}

void DocumentsPage::setMode(DocumentTabMode mode) {
    int id = static_cast<int>(mode);
    if (auto *btn = m_tabButtonGroup->button(id)) {
        btn->setChecked(true);
        m_panelStack->setCurrentIndex(id);
        updateTabDescription(id);
    }
}

void DocumentsPage::resetWorkspace() {
    m_frontImagePath.clear();
    m_backImagePath.clear();
    m_passportPhotoPath.clear();

    m_frontPreview->setPixmap(IconUtils::getPixmap(
        IconType::Image, Theme::color(ColorToken::Accent), 30));
    m_backPreview->setPixmap(IconUtils::getPixmap(
        IconType::Image, Theme::color(ColorToken::Accent), 30));
    m_passportPhotoPreview->setText(tr("Passport\nPreview"));
    m_passportPhotoPreview->setPixmap(QPixmap());

    m_pageStack->setCurrentIndex(0);
}

void DocumentsPage::processCurrentAction() {
    const int currentTab = m_panelStack->currentIndex();
    const QString defaultDir = Services::SettingsService::instance().getDefaultOutputDir();

    if (currentTab == 0) { // ID Card Fotokopisi
        if (m_frontImagePath.isEmpty()) {
            m_statusBanner->showBanner(BannerType::Warning, tr("Please select at least the front side ID photo."));
            return;
        }
        if (m_frontImagePath == m_backImagePath) {
            m_statusBanner->showBanner(BannerType::Warning, tr("You selected the same photo for front and back sides. Please select different photos."));
            return;
        }

    } else if (currentTab == 1) { // Passport Fotoğraf
        if (m_passportPhotoPath.isEmpty()) {
            m_statusBanner->showBanner(BannerType::Warning, tr("Please select a passport / portrait photo."));
            return;
        }
    }

    const QString frontPath = m_frontImagePath;
    const QString backPath = m_backImagePath;
    const QString passportPath = m_passportPhotoPath;
    const QString idExtension = m_idFormatCombo->currentData().toInt() == 0 ? "pdf" : "png";
    const QString passportExtension = m_passportOutFormatCombo->currentData().toString();
    const int passportPreset = m_passportPresetCombo->currentIndex();
    const int passportCount = m_passportCountCombo->currentData().toInt();
    const bool passportA4 = m_passportPaperCombo->currentData().toBool();

    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_actionBtn->setEnabled(false);
    AsyncTask::run<DocumentTaskOutcome>(
        this,
        [=](AsyncTask::CancelCheck cancel) {
            DocumentTaskOutcome outcome;
            if (cancel()) {
                outcome.result = Core::Result<void>::fail(
                    Core::PdfErrorCategory::Cancelled, "Operation was cancelled.");
                return outcome;
            }

            if (currentTab == 0) {
                const QImage frontImage(frontPath);
                if (frontImage.isNull()) {
                    outcome.validationMessage = "Failed to load front side image.";
                    return outcome;
                }
                QString frontError;
                if (!Core::NativeVisionBackend::isValidIdCard(frontImage, &frontError)) {
                    outcome.validationMessage = frontError.isEmpty()
                        ? QStringLiteral("Selected image for front side is not a valid ID card.")
                        : frontError;
                    return outcome;
                }

                const auto tempFront = Services::FileService::instance().createTempFile("id_front_", ".png");
                if (tempFront.hasError()) {
                    outcome.result = Core::Result<void>::fail(tempFront.error());
                    return outcome;
                }
                const QString correctedFrontPath =
                    QString::fromStdWString(tempFront.value().wstring());
                if (!Core::NativeVisionBackend::autoCropAndCorrectPerspective(frontImage)
                         .save(correctedFrontPath)) {
                    outcome.result = Core::Result<void>::fail(
                        Core::PdfErrorCategory::BackendError,
                        "Corrected front-side working image could not be saved.");
                    return outcome;
                }

                QString correctedBackPath;
                if (!backPath.isEmpty()) {
                    const QImage backImage(backPath);
                    QString backError;
                    if (backImage.isNull() ||
                        !Core::NativeVisionBackend::isValidIdCard(backImage, &backError)) {
                        QFile::remove(correctedFrontPath);
                        outcome.validationMessage = backError.isEmpty()
                            ? QStringLiteral("Selected image for back side is not a valid ID card.")
                            : backError;
                        return outcome;
                    }
                    const auto tempBack = Services::FileService::instance().createTempFile("id_back_", ".png");
                    if (tempBack.hasError()) {
                        QFile::remove(correctedFrontPath);
                        outcome.result = Core::Result<void>::fail(tempBack.error());
                        return outcome;
                    }
                    correctedBackPath = QString::fromStdWString(tempBack.value().wstring());
                    if (!Core::NativeVisionBackend::autoCropAndCorrectPerspective(backImage)
                             .save(correctedBackPath)) {
                        QFile::remove(correctedFrontPath);
                        outcome.result = Core::Result<void>::fail(
                            Core::PdfErrorCategory::BackendError,
                            "Corrected back-side working image could not be saved.");
                        return outcome;
                    }
                }

                outcome.outputPath = Services::FileService::instance().generateUniqueOutputPath(
                    defaultDir + "/ID Card_Fotokopisi_" +
                    QString::number(QDateTime::currentMSecsSinceEpoch()) + "." + idExtension);
                Core::DocumentBackend::IdCopyOptions options;
                options.copyCount = 1;
                options.isA4 = true;
                options.dpi = 300;
                options.drawCutLines = true;
                outcome.result = Core::DocumentBackend::createIdCopySheet(
                    correctedFrontPath, correctedBackPath, outcome.outputPath, options);
                QFile::remove(correctedFrontPath);
                if (!correctedBackPath.isEmpty()) QFile::remove(correctedBackPath);
            } else {
                outcome.outputPath = Services::FileService::instance().generateUniqueOutputPath(
                    defaultDir + "/Vesikalik_Baski_Sablonu_" +
                    QString::number(QDateTime::currentMSecsSinceEpoch()) + "." + passportExtension);
                Core::DocumentBackend::PassportPhotoOptions options;
                if (passportPreset == 0) {
                    options.photoWidthMm = 35.0;
                    options.photoHeightMm = 45.0;
                } else if (passportPreset == 1) {
                    options.photoWidthMm = 50.0;
                    options.photoHeightMm = 50.0;
                } else {
                    options.photoWidthMm = 30.0;
                    options.photoHeightMm = 40.0;
                }
                options.photoCount = passportCount;
                options.isA4Paper = passportA4;
                options.dpi = 300;
                options.drawBorder = true;
                outcome.result = Core::DocumentBackend::generatePassportPhotoGrid(
                    passportPath, outcome.outputPath, options);
            }
            return outcome;
        },
        [this](DocumentTaskOutcome outcome) {
            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            m_actionBtn->setEnabled(true);
            if (!outcome.validationMessage.isEmpty()) {
                m_statusBanner->showBanner(BannerType::Error, outcome.validationMessage);
            } else if (outcome.result.hasValue()) {
                Services::SettingsService::instance().addRecentFile(outcome.outputPath);
                m_resultScreen->showResult(
                    outcome.outputPath, tr("Document Operation Completed Successfully!"));
                m_pageStack->setCurrentIndex(1);
            } else {
                m_statusBanner->showBanner(
                    BannerType::Error,
                    QString::fromStdString(outcome.result.error().toUserFriendlyMessage()));
            }
        });
}

void DocumentsPage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void DocumentsPage::retranslateUi() {
    setDocumentTabText(m_idCopyTabBtn, tr("ID Card Copy (A4)"));
    setDocumentTabText(m_passportTabBtn, tr("Passport & Biometric Photo"));

    if (m_frontStatusLabel) m_frontStatusLabel->setText(tr("Front Side"));
    if (m_backStatusLabel) m_backStatusLabel->setText(tr("Back Side (Optional)"));
    if (m_frontPreview) m_frontPreview->setAccessibleName(tr("Front Side"));
    if (m_backPreview) m_backPreview->setAccessibleName(tr("Back Side (Optional)"));
    if (m_frontBtn) m_frontBtn->setText(tr("Select Front Photo"));
    if (m_backBtn) m_backBtn->setText(tr("Select Back Photo"));

    if (m_idFormatCombo && m_idFormatCombo->count() >= 2) {
        m_idFormatCombo->setItemText(0, tr("Print-Ready PDF (.pdf)"));
        m_idFormatCombo->setItemText(1, tr("High Quality Image (.png)"));
    }

    if (m_passportDescLabel) {
        m_passportDescLabel->setText(tr("Create a multi-print template (10x15 cm or A4) according to official passport, visa, and ID standards."));
    }
    if (m_passportPhotoPreview && m_passportPhotoPath.isEmpty()) {
        m_passportPhotoPreview->setText(tr("Passport\nPreview"));
    }
    if (m_passportSelectBtn) {
        m_passportSelectBtn->setText(tr("Select Portrait / Passport Photo"));
    }
    if (m_photoStandardLabel) m_photoStandardLabel->setText(tr("Photo Standard:"));
    if (m_photoCountLabel) m_photoCountLabel->setText(tr("Count:"));
    if (m_printPaperLabel) m_printPaperLabel->setText(tr("Print Paper:"));
    if (m_outFormatLabel) m_outFormatLabel->setText(tr("Format:"));

    if (m_passportCountCombo && m_passportCountCombo->count() >= 3) {
        m_passportCountCombo->setItemText(0, tr("4 Photos"));
        m_passportCountCombo->setItemText(1, tr("6 Photos"));
        m_passportCountCombo->setItemText(2, tr("8 Photos"));
    }

    if (m_passportPaperCombo && m_passportPaperCombo->count() >= 2) {
        m_passportPaperCombo->setItemText(0, tr("10 × 15 cm Standard Photo Card"));
        m_passportPaperCombo->setItemText(1, tr("A4 Document Sheet"));
    }

    if (m_passportPresetCombo && m_passportPresetCombo->count() >= 3) {
        m_passportPresetCombo->setItemText(0, tr("35 × 45 mm (TR Passport, Visa & ID)"));
        m_passportPresetCombo->setItemText(1, tr("50 × 50 mm (US Visa & Biometric)"));
        m_passportPresetCombo->setItemText(2, tr("30 × 40 mm (Classic Passport)"));
    }

    int currentTab = m_panelStack ? m_panelStack->currentIndex() : 0;
    updateTabDescription(currentTab);
    if (m_actionBtn) {
        if (currentTab == 0) m_actionBtn->setText(tr("Create A4 Copy"));
        else if (currentTab == 1) m_actionBtn->setText(tr("Create Print Template"));
    }
}

} // namespace LocalTools::UI
