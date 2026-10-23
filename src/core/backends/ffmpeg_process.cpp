#include "ffmpeg_process.h"

#include <QProcess>

namespace LocalTools::Core {

Result<QByteArray> FFmpegProcess::run(const QString& executable,
                                      const QStringList& arguments,
                                      CancelCallback cancelRequested,
                                      OutputCallback outputReceived) {
    if (executable.isEmpty()) {
        return Result<QByteArray>::fail(PdfErrorCategory::UnsupportedFeature,
                                        "Bundled FFmpeg runtime is unavailable.");
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(executable, arguments, QIODevice::ReadOnly);
    if (!process.waitForStarted(5000)) {
        return Result<QByteArray>::fail(PdfErrorCategory::BackendError,
                                        "Could not start the bundled FFmpeg process.");
    }

    QByteArray standardOutput;
    QByteArray diagnosticTail;
    constexpr qsizetype kMaxDiagnosticBytes = 64 * 1024;

    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(100);
        standardOutput += process.readAllStandardOutput();
        const QByteArray errorChunk = process.readAllStandardError();
        if (!errorChunk.isEmpty()) {
            diagnosticTail += errorChunk;
            if (diagnosticTail.size() > kMaxDiagnosticBytes) {
                diagnosticTail = diagnosticTail.right(kMaxDiagnosticBytes);
            }
            if (outputReceived) outputReceived(errorChunk);
        }

        if (cancelRequested && cancelRequested()) {
            process.terminate();
            if (!process.waitForFinished(1500)) {
                process.kill();
                process.waitForFinished(1500);
            }
            return Result<QByteArray>::fail(PdfErrorCategory::Cancelled,
                                            "Operation was cancelled.");
        }
    }

    standardOutput += process.readAllStandardOutput();
    diagnosticTail += process.readAllStandardError();
    if (diagnosticTail.size() > kMaxDiagnosticBytes) {
        diagnosticTail = diagnosticTail.right(kMaxDiagnosticBytes);
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return Result<QByteArray>::fail(PdfErrorCategory::BackendError,
                                        "FFmpeg could not process the selected file.",
                                        diagnosticTail.toStdString());
    }

    return Result<QByteArray>::ok(std::move(standardOutput));
}

} // namespace LocalTools::Core
