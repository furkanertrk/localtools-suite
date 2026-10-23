#include "compress_audio.h"
#include "../backends/audio_backend.h"
#include "../../services/file_service.h"
#include <QFileInfo>
#include <QDir>

namespace LocalTools::Core {

CompressAudioOperation::CompressAudioOperation() = default;

void CompressAudioOperation::setInputFiles(const QStringList& files) {
    m_inputFiles = files;
}

void CompressAudioOperation::setOutputDirectory(const QString& dir) {
    m_outputDir = dir;
}

void CompressAudioOperation::setTargetBitrate(int kbps) {
    m_targetBitrateKbps = kbps;
}



Result<void> CompressAudioOperation::execute() {
    if (m_inputFiles.isEmpty()) {
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "Input file was not selected.");
    }
    
    if (!AudioBackend::isFFmpegAvailable()) {
        return Result<void>::fail(PdfErrorCategory::UnsupportedFeature, "FFmpeg was not found.");
    }

    int totalFiles = m_inputFiles.size();
    int completedFiles = 0;

    for (const QString& file : m_inputFiles) {
        if (isCancelRequested()) {
            return Result<void>::fail(PdfErrorCategory::Cancelled, "Operation was cancelled.");
        }

        QFileInfo fi(file);
        QString outName = fi.completeBaseName() + "_compressed." + fi.suffix();
        QString outPath = Services::FileService::instance().generateUniqueOutputPath(
            QDir(m_outputDir.isEmpty() ? fi.absolutePath() : m_outputDir).absoluteFilePath(outName));

        // Under the hood, compression is just conversion to the same format with a lower bitrate
        AudioBackend::ConvertOptions options;
        options.targetFormat = fi.suffix();
        options.bitrateKbps = m_targetBitrateKbps;
        options.stripMetadata = false;

        auto stepProgress = [&](int fileProgress) {
            double base = (completedFiles * 100.0) / totalFiles;
            double chunk = (fileProgress / 100.0) * (100.0 / totalFiles);
            reportProgress(static_cast<int>(base + chunk));
        };

        auto res = AudioBackend::convertFormat(
            file, outPath, options, stepProgress, [this]() { return isCancelRequested(); });
        if (!res.hasValue()) {
            return res;
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
