#include "convert_audio.h"
#include "../backends/audio_backend.h"
#include "../../services/file_service.h"
#include <QFileInfo>
#include <QDir>

namespace LocalTools::Core {

ConvertAudioOperation::ConvertAudioOperation() = default;

void ConvertAudioOperation::setInputFiles(const QStringList& files) {
    m_inputFiles = files;
}

void ConvertAudioOperation::setOutputDirectory(const QString& dir) {
    m_outputDir = dir;
}

void ConvertAudioOperation::setTargetFormat(const QString& format) {
    m_targetFormat = format;
}

void ConvertAudioOperation::setTargetBitrate(int kbps) {
    m_targetBitrateKbps = kbps;
}

void ConvertAudioOperation::setStripMetadata(bool strip) {
    m_stripMetadata = strip;
}



Result<void> ConvertAudioOperation::execute() {
    if (m_inputFiles.isEmpty()) {
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "Input file was not selected.");
    }
    
    if (!AudioBackend::isFFmpegAvailable()) {
        return Result<void>::fail(PdfErrorCategory::UnsupportedFeature, "FFmpeg not found. Please install FFmpeg.");
    }

    int totalFiles = m_inputFiles.size();
    int completedFiles = 0;

    for (const QString& file : m_inputFiles) {
        if (isCancelRequested()) {
            return Result<void>::fail(PdfErrorCategory::Cancelled, "Operation was cancelled.");
        }

        QFileInfo fi(file);
        QString outName = fi.completeBaseName() + "_converted." + m_targetFormat;
        QString outPath = Services::FileService::instance().generateUniqueOutputPath(
            QDir(m_outputDir.isEmpty() ? fi.absolutePath() : m_outputDir).absoluteFilePath(outName));

        AudioBackend::ConvertOptions options;
        options.targetFormat = m_targetFormat;
        options.bitrateKbps = m_targetBitrateKbps;
        options.stripMetadata = m_stripMetadata;

        auto stepProgress = [&](int fileProgress) {
            double base = (completedFiles * 100.0) / totalFiles;
            double chunk = (fileProgress / 100.0) * (100.0 / totalFiles);
            reportProgress(static_cast<int>(base + chunk));
        };

        auto res = AudioBackend::convertFormat(
            file, outPath, options, stepProgress, [this]() { return isCancelRequested(); });
        if (!res.hasValue()) {
            return res; // Fail fast on error
        }

        completedFiles++;
    }

    if (isCancelRequested()) {
        return Result<void>::fail(PdfErrorCategory::Cancelled, "Operation was cancelled.");
    }
    reportProgress(100);
    return Result<void>::ok();
}

} // namespace LocalTools::Core
