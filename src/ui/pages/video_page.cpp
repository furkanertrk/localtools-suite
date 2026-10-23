#include "video_page.h"
#include <QResizeEvent>
#include <QScrollArea>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileInfo>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QDateTime>
#include <QAudioOutput>
#include <QPainter>
#include <QPainterPath>

#include <functional>

#include "../../services/settings_service.h"
#include "../../services/file_service.h"
#include "../../services/logging_service.h"
#include "../common/async_task.h"
#include "../theme.h"

namespace LocalTools::UI {

class DynamicStackedWidget : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;

    QSize sizeHint() const override {
        if (currentWidget()) {
            return currentWidget()->sizeHint();
        }
        return QStackedWidget::sizeHint();
    }

    QSize minimumSizeHint() const override {
        if (currentWidget()) {
            return currentWidget()->minimumSizeHint();
        }
        return QStackedWidget::minimumSizeHint();
    }
};

static QWidget* createPreviewSizeSegmentControl(
    QWidget* parent,
    QLabel*& titleLabel,
    QPushButton*& fitBtn,
    QPushButton*& halfBtn,
    QPushButton*& fullBtn,
    QButtonGroup*& group,
    std::function<void(VideoPreviewSize)> onChanged)
{
    auto* container = new QWidget(parent);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Theme::space(Space::XS));

    titleLabel = new QLabel(QObject::tr("Preview Size:"), container);
    titleLabel->setObjectName("InfoRowKey");
    titleLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    titleLabel->setMinimumWidth(titleLabel->fontMetrics().horizontalAdvance(titleLabel->text()) + 4);
    layout->addWidget(titleLabel, 0, Qt::AlignVCenter);

    group = new QButtonGroup(container);
    group->setExclusive(true);

    auto makeBtn = [&](const QString& text, VideoPreviewSize mode, bool checked) {
        auto* btn = new QPushButton(text, container);
        btn->setObjectName("ToolTabButtonCompact");
        btn->setCheckable(true);
        btn->setChecked(checked);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        group->addButton(btn, static_cast<int>(mode));
        layout->addWidget(btn);
        return btn;
    };

    fitBtn = makeBtn(QObject::tr("Fit"), VideoPreviewSize::Fit, true);
    halfBtn = makeBtn(QObject::tr("50%"), VideoPreviewSize::Half, false);
    fullBtn = makeBtn(QObject::tr("100%"), VideoPreviewSize::Full, false);

    QObject::connect(group, &QButtonGroup::idClicked, [onChanged](int id) {
        if (onChanged) {
            onChanged(static_cast<VideoPreviewSize>(id));
        }
    });

    return container;
}

VideoPage::VideoPage(QWidget *parent)
    : QWidget(parent) {
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    connect(m_player, &QMediaPlayer::positionChanged, this, &VideoPage::handlePositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &VideoPage::handleDurationChanged);

    setupUi();
}

VideoPage::~VideoPage() {
    if (m_player) {
        m_player->stop();
    }
}

void VideoPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_panelStack) {
        if (m_panelStack->currentIndex() == 1) {
            updateGifPlayerSize();
        } else if (m_panelStack->currentIndex() == 3) {
            updatePlayerSize();
        }
    }
}

void VideoPage::setupUi() {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_pageStack = new QStackedWidget(this);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    auto *mainContainer = new QWidget(scrollArea);
    auto *mainLayout = new QVBoxLayout(mainContainer);
    mainLayout->setContentsMargins(28, 20, 28, 20);
    mainLayout->setSpacing(14);

    // 1. Top Sub-navigation Tabs
    mainLayout->addWidget(createTabHeader());

    // 2. Tab Description Subtitle
    m_tabDescriptionLabel = new QLabel(tr("Optimize and reduce video file size without quality loss."), this);
    m_tabDescriptionLabel->setStyleSheet("font-size: 13px; color: palette(text); opacity: 0.75; margin-bottom: 2px;");
    mainLayout->addWidget(m_tabDescriptionLabel);

    // 3. Inline Status Banner
    m_statusBanner = new StatusBannerWidget(this);
    mainLayout->addWidget(m_statusBanner);

    // 3. Drop Zone for Video Files
    m_dropZone = new DropZoneWidget(this);
    m_dropZone->setInstruction(tr("Drag Video File Here"),
                               tr("or click to select video (MP4, MKV, AVI, MOV, WEBM)"));
    m_dropZone->setAcceptedFileExtensions({"mp4", "mkv", "avi", "mov", "webm", "flv", "wmv", "m4v"},
                                          tr("Select Video File"),
                                          tr("Only Video Files Accepted"));
    connect(m_dropZone, &DropZoneWidget::filesDropped, this, &VideoPage::handleVideoDropped);
    mainLayout->addWidget(m_dropZone);

    // 4. Options Container (Revealed when a video is loaded) - 2-Column Side-by-Side Layout
    m_optionsContainer = new QWidget(this);
    m_optionsContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *optsLayout = new QHBoxLayout(m_optionsContainer);
    optsLayout->setContentsMargins(0, 0, 0, 0);
    optsLayout->setSpacing(20);
    optsLayout->setAlignment(Qt::AlignTop);

    // Left Column: Tool sub-panels (Compress, GIF, Audio, Trim)
    m_panelStack = new DynamicStackedWidget(m_optionsContainer);
    m_panelStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_compressPanel = createCompressPanel();
    m_gifPanel = createGifPanel();
    m_audioPanel = createAudioPanel();
    m_trimPanel = createTrimPanel();

    m_panelStack->addWidget(m_compressPanel);
    m_panelStack->addWidget(m_gifPanel);
    m_panelStack->addWidget(m_audioPanel);
    m_panelStack->addWidget(m_trimPanel);
    optsLayout->addWidget(m_panelStack, 1, Qt::AlignTop);

    // Right Column: Loaded Video File Card (Fixed, Small & Compact)
    optsLayout->addWidget(createVideoCard(), 0, Qt::AlignTop);

    m_optionsContainer->hide();
    mainLayout->addWidget(m_optionsContainer);

    // Progress Bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    // Stretch at bottom absorbs all remaining window height
    mainLayout->addStretch(1);

    scrollArea->setWidget(mainContainer);
    m_pageStack->addWidget(scrollArea);

    // 5. Result Screen
    m_resultScreen = new ResultScreenWidget(this);
    connect(m_resultScreen, &ResultScreenWidget::backToWorkspaceRequested, this, &VideoPage::resetWorkspace);
    m_pageStack->addWidget(m_resultScreen);

    rootLayout->addWidget(m_pageStack);
    retranslateUi();
}

QWidget* VideoPage::createTabHeader() {
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
        btn->setIcon(IconUtils::getIcon(iconType, QColor("#64748b"), 16));
        btn->setIconSize(QSize(16, 16));
        btn->setCursor(Qt::PointingHandCursor);
        m_tabButtonGroup->addButton(btn, id);
        layout->addWidget(btn);
        return btn;
    };

    m_compressTabBtn = createTabBtn(tr("Compress Video"), IconType::Compress, 0);
    m_gifTabBtn = createTabBtn(tr("Convert to GIF"), IconType::Convert, 1);
    m_audioTabBtn = createTabBtn(tr("Extract Audio (MP3)"), IconType::Extract, 2);
    m_trimTabBtn = createTabBtn(tr("Video Kes"), IconType::Split, 3);

    m_compressTabBtn->setChecked(true);
    layout->addStretch();

    connect(m_tabButtonGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_panelStack->setCurrentIndex(id);
        updateTabDescription(id);

        if (m_videoCard) {
            m_videoCard->setFixedHeight(300);
        }

        if (id == 1) { // GIF Tab
            if (m_player) {
                m_player->setVideoOutput(m_gifVideoWidget);
                if (m_gifStartSpin) {
                    m_player->setPosition(static_cast<qint64>(m_gifStartSpin->value() * 1000.0));
                }
            }
            updateGifPlayerSize();
        } else if (id == 3) { // Video Kes Tab
            if (m_player) {
                m_player->setVideoOutput(m_videoWidget);
                if (m_trimStartSpin) {
                    m_player->setPosition(static_cast<qint64>(m_trimStartSpin->value() * 1000.0));
                }
            }
            updatePlayerSize();
        } else { // Compress (0) or Audio (2)
            if (m_player) {
                m_player->pause();
                setPlayPauseAppearance(false);
            }
        }

        m_panelStack->updateGeometry();
        m_optionsContainer->updateGeometry();
    });

    return headerWidget;
}

QWidget* VideoPage::createVideoCard() {
    m_videoCard = new QFrame(m_optionsContainer);
    m_videoCard->setObjectName("CardFrame");
    m_videoCard->setFixedWidth(320);
    m_videoCard->setFixedHeight(300);
    // No local stylesheet: a widget-level sheet overrides the application one
    // for its whole subtree, and the QLabel rule it used to carry specified no
    // colour, so every label here fell back to the default palette text.

    auto *cardLayout = new QVBoxLayout(m_videoCard);
    const int pad = Theme::space(Space::M);
    cardLayout->setContentsMargins(pad, pad, pad, pad);
    cardLayout->setSpacing(Theme::space(Space::S));

    // 1. Top header: badge + file name
    auto *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(Theme::space(Space::S));

    m_videoCardBadge = new QLabel(m_videoCard);
    m_videoCardBadge->setObjectName("IconBadge");
    m_videoCardBadge->setFixedSize(40, 40);
    m_videoCardBadge->setAlignment(Qt::AlignCenter);
    headerRow->addWidget(m_videoCardBadge, 0, Qt::AlignTop);

    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    auto *sectionTitle = new QLabel(tr("LOADED VIDEO"), m_videoCard);
    sectionTitle->setObjectName("CardEyebrow");

    m_videoCardNameLabel = new QLabel(m_videoCard);
    m_videoCardNameLabel->setObjectName("ToolCardName");
    m_videoCardNameLabel->setWordWrap(true);

    titleBox->addWidget(sectionTitle);
    titleBox->addWidget(m_videoCardNameLabel);
    headerRow->addLayout(titleBox, 1);
    cardLayout->addLayout(headerRow);

    // 2. Divider
    auto *divider = new QFrame(m_videoCard);
    divider->setObjectName("CardDivider");
    divider->setFrameShape(QFrame::HLine);
    divider->setFixedHeight(1);
    cardLayout->addWidget(divider);

    // 3. Info grid. Icons are themed vectors on their own column so the value
    //    text stays aligned and vertically centred against them.
    auto *infoGrid = new QGridLayout();
    infoGrid->setContentsMargins(0, 0, 0, 0);
    infoGrid->setHorizontalSpacing(Theme::space(Space::XS));
    infoGrid->setVerticalSpacing(Theme::space(Space::XXS));
    infoGrid->setColumnStretch(2, 1);

    for (int row = 0; row < RowCount; ++row) {
        m_infoIcon[row] = new QLabel(m_videoCard);
        m_infoIcon[row]->setFixedSize(16, 16);
        m_infoIcon[row]->setAlignment(Qt::AlignCenter);

        m_infoKey[row] = new QLabel(m_videoCard);
        m_infoKey[row]->setObjectName("InfoRowKey");

        m_infoValue[row] = new QLabel(m_videoCard);
        m_infoValue[row]->setObjectName("InfoRowValue");

        infoGrid->addWidget(m_infoIcon[row],  row, 0, Qt::AlignVCenter);
        infoGrid->addWidget(m_infoKey[row],   row, 1, Qt::AlignVCenter);
        infoGrid->addWidget(m_infoValue[row], row, 2, Qt::AlignVCenter | Qt::AlignRight);
    }
    cardLayout->addLayout(infoGrid);

    cardLayout->addStretch(1);

    // 4. Remove / change video
    m_videoCardRemoveBtn = new QPushButton(tr("Remove Video"), m_videoCard);
    m_videoCardRemoveBtn->setObjectName("DangerButton");
    m_videoCardRemoveBtn->setCursor(Qt::PointingHandCursor);
    m_videoCardRemoveBtn->setMinimumHeight(36);
    connect(m_videoCardRemoveBtn, &QPushButton::clicked, this, &VideoPage::resetWorkspace);
    cardLayout->addWidget(m_videoCardRemoveBtn);

    refreshVideoCardIcons();
    return m_videoCard;
}

void VideoPage::refreshVideoCardIcons() {
    if (m_videoCardBadge) {
        m_videoCardBadge->setPixmap(
            IconUtils::getPixmap(IconType::Video, Theme::color(ColorToken::Accent), 22));
    }
    if (m_videoCardRemoveBtn) {
        m_videoCardRemoveBtn->setIcon(
            IconUtils::getIcon(IconType::Delete, Theme::color(ColorToken::ErrorText), 15));
        m_videoCardRemoveBtn->setIconSize(QSize(15, 15));
    }

    static constexpr IconType kRowIcons[RowCount] = {
        IconType::Clock, IconType::Monitor, IconType::Drive, IconType::Film
    };
    const QColor tint = Theme::color(ColorToken::TextTertiary);
    for (int row = 0; row < RowCount; ++row) {
        if (m_infoIcon[row]) {
            m_infoIcon[row]->setPixmap(IconUtils::getPixmap(kRowIcons[row], tint, 15));
        }
    }
}

QWidget* VideoPage::createCompressPanel() {
    auto *card = new QFrame(this);
    card->setObjectName("CardFrame");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setFixedHeight(300);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 14, 20, 14);
    layout->setSpacing(8);

    // 1. Resolution row
    auto *resRow = new QHBoxLayout();
    resRow->setContentsMargins(0, 0, 0, 0);
    resRow->setSpacing(12);
    auto *resLbl = new QLabel(tr("Target Resolution:"), card);
    resLbl->setStyleSheet("font-weight: 600; font-size: 13px; min-width: 130px;");
    resRow->addWidget(resLbl);

    m_resCombo = new QComboBox(card);
    m_resCombo->setFixedHeight(34);
    m_resCombo->addItem(tr("1080p Full HD (1920 px)"), 1920);
    m_resCombo->addItem(tr("720p HD (1280 px) - Recommended"), 1280);
    m_resCombo->addItem(tr("480p SD (854 px) - Small File"), 854);
    m_resCombo->setCurrentIndex(1);
    resRow->addWidget(m_resCombo, 1);
    layout->addLayout(resRow);

    // 2. Compression Level Section
    auto *qualLbl = new QLabel(tr("Compression Level:"), card);
    qualLbl->setStyleSheet("font-weight: 600; font-size: 13px; margin-top: 2px;");
    layout->addWidget(qualLbl);

    // 3. Full-width Pill Slider Widget
    m_qualitySlider = new PillSliderWidget(card);
    layout->addWidget(m_qualitySlider);

    // 4. Mute Audio Checkbox
    m_muteAudioCheck = new QCheckBox(tr("Mute Audio (Video Only)"), card);
    m_muteAudioCheck->setStyleSheet("font-size: 12.5px; padding: 2px 0;");
    layout->addWidget(m_muteAudioCheck);

    layout->addStretch(1);

    // 5. Action Button Row
    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->addStretch(1);
    auto *btn = new QPushButton(tr("Compress Video"), card);
    btn->setObjectName("PrimaryButton");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumWidth(180);
    btn->setFixedHeight(36);
    connect(btn, &QPushButton::clicked, this, &VideoPage::executeCompress);
    btnRow->addWidget(btn);
    layout->addLayout(btnRow);

    return card;
}

QWidget* VideoPage::createGifPanel() {
    auto *card = new QFrame(this);
    card->setObjectName("CardFrame");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);

    // 1. Embedded Video Player Container (Centered & Smooth Resizing)
    m_gifPlayerContainer = new QFrame(card);
    m_gifPlayerContainer->setObjectName("GifPlayerContainer");
    m_gifPlayerContainer->setStyleSheet(R"(
        QFrame#GifPlayerContainer {
            background-color: #020617;
            border-radius: 10px;
            border: 1px solid #1e293b;
        }
    )");
    auto *playerLayout = new QVBoxLayout(m_gifPlayerContainer);
    playerLayout->setContentsMargins(0, 0, 0, 0);

    m_gifVideoWidget = new QVideoWidget(m_gifPlayerContainer);
    m_gifVideoWidget->setAspectRatioMode(Qt::IgnoreAspectRatio);
    m_gifVideoWidget->setStyleSheet("background-color: #020617; border-radius: 10px;");
    playerLayout->addWidget(m_gifVideoWidget);

    auto *playerCenterRow = new QHBoxLayout();
    playerCenterRow->setContentsMargins(0, 0, 0, 0);
    playerCenterRow->addStretch(1);
    playerCenterRow->addWidget(m_gifPlayerContainer);
    playerCenterRow->addStretch(1);
    layout->addLayout(playerCenterRow);

    // 2. Playback & Zoom Control Bar
    auto *playerCtrlRow = new QHBoxLayout();
    playerCtrlRow->setContentsMargins(4, 0, 4, 0);
    playerCtrlRow->setSpacing(10);

    // Icon comes from the themed SVG set, not a glyph inside the label: the
    // play/pause characters render as a tofu box in the UI font.
    m_gifPlayPauseBtn = new QPushButton(tr("Play"), card);
    m_gifPlayPauseBtn->setObjectName("SecondaryButton");
    m_gifPlayPauseBtn->setCursor(Qt::PointingHandCursor);
    m_gifPlayPauseBtn->setIcon(IconUtils::getIcon(IconType::Play, Theme::color(ColorToken::TextPrimary), 14));
    m_gifPlayPauseBtn->setIconSize(QSize(14, 14));
    m_gifPlayPauseBtn->setMinimumWidth(100);
    m_gifPlayPauseBtn->setFixedHeight(32);
    connect(m_gifPlayPauseBtn, &QPushButton::clicked, this, &VideoPage::togglePlayPause);
    playerCtrlRow->addWidget(m_gifPlayPauseBtn);

    m_gifPlaybackTimeLabel = new QLabel("00:00 / 00:00", card);
    m_gifPlaybackTimeLabel->setObjectName("PlaybackTime");
    playerCtrlRow->addWidget(m_gifPlaybackTimeLabel);

    playerCtrlRow->addStretch();

    playerCtrlRow->addWidget(createPreviewSizeSegmentControl(
        card,
        m_gifZoomTitleLabel,
        m_gifFitBtn,
        m_gifHalfBtn,
        m_gifFullBtn,
        m_gifZoomGroup,
        [this](VideoPreviewSize size) {
            m_gifPreviewSize = size;
            updateGifPlayerSize();
        }
    ));

    layout->addLayout(playerCtrlRow);

    // 3. GIF Timeline Range Slider & Duration Header
    auto *sliderHeaderRow = new QHBoxLayout();
    auto *rangeLbl = new QLabel(tr("GIF Trim Range:"), card);
    rangeLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    sliderHeaderRow->addWidget(rangeLbl);
    sliderHeaderRow->addStretch();
    m_gifSelectedDurationLabel = new QLabel(tr("Selected Duration: 5.0 sec"), card);
    m_gifSelectedDurationLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #3b82f6;");
    sliderHeaderRow->addWidget(m_gifSelectedDurationLabel);
    layout->addLayout(sliderHeaderRow);

    m_gifRangeSlider = new VideoRangeSlider(card);
    layout->addWidget(m_gifRangeSlider);

    // 4. Fine-tuning SpinBoxes & GIF Options
    auto *fineGrid = new QGridLayout();
    fineGrid->setHorizontalSpacing(16);
    fineGrid->setVerticalSpacing(8);

    auto *startLbl = new QLabel(tr("Start Second:"), card);
    startLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    fineGrid->addWidget(startLbl, 0, 0);

    m_gifStartSpin = new QDoubleSpinBox(card);
    m_gifStartSpin->setFixedHeight(34);
    m_gifStartSpin->setRange(0, 36000);
    m_gifStartSpin->setDecimals(1);
    m_gifStartSpin->setSingleStep(0.5);
    m_gifStartSpin->setValue(0.0);
    m_gifStartSpin->setSuffix(" sn");
    fineGrid->addWidget(m_gifStartSpin, 0, 1);

    auto *durLbl = new QLabel(tr("Duration (Seconds):"), card);
    durLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    fineGrid->addWidget(durLbl, 0, 2);

    m_gifDurationSpin = new QDoubleSpinBox(card);
    m_gifDurationSpin->setFixedHeight(34);
    m_gifDurationSpin->setRange(0.5, 60.0);
    m_gifDurationSpin->setDecimals(1);
    m_gifDurationSpin->setSingleStep(0.5);
    m_gifDurationSpin->setValue(5.0);
    m_gifDurationSpin->setSuffix(" sn");
    fineGrid->addWidget(m_gifDurationSpin, 0, 3);

    auto *wLbl = new QLabel(tr("Width:"), card);
    wLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    fineGrid->addWidget(wLbl, 1, 0);

    m_gifWidthCombo = new QComboBox(card);
    m_gifWidthCombo->setFixedHeight(34);
    m_gifWidthCombo->addItem(tr("320 px (Small)"), 320);
    m_gifWidthCombo->addItem(tr("480 px (Standard)"), 480);
    m_gifWidthCombo->addItem(tr("640 px (Large HD)"), 640);
    m_gifWidthCombo->setCurrentIndex(1);
    fineGrid->addWidget(m_gifWidthCombo, 1, 1);

    auto *fpsLbl = new QLabel(tr("Frame Rate (FPS):"), card);
    fpsLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    fineGrid->addWidget(fpsLbl, 1, 2);

    m_gifFpsCombo = new QComboBox(card);
    m_gifFpsCombo->setFixedHeight(34);
    m_gifFpsCombo->addItem(tr("10 FPS"), 10);
    m_gifFpsCombo->addItem(tr("15 FPS (Smooth)"), 15);
    m_gifFpsCombo->addItem(tr("24 FPS (High)"), 24);
    m_gifFpsCombo->setCurrentIndex(1);
    fineGrid->addWidget(m_gifFpsCombo, 1, 3);

    layout->addLayout(fineGrid);

    // Connect GIF Range Slider <--> SpinBoxes
    connect(m_gifRangeSlider, &VideoRangeSlider::rangeChanged, this, [this](double start, double end) {
        m_gifStartSpin->blockSignals(true);
        m_gifDurationSpin->blockSignals(true);
        m_gifStartSpin->setValue(start);
        double dur = std::max(0.5, end - start);
        m_gifDurationSpin->setValue(dur);
        m_gifStartSpin->blockSignals(false);
        m_gifDurationSpin->blockSignals(false);
        updateGifDurationLabels();
    });

    connect(m_gifRangeSlider, &VideoRangeSlider::seekRequested, this, [this](double pos) {
        if (m_player) {
            m_player->setPosition(static_cast<qint64>(pos * 1000.0));
        }
    });

    connect(m_gifStartSpin, &QDoubleSpinBox::valueChanged, this, [this](double val) {
        double dur = m_gifDurationSpin->value();
        m_gifRangeSlider->setStartValue(val);
        m_gifRangeSlider->setEndValue(std::min(m_totalDurationSec, val + dur));
        updateGifDurationLabels();
        if (m_player) {
            m_player->setPosition(static_cast<qint64>(val * 1000.0));
        }
    });

    connect(m_gifDurationSpin, &QDoubleSpinBox::valueChanged, this, [this](double dur) {
        double start = m_gifStartSpin->value();
        m_gifRangeSlider->setEndValue(std::min(m_totalDurationSec, start + dur));
        updateGifDurationLabels();
    });

    // 5. Action Row
    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->addStretch();
    auto *btn = new QPushButton(tr("Create GIF"), card);
    btn->setObjectName("PrimaryButton");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumWidth(180);
    btn->setFixedHeight(36);
    connect(btn, &QPushButton::clicked, this, &VideoPage::executeGif);
    btnRow->addWidget(btn);
    layout->addLayout(btnRow);

    return card;
}

QWidget* VideoPage::createAudioPanel() {
    auto *card = new QFrame(this);
    card->setObjectName("CardFrame");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    auto *grid = new QGridLayout();
    grid->setVerticalSpacing(12);
    grid->setHorizontalSpacing(16);

    auto *fmtLbl = new QLabel(tr("Audio Format:"), card);
    fmtLbl->setStyleSheet("font-weight: 600; font-size: 13.5px; min-width: 90px;");
    grid->addWidget(fmtLbl, 0, 0);

    m_audioFormatCombo = new QComboBox(card);
    m_audioFormatCombo->setFixedHeight(38);
    m_audioFormatCombo->addItem(tr("MP3 (.mp3)"), "mp3");
    m_audioFormatCombo->addItem(tr("WAV (Lossless .wav)"), "wav");
    m_audioFormatCombo->addItem(tr("AAC (.aac)"), "aac");
    grid->addWidget(m_audioFormatCombo, 0, 1);

    auto *bitLbl = new QLabel(tr("Bitrate (Quality):"), card);
    bitLbl->setStyleSheet("font-weight: 600; font-size: 13.5px; min-width: 100px;");
    grid->addWidget(bitLbl, 0, 2);

    m_audioBitrateCombo = new QComboBox(card);
    m_audioBitrateCombo->setFixedHeight(38);
    m_audioBitrateCombo->addItem(tr("128 kbps (Standard)"), 128);
    m_audioBitrateCombo->addItem(tr("192 kbps (High Quality)"), 192);
    m_audioBitrateCombo->addItem(tr("320 kbps (Studio)"), 320);
    m_audioBitrateCombo->setCurrentIndex(1);
    grid->addWidget(m_audioBitrateCombo, 0, 3);

    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    layout->addLayout(grid);

    auto *infoNote = new QLabel(tr("Extracts only the audio track from the video and saves it as a separate audio file at its original quality."), card);
    infoNote->setStyleSheet("font-size: 12.5px; color: #2563eb; font-weight: 500; padding: 4px 0;");
    infoNote->setWordWrap(true);
    layout->addWidget(infoNote);

    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 4, 0, 0);
    btnRow->addStretch();
    auto *btn = new QPushButton(tr("Extract Audio"), card);
    btn->setObjectName("PrimaryButton");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumWidth(180);
    btn->setFixedHeight(40);
    connect(btn, &QPushButton::clicked, this, &VideoPage::executeAudio);
    btnRow->addWidget(btn);
    layout->addLayout(btnRow);

    return card;
}

QWidget* VideoPage::createTrimPanel() {
    auto *card = new QFrame(this);
    card->setObjectName("CardFrame");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);

    // 1. Embedded Video Player Container (Centered & dynamically sized with zero black borders)
    m_playerContainer = new QFrame(card);
    m_playerContainer->setObjectName("VideoPlayerContainer");
    m_playerContainer->setStyleSheet(R"(
        QFrame#VideoPlayerContainer {
            background-color: #020617;
            border-radius: 10px;
            border: 1px solid #1e293b;
        }
    )");
    auto *playerLayout = new QVBoxLayout(m_playerContainer);
    playerLayout->setContentsMargins(0, 0, 0, 0);

    m_videoWidget = new QVideoWidget(m_playerContainer);
    m_videoWidget->setAspectRatioMode(Qt::IgnoreAspectRatio);
    m_videoWidget->setStyleSheet("background-color: #020617; border-radius: 10px;");
    m_player->setVideoOutput(m_videoWidget);
    playerLayout->addWidget(m_videoWidget);

    auto *playerCenterRow = new QHBoxLayout();
    playerCenterRow->setContentsMargins(0, 0, 0, 0);
    playerCenterRow->addStretch(1);
    playerCenterRow->addWidget(m_playerContainer);
    playerCenterRow->addStretch(1);
    layout->addLayout(playerCenterRow);

    // 2. Playback & Zoom Control Bar
    auto *playerCtrlRow = new QHBoxLayout();
    playerCtrlRow->setContentsMargins(4, 0, 4, 0);
    playerCtrlRow->setSpacing(10);

    m_playPauseBtn = new QPushButton(tr("Play"), card);
    m_playPauseBtn->setObjectName("SecondaryButton");
    m_playPauseBtn->setCursor(Qt::PointingHandCursor);
    m_playPauseBtn->setIcon(IconUtils::getIcon(IconType::Play, Theme::color(ColorToken::TextPrimary), 14));
    m_playPauseBtn->setIconSize(QSize(14, 14));
    m_playPauseBtn->setMinimumWidth(100);
    m_playPauseBtn->setFixedHeight(32);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &VideoPage::togglePlayPause);
    playerCtrlRow->addWidget(m_playPauseBtn);

    m_playbackTimeLabel = new QLabel("00:00 / 00:00", card);
    m_playbackTimeLabel->setObjectName("PlaybackTime");
    playerCtrlRow->addWidget(m_playbackTimeLabel);

    playerCtrlRow->addStretch();

    playerCtrlRow->addWidget(createPreviewSizeSegmentControl(
        card,
        m_trimZoomTitleLabel,
        m_trimFitBtn,
        m_trimHalfBtn,
        m_trimFullBtn,
        m_trimZoomGroup,
        [this](VideoPreviewSize size) {
            m_previewSize = size;
            updatePlayerSize();
        }
    ));

    layout->addLayout(playerCtrlRow);

    // 3. Visual Range Slider / Timeline Scrubber
    auto *sliderHeaderRow = new QHBoxLayout();
    auto *rangeLbl = new QLabel(tr("Timeline and Trim Range:"), card);
    rangeLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    sliderHeaderRow->addWidget(rangeLbl);
    sliderHeaderRow->addStretch();
    m_selectedDurationLabel = new QLabel(tr("Selected Duration: 0.0 sec"), card);
    m_selectedDurationLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #3b82f6;");
    sliderHeaderRow->addWidget(m_selectedDurationLabel);
    layout->addLayout(sliderHeaderRow);

    m_rangeSlider = new VideoRangeSlider(card);
    layout->addWidget(m_rangeSlider);

    // 4. Fine-tuning Numeric SpinBoxes Row
    auto *fineGrid = new QGridLayout();
    fineGrid->setVerticalSpacing(8);
    fineGrid->setHorizontalSpacing(16);

    auto *startLbl = new QLabel(tr("Start Second:"), card);
    startLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    fineGrid->addWidget(startLbl, 0, 0);

    m_trimStartSpin = new QDoubleSpinBox(card);
    m_trimStartSpin->setFixedHeight(34);
    m_trimStartSpin->setRange(0, 36000);
    m_trimStartSpin->setDecimals(1);
    m_trimStartSpin->setSingleStep(0.5);
    m_trimStartSpin->setSuffix(" sn");
    fineGrid->addWidget(m_trimStartSpin, 0, 1);

    auto *endLbl = new QLabel(tr("End Second:"), card);
    endLbl->setStyleSheet("font-weight: 600; font-size: 13px;");
    fineGrid->addWidget(endLbl, 0, 2);

    m_trimEndSpin = new QDoubleSpinBox(card);
    m_trimEndSpin->setFixedHeight(34);
    m_trimEndSpin->setRange(0.1, 36000);
    m_trimEndSpin->setDecimals(1);
    m_trimEndSpin->setSingleStep(0.5);
    m_trimEndSpin->setSuffix(" sn");
    fineGrid->addWidget(m_trimEndSpin, 0, 3);

    m_trimReencodeCheck = new QCheckBox(tr("Lossless Fast Trim (Without Re-encoding)"), card);
    m_trimReencodeCheck->setChecked(false);
    fineGrid->addWidget(m_trimReencodeCheck, 1, 0, 1, 4);

    layout->addLayout(fineGrid);

    // Connect Range Slider <--> SpinBoxes
    connect(m_rangeSlider, &VideoRangeSlider::rangeChanged, this, [this](double start, double end) {
        m_trimStartSpin->blockSignals(true);
        m_trimEndSpin->blockSignals(true);
        m_trimStartSpin->setValue(start);
        m_trimEndSpin->setValue(end);
        m_trimStartSpin->blockSignals(false);
        m_trimEndSpin->blockSignals(false);
        updateDurationLabels();
    });

    connect(m_rangeSlider, &VideoRangeSlider::seekRequested, this, [this](double pos) {
        if (m_player) {
            m_player->setPosition(static_cast<qint64>(pos * 1000.0));
        }
    });

    connect(m_trimStartSpin, &QDoubleSpinBox::valueChanged, this, [this](double val) {
        m_rangeSlider->setStartValue(val);
        updateDurationLabels();
    });

    connect(m_trimEndSpin, &QDoubleSpinBox::valueChanged, this, [this](double val) {
        m_rangeSlider->setEndValue(val);
        updateDurationLabels();
    });

    // 5. Action Row
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *btn = new QPushButton(tr("Videoyu Kes ve Kaydet"), card);
    btn->setObjectName("PrimaryButton");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumWidth(180);
    btn->setFixedHeight(36);
    connect(btn, &QPushButton::clicked, this, &VideoPage::executeTrim);
    btnRow->addWidget(btn);
    layout->addLayout(btnRow);

    return card;
}

void VideoPage::updatePlayerSize() {
    if (!m_playerContainer || !m_videoWidget) return;

    int parentW = m_panelStack ? m_panelStack->width() : 700;
    if (parentW <= 100) {
        parentW = width() > 400 ? (width() - 380) : 700;
    }
    int availableWidth = parentW - 48;
    if (availableWidth < 240) availableWidth = 240;

    double aspect = (m_videoNativeWidth > 0 && m_videoNativeHeight > 0)
        ? (static_cast<double>(m_videoNativeWidth) / static_cast<double>(m_videoNativeHeight))
        : (16.0 / 9.0);

    int targetW = 240;
    int targetH = 135;

    if (m_previewSize == VideoPreviewSize::Half) {
        targetW = (availableWidth * 50) / 100;
        if (targetW < 220) targetW = 220;
        targetH = static_cast<int>(targetW / aspect);
        if (targetH < 120) {
            targetH = 120;
            targetW = static_cast<int>(targetH * aspect);
        }
    } else if (m_previewSize == VideoPreviewSize::Full) {
        targetW = availableWidth;
        if (targetW < 220) targetW = 220;
        targetH = static_cast<int>(targetW / aspect);
        if (targetH < 120) {
            targetH = 120;
            targetW = static_cast<int>(targetH * aspect);
        }
    } else { // Fit (default)
        int availableHeight = 360;
        if (height() > 400) {
            availableHeight = std::max(220, height() - 380);
        }

        double widthRatio = static_cast<double>(availableWidth) / std::max(1, m_videoNativeWidth);
        double heightRatio = static_cast<double>(availableHeight) / std::max(1, m_videoNativeHeight);
        double scale = std::min(widthRatio, heightRatio);

        targetW = std::clamp(static_cast<int>(m_videoNativeWidth * scale), 220, availableWidth);
        targetH = static_cast<int>(targetW / aspect);

        if (targetH > availableHeight) {
            targetH = availableHeight;
            targetW = static_cast<int>(targetH * aspect);
        }
        if (targetH < 120) {
            targetH = 120;
            targetW = static_cast<int>(targetH * aspect);
        }
    }

    m_playerContainer->setFixedSize(targetW, targetH);
    m_videoWidget->setFixedSize(targetW, targetH);
}

void VideoPage::updateGifPlayerSize() {
    if (!m_gifPlayerContainer || !m_gifVideoWidget) return;

    int parentW = m_panelStack ? m_panelStack->width() : 700;
    if (parentW <= 100) {
        parentW = width() > 400 ? (width() - 380) : 700;
    }
    int availableWidth = parentW - 48;
    if (availableWidth < 240) availableWidth = 240;

    double aspect = (m_videoNativeWidth > 0 && m_videoNativeHeight > 0)
        ? (static_cast<double>(m_videoNativeWidth) / static_cast<double>(m_videoNativeHeight))
        : (16.0 / 9.0);

    int targetW = 240;
    int targetH = 135;

    if (m_gifPreviewSize == VideoPreviewSize::Half) {
        targetW = (availableWidth * 50) / 100;
        if (targetW < 220) targetW = 220;
        targetH = static_cast<int>(targetW / aspect);
        if (targetH < 120) {
            targetH = 120;
            targetW = static_cast<int>(targetH * aspect);
        }
    } else if (m_gifPreviewSize == VideoPreviewSize::Full) {
        targetW = availableWidth;
        if (targetW < 220) targetW = 220;
        targetH = static_cast<int>(targetW / aspect);
        if (targetH < 120) {
            targetH = 120;
            targetW = static_cast<int>(targetH * aspect);
        }
    } else { // Fit (default)
        int availableHeight = 360;
        if (height() > 400) {
            availableHeight = std::max(220, height() - 380);
        }

        double widthRatio = static_cast<double>(availableWidth) / std::max(1, m_videoNativeWidth);
        double heightRatio = static_cast<double>(availableHeight) / std::max(1, m_videoNativeHeight);
        double scale = std::min(widthRatio, heightRatio);

        targetW = std::clamp(static_cast<int>(m_videoNativeWidth * scale), 220, availableWidth);
        targetH = static_cast<int>(targetW / aspect);

        if (targetH > availableHeight) {
            targetH = availableHeight;
            targetW = static_cast<int>(targetH * aspect);
        }
        if (targetH < 120) {
            targetH = 120;
            targetW = static_cast<int>(targetH * aspect);
        }
    }

    m_gifPlayerContainer->setFixedSize(targetW, targetH);
    m_gifVideoWidget->setFixedSize(targetW, targetH);
}

void VideoPage::updateDurationLabels() {
    if (!m_rangeSlider || !m_selectedDurationLabel) return;
    double start = m_rangeSlider->startValue();
    double end = m_rangeSlider->endValue();
    double dur = std::max(0.0, end - start);
    m_selectedDurationLabel->setText(tr("Selected Duration: %1 sec").arg(QString::number(dur, 'f', 1)));
}

void VideoPage::updateGifDurationLabels() {
    if (!m_gifRangeSlider || !m_gifSelectedDurationLabel) return;
    double start = m_gifRangeSlider->startValue();
    double end = m_gifRangeSlider->endValue();
    double dur = std::max(0.0, end - start);
    m_gifSelectedDurationLabel->setText(tr("Selected Duration: %1 sec").arg(QString::number(dur, 'f', 1)));
}

void VideoPage::togglePlayPause() {
    if (!m_player) return;

    const bool playing = (m_player->playbackState() == QMediaPlayer::PlayingState);
    if (playing) {
        m_player->pause();
    } else {
        m_player->play();
    }
    // After the toggle the button offers the opposite action.
    setPlayPauseAppearance(!playing);
}

void VideoPage::setPlayPauseAppearance(bool playing) {
    const QString label = playing ? tr("Pause") : tr("Play");
    const IconType icon = playing ? IconType::Pause : IconType::Play;
    const QIcon themed = IconUtils::getIcon(icon, Theme::color(ColorToken::TextPrimary), 14);

    for (QPushButton* btn : { m_playPauseBtn, m_gifPlayPauseBtn }) {
        if (!btn) continue;
        btn->setText(label);
        btn->setIcon(themed);
        btn->setIconSize(QSize(14, 14));
        btn->setAccessibleName(label);
    }
}

void VideoPage::handlePositionChanged(qint64 positionMs) {
    double sec = positionMs / 1000.0;
    if (m_rangeSlider) {
        m_rangeSlider->setPlayheadPosition(sec);
    }
    if (m_gifRangeSlider) {
        m_gifRangeSlider->setPlayheadPosition(sec);
    }

    int curSec = static_cast<int>(sec);
    int totSec = static_cast<int>(m_totalDurationSec);
    QString timeStr = QString("%1:%2 / %3:%4")
        .arg(curSec / 60, 2, 10, QChar('0'))
        .arg(curSec % 60, 2, 10, QChar('0'))
        .arg(totSec / 60, 2, 10, QChar('0'))
        .arg(totSec % 60, 2, 10, QChar('0'));

    if (m_playbackTimeLabel) {
        m_playbackTimeLabel->setText(timeStr);
    }
    if (m_gifPlaybackTimeLabel) {
        m_gifPlaybackTimeLabel->setText(timeStr);
    }
}

void VideoPage::handleDurationChanged(qint64 durationMs) {
    if (durationMs > 0) {
        m_totalDurationSec = durationMs / 1000.0;
        if (m_rangeSlider) {
            m_rangeSlider->setRange(0.0, m_totalDurationSec);
            m_rangeSlider->setEndValue(m_totalDurationSec);
        }
        if (m_trimEndSpin) {
            m_trimEndSpin->setMaximum(m_totalDurationSec);
            m_trimEndSpin->setValue(m_totalDurationSec);
        }
        if (m_gifRangeSlider) {
            m_gifRangeSlider->setRange(0.0, m_totalDurationSec);
            m_gifRangeSlider->setStartValue(0.0);
            m_gifRangeSlider->setEndValue(std::min(5.0, m_totalDurationSec));
        }
        if (m_gifStartSpin) {
            m_gifStartSpin->setMaximum(m_totalDurationSec);
            m_gifStartSpin->setValue(0.0);
        }
        if (m_gifDurationSpin) {
            m_gifDurationSpin->setMaximum(m_totalDurationSec);
            m_gifDurationSpin->setValue(std::min(5.0, m_totalDurationSec));
        }
        updateDurationLabels();
        updateGifDurationLabels();
    }
}

void VideoPage::updateTabDescription(int id) {
    if (!m_tabDescriptionLabel) return;
    if (id == 0) {
        m_tabDescriptionLabel->setText(tr("Optimize and reduce video file size without quality loss."));
    } else if (id == 1) {
        m_tabDescriptionLabel->setText(tr("Create smooth and high quality animated GIFs from your video clips."));
    } else if (id == 2) {
        m_tabDescriptionLabel->setText(tr("Extract audio track from videos in MP3, WAV, or AAC format."));
    } else if (id == 3) {
        m_tabDescriptionLabel->setText(tr("Trim videos precisely via visual timeline."));
    }
}

void VideoPage::setMode(VideoTabMode mode) {
    int idx = static_cast<int>(mode);
    if (auto *btn = m_tabButtonGroup->button(idx)) {
        btn->setChecked(true);
    }
    m_panelStack->setCurrentIndex(idx);
    updateTabDescription(idx);
    if (m_videoCard) {
        m_videoCard->setFixedHeight(300);
    }

    // setMode() is also called from navigation shortcuts, where the tab-click
    // handler is not involved. Keep the selected preview sized correctly in
    // that path as well as after an interactive tab change.
    if (idx == static_cast<int>(VideoTabMode::ToGif)) {
        updateGifPlayerSize();
    } else if (idx == static_cast<int>(VideoTabMode::Trim)) {
        updatePlayerSize();
    }
}

void VideoPage::handleVideoDropped(const QStringList& files) {
    if (files.isEmpty()) return;

    m_loadedVideoPath = files.first();
    const QString inputPath = m_loadedVideoPath;
    const QFileInfo fi(inputPath);

    m_videoCardNameLabel->setText(fi.fileName());
    m_infoKey[RowDuration]->setText(tr("Duration"));
    m_infoKey[RowResolution]->setText(tr("Resolution"));
    m_infoKey[RowSize]->setText(tr("Size"));
    m_infoKey[RowFormat]->setText(tr("Format"));
    m_infoValue[RowDuration]->setText(tr("Analyzing..."));
    m_infoValue[RowResolution]->setText(QStringLiteral("-"));
    m_infoValue[RowSize]->setText(
        QString::number(fi.size() / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB"));
    m_infoValue[RowFormat]->setText(fi.suffix().toUpper());

    m_dropZone->hide();
    m_optionsContainer->show();
    m_optionsContainer->setEnabled(false);
    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_statusBanner->hideBanner();

    AsyncTask::run<Core::Result<Core::VideoBackend::VideoInfo>>(
        this,
        [inputPath](AsyncTask::CancelCheck) {
            return Core::VideoBackend::probeVideo(inputPath);
        },
        [this, inputPath](Core::Result<Core::VideoBackend::VideoInfo> probeRes) {
            if (m_loadedVideoPath != inputPath) {
                return;
            }

            m_progressBar->hide();
            m_progressBar->setRange(0, 100);
            m_optionsContainer->setEnabled(true);

            if (!probeRes.hasValue()) {
                m_loadedVideoPath.clear();
                m_optionsContainer->hide();
                m_dropZone->show();
                m_statusBanner->showBanner(
                    BannerType::Error,
                    QString::fromStdString(probeRes.error().toUserFriendlyMessage()));
                return;
            }

            applyVideoInfo(inputPath, probeRes.value());
        });
}

void VideoPage::applyVideoInfo(const QString& filePath,
                               const Core::VideoBackend::VideoInfo& info) {
    const QFileInfo fi(filePath);

    m_totalDurationSec = info.durationSeconds > 0.0 ? info.durationSeconds : 10.0;

    int durSec = static_cast<int>(m_totalDurationSec);
    QString durStr = QString("%1:%2")
        .arg(durSec / 60, 2, 10, QChar('0'))
        .arg(durSec % 60, 2, 10, QChar('0'));

    QString sizeStr = QString::number(fi.size() / (1024.0 * 1024.0), 'f', 1) + " MB";

    m_infoValue[RowDuration]->setText(durStr);
    m_infoValue[RowResolution]->setText(
        QStringLiteral("%1 x %2").arg(info.width > 0 ? info.width : 1920)
                                 .arg(info.height > 0 ? info.height : 1080));
    m_infoValue[RowSize]->setText(sizeStr);
    m_infoValue[RowFormat]->setText(fi.suffix().toUpper());

    m_videoNativeWidth = info.width > 0 ? info.width : 1920;
    m_videoNativeHeight = info.height > 0 ? info.height : 1080;
    updatePlayerSize();

    // Configure Trim Panel
    if (m_trimStartSpin) {
        m_trimStartSpin->setValue(0.0);
        m_trimStartSpin->setMaximum(m_totalDurationSec);
    }
    if (m_trimEndSpin) {
        m_trimEndSpin->setMaximum(m_totalDurationSec);
        m_trimEndSpin->setValue(m_totalDurationSec);
    }
    if (m_rangeSlider) {
        m_rangeSlider->setRange(0.0, m_totalDurationSec);
        m_rangeSlider->setStartValue(0.0);
        m_rangeSlider->setEndValue(m_totalDurationSec);
    }
    updateDurationLabels();

    // Configure GIF Panel
    if (m_gifStartSpin) {
        m_gifStartSpin->setValue(0.0);
        m_gifStartSpin->setMaximum(m_totalDurationSec);
    }
    if (m_gifDurationSpin) {
        m_gifDurationSpin->setMaximum(m_totalDurationSec);
        m_gifDurationSpin->setValue(std::min(5.0, m_totalDurationSec));
    }
    if (m_gifRangeSlider) {
        m_gifRangeSlider->setRange(0.0, m_totalDurationSec);
        m_gifRangeSlider->setStartValue(0.0);
        m_gifRangeSlider->setEndValue(std::min(5.0, m_totalDurationSec));
    }
    updateGifDurationLabels();
    updateGifPlayerSize();

    // Load into video player
    if (m_player) {
        m_player->setSource(QUrl::fromLocalFile(filePath));
    }

    if (m_videoCard) {
        m_videoCard->setFixedHeight(300);
    }

    m_statusBanner->hideBanner();
}

void VideoPage::executeCompress() {
    if (m_loadedVideoPath.isEmpty()) return;

    QString defaultDir = Services::SettingsService::instance().getDefaultOutputDir();
    QDir().mkpath(defaultDir);

    QString outPath = Services::FileService::instance().generateUniqueOutputPath(
        defaultDir + "/compressed_" + QFileInfo(m_loadedVideoPath).completeBaseName() + ".mp4");

    Core::VideoBackend::CompressOptions opts;
    opts.targetWidth = m_resCombo->currentData().toInt();
    opts.crf = m_qualitySlider ? m_qualitySlider->currentCrf() : 26;
    opts.muteAudio = m_muteAudioCheck->isChecked();

    const QString inputPath = m_loadedVideoPath;
    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_optionsContainer->setEnabled(false);
    AsyncTask::run<Core::Result<void>>(
        this,
        [inputPath, outPath, opts](AsyncTask::CancelCheck cancel) {
            return Core::VideoBackend::compressVideo(inputPath, outPath, opts, nullptr, std::move(cancel));
        },
        [this, outPath](Core::Result<void> res) {
            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            m_optionsContainer->setEnabled(true);
            if (res.hasValue()) {
                Services::SettingsService::instance().addRecentFile(outPath);
                m_resultScreen->showResult(outPath, tr("Video Compressed Successfully!"));
                m_pageStack->setCurrentIndex(1);
            } else {
                m_statusBanner->showBanner(BannerType::Error, QString::fromStdString(res.error().toUserFriendlyMessage()));
            }
        });
}

void VideoPage::executeGif() {
    if (m_loadedVideoPath.isEmpty()) return;

    QString defaultDir = Services::SettingsService::instance().getDefaultOutputDir();
    QDir().mkpath(defaultDir);

    QString outPath = Services::FileService::instance().generateUniqueOutputPath(
        defaultDir + "/clip_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".gif");

    Core::VideoBackend::GifOptions opts;
    opts.startTimeSec = m_gifStartSpin->value();
    opts.durationSec = m_gifDurationSpin->value();
    opts.width = m_gifWidthCombo->currentData().toInt();
    opts.fps = m_gifFpsCombo->currentData().toInt();

    const QString inputPath = m_loadedVideoPath;
    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_optionsContainer->setEnabled(false);
    AsyncTask::run<Core::Result<void>>(
        this,
        [inputPath, outPath, opts](AsyncTask::CancelCheck cancel) {
            return Core::VideoBackend::convertToGif(inputPath, outPath, opts, nullptr, std::move(cancel));
        },
        [this, outPath](Core::Result<void> res) {
            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            m_optionsContainer->setEnabled(true);
            if (res.hasValue()) {
                Services::SettingsService::instance().addRecentFile(outPath);
                m_resultScreen->showResult(outPath, tr("GIF Animation Created Successfully!"));
                m_pageStack->setCurrentIndex(1);
            } else {
                m_statusBanner->showBanner(BannerType::Error, QString::fromStdString(res.error().toUserFriendlyMessage()));
            }
        });
}

void VideoPage::executeAudio() {
    if (m_loadedVideoPath.isEmpty()) return;

    QString fmt = m_audioFormatCombo->currentData().toString();
    QString defaultDir = Services::SettingsService::instance().getDefaultOutputDir();
    QDir().mkpath(defaultDir);

    QString outPath = Services::FileService::instance().generateUniqueOutputPath(
        defaultDir + "/audio_" + QFileInfo(m_loadedVideoPath).completeBaseName() + "." + fmt);

    const QString inputPath = m_loadedVideoPath;
    const int bitrate = m_audioBitrateCombo->currentData().toInt();
    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_optionsContainer->setEnabled(false);
    AsyncTask::run<Core::Result<void>>(
        this,
        [inputPath, outPath, fmt, bitrate](AsyncTask::CancelCheck cancel) {
            return Core::VideoBackend::extractAudio(inputPath, outPath, fmt, bitrate, nullptr, std::move(cancel));
        },
        [this, outPath](Core::Result<void> res) {
            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            m_optionsContainer->setEnabled(true);
            if (res.hasValue()) {
                Services::SettingsService::instance().addRecentFile(outPath);
                m_resultScreen->showResult(outPath, tr("Audio File Extracted Successfully!"));
                m_pageStack->setCurrentIndex(1);
            } else {
                m_statusBanner->showBanner(BannerType::Error, QString::fromStdString(res.error().toUserFriendlyMessage()));
            }
        });
}

void VideoPage::executeTrim() {
    if (m_loadedVideoPath.isEmpty()) return;

    QString defaultDir = Services::SettingsService::instance().getDefaultOutputDir();
    QDir().mkpath(defaultDir);

    QString outPath = Services::FileService::instance().generateUniqueOutputPath(
        defaultDir + "/trimmed_" + QFileInfo(m_loadedVideoPath).fileName());

    Core::VideoBackend::TrimOptions opts;
    opts.startTimeSec = m_trimStartSpin->value();
    opts.endTimeSec = m_trimEndSpin->value();
    opts.reEncode = !m_trimReencodeCheck->isChecked();

    const QString inputPath = m_loadedVideoPath;
    m_progressBar->setRange(0, 0);
    m_progressBar->show();
    m_optionsContainer->setEnabled(false);
    AsyncTask::run<Core::Result<void>>(
        this,
        [inputPath, outPath, opts](AsyncTask::CancelCheck cancel) {
            return Core::VideoBackend::trimVideo(inputPath, outPath, opts, nullptr, std::move(cancel));
        },
        [this, outPath](Core::Result<void> res) {
            m_progressBar->setRange(0, 100);
            m_progressBar->hide();
            m_optionsContainer->setEnabled(true);
            if (res.hasValue()) {
                Services::SettingsService::instance().addRecentFile(outPath);
                m_resultScreen->showResult(outPath, tr("Video Trimmed Successfully!"));
                m_pageStack->setCurrentIndex(1);
            } else {
                m_statusBanner->showBanner(BannerType::Error, QString::fromStdString(res.error().toUserFriendlyMessage()));
            }
        });
}

void VideoPage::resetWorkspace() {
    if (m_player) {
        m_player->stop();
    }
    m_loadedVideoPath.clear();
    m_optionsContainer->hide();
    m_dropZone->show();
    m_pageStack->setCurrentIndex(0);
}

void VideoPage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void VideoPage::retranslateUi() {
    if (m_compressTabBtn) m_compressTabBtn->setText(tr("Compress Video"));
    if (m_gifTabBtn) m_gifTabBtn->setText(tr("Convert to GIF"));
    if (m_audioTabBtn) m_audioTabBtn->setText(tr("Extract Audio"));
    if (m_trimTabBtn) m_trimTabBtn->setText(tr("Trim Video"));

    if (m_trimZoomTitleLabel) m_trimZoomTitleLabel->setText(tr("Preview Size:"));
    if (m_trimFitBtn) m_trimFitBtn->setText(tr("Fit"));
    if (m_trimHalfBtn) m_trimHalfBtn->setText(tr("50%"));
    if (m_trimFullBtn) m_trimFullBtn->setText(tr("100%"));

    if (m_gifZoomTitleLabel) m_gifZoomTitleLabel->setText(tr("Preview Size:"));
    if (m_gifFitBtn) m_gifFitBtn->setText(tr("Fit"));
    if (m_gifHalfBtn) m_gifHalfBtn->setText(tr("50%"));
    if (m_gifFullBtn) m_gifFullBtn->setText(tr("100%"));

    updateTabDescription(m_panelStack->currentIndex());
    setMode(static_cast<VideoTabMode>(m_panelStack->currentIndex()));
}

} // namespace LocalTools::UI
