#ifndef LOCALTOOLS_CORE_BACKENDS_VIDEO_BACKEND_H
#define LOCALTOOLS_CORE_BACKENDS_VIDEO_BACKEND_H

#include <QString>
#include <functional>
#include "../result.h"

namespace LocalTools::Core {

/**
 * Backend for Video Utilities:
 * - FFmpeg detection & probing
 * - Video compression (CRF & Target Resolution)
 * - Video to GIF conversion
 * - Audio extraction (MP3 / WAV / Stream Copy)
 * - Lossless video trimming (start/end timestamps)
 */
class VideoBackend {
public:
    struct VideoInfo {
        double durationSeconds{0.0};
        int width{0};
        int height{0};
        QString videoCodec;
        QString audioCodec;
        qint64 fileSizeBytes{0};
        double frameRate{30.0};
    };

    struct CompressOptions {
        int targetWidth{1280};     // 1920, 1280, 854, 640
        int crf{26};               // 18-35 (lower = better quality)
        QString preset{"medium"};  // ultrafast, fast, medium, slow
        bool muteAudio{false};
    };

    struct GifOptions {
        int width{480};            // Output GIF width (aspect ratio maintained)
        int fps{12};               // Frame rate
        double startTimeSec{0.0};
        double durationSec{5.0};
    };

    struct TrimOptions {
        double startTimeSec{0.0};
        double endTimeSec{0.0};
        bool reEncode{false};      // Fast stream copy vs frame-accurate re-encode
    };

    // System inspection
    static bool isFFmpegAvailable();
    static QString getFFmpegPath();
    static QString getFFprobePath();

    // Operations
    static Result<VideoInfo> probeVideo(const QString& inputPath);
    static Result<void> compressVideo(const QString& inputPath,
                                      const QString& outputPath,
                                      const CompressOptions& options,
                                      std::function<void(int progressPercent)> progressCallback = nullptr,
                                      std::function<bool()> cancelRequested = nullptr);
    static Result<void> convertToGif(const QString& inputPath,
                                     const QString& outputPath,
                                     const GifOptions& options,
                                     std::function<void(int progressPercent)> progressCallback = nullptr,
                                     std::function<bool()> cancelRequested = nullptr);
    static Result<void> extractAudio(const QString& inputPath,
                                     const QString& outputPath,
                                     const QString& audioFormat = "mp3",
                                     int bitrateKbps = 192,
                                     std::function<void(int progressPercent)> progressCallback = nullptr,
                                     std::function<bool()> cancelRequested = nullptr);
    static Result<void> trimVideo(const QString& inputPath,
                                  const QString& outputPath,
                                  const TrimOptions& options,
                                  std::function<void(int progressPercent)> progressCallback = nullptr,
                                  std::function<bool()> cancelRequested = nullptr);
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_BACKENDS_VIDEO_BACKEND_H
