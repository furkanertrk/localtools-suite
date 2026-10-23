#include "audio_page.h"
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QUrl>
#include <algorithm>

#include "../../core/job_queue.h"
#include "../../core/operations/convert_audio.h"
#include "../../core/operations/compress_audio.h"
#include "../../core/operations/trim_audio.h"
#include "../../services/settings_service.h"
#include "../common/icon_utils.h"
#include "../common/async_task.h"
#include "../theme.h"

namespace LocalTools::UI {

AudioPage::AudioPage(QWidget *parent)
    : QWidget(parent) {
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    connect(m_player, &QMediaPlayer::positionChanged, this, &AudioPage::handlePositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &AudioPage::handleDurationChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState state) {
                updateTrimPlaybackButton(state == QMediaPlayer::PlayingState);
            });

    connect(&Core::JobQueue::instance(), &Core::JobQueue::progressChanged, this, [this](int progress, const QString& msg) {
        Q_UNUSED(msg);
        if (isVisible()) {
            m_progressBar->show();
            m_progressBar->setValue(progress);
        }
    });

    connect(&Core::JobQueue::instance(), &Core::JobQueue::finished, this, [this](bool success, const QString& msg) {
        if (isVisible()) {
            m_progressBar->hide();
            m_isProcessing = false;
            updateProcessButtonState();
            if (success) {
                if (m_player) {
                    m_player->stop();
                }
                if (m_trimPlayPauseBtn) {
                    updateTrimPlaybackButton(false);
                }
                Services::SettingsService::instance().addRecentFile(m_lastOutPath);
                m_resultScreen->showResult(m_lastOutPath, m_successTitle.isEmpty() ? tr("Audio Operation Completed Successfully!") : m_successTitle);
                m_pageStack->setCurrentIndex(1);
            } else {
                m_statusBanner->showBanner(BannerType::Error, msg);
            }
        }
    });

    setupUi();
    setMode(Mode::Convert);
}

AudioPage::~AudioPage() {
    if (m_player) {
        m_player->stop();
    }
}

void AudioPage::setupUi() {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_pageStack = new QStackedWidget(this);

    // Page 0: Workspace Container (ScrollArea)
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    auto *mainContainer = new QWidget(scrollArea);
    m_mainLayout = new QVBoxLayout(mainContainer);
    m_mainLayout->setContentsMargins(28, 20, 28, 20);
    m_mainLayout->setSpacing(14);

    // 1. Top Sub-Navigation Tabs (Convert, Compress, Trim)
    m_tabContainer = new QWidget(this);
    auto *tabLayout = new QHBoxLayout(m_tabContainer);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(8);

    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);

    auto createTabBtn = [this, tabLayout](const QString& text, IconType iconType, Mode mode, int id) {
        auto *btn = new QPushButton(text, this);
        btn->setObjectName("ToolTabButton");
        btn->setCheckable(true);
        btn->setFocusPolicy(Qt::StrongFocus);
        btn->setIcon(IconUtils::getIcon(iconType, Theme::color(ColorToken::TextSecondary), 16));
        btn->setIconSize(QSize(16, 16));
        btn->setCursor(Qt::PointingHandCursor);
        m_tabGroup->addButton(btn, id);
        tabLayout->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, mode]() {
            if (!m_isProcessing) setMode(mode);
        });

        return btn;
    };

    createTabBtn(tr("Convert Audio"), IconType::Convert, Mode::Convert, 0)->setChecked(true);
    createTabBtn(tr("Compress Audio"), IconType::Compress, Mode::Compress, 1);
    createTabBtn(tr("Trim Audio"), IconType::Trim, Mode::Trim, 2);
    tabLayout->addStretch();

    m_mainLayout->addWidget(m_tabContainer);

    // 2. Subtitle / Tab Description (Theme Dynamic & Dark Mode Compatible)
    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setStyleSheet("font-size: 13px; color: palette(text); opacity: 0.75; margin-bottom: 2px;");
    m_subtitleLabel->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->addWidget(m_subtitleLabel);

    // 3. Inline Status Banner
    m_statusBanner = new StatusBannerWidget(this);
    m_statusBanner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_mainLayout->addWidget(m_statusBanner);

    // 4. Progress Bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    m_mainLayout->addWidget(m_progressBar);

    // 5. Drop Zone for Audio Files
    m_dropZone = new DropZoneWidget(this);
    m_dropZone->setInstruction(tr("Drop Audio Files Here"), tr("or click to select file (MP3, WAV, FLAC, OGG, M4A, AAC)"));
    m_dropZone->setAcceptedFileExtensions({"mp3", "wav", "flac", "ogg", "m4a", "aac"}, tr("Select Audio File"), tr("Only audio files are accepted (MP3, WAV, FLAC, OGG, M4A, AAC)"));
    connect(m_dropZone, &DropZoneWidget::filesDropped, this, &AudioPage::onFilesDropped);
    m_mainLayout->addWidget(m_dropZone);

    // 6. Workspace Container (Revealed when audio files are loaded)
    m_workspaceContainer = new QFrame(this);
    m_workspaceContainer->setObjectName("CardFrame");
    m_workspaceContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_workspaceContainer->setFixedHeight(300);
    // Styled by the application stylesheet via its objectName. A local sheet
    // would override the app one for this subtree, and its QLabel rule carried
    // no colour - which is what rendered these labels black in dark mode.

    auto *workLayout = new QVBoxLayout(m_workspaceContainer);
    workLayout->setContentsMargins(20, 16, 20, 16);
    workLayout->setSpacing(10);

    // Header info row
    auto *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(12);

    auto *iconBadge = new QLabel(m_workspaceContainer);
    iconBadge->setFixedSize(36, 36);
    iconBadge->setStyleSheet("background-color: rgba(37, 99, 235, 0.12); border-radius: 8px; border: 1px solid rgba(37, 99, 235, 0.25);");
    iconBadge->setAlignment(Qt::AlignCenter);
    iconBadge->setPixmap(IconUtils::getPixmap(IconType::Music, QColor("#2563eb"), 20));
    headerRow->addWidget(iconBadge);

    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    auto *sectionTitle = new QLabel(tr("LOADED AUDIO FILE"), m_workspaceContainer);
    sectionTitle->setStyleSheet("font-size: 11px; font-weight: 800; color: #2563eb; letter-spacing: 0.5px;");

    m_selectedFilesLabel = new QLabel(m_workspaceContainer);
    m_selectedFilesLabel->setStyleSheet("font-size: 13.5px; font-weight: 700; color: palette(text);");
    m_selectedFilesLabel->setWordWrap(true);

    titleBox->addWidget(sectionTitle);
    titleBox->addWidget(m_selectedFilesLabel);
    headerRow->addLayout(titleBox, 1);

    m_changeFilesBtn = new QPushButton(tr("Remove File"), m_workspaceContainer);
    m_changeFilesBtn->setObjectName("ToolTabButton");
    m_changeFilesBtn->setCursor(Qt::PointingHandCursor);
    m_changeFilesBtn->setFixedHeight(34);
    m_changeFilesBtn->setIcon(IconUtils::getIcon(IconType::Delete, QColor("#ef4444"), 14));
    m_changeFilesBtn->setStyleSheet(R"(
        QPushButton#ToolTabButton {
            color: #ef4444;
            background-color: rgba(239, 68, 68, 0.06);
            border: 1px solid rgba(239, 68, 68, 0.35);
            border-radius: 8px;
            padding: 4px 14px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#ToolTabButton:hover {
            background-color: rgba(239, 68, 68, 0.15);
            border-color: #ef4444;
        }
    )");
    connect(m_changeFilesBtn, &QPushButton::clicked, this, &AudioPage::resetPage);
    headerRow->addWidget(m_changeFilesBtn);

    workLayout->addLayout(headerRow);

    // Divider line
    auto *divider = new QFrame(m_workspaceContainer);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("background-color: rgba(148, 163, 184, 0.2); max-height: 1px; border: none; margin: 2px 0;");
    workLayout->addWidget(divider);

    // Settings panel stack
    m_panelStack = new QStackedWidget(m_workspaceContainer);
    createConvertPanel();
    createCompressPanel();
    createTrimPanel();
    workLayout->addWidget(m_panelStack, 1);

    // Action Row
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);

    m_processButton = new QPushButton(tr("Convert Format"), m_workspaceContainer);
    m_processButton->setObjectName("PrimaryButton");
    m_processButton->setCursor(Qt::PointingHandCursor);
    m_processButton->setMinimumWidth(160);
    m_processButton->setFixedHeight(36);
    connect(m_processButton, &QPushButton::clicked, this, &AudioPage::onProcessClicked);
    btnRow->addWidget(m_processButton);

    workLayout->addLayout(btnRow);

    m_workspaceContainer->hide();
    m_mainLayout->addWidget(m_workspaceContainer);

    // Absorbs bottom stretch
    m_mainLayout->addStretch(1);

    scrollArea->setWidget(mainContainer);
    m_pageStack->addWidget(scrollArea);

    // Page 1: Standard Result Screen
    m_resultScreen = new ResultScreenWidget(this);
    connect(m_resultScreen, &ResultScreenWidget::backToWorkspaceRequested, this, &AudioPage::resetPage);
    m_pageStack->addWidget(m_resultScreen);

    rootLayout->addWidget(m_pageStack);
    retranslateUi();
}

void AudioPage::createConvertPanel() {
    m_convertPanel = new QWidget(this);
    auto *layout = new QVBoxLayout(m_convertPanel);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(10);

    auto *formatRow = new QHBoxLayout();
    auto *fmtLbl = new QLabel(tr("Target Format:"), m_convertPanel);
    fmtLbl->setStyleSheet("font-weight: 600; font-size: 13px; min-width: 120px;");
    formatRow->addWidget(fmtLbl);

    m_convertFormatCombo = new QComboBox(m_convertPanel);
    m_convertFormatCombo->setFixedHeight(34);
    m_convertFormatCombo->addItem("MP3 (.mp3)", "mp3");
    m_convertFormatCombo->addItem("WAV (.wav)", "wav");
    m_convertFormatCombo->addItem("FLAC (.flac)", "flac");
    m_convertFormatCombo->addItem("OGG (.ogg)", "ogg");
    m_convertFormatCombo->addItem("M4A (.m4a)", "m4a");
    formatRow->addWidget(m_convertFormatCombo, 1);
    layout->addLayout(formatRow);

    auto *bitrateRow = new QHBoxLayout();
    auto *bitLbl = new QLabel(tr("Audio Quality (Bitrate):"), m_convertPanel);
    bitLbl->setStyleSheet("font-weight: 600; font-size: 13px; min-width: 120px;");
    bitrateRow->addWidget(bitLbl);

    m_convertBitrateCombo = new QComboBox(m_convertPanel);
    m_convertBitrateCombo->setFixedHeight(34);
    m_convertBitrateCombo->addItem("320 kbps (Highest Quality)", 320);
    m_convertBitrateCombo->addItem("256 kbps (High Quality)", 256);
    m_convertBitrateCombo->addItem("192 kbps (Standard - Recommended)", 192);
    m_convertBitrateCombo->addItem("128 kbps (Low Size)", 128);
    m_convertBitrateCombo->setCurrentIndex(2); // 192k default
    bitrateRow->addWidget(m_convertBitrateCombo, 1);
    layout->addLayout(bitrateRow);

    layout->addStretch();
    m_panelStack->addWidget(m_convertPanel);
}

void AudioPage::createCompressPanel() {
    m_compressPanel = new QWidget(this);
    auto *layout = new QVBoxLayout(m_compressPanel);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(8);

    auto *qualLbl = new QLabel(tr("Compression Level:"), m_compressPanel);
    qualLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    layout->addWidget(qualLbl);

    m_compressBitrateSlider = new PillSliderWidget(m_compressPanel);
    layout->addWidget(m_compressBitrateSlider);

    layout->addStretch();
    m_panelStack->addWidget(m_compressPanel);
}

void AudioPage::createTrimPanel() {
    m_trimPanel = new QWidget(this);
    auto *layout = new QVBoxLayout(m_trimPanel);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(8);

    // 1. Playback Controls Row (Play/Pause Button + Time Label)
    auto *playCtrlRow = new QHBoxLayout();
    playCtrlRow->setContentsMargins(0, 0, 0, 0);
    playCtrlRow->setSpacing(12);

    m_trimPlayPauseBtn = new QPushButton(m_trimPanel);
    m_trimPlayPauseBtn->setObjectName("ToolTabButton");
    m_trimPlayPauseBtn->setCursor(Qt::PointingHandCursor);
    m_trimPlayPauseBtn->setFixedHeight(32);
    m_trimPlayPauseBtn->setMinimumWidth(90);
    updateTrimPlaybackButton(false);
    connect(m_trimPlayPauseBtn, &QPushButton::clicked, this, &AudioPage::togglePlayPause);
    playCtrlRow->addWidget(m_trimPlayPauseBtn);

    m_trimPlaybackTimeLabel = new QLabel("00:00 / 00:00", m_trimPanel);
    m_trimPlaybackTimeLabel->setStyleSheet("font-size: 12.5px; font-weight: 700; color: palette(text); min-width: 100px;");
    playCtrlRow->addWidget(m_trimPlaybackTimeLabel);

    playCtrlRow->addStretch();
    layout->addLayout(playCtrlRow);

    // 2. Visual Range Slider / Timeline Header & Slider
    auto *sliderHeaderRow = new QHBoxLayout();
    auto *rangeLbl = new QLabel(tr("Timeline and Trim Range:"), m_trimPanel);
    rangeLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    sliderHeaderRow->addWidget(rangeLbl);
    sliderHeaderRow->addStretch();

    m_trimSelectedDurationLabel = new QLabel(tr("Selected Duration: 0.0 sec"), m_trimPanel);
    m_trimSelectedDurationLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #3b82f6;");
    sliderHeaderRow->addWidget(m_trimSelectedDurationLabel);
    layout->addLayout(sliderHeaderRow);

    m_trimRangeSlider = new VideoRangeSlider(m_trimPanel);
    layout->addWidget(m_trimRangeSlider);

    // 3. Fine-tuning Numeric SpinBoxes Row
    auto *fineGrid = new QGridLayout();
    fineGrid->setVerticalSpacing(6);
    fineGrid->setHorizontalSpacing(16);

    auto *startLbl = new QLabel(tr("Start Second:"), m_trimPanel);
    startLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    fineGrid->addWidget(startLbl, 0, 0);

    m_trimStartSpin = new QDoubleSpinBox(m_trimPanel);
    m_trimStartSpin->setFixedHeight(32);
    m_trimStartSpin->setRange(0, 36000);
    m_trimStartSpin->setDecimals(1);
    m_trimStartSpin->setSingleStep(0.5);
    m_trimStartSpin->setSuffix(" sn");
    fineGrid->addWidget(m_trimStartSpin, 0, 1);

    auto *endLbl = new QLabel(tr("End Second:"), m_trimPanel);
    endLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    fineGrid->addWidget(endLbl, 0, 2);

    m_trimEndSpin = new QDoubleSpinBox(m_trimPanel);
    m_trimEndSpin->setFixedHeight(32);
    m_trimEndSpin->setRange(0.1, 36000);
    m_trimEndSpin->setDecimals(1);
    m_trimEndSpin->setSingleStep(0.5);
    m_trimEndSpin->setSuffix(" sn");
    fineGrid->addWidget(m_trimEndSpin, 0, 3);

    layout->addLayout(fineGrid);

    // Connections between Range Slider <--> SpinBoxes & Player
    connect(m_trimRangeSlider, &VideoRangeSlider::rangeChanged, this, [this](double start, double end) {
        m_trimStartSpin->blockSignals(true);
        m_trimEndSpin->blockSignals(true);
        m_trimStartSpin->setValue(start);
        m_trimEndSpin->setValue(end);
        m_trimStartSpin->blockSignals(false);
        m_trimEndSpin->blockSignals(false);
        updateTrimDurationLabels();
    });

    connect(m_trimRangeSlider, &VideoRangeSlider::seekRequested, this, [this](double pos) {
        if (m_player) {
            m_player->setPosition(static_cast<qint64>(pos * 1000.0));
        }
    });

    connect(m_trimStartSpin, &QDoubleSpinBox::valueChanged, this, [this](double val) {
        m_trimRangeSlider->setStartValue(val);
        updateTrimDurationLabels();
    });

    connect(m_trimEndSpin, &QDoubleSpinBox::valueChanged, this, [this](double val) {
        m_trimRangeSlider->setEndValue(val);
        updateTrimDurationLabels();
    });

    m_panelStack->addWidget(m_trimPanel);
}

void AudioPage::togglePlayPause() {
    if (!m_player) return;
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else {
        m_player->play();
    }
}

void AudioPage::updateTrimPlaybackButton(bool playing) {
    if (!m_trimPlayPauseBtn) return;

    m_trimPlayPauseBtn->setText(playing ? tr("Pause") : tr("Play"));
    m_trimPlayPauseBtn->setIcon(IconUtils::getIcon(
        playing ? IconType::Pause : IconType::Play,
        Theme::color(ColorToken::TextSecondary), 16));
    m_trimPlayPauseBtn->setIconSize(QSize(16, 16));
    m_trimPlayPauseBtn->setToolTip(playing ? tr("Pause") : tr("Play"));
}

void AudioPage::handlePositionChanged(qint64 positionMs) {
    double sec = positionMs / 1000.0;
    if (m_trimRangeSlider) {
        m_trimRangeSlider->setPlayheadPosition(sec);
    }

    int curSec = static_cast<int>(sec);
    int totSec = static_cast<int>(m_totalDurationSec);
    QString timeStr = QString("%1:%2 / %3:%4")
        .arg(curSec / 60, 2, 10, QChar('0'))
        .arg(curSec % 60, 2, 10, QChar('0'))
        .arg(totSec / 60, 2, 10, QChar('0'))
        .arg(totSec % 60, 2, 10, QChar('0'));

    if (m_trimPlaybackTimeLabel) {
        m_trimPlaybackTimeLabel->setText(timeStr);
    }
}

void AudioPage::handleDurationChanged(qint64 durationMs) {
    if (durationMs > 0) {
        m_totalDurationSec = durationMs / 1000.0;
        updateTrimDurationRange();
    }
}

void AudioPage::updateTrimDurationRange() {
    if (m_trimRangeSlider) {
        m_trimRangeSlider->setRange(0.0, m_totalDurationSec);
        m_trimRangeSlider->setStartValue(0.0);
        m_trimRangeSlider->setEndValue(m_totalDurationSec);
    }
    if (m_trimStartSpin) {
        m_trimStartSpin->setMaximum(m_totalDurationSec);
        m_trimStartSpin->setValue(0.0);
    }
    if (m_trimEndSpin) {
        m_trimEndSpin->setMaximum(m_totalDurationSec);
        m_trimEndSpin->setValue(m_totalDurationSec);
    }
    updateTrimDurationLabels();
}

void AudioPage::updateTrimDurationLabels() {
    if (!m_trimRangeSlider || !m_trimSelectedDurationLabel) return;
    double start = m_trimRangeSlider->startValue();
    double end = m_trimRangeSlider->endValue();
    double dur = std::max(0.0, end - start);
    m_trimSelectedDurationLabel->setText(tr("Selected Duration: %1 sec").arg(QString::number(dur, 'f', 1)));
}

void AudioPage::setMode(Mode mode) {
    m_currentMode = mode;

    int idx = static_cast<int>(mode);
    if (auto *btn = m_tabGroup->button(idx)) {
        btn->setChecked(true);
    }

    if (m_workspaceContainer) {
        if (mode == Mode::Trim) {
            m_workspaceContainer->setFixedHeight(340);
        } else {
            m_workspaceContainer->setFixedHeight(300);
        }
    }

    // Stop playback if leaving Trim mode
    if (mode != Mode::Trim && m_player && m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
        updateTrimPlaybackButton(false);
    }

    // If we have loaded files and we switch to Trim mode, make sure player and trim duration are synced
    if (mode == Mode::Trim && !m_inputFiles.isEmpty()) {
        QString firstFile = m_inputFiles.first();
        if (m_player) {
            m_player->setSource(QUrl::fromLocalFile(firstFile));
        }
        updateTrimDurationRange();
    }

    switch (mode) {
        case Mode::Convert:
            m_subtitleLabel->setText(tr("Convert audio files to MP3, WAV, FLAC, OGG, and M4A formats."));
            m_panelStack->setCurrentWidget(m_convertPanel);
            m_dropZone->setInstruction(tr("Drop Audio Files to Convert Here"), tr("Batch conversion supported (MP3, WAV, FLAC, OGG, M4A, AAC)"));
            m_processButton->setText(tr("Convert Format"));
            break;
        case Mode::Compress:
            m_subtitleLabel->setText(tr("Optimize and reduce audio file size without sacrificing quality."));
            m_panelStack->setCurrentWidget(m_compressPanel);
            m_dropZone->setInstruction(tr("Drop Audio Files to Compress Here"), tr("Batch compression supported (MP3, WAV, FLAC, OGG, M4A, AAC)"));
            m_processButton->setText(tr("Compress Audio"));
            break;
        case Mode::Trim:
            m_subtitleLabel->setText(tr("Cut audio file between start and end points."));
            m_panelStack->setCurrentWidget(m_trimPanel);
            m_dropZone->setInstruction(tr("Drop Audio File to Trim Here"), tr("Select a single audio file"));
            m_processButton->setText(tr("Trim and Save Audio"));
            break;
    }

    // Preserve UI state across tab transitions
    if (!m_inputFiles.isEmpty()) {
        m_dropZone->hide();
        m_workspaceContainer->show();
    } else {
        m_workspaceContainer->hide();
        m_dropZone->show();
    }

    updateProcessButtonState();
}

void AudioPage::resetPage() {
    if (m_player) {
        m_player->stop();
    }
    if (m_trimPlayPauseBtn) {
        updateTrimPlaybackButton(false);
    }
    m_inputFiles.clear();

    m_workspaceContainer->hide();
    m_dropZone->show();
    m_progressBar->hide();
    m_progressBar->setValue(0);
    m_statusBanner->hideBanner();
    m_pageStack->setCurrentIndex(0);

    m_isProcessing = false;
    m_isProbing = false;
    updateProcessButtonState();
}

void AudioPage::onFilesDropped(const QStringList& paths) {
    if (paths.isEmpty()) return;

    m_isProbing = false;

    if (m_currentMode == Mode::Trim) {
        m_inputFiles = QStringList{paths.first()};
    } else {
        m_inputFiles = paths;
    }

    if (m_inputFiles.size() == 1) {
        QString filePath = m_inputFiles.first();
        QFileInfo fi(filePath);
        m_selectedFilesLabel->setText(fi.fileName() + QString(" (%1)").arg(QLocale().formattedDataSize(fi.size())));

        m_totalDurationSec = 10.0;

        // Set player source
        if (m_player) {
            m_player->setSource(QUrl::fromLocalFile(filePath));
        }
        updateTrimDurationRange();
        startAudioProbe(filePath);
    } else {
        m_selectedFilesLabel->setText(tr("%1 Audio File(s) Selected").arg(m_inputFiles.size()));
    }

    m_dropZone->hide();
    m_workspaceContainer->show();
    m_statusBanner->hideBanner();
    updateProcessButtonState();
}

void AudioPage::updateProcessButtonState() {
    m_processButton->setEnabled(!m_inputFiles.isEmpty() && !m_isProcessing && !m_isProbing);
    if (m_isProcessing) {
        m_processButton->setText(tr("Processing..."));
    } else if (m_isProbing) {
        m_processButton->setText(tr("Analyzing..."));
    } else {
        switch (m_currentMode) {
            case Mode::Convert: m_processButton->setText(tr("Convert Format")); break;
            case Mode::Compress: m_processButton->setText(tr("Compress Audio")); break;
            case Mode::Trim: m_processButton->setText(tr("Trim and Save Audio")); break;
        }
    }
}

void AudioPage::startAudioProbe(const QString& filePath) {
    m_isProbing = true;
    updateProcessButtonState();

    AsyncTask::run<Core::Result<Core::AudioBackend::AudioInfo>>(
        this,
        [filePath](AsyncTask::CancelCheck) {
            return Core::AudioBackend::probeAudio(filePath);
        },
        [this, filePath](Core::Result<Core::AudioBackend::AudioInfo> probeRes) {
            if (m_inputFiles.isEmpty() || m_inputFiles.first() != filePath) {
                return;
            }

            m_isProbing = false;
            if (!probeRes.hasValue()) {
                if (m_player) {
                    m_player->stop();
                    m_player->setSource(QUrl());
                }
                m_inputFiles.clear();
                m_workspaceContainer->hide();
                m_dropZone->show();
                m_statusBanner->showBanner(
                    BannerType::Error,
                    QString::fromStdString(probeRes.error().toUserFriendlyMessage()));
                updateProcessButtonState();
                return;
            }

            if (probeRes.value().durationSeconds > 0.0) {
                m_totalDurationSec = probeRes.value().durationSeconds;
            }
            updateTrimDurationRange();
            updateProcessButtonState();
        });
}

void AudioPage::onProcessClicked() {
    if (m_inputFiles.isEmpty()) return;

    m_isProcessing = true;
    updateProcessButtonState();
    m_statusBanner->hideBanner();

    switch (m_currentMode) {
        case Mode::Convert: processConvert(); break;
        case Mode::Compress: processCompress(); break;
        case Mode::Trim: processTrim(); break;
    }
}

void AudioPage::processConvert() {
    if (m_inputFiles.isEmpty()) return;

    QString defaultDir = Services::SettingsService::instance().getDefaultOutputDir();
    QDir().mkpath(defaultDir);

    QString targetFormat = m_convertFormatCombo->currentData().toString();
    if (m_inputFiles.size() == 1) {
        QFileInfo fi(m_inputFiles.first());
        QString outName = fi.completeBaseName() + "_converted." + targetFormat;
        m_lastOutPath = QDir(defaultDir).absoluteFilePath(outName);
        m_successTitle = tr("Audio Format Converted Successfully!");
    } else {
        m_lastOutPath = defaultDir;
        m_successTitle = tr("%1 Audio File(s) Converted Successfully!").arg(m_inputFiles.size());
    }

    auto op = std::make_unique<Core::ConvertAudioOperation>();
    op->setInputFiles(m_inputFiles);
    op->setOutputDirectory(defaultDir);
    op->setTargetFormat(targetFormat);
    op->setTargetBitrate(m_convertBitrateCombo->currentData().toInt());

    Core::JobQueue::instance().submit(std::move(op));
}

void AudioPage::processCompress() {
    if (m_inputFiles.isEmpty()) return;

    QString defaultDir = Services::SettingsService::instance().getDefaultOutputDir();
    QDir().mkpath(defaultDir);

    if (m_inputFiles.size() == 1) {
        QFileInfo fi(m_inputFiles.first());
        QString outName = fi.completeBaseName() + "_compressed." + fi.suffix();
        m_lastOutPath = QDir(defaultDir).absoluteFilePath(outName);
        m_successTitle = tr("Audio Compressed Successfully!");
    } else {
        m_lastOutPath = defaultDir;
        m_successTitle = tr("%1 Audio File(s) Compressed Successfully!").arg(m_inputFiles.size());
    }

    auto op = std::make_unique<Core::CompressAudioOperation>();
    op->setInputFiles(m_inputFiles);

    int val = m_compressBitrateSlider->currentStep();
    int bitrate = (val == 1) ? 160 : (val == 2) ? 128 : (val == 3) ? 96 : 64;
    op->setTargetBitrate(bitrate);
    op->setOutputDirectory(defaultDir);

    Core::JobQueue::instance().submit(std::move(op));
}

void AudioPage::processTrim() {
    if (m_inputFiles.isEmpty()) return;

    QString defaultDir = Services::SettingsService::instance().getDefaultOutputDir();
    QDir().mkpath(defaultDir);

    QFileInfo fi(m_inputFiles.first());
    QString outName = fi.completeBaseName() + "_trimmed." + fi.suffix();
    m_lastOutPath = QDir(defaultDir).absoluteFilePath(outName);
    m_successTitle = tr("Audio Trimmed Successfully!");

    double startSec = m_trimStartSpin ? m_trimStartSpin->value() : 0.0;
    double endSec = m_trimEndSpin ? m_trimEndSpin->value() : m_totalDurationSec;

    auto op = std::make_unique<Core::TrimAudioOperation>();
    op->setInputFile(m_inputFiles.first());
    op->setOutputDirectory(defaultDir);
    op->setTrimRange(startSec, endSec);

    Core::JobQueue::instance().submit(std::move(op));
}

void AudioPage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void AudioPage::retranslateUi() {
    if (auto *btn = m_tabGroup->button(0)) btn->setText(tr("Convert Audio"));
    if (auto *btn = m_tabGroup->button(1)) btn->setText(tr("Compress Audio"));
    if (auto *btn = m_tabGroup->button(2)) btn->setText(tr("Trim Audio"));

    if (m_convertBitrateCombo && m_convertBitrateCombo->count() >= 4) {
        m_convertBitrateCombo->setItemText(0, tr("320 kbps (Highest Quality)"));
        m_convertBitrateCombo->setItemText(1, tr("256 kbps (High Quality)"));
        m_convertBitrateCombo->setItemText(2, tr("192 kbps (Standard - Recommended)"));
        m_convertBitrateCombo->setItemText(3, tr("128 kbps (Small Size)"));
    }

    updateTrimPlaybackButton(m_player && m_player->playbackState() == QMediaPlayer::PlayingState);

    setMode(m_currentMode);
}

} // namespace LocalTools::UI
