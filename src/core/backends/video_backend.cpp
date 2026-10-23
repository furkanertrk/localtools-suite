#include "video_backend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "ffmpeg_process.h"

namespace LocalTools::Core {
namespace {

Result<void> validateInputAndOutput(const QString& inputPath, const QString& outputPath) {
    const QFileInfo inputInfo(inputPath);
    if (!inputInfo.exists() || !inputInfo.isFile()) {
        return Result<void>::fail(PdfErrorCategory::FileNotFound, "Input media file was not found.");
    }
    if (outputPath.trimmed().isEmpty()) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied, "Output media path is empty.");
    }

    const QFileInfo outputInfo(outputPath);
    const QString inputComparable = inputInfo.canonicalFilePath().isEmpty()
        ? inputInfo.absoluteFilePath()
        : inputInfo.canonicalFilePath();
    const QString outputComparable = outputInfo.canonicalFilePath().isEmpty()
        ? outputInfo.absoluteFilePath()
        : outputInfo.canonicalFilePath();
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif
    if (QString::compare(QDir::cleanPath(inputComparable),
                         QDir::cleanPath(outputComparable),
                         pathCase) == 0) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                  "The output file cannot replace the source file.");
    }
    return Result<void>::ok();
}

Result<QJsonObject> probeMediaJson(const QString& inputPath) {
    const QFileInfo inputInfo(inputPath);
    if (!inputInfo.exists() || !inputInfo.isFile()) {
        return Result<QJsonObject>::fail(PdfErrorCategory::FileNotFound,
                                         "Media file was not found.");
    }

    const QString ffprobe = VideoBackend::getFFprobePath();
    if (ffprobe.isEmpty()) {
        return Result<QJsonObject>::fail(PdfErrorCategory::UnsupportedFeature,
                                         "Bundled FFprobe runtime is unavailable.");
    }

    const auto result = FFmpegProcess::run(
        ffprobe,
        {"-v", "error",
         "-show_entries", "format=duration:stream=codec_type,codec_name,width,height,r_frame_rate,sample_rate,channels,bit_rate",
         "-of", "json",
         inputPath});
    if (result.hasError()) {
        return Result<QJsonObject>::fail(result.error());
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(result.value(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return Result<QJsonObject>::fail(PdfErrorCategory::InvalidPdf,
                                         "The selected media file could not be inspected.");
    }
    return Result<QJsonObject>::ok(document.object());
}

int mpeg4QualityFromCrf(int crf) {
    return qBound(2, 3 + ((qBound(18, crf, 35) - 18) * 20 / 17), 31);
}

} // namespace

bool VideoBackend::isFFmpegAvailable() {
    return !getFFmpegPath().isEmpty() && !getFFprobePath().isEmpty();
}

QString VideoBackend::getFFmpegPath() {
    const QString path = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("ffmpeg.exe");
    return QFileInfo::exists(path) ? path : QString{};
}

QString VideoBackend::getFFprobePath() {
    const QString path = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("ffprobe.exe");
    return QFileInfo::exists(path) ? path : QString{};
}

Result<VideoBackend::VideoInfo> VideoBackend::probeVideo(const QString& inputPath) {
    const auto probeResult = probeMediaJson(inputPath);
    if (probeResult.hasError()) {
        return Result<VideoInfo>::fail(probeResult.error());
    }

    VideoInfo info;
    info.fileSizeBytes = QFileInfo(inputPath).size();
    info.durationSeconds = probeResult.value()
        .value("format").toObject()
        .value("duration").toString().toDouble();

    bool foundVideo = false;
    const QJsonArray streams = probeResult.value().value("streams").toArray();
    for (const QJsonValue& value : streams) {
        const QJsonObject stream = value.toObject();
        const QString type = stream.value("codec_type").toString();
        if (type == "video" && !foundVideo) {
            foundVideo = true;
            info.videoCodec = stream.value("codec_name").toString();
            info.width = stream.value("width").toInt();
            info.height = stream.value("height").toInt();
            const QStringList rateParts = stream.value("r_frame_rate").toString().split('/');
            if (rateParts.size() == 2 && rateParts[1].toDouble() > 0.0) {
                info.frameRate = rateParts[0].toDouble() / rateParts[1].toDouble();
            }
        } else if (type == "audio" && info.audioCodec.isEmpty()) {
            info.audioCodec = stream.value("codec_name").toString();
        }
    }

    if (!foundVideo || info.width <= 0 || info.height <= 0 || info.durationSeconds <= 0.0) {
        return Result<VideoInfo>::fail(PdfErrorCategory::UnsupportedFeature,
                                       "The selected file does not contain a supported video stream.");
    }
    return Result<VideoInfo>::ok(std::move(info));
}

Result<void> VideoBackend::compressVideo(const QString& inputPath,
                                         const QString& outputPath,
                                         const CompressOptions& options,
                                         std::function<void(int)> progressCallback,
                                         std::function<bool()> cancelRequested) {
    const auto validation = validateInputAndOutput(inputPath, outputPath);
    if (validation.hasError()) return validation;

    const QString ffmpeg = getFFmpegPath();
    QStringList args{"-nostdin", "-hide_banner", "-y", "-i", inputPath};
    const QString scaleFilter = QStringLiteral("scale='min(%1,iw)':-2").arg(qMax(2, options.targetWidth));
    args << "-vf" << scaleFilter
         << "-c:v" << "mpeg4"
         << "-q:v" << QString::number(mpeg4QualityFromCrf(options.crf))
         << "-movflags" << "+faststart";
    if (options.muteAudio) {
        args << "-an";
    } else {
        args << "-c:a" << "aac" << "-b:a" << "128k";
    }
    args << outputPath;

    if (progressCallback) progressCallback(5);
    const auto result = FFmpegProcess::run(ffmpeg, args, std::move(cancelRequested));
    if (result.hasError()) return Result<void>::fail(result.error());
    if (progressCallback) progressCallback(100);
    return Result<void>::ok();
}

Result<void> VideoBackend::convertToGif(const QString& inputPath,
                                        const QString& outputPath,
                                        const GifOptions& options,
                                        std::function<void(int)> progressCallback,
                                        std::function<bool()> cancelRequested) {
    const auto validation = validateInputAndOutput(inputPath, outputPath);
    if (validation.hasError()) return validation;

    QStringList args{"-nostdin", "-hide_banner", "-y"};
    if (options.startTimeSec > 0.0) {
        args << "-ss" << QString::number(options.startTimeSec, 'f', 3);
    }
    if (options.durationSec > 0.0) {
        args << "-t" << QString::number(options.durationSec, 'f', 3);
    }
    args << "-i" << inputPath;
    const QString filter =
        QStringLiteral("fps=%1,scale=%2:-1:flags=lanczos,split[s0][s1];"
                       "[s0]palettegen[p];[s1][p]paletteuse")
            .arg(qBound(1, options.fps, 60))
            .arg(qMax(16, options.width));
    args << "-vf" << filter << outputPath;

    if (progressCallback) progressCallback(5);
    const auto result = FFmpegProcess::run(getFFmpegPath(), args, std::move(cancelRequested));
    if (result.hasError()) return Result<void>::fail(result.error());
    if (progressCallback) progressCallback(100);
    return Result<void>::ok();
}

Result<void> VideoBackend::extractAudio(const QString& inputPath,
                                        const QString& outputPath,
                                        const QString& audioFormat,
                                        int bitrateKbps,
                                        std::function<void(int)> progressCallback,
                                        std::function<bool()> cancelRequested) {
    const auto validation = validateInputAndOutput(inputPath, outputPath);
    if (validation.hasError()) return validation;

    QStringList args{"-nostdin", "-hide_banner", "-y", "-i", inputPath, "-vn"};
    const QString format = audioFormat.toLower();
    if (format == "mp3") {
        args << "-c:a" << "libmp3lame" << "-b:a" << QString::number(bitrateKbps) + "k";
    } else if (format == "wav") {
        args << "-c:a" << "pcm_s16le";
    } else {
        return Result<void>::fail(PdfErrorCategory::UnsupportedFeature,
                                  "Only MP3 and WAV extraction are supported.");
    }
    args << outputPath;

    if (progressCallback) progressCallback(5);
    const auto result = FFmpegProcess::run(getFFmpegPath(), args, std::move(cancelRequested));
    if (result.hasError()) return Result<void>::fail(result.error());
    if (progressCallback) progressCallback(100);
    return Result<void>::ok();
}

Result<void> VideoBackend::trimVideo(const QString& inputPath,
                                     const QString& outputPath,
                                     const TrimOptions& options,
                                     std::function<void(int)> progressCallback,
                                     std::function<bool()> cancelRequested) {
    const auto validation = validateInputAndOutput(inputPath, outputPath);
    if (validation.hasError()) return validation;
    if (options.startTimeSec < 0.0 || options.endTimeSec <= options.startTimeSec) {
        return Result<void>::fail(PdfErrorCategory::InvalidPageRange,
                                  "Video trim range is invalid.");
    }

    const double duration = options.endTimeSec - options.startTimeSec;
    QStringList args{"-nostdin", "-hide_banner", "-y",
                     "-ss", QString::number(options.startTimeSec, 'f', 3),
                     "-i", inputPath,
                     "-t", QString::number(duration, 'f', 3)};
    if (options.reEncode) {
        args << "-c:v" << "mpeg4" << "-q:v" << "4"
             << "-c:a" << "aac" << "-movflags" << "+faststart";
    } else {
        args << "-c" << "copy";
    }
    args << outputPath;

    if (progressCallback) progressCallback(5);
    const auto result = FFmpegProcess::run(getFFmpegPath(), args, std::move(cancelRequested));
    if (result.hasError()) return Result<void>::fail(result.error());
    if (progressCallback) progressCallback(100);
    return Result<void>::ok();
}

} // namespace LocalTools::Core
