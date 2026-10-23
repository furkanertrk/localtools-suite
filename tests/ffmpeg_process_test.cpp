#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QThread>

#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

#include "core/backends/ffmpeg_process.h"

namespace {

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

#ifdef Q_OS_WIN
DWORD currentHandleCount() {
    DWORD count = 0;
    return GetProcessHandleCount(GetCurrentProcess(), &count) ? count : 0;
}

int childFfmpegProcessCount() {
    const DWORD currentPid = GetCurrentProcessId();
    int count = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return -1;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ParentProcessID == currentPid
                && _wcsicmp(entry.szExeFile, L"ffmpeg.exe") == 0) {
                ++count;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return count;
}
#endif

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    if (!check(argc >= 2, "verified FFmpeg path must be supplied")) return 1;

    const QString ffmpegPath = QString::fromLocal8Bit(argv[1]);
    if (!check(QFileInfo::exists(ffmpegPath), "verified FFmpeg executable must exist")) return 1;

#ifdef Q_OS_WIN
    const DWORD baselineHandles = currentHandleCount();
#endif

    for (int iteration = 0; iteration < 20; ++iteration) {
        auto version = LocalTools::Core::FFmpegProcess::run(
            ffmpegPath, {QStringLiteral("-nostdin"), QStringLiteral("-version")});
        if (!check(version.hasValue(), "repeated FFmpeg probe must succeed")) return 1;
        if (!check(version.value().contains("ffmpeg version"),
                   "FFmpeg version output must be captured")) return 1;
    }

    QElapsedTimer cancellationTimer;
    cancellationTimer.start();
    auto cancelled = LocalTools::Core::FFmpegProcess::run(
        ffmpegPath,
        {QStringLiteral("-nostdin"), QStringLiteral("-hide_banner"),
         QStringLiteral("-re"), QStringLiteral("-f"), QStringLiteral("lavfi"),
         QStringLiteral("-i"), QStringLiteral("testsrc=size=320x240:rate=30"),
         QStringLiteral("-t"), QStringLiteral("30"),
         QStringLiteral("-f"), QStringLiteral("null"), QStringLiteral("-")},
        [&cancellationTimer]() { return cancellationTimer.elapsed() >= 150; });
    if (!check(cancelled.hasError(), "cancelled FFmpeg run must report an error result")) return 1;
    if (!check(cancelled.error().category == LocalTools::Core::PdfErrorCategory::Cancelled,
               "cancelled FFmpeg run must use the Cancelled category")) return 1;
    if (!check(cancellationTimer.elapsed() < 2500,
               "FFmpeg cancellation must complete within the two-second budget")) return 1;

    QThread::msleep(200);
#ifdef Q_OS_WIN
    if (!check(childFfmpegProcessCount() == 0,
               "no child ffmpeg.exe may remain after cancellation")) return 1;
    const DWORD finalHandles = currentHandleCount();
    if (!check(finalHandles <= baselineHandles + 8,
               "twenty FFmpeg runs must not continuously leak process handles")) return 1;
#endif

    std::cout << "[PASS] FFmpeg repeated-run, cancellation, orphan-process and handle checks passed.\n";
    return 0;
}
