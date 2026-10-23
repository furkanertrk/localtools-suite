#include "audio_backend.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "ffmpeg_process.h"
#include "video_backend.h"

namespace LocalTools::Core {
namespace {

Result<void> validateInputAndOutput(const QString& inputPath, const QString& outputPath) {
    const QFileInfo inputInfo(inputPath);
    if (!inputInfo.exists() || !inputInfo.isFile()) {
        return Result<void>::fail(PdfErrorCategory::FileNotFound, "Input audio file was not found.");
    }
    if (outputPath.trimmed().isEmpty()) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied, "Output audio path is empty.");
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
                                         "Audio file was not found.");
    }

    const auto result = FFmpegProcess::run(
        VideoBackend::getFFprobePath(),
        {"-v", "error",
         "-show_entries", "format=duration:stream=codec_type,codec_name,sample_rate,channels,bit_rate",
         "-of", "json",
         inputPath});
    if (result.hasError()) return Result<QJsonObject>::fail(result.error());

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(result.value(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return Result<QJsonObject>::fail(PdfErrorCategory::UnsupportedFeature,
                                         "The selected audio file could not be inspected.");
    }
    return Result<QJsonObject>::ok(document.object());
}

Result<void> appendAudioCodecArguments(QStringList& args, const QString& format, int bitrateKbps) {
    const QString normalizedFormat = format.toLower();
    if (normalizedFormat == "mp3") {
        args << "-c:a" << "libmp3lame" << "-b:a" << QString::number(bitrateKbps) + "k";
    } else if (normalizedFormat == "wav") {
        args << "-c:a" << "pcm_s16le";
    } else if (normalizedFormat == "flac") {
        args << "-c:a" << "flac";
    } else if (normalizedFormat == "ogg") {
        args << "-c:a" << "libvorbis" << "-b:a" << QString::number(bitrateKbps) + "k";
    } else if (normalizedFormat == "m4a") {
        args << "-c:a" << "aac" << "-b:a" << QString::number(bitrateKbps) + "k";
    } else {
        return Result<void>::fail(PdfErrorCategory::UnsupportedFeature,
                                  "Selected audio output format is unsupported.");
    }
    return Result<void>::ok();
}

} // namespace

bool AudioBackend::isFFmpegAvailable() {
    return VideoBackend::isFFmpegAvailable();
}

QString AudioBackend::getFFmpegPath() {
    return VideoBackend::getFFmpegPath();
}

Result<AudioBackend::AudioInfo> AudioBackend::probeAudio(const QString& inputPath) {
    const auto probeResult = probeMediaJson(inputPath);
    if (probeResult.hasError()) return Result<AudioInfo>::fail(probeResult.error());

    AudioInfo info;
    info.fileSizeBytes = QFileInfo(inputPath).size();
    info.durationSeconds = probeResult.value()
        .value("format").toObject()
        .value("duration").toString().toDouble();

    bool foundAudio = false;
    const QJsonArray streams = probeResult.value().value("streams").toArray();
    for (const QJsonValue& value : streams) {
        const QJsonObject stream = value.toObject();
        if (stream.value("codec_type").toString() != "audio") continue;
        foundAudio = true;
        info.audioCodec = stream.value("codec_name").toString();
        info.sampleRateHz = stream.value("sample_rate").toString().toInt();
        info.channels = stream.value("channels").toInt();
        info.bitrateKbps = stream.value("bit_rate").toString().toInt() / 1000;
        break;
    }

    if (!foundAudio || info.sampleRateHz <= 0 || info.channels <= 0) {
        return Result<AudioInfo>::fail(PdfErrorCategory::UnsupportedFeature,
                                       "The selected file does not contain a supported audio stream.");
    }
    return Result<AudioInfo>::ok(std::move(info));
}

Result<void> AudioBackend::convertFormat(const QString& inputPath,
                                         const QString& outputPath,
                                         const ConvertOptions& options,
                                         std::function<void(int)> progressCallback,
                                         std::function<bool()> cancelRequested) {
    const auto validation = validateInputAndOutput(inputPath, outputPath);
    if (validation.hasError()) return validation;

    QStringList args{"-nostdin", "-hide_banner", "-y", "-i", inputPath};
    if (options.stripMetadata) args << "-map_metadata" << "-1";
    const auto codecResult = appendAudioCodecArguments(args, options.targetFormat, options.bitrateKbps);
    if (codecResult.hasError()) return codecResult;
    args << outputPath;

    if (progressCallback) progressCallback(5);
    const auto result = FFmpegProcess::run(getFFmpegPath(), args, std::move(cancelRequested));
    if (result.hasError()) return Result<void>::fail(result.error());
    if (progressCallback) progressCallback(100);
    return Result<void>::ok();
}

Result<void> AudioBackend::trimAudio(const QString& inputPath,
                                     const QString& outputPath,
                                     const TrimOptions& options,
                                     std::function<void(int)> progressCallback,
                                     std::function<bool()> cancelRequested) {
    const auto validation = validateInputAndOutput(inputPath, outputPath);
    if (validation.hasError()) return validation;
    if (options.startTimeSec < 0.0 || options.endTimeSec <= options.startTimeSec) {
        return Result<void>::fail(PdfErrorCategory::InvalidPageRange,
                                  "Audio trim range is invalid.");
    }

    const double duration = options.endTimeSec - options.startTimeSec;
    QStringList args{"-nostdin", "-hide_banner", "-y",
                     "-ss", QString::number(options.startTimeSec, 'f', 3),
                     "-i", inputPath,
                     "-t", QString::number(duration, 'f', 3)};
    if (options.reEncode) {
        const auto codecResult = appendAudioCodecArguments(
            args, QFileInfo(outputPath).suffix(), 192);
        if (codecResult.hasError()) return codecResult;
    } else {
        args << "-c:a" << "copy";
    }
    args << outputPath;

    if (progressCallback) progressCallback(5);
    const auto result = FFmpegProcess::run(getFFmpegPath(), args, std::move(cancelRequested));
    if (result.hasError()) return Result<void>::fail(result.error());
    if (progressCallback) progressCallback(100);
    return Result<void>::ok();
}

} // namespace LocalTools::Core

