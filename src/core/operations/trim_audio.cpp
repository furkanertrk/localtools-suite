#include "trim_audio.h"
#include "../backends/audio_backend.h"
#include "../../services/file_service.h"
#include <QFileInfo>
#include <QDir>

namespace LocalTools::Core {

TrimAudioOperation::TrimAudioOperation() = default;

void TrimAudioOperation::setInputFile(const QString& file) {
    m_inputFile = file;
}

void TrimAudioOperation::setOutputDirectory(const QString& dir) {
    m_outputDir = dir;
}

void TrimAudioOperation::setTrimRange(double startSec, double endSec) {
    m_startSec = startSec;
    m_endSec = endSec;
}



Result<void> TrimAudioOperation::execute() {
    if (m_inputFile.isEmpty()) {
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "Input file was not selected.");
    }
    
    if (m_endSec <= m_startSec) {
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "End time must be greater than start time.");
    }
    
    if (!AudioBackend::isFFmpegAvailable()) {
        return Result<void>::fail(PdfErrorCategory::UnsupportedFeature, "FFmpeg was not found.");
    }

    if (isCancelRequested()) {
        return Result<void>::fail(PdfErrorCategory::Cancelled, "Operation was cancelled.");
    }

    QFileInfo fi(m_inputFile);
    QString outName = fi.completeBaseName() + "_trimmed." + fi.suffix();
    QString outPath = Services::FileService::instance().generateUniqueOutputPath(
        QDir(m_outputDir.isEmpty() ? fi.absolutePath() : m_outputDir).absoluteFilePath(outName));

    AudioBackend::TrimOptions options;
    options.startTimeSec = m_startSec;
    options.endTimeSec = m_endSec;
    options.reEncode = true;

    auto stepProgress = [this](int fileProgress) {
        reportProgress(fileProgress);
    };

    auto res = AudioBackend::trimAudio(
        m_inputFile, outPath, options, stepProgress, [this]() { return isCancelRequested(); });
    return res;
}

} // namespace LocalTools::Core
