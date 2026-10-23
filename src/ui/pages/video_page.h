#ifndef LOCALTOOLS_UI_PAGES_VIDEO_PAGE_H
#define LOCALTOOLS_UI_PAGES_VIDEO_PAGE_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QButtonGroup>
#include <QFrame>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>

#include "../common/drop_zone.h"
#include "../common/status_banner.h"
#include "../common/icon_utils.h"
#include "../common/pill_slider.h"
#include "../common/video_range_slider.h"
#include "result_screen.h"
#include "../../core/backends/video_backend.h"

namespace LocalTools::UI {

enum class VideoTabMode {
    Compress = 0,
    ToGif = 1,
    ExtractAudio = 2,
    Trim = 3
};

enum class VideoPreviewSize {
    Fit = 0,
    Half = 1,
    Full = 2
};

class VideoPage : public QWidget {
    Q_OBJECT

public:
    explicit VideoPage(QWidget *parent = nullptr);
    ~VideoPage() override;

    void setMode(VideoTabMode mode);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void handleVideoDropped(const QStringList& files);
    void executeCompress();
    void executeGif();
    void executeAudio();
    void executeTrim();
    void resetWorkspace();
    void togglePlayPause();
    void handlePositionChanged(qint64 positionMs);
    void handleDurationChanged(qint64 durationMs);

private:
    void setupUi();
    void retranslateUi();
    QWidget* createTabHeader();
    QWidget* createVideoCard();
    QWidget* createCompressPanel();
    QWidget* createGifPanel();
    QWidget* createAudioPanel();
    QWidget* createTrimPanel();
    void updateDurationLabels();

    // Tab buttons & Subtitle
    QPushButton* m_compressTabBtn{nullptr};
    QPushButton* m_gifTabBtn{nullptr};
    QPushButton* m_audioTabBtn{nullptr};
    QPushButton* m_trimTabBtn{nullptr};
    QButtonGroup* m_tabButtonGroup{nullptr};
    QLabel* m_tabDescriptionLabel{nullptr};
    void updateTabDescription(int id);

    // Sub-panels
    QStackedWidget* m_panelStack{nullptr};
    QWidget* m_compressPanel{nullptr};
    QWidget* m_gifPanel{nullptr};
    QWidget* m_audioPanel{nullptr};
    QWidget* m_trimPanel{nullptr};

    // File State & Video Card
    QString m_loadedVideoPath;
    DropZoneWidget* m_dropZone{nullptr};
    QWidget* m_optionsContainer{nullptr};
    QFrame* m_videoCard{nullptr};
    QLabel* m_videoCardNameLabel{nullptr};
    QPushButton* m_videoCardRemoveBtn{nullptr};

    // "Loaded video" info rows: duration / resolution / size / format.
    // A widget grid rather than an HTML table so the icons are themed vectors
    // and the text follows the palette.
    enum InfoRow { RowDuration = 0, RowResolution, RowSize, RowFormat, RowCount };
    QLabel* m_infoIcon[RowCount]{};
    QLabel* m_infoKey[RowCount]{};
    QLabel* m_infoValue[RowCount]{};
    QLabel* m_videoCardBadge{nullptr};
    /** Re-tints the info icons and the loaded-video badge for the current theme. */
    void refreshVideoCardIcons();
    /** Sets both play/pause buttons to offer pause (playing) or play (paused). */
    void setPlayPauseAppearance(bool playing);

    // Compress controls
    QComboBox* m_resCombo{nullptr};
    PillSliderWidget* m_qualitySlider{nullptr};
    QCheckBox* m_muteAudioCheck{nullptr};

    // GIF controls & Preview
    QFrame* m_gifPlayerContainer{nullptr};
    QVideoWidget* m_gifVideoWidget{nullptr};
    QPushButton* m_gifPlayPauseBtn{nullptr};
    QLabel* m_gifPlaybackTimeLabel{nullptr};
    QLabel* m_gifZoomTitleLabel{nullptr};
    QPushButton* m_gifFitBtn{nullptr};
    QPushButton* m_gifHalfBtn{nullptr};
    QPushButton* m_gifFullBtn{nullptr};
    QButtonGroup* m_gifZoomGroup{nullptr};
    VideoPreviewSize m_gifPreviewSize{VideoPreviewSize::Fit};
    VideoRangeSlider* m_gifRangeSlider{nullptr};
    QLabel* m_gifSelectedDurationLabel{nullptr};
    QDoubleSpinBox* m_gifStartSpin{nullptr};
    QDoubleSpinBox* m_gifDurationSpin{nullptr};
    QComboBox* m_gifWidthCombo{nullptr};
    QComboBox* m_gifFpsCombo{nullptr};
    void updateGifPlayerSize();
    void updateGifDurationLabels();
    void applyVideoInfo(const QString& filePath, const Core::VideoBackend::VideoInfo& info);

    // Audio controls
    QComboBox* m_audioFormatCombo{nullptr};
    QComboBox* m_audioBitrateCombo{nullptr};

    // Trim & Player controls
    QMediaPlayer* m_player{nullptr};
    QAudioOutput* m_audioOutput{nullptr};
    QFrame* m_playerContainer{nullptr};
    QVideoWidget* m_videoWidget{nullptr};
    QPushButton* m_playPauseBtn{nullptr};
    QLabel* m_playbackTimeLabel{nullptr};
    QLabel* m_trimZoomTitleLabel{nullptr};
    QPushButton* m_trimFitBtn{nullptr};
    QPushButton* m_trimHalfBtn{nullptr};
    QPushButton* m_trimFullBtn{nullptr};
    QButtonGroup* m_trimZoomGroup{nullptr};
    VideoPreviewSize m_previewSize{VideoPreviewSize::Fit};
    VideoRangeSlider* m_rangeSlider{nullptr};
    QLabel* m_selectedDurationLabel{nullptr};
    QDoubleSpinBox* m_trimStartSpin{nullptr};
    QDoubleSpinBox* m_trimEndSpin{nullptr};
    QCheckBox* m_trimReencodeCheck{nullptr};
    double m_totalDurationSec{0.0};
    int m_videoNativeWidth{1920};
    int m_videoNativeHeight{1080};
    void updatePlayerSize();

    // Status Banner & Stack
    StatusBannerWidget* m_statusBanner{nullptr};
    QProgressBar* m_progressBar{nullptr};
    ResultScreenWidget* m_resultScreen{nullptr};
    QStackedWidget* m_pageStack{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_VIDEO_PAGE_H
