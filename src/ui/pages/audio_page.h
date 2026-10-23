#ifndef LOCALTOOLS_UI_PAGES_AUDIO_PAGE_H
#define LOCALTOOLS_UI_PAGES_AUDIO_PAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QProgressBar>
#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QFrame>
#include <QLabel>
#include <QPushButton>

#include "../common/drop_zone.h"
#include "../common/status_banner.h"
#include "../common/pill_slider.h"
#include "../common/video_range_slider.h"
#include "result_screen.h"
#include "../../core/backends/audio_backend.h"

namespace LocalTools::UI {

class AudioPage : public QWidget {
    Q_OBJECT

public:
    explicit AudioPage(QWidget *parent = nullptr);
    ~AudioPage() override;

    enum class Mode {
        Convert = 0,
        Compress = 1,
        Trim = 2
    };

    void setMode(Mode mode);
    void resetPage();

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void onFilesDropped(const QStringList& paths);
    void onProcessClicked();
    void togglePlayPause();
    void handlePositionChanged(qint64 positionMs);
    void handleDurationChanged(qint64 durationMs);

private:
    void setupUi();
    void retranslateUi();
    void createConvertPanel();
    void createCompressPanel();
    void createTrimPanel();
    void updateTrimDurationLabels();
    void updateTrimDurationRange();
    void updateTrimPlaybackButton(bool playing);
    void updateProcessButtonState();
    void startAudioProbe(const QString& filePath);
    
    void processConvert();
    void processCompress();
    void processTrim();

    Mode m_currentMode{Mode::Convert};
    QStringList m_inputFiles;
    double m_totalDurationSec{10.0};
    QString m_lastOutPath;
    QString m_successTitle;

    // Stacked Pages (0: Workspace, 1: Result Screen)
    QStackedWidget* m_pageStack{nullptr};
    ResultScreenWidget* m_resultScreen{nullptr};
    
    QVBoxLayout* m_mainLayout{nullptr};
    
    // Header & Tabs
    QButtonGroup* m_tabGroup{nullptr};
    QWidget* m_tabContainer{nullptr};
    QLabel* m_subtitleLabel{nullptr};
    
    // Drop zone
    DropZoneWidget* m_dropZone{nullptr};
    
    // Workspace Card Container (Shown when files are loaded)
    QFrame* m_workspaceContainer{nullptr};
    QLabel* m_selectedFilesLabel{nullptr};
    QPushButton* m_changeFilesBtn{nullptr};
    
    // Dynamic settings panels
    QStackedWidget* m_panelStack{nullptr};
    QWidget* m_convertPanel{nullptr};
    QWidget* m_compressPanel{nullptr};
    QWidget* m_trimPanel{nullptr};
    
    // Convert settings
    QComboBox* m_convertFormatCombo{nullptr};
    QComboBox* m_convertBitrateCombo{nullptr};
    
    // Compress settings
    PillSliderWidget* m_compressBitrateSlider{nullptr};
    
    // Trim settings (Player + Timeline + SpinBoxes)
    QMediaPlayer* m_player{nullptr};
    QAudioOutput* m_audioOutput{nullptr};
    QPushButton* m_trimPlayPauseBtn{nullptr};
    QLabel* m_trimPlaybackTimeLabel{nullptr};
    QLabel* m_trimSelectedDurationLabel{nullptr};
    VideoRangeSlider* m_trimRangeSlider{nullptr};
    QDoubleSpinBox* m_trimStartSpin{nullptr};
    QDoubleSpinBox* m_trimEndSpin{nullptr};
    
    // Actions & Progress
    QPushButton* m_processButton{nullptr};
    QProgressBar* m_progressBar{nullptr};
    StatusBannerWidget* m_statusBanner{nullptr};
    
    // State
    bool m_isProcessing{false};
    bool m_isProbing{false};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_AUDIO_PAGE_H
