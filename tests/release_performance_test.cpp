#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

#include "core/backend/pdfium_backend.h"
#include "core/backends/file_utils_backend.h"
#include "core/backends/image_backend.h"
#include "core/operations/merge_pdf.h"

namespace {

bool check(bool condition, const QString& message) {
    if (!condition) std::cerr << "[FAIL] " << message.toStdString() << '\n';
    return condition;
}

quint64 peakWorkingSetBytes() {
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return static_cast<quint64>(counters.PeakWorkingSetSize);
    }
#endif
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    if (argc < 3) {
        std::cerr << "[FAIL] Usage: release_performance_test <test.pdf> <result.json>\n";
        return 1;
    }

    const QString fixturePath = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    const QString resultPath = QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath();
    QTemporaryDir workspace;
    if (!check(workspace.isValid(), QStringLiteral("temporary workspace is unavailable"))) return 1;

    const QString largePdfPath = workspace.filePath(QStringLiteral("synthetic_100MiB.pdf"));
    if (!check(QFile::copy(fixturePath, largePdfPath), QStringLiteral("could not copy PDF fixture"))) return 1;
    QFile largePdf(largePdfPath);
    if (!check(largePdf.open(QIODevice::ReadWrite), QStringLiteral("could not open synthetic large PDF"))) return 1;
    constexpr qint64 largeBoundary = 100LL * 1024LL * 1024LL + 4096LL;
    if (!check(largePdf.resize(largeBoundary), QStringLiteral("could not grow synthetic PDF"))) return 1;
    largePdf.close();

    QElapsedTimer timer;
    timer.start();
    LocalTools::Core::PdfiumBackend largeBackend;
    const bool largePdfOpened = largeBackend.openDocument(largePdfPath.toStdString());
    const qint64 largePdfOpenMs = timer.elapsed();
    if (!check(largePdfOpened && largeBackend.getPageCount() > 0,
               QStringLiteral("100 MiB PDF boundary fixture was not accepted"))) return 1;
    largeBackend.closeDocument();

    timer.restart();
    auto hashes = LocalTools::Core::FileUtilsBackend::computeHashes(largePdfPath);
    const qint64 hashMs = timer.elapsed();
    if (!check(hashes.hasValue() && hashes.value().fileSize == largeBoundary,
               QStringLiteral("100 MiB file hashing failed"))) return 1;
    if (!check(hashMs < 30000, QStringLiteral("100 MiB hashing exceeded 30 seconds"))) return 1;

    std::vector<std::string> mergeInputs(100, fixturePath.toStdString());
    const QString manyPagePath = workspace.filePath(QStringLiteral("many_pages.pdf"));
    timer.restart();
    auto mergeResult = LocalTools::Core::Operations::MergePdfOperation::execute(
        mergeInputs, manyPagePath.toStdString());
    const qint64 mergeMs = timer.elapsed();
    if (!check(mergeResult.hasValue(), QStringLiteral("100-input PDF merge failed"))) return 1;
    LocalTools::Core::PdfiumBackend manyPageBackend;
    if (!check(manyPageBackend.openDocument(manyPagePath.toStdString()),
               QStringLiteral("many-page output could not be reopened"))) return 1;
    const int manyPageCount = manyPageBackend.getPageCount();
    manyPageBackend.closeDocument();
    if (!check(manyPageCount >= 200, QStringLiteral("many-page output is unexpectedly small"))) return 1;
    if (!check(mergeMs < 30000, QStringLiteral("100-input PDF merge exceeded 30 seconds"))) return 1;

    const QString largeImagePath = workspace.filePath(QStringLiteral("large_24mp.png"));
    QImage largeImage(6000, 4000, QImage::Format_ARGB32);
    if (!check(!largeImage.isNull(), QStringLiteral("24 MP image allocation failed"))) return 1;
    largeImage.fill(QColor(52, 152, 219));
    if (!check(largeImage.save(largeImagePath), QStringLiteral("24 MP image save failed"))) return 1;
    largeImage = QImage();

    LocalTools::Core::ImageBackend::ResizeOptions resizeOptions;
    resizeOptions.width = 1500;
    resizeOptions.height = 1000;
    resizeOptions.keepAspectRatio = true;
    const QString resizedPath = workspace.filePath(QStringLiteral("large_resized.png"));
    timer.restart();
    auto resizeResult = LocalTools::Core::ImageBackend::resize(
        largeImagePath, resizedPath, resizeOptions);
    const qint64 imageResizeMs = timer.elapsed();
    if (!check(resizeResult.hasValue() && QFileInfo::exists(resizedPath),
               QStringLiteral("24 MP resize failed"))) return 1;
    if (!check(imageResizeMs < 30000, QStringLiteral("24 MP resize exceeded 30 seconds"))) return 1;

    const quint64 peakBytes = peakWorkingSetBytes();
    if (!check(peakBytes == 0 || peakBytes < 1536ULL * 1024ULL * 1024ULL,
               QStringLiteral("peak working set exceeded 1.5 GiB"))) return 1;

    QJsonObject result{
        {QStringLiteral("syntheticLargePdfBytes"), static_cast<double>(largeBoundary)},
        {QStringLiteral("syntheticLargePdfOpenMs"), largePdfOpenMs},
        {QStringLiteral("hash100MiBMs"), hashMs},
        {QStringLiteral("mergedInputCount"), 100},
        {QStringLiteral("mergedPageCount"), manyPageCount},
        {QStringLiteral("mergeMs"), mergeMs},
        {QStringLiteral("largeImageWidth"), 6000},
        {QStringLiteral("largeImageHeight"), 4000},
        {QStringLiteral("largeImageResizeMs"), imageResizeMs},
        {QStringLiteral("peakWorkingSetBytes"), static_cast<double>(peakBytes)}
    };
    QFile resultFile(resultPath);
    if (!check(resultFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
               QStringLiteral("performance result could not be written"))) return 1;
    resultFile.write(QJsonDocument(result).toJson(QJsonDocument::Indented));
    resultFile.close();

    std::cout << "[PASS] 100 MiB PDF/hash boundary, 100-input merge ("
              << manyPageCount << " pages), and 24 MP image resize completed; peak="
              << peakBytes << " bytes.\n";
    return 0;
}
