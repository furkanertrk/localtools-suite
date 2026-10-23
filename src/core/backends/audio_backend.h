#ifndef LOCALTOOLS_CORE_BACKENDS_AUDIO_BACKEND_H
#define LOCALTOOLS_CORE_BACKENDS_AUDIO_BACKEND_H

#include <QString>
#include <functional>
#include "../result.h"

namespace LocalTools::Core {

/**
 * Backend for Audio Utilities (V3):
 * - FFmpeg detection & probing
 * - Audio format conversion (MP3, WAV, FLAC, OGG, M4A)
 * - Audio compression (Bitrate reduction)
 * - Audio trimming
 * - Metadata stripping
 */
class AudioBackend {
public:
    struct AudioInfo {
        double durationSeconds{0.0};
        QString audioCodec;
        qint64 fileSizeBytes{0};
        int bitrateKbps{0};
        int sampleRateHz{0};
        int channels{0};
    };

    struct ConvertOptions {
        QString targetFormat{"mp3"}; // mp3, wav, flac, ogg, m4a
        int bitrateKbps{192};        // 64, 128, 192, 256, 320
        bool stripMetadata{false};
    };

    struct TrimOptions {
        double startTimeSec{0.0};
        double endTimeSec{0.0};
        bool reEncode{true}; // True by default for precise trimming
    };

    // Use VideoBackend's FFmpeg path logic implicitly, but expose helper here for clarity
    static bool isFFmpegAvailable();
    static QString getFFmpegPath();

    // Operations
    static Result<AudioInfo> probeAudio(const QString& inputPath);
    
    static Result<void> convertFormat(const QString& inputPath,
                                      const QString& outputPath,
                                      const ConvertOptions& options,
                                      std::function<void(int progressPercent)> progressCallback = nullptr,
                                      std::function<bool()> cancelRequested = nullptr);
                                      
    static Result<void> trimAudio(const QString& inputPath,
                                  const QString& outputPath,
                                  const TrimOptions& options,
                                  std::function<void(int progressPercent)> progressCallback = nullptr,
                                  std::function<bool()> cancelRequested = nullptr);
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_BACKENDS_AUDIO_BACKEND_H
