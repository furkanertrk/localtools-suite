#include <QGuiApplication>
#include <QImage>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QProcess>
#include <QEventLoop>
#include <QThread>
#include <QTimer>
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

// Keep every test assertion active in Release builds. The standard assert()
// macro is compiled out when NDEBUG is defined, which previously allowed a
// failing Release test to print "passed" and return zero.
#ifdef assert
#undef assert
#endif
#define assert(condition)                                                        \
    do {                                                                         \
        if (!(condition)) {                                                      \
            std::cerr << "[FAIL] " << #condition << " at " << __FILE__ << ':'  \
                      << __LINE__ << std::endl;                                  \
            return 1;                                                            \
        }                                                                        \
    } while (false)

#include "../third_party/qrcodegen/qrcodegen.hpp"
#include "../src/core/backend/pdfium_backend.h"
#include "../src/core/backends/image_backend.h"
#include "../src/core/backends/document_backend.h"
#include "../src/core/backends/file_utils_backend.h"
#include "../src/core/backends/quick_tools_backend.h"
#include "../src/core/backends/video_backend.h"
#include "../src/core/backends/audio_backend.h"
#include "../src/core/operations/watermark_pdf.h"
#include "../src/core/tool_registry.h"
#include "../src/core/tool_operation.h"
#include "../src/core/operations/merge_pdf.h"
#include "../src/core/operations/split_pdf.h"
#include "../src/core/operations/extract_pages.h"
#include "../src/core/operations/delete_pages.h"
#include "../src/core/operations/reorder_pages.h"
#include "../src/core/operations/rotate_pages.h"
#include "../src/core/operations/images_to_pdf.h"
#include "../src/core/operations/pdf_to_images.h"
#include "../src/core/operations/pdf_to_text.h"
#include "../src/core/operations/compress_pdf.h"
#include "../src/services/localization_service.h"
#include "../src/services/settings_service.h"
#include "../src/workers/pdf_worker.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    std::cout << "=== LocalTools V2 - Automated Unit & Integration Tests ===" << std::endl;

    std::string testPath = "test.pdf";
    if (argc > 1) {
        testPath = argv[1];
    }

    QFileInfo checkFile(QString::fromStdString(testPath));
    if (!checkFile.exists()) {
        testPath = "../test.pdf";
        checkFile = QFileInfo(QString::fromStdString(testPath));
    }
    if (!checkFile.exists()) {
        std::cerr << "[FAIL] Test file not found: " << testPath << std::endl;
        return 1;
    }
    testPath = checkFile.absoluteFilePath().toStdString();
    std::cout << "[INFO] Using test file: " << testPath << std::endl << std::flush;

    QTemporaryDir testWorkspace;
    assert(testWorkspace.isValid());
    assert(QDir::setCurrent(testWorkspace.path()));

    // Measure the actual PdfWorker execution context. This guards against a
    // deceptively asynchronous QThread wiring that invokes process() back on
    // the application thread.
    {
        QThread workerThread;
        auto* worker = new LocalTools::Workers::PdfWorker();
        worker->prepareMerge({QString::fromStdString(testPath)},
                             testWorkspace.filePath("worker_thread_probe.pdf"));
        worker->moveToThread(&workerThread);

        QThread* observedExecutionThread = nullptr;
        bool workerFinished = false;
        bool workerSucceeded = false;
        QEventLoop workerLoop;
        QTimer workerDeadline;
        workerDeadline.setSingleShot(true);
        workerDeadline.setInterval(10000);

        QObject::connect(&workerThread, &QThread::started,
                         worker, &LocalTools::Workers::PdfWorker::process);
        QObject::connect(worker, &LocalTools::Workers::PdfWorker::progressChanged,
                         worker, [&](int) {
            observedExecutionThread = QThread::currentThread();
        }, Qt::DirectConnection);
        QObject::connect(worker, &LocalTools::Workers::PdfWorker::finished,
                         &app, [&](const LocalTools::Core::Result<void>& result) {
            workerFinished = true;
            workerSucceeded = result.hasValue();
            workerThread.quit();
            workerLoop.quit();
        });
        QObject::connect(&workerDeadline, &QTimer::timeout,
                         &workerLoop, &QEventLoop::quit);
        QObject::connect(&workerThread, &QThread::finished,
                         worker, &QObject::deleteLater);

        workerThread.start();
        workerDeadline.start();
        workerLoop.exec();
        workerThread.quit();
        workerThread.wait();

        assert(workerFinished);
        assert(workerSucceeded);
        assert(observedExecutionThread == &workerThread);
        assert(observedExecutionThread != app.thread());
        std::cout << "  - PdfWorker executed on its assigned worker thread." << std::endl;
    }

    // 1. PDFium Open & Render Smoke Test
    LocalTools::Core::PdfiumBackend backend;
    std::cout << "[Test 1] Opening test.pdf..." << std::endl;
    bool opened = backend.openDocument(testPath);
    assert(opened);
    int totalPages = backend.getPageCount();
    std::cout << "Total pages in test.pdf: " << totalPages << std::endl;
    assert(totalPages >= 2);
    backend.closeDocument();

    // 2. Unit Test: MergePdfOperation
    std::cout << "[Test 2] Testing MergePdfOperation..." << std::endl;
    std::vector<std::string> mergeInputs = {testPath, testPath};
    std::string mergedOutput = "test_merged_output.pdf";
    auto mergeRes = LocalTools::Core::Operations::MergePdfOperation::execute(mergeInputs, mergedOutput);
    assert(mergeRes.hasValue());

    LocalTools::Core::PdfiumBackend mergedBackend;
    assert(mergedBackend.openDocument(mergedOutput));
    assert(mergedBackend.getPageCount() == totalPages * 2);
    std::cout << "Merge test passed! Merged page count: " << mergedBackend.getPageCount() << std::endl;
    mergedBackend.closeDocument();

    // 3. Unit Test: ExtractPagesOperation
    std::cout << "[Test 3] Testing ExtractPagesOperation..." << std::endl;
    std::string extractedOutput = "test_extracted_output.pdf";
    auto extractRes = LocalTools::Core::Operations::ExtractPagesOperation::execute(testPath, {0}, extractedOutput);
    assert(extractRes.hasValue());

    LocalTools::Core::PdfiumBackend extractedBackend;
    assert(extractedBackend.openDocument(extractedOutput));
    assert(extractedBackend.getPageCount() == 1);
    std::cout << "Extract test passed! Extracted page count: " << extractedBackend.getPageCount() << std::endl;

    // 4. Unit Test: DeletePagesOperation
    std::cout << "[Test 4] Testing DeletePagesOperation..." << std::endl;
    std::string deleteOutput = "test_deleted_output.pdf";
    auto deleteRes = LocalTools::Core::Operations::DeletePagesOperation::execute(testPath, {1}, deleteOutput);
    assert(deleteRes.hasValue());

    LocalTools::Core::PdfiumBackend deleteBackend;
    assert(deleteBackend.openDocument(deleteOutput));
    assert(deleteBackend.getPageCount() == totalPages - 1);
    std::cout << "Delete test passed! Remaining page count: " << deleteBackend.getPageCount() << std::endl;

    // 5. Unit Test: RotatePagesOperation
    std::cout << "[Test 5] Testing RotatePagesOperation..." << std::endl;
    std::string rotateOutput = "test_rotated_output.pdf";
    auto rotateRes = LocalTools::Core::Operations::RotatePagesOperation::execute(testPath, {{0, 90}}, rotateOutput);
    assert(rotateRes.hasValue());

    LocalTools::Core::PdfiumBackend rotateBackend;
    assert(rotateBackend.openDocument(rotateOutput));
    assert(rotateBackend.getPageRotation(0) == 90);
    std::cout << "Rotate test passed! Rotated page count: " << rotateBackend.getPageCount() << std::endl;

    // 6. Unit Test: ImagesToPdfOperation
    std::cout << "[Test 6] Testing ImagesToPdfOperation..." << std::endl;
    std::string sampleImgPath = "test_sample_image.png";
    QImage sampleImg(200, 200, QImage::Format_RGB32);
    sampleImg.fill(Qt::blue);
    assert(sampleImg.save(QString::fromStdString(sampleImgPath)));
    std::string sampleImgPath2 = "test_sample_image_2.png";
    QImage sampleImg2(240, 160, QImage::Format_RGB32);
    sampleImg2.fill(Qt::green);
    assert(sampleImg2.save(QString::fromStdString(sampleImgPath2)));

    std::string imgPdfOutput = "test_images_to_pdf.pdf";
    auto imgPdfRes = LocalTools::Core::Operations::ImagesToPdfOperation::execute(
        {sampleImgPath, sampleImgPath2}, imgPdfOutput);
    assert(imgPdfRes.hasValue());

    LocalTools::Core::PdfiumBackend imgPdfBackend;
    assert(imgPdfBackend.openDocument(imgPdfOutput));
    assert(imgPdfBackend.getPageCount() == 2);
    std::cout << "Images -> PDF test passed! Page count: " << imgPdfBackend.getPageCount() << std::endl;

    // 7. Unit Test: PdfToImagesOperation
    std::cout << "[Test 7] Testing PdfToImagesOperation..." << std::endl;
    std::string imgOutDir = "test_output_images";
    auto pdfImgRes = LocalTools::Core::Operations::PdfToImagesOperation::execute(testPath, imgOutDir, "png", 150);
    assert(pdfImgRes.hasValue());
    QDir testOutDir(QString::fromStdString(imgOutDir));
    assert(testOutDir.exists());
    assert(testOutDir.entryList(QStringList() << "*.png", QDir::Files).size() == totalPages);
    std::cout << "PDF -> Images test passed! Rendered images count: " << testOutDir.entryList(QStringList() << "*.png", QDir::Files).size() << std::endl;

    // 8. Unit Test: PdfToTextOperation
    std::cout << "[Test 8] Testing PdfToTextOperation..." << std::endl;
    std::string textOutput = "test_extracted_text.txt";
    auto textRes = LocalTools::Core::Operations::PdfToTextOperation::execute(testPath, textOutput);
    assert(textRes.hasValue());
    QFile txtFile(QString::fromStdString(textOutput));
    assert(txtFile.exists() && txtFile.size() > 0);
    std::cout << "PDF -> Text test passed! Output text file size: " << txtFile.size() << " bytes" << std::endl;

    // 9. Unit Test: Smart Hybrid CompressPdfOperation
    std::cout << "[Test 9] Testing CompressPdfOperation..." << std::endl;
    std::string textCompOutput = "test_text_compressed.pdf";
    auto textCompRes = LocalTools::Core::Operations::CompressPdfOperation::execute(
        testPath, textCompOutput, LocalTools::Core::Operations::CompressionLevel::Medium
    );
    assert(textCompRes.hasValue());
    LocalTools::Core::PdfiumBackend compressedBackend;
    assert(compressedBackend.openDocument(textCompOutput));
    assert(compressedBackend.getPageCount() == totalPages);
    compressedBackend.closeDocument();
    std::cout << "Compress test passed! Compression succeeded." << std::endl;

    // 10. Unit Test: Metadata Inspection & Saving
    std::cout << "[Test 10] Testing Metadata Read & Write..." << std::endl;
    std::string metaOutput = "test_metadata_output.pdf";
    auto metaRes = LocalTools::Core::PdfiumBackend::saveWithMetadata(
        testPath, metaOutput, "Test Title", "Test Author", "Test Subject", "Test Keywords", "LocalTools Creator", "LocalTools Producer"
    );
    assert(metaRes.hasValue());
    LocalTools::Core::PdfiumBackend metaBackend;
    assert(metaBackend.openDocument(metaOutput));
    auto metaRead = metaBackend.getMetadata();
    assert(metaRead.title == "Test Title");
    assert(metaRead.author == "Test Author");
    metaBackend.closeDocument();
    std::cout << "Metadata test passed! Saved title: " << metaRead.title.toStdString() << std::endl;

    // 11. V2 ToolRegistry Search & Categorization Tests
    std::cout << "[Test 11] Testing V2 ToolRegistry..." << std::endl;
    auto& registry = LocalTools::Core::ToolRegistry::instance();
    auto allTools = registry.allTools();
    assert(!allTools.empty());
    std::cout << "  - Total registered tools: " << allTools.size() << std::endl;

    const QStringList expectedAvailableToolIds{
        "pdf.merge", "pdf.split", "pdf.extract", "pdf.delete", "pdf.reorder", "pdf.rotate",
        "pdf.images-to-pdf", "pdf.pdf-to-images", "pdf.pdf-to-text", "pdf.compress", "pdf.metadata",
        "image.convert", "image.compress", "document.id-copy", "document.passport-photo", "file.hash",
        "video.compress", "video.to-gif", "video.extract-audio", "video.trim",
        "audio.convert", "audio.compress", "audio.trim",
        "quick.qrcode", "quick.qrread", "quick.metadata"
    };
    QStringList actualAvailableToolIds;
    for (const auto& tool : allTools) {
        if (tool.isAvailable) actualAvailableToolIds.push_back(tool.id);
    }
    actualAvailableToolIds.sort();
    QStringList sortedExpectedAvailableToolIds = expectedAvailableToolIds;
    sortedExpectedAvailableToolIds.sort();
    assert(actualAvailableToolIds == sortedExpectedAvailableToolIds);

    // Category query test
    auto pdfTools = registry.toolsByCategory("PDF");
    assert(!pdfTools.empty());
    std::cout << "  - PDF category tool count: " << pdfTools.size() << std::endl;

    // Search scoring test
    auto searchMerge = registry.search("merge");
    assert(!searchMerge.empty());
    assert(searchMerge[0]->id == "pdf.merge");
    std::cout << "  - Search 'merge' returned: " << searchMerge[0]->name.toStdString() << " (OK)" << std::endl;

    auto searchCompress = registry.search("sıkıştır");
    assert(searchCompress.size() >= 2); // PDF and Image compress
    std::cout << "  - Search 'sıkıştır' returned " << searchCompress.size() << " tools (OK)" << std::endl;

    // Favorites test
    registry.setFavorite("pdf.merge", true);
    assert(registry.isFavorite("pdf.merge"));
    auto favs = registry.favorites();
    assert(!favs.empty());
    registry.setFavorite("pdf.merge", false);
    assert(!registry.isFavorite("pdf.merge"));
    std::cout << "  - ToolRegistry favorites test passed!" << std::endl;

    // 12. V2 ImageBackend Zero-Dependency Tests (Phase V2.1)
    std::cout << "[Test 12] Testing V2 ImageBackend Engine (5 Core Formats & Operations)..." << std::endl;
    QString testPngPath = "test_image_engine.png";
    QImage testImg(400, 300, QImage::Format_ARGB32);
    testImg.fill(QColor(100, 150, 200));
    assert(testImg.save(testPngPath));

    // Convert format PNG -> JPG
    QString testJpgPath = "test_image_converted.jpg";
    auto convRes = LocalTools::Core::ImageBackend::convertFormat(testPngPath, testJpgPath, "jpg", 90);
    assert(convRes.hasValue());
    assert(QFileInfo::exists(testJpgPath));
    std::cout << "  - Format conversion (PNG -> JPG) passed!" << std::endl;

    // Convert format JPG -> BMP
    QString testBmpPath = "test_image_converted.bmp";
    auto convBmpRes = LocalTools::Core::ImageBackend::convertFormat(testJpgPath, testBmpPath, "bmp");
    assert(convBmpRes.hasValue());
    assert(QFileInfo::exists(testBmpPath));
    std::cout << "  - Format conversion (JPG -> BMP) passed!" << std::endl;

    // Convert format BMP -> PNG
    QString testPngBackPath = "test_image_converted_back.png";
    auto convPngRes = LocalTools::Core::ImageBackend::convertFormat(testBmpPath, testPngBackPath, "png");
    assert(convPngRes.hasValue());
    assert(QFileInfo::exists(testPngBackPath));
    std::cout << "  - Format conversion (BMP -> PNG) passed!" << std::endl;

    // Test WEBP if available
    auto writeFmts = LocalTools::Core::ImageBackend::supportedWriteFormats();
    if (writeFmts.contains("webp")) {
        QString testWebpPath = "test_image_converted.webp";
        auto convWebpRes = LocalTools::Core::ImageBackend::convertFormat(testJpgPath, testWebpPath, "webp", 85);
        assert(convWebpRes.hasValue());
        assert(QFileInfo::exists(testWebpPath));
        std::cout << "  - Format conversion (JPG -> WEBP) passed!" << std::endl;
    }

    // Test TIFF if available
    if (writeFmts.contains("tiff") || writeFmts.contains("tif")) {
        QString testTiffPath = "test_image_converted.tiff";
        auto convTiffRes = LocalTools::Core::ImageBackend::convertFormat(testBmpPath, testTiffPath, "tiff");
        assert(convTiffRes.hasValue());
        assert(QFileInfo::exists(testTiffPath));
        std::cout << "  - Format conversion (BMP -> TIFF) passed!" << std::endl;
    }

    // Image Resize (Aspect ratio preserved)
    QString testResizePath = "test_image_resized.png";
    LocalTools::Core::ImageBackend::ResizeOptions resizeOpts;
    resizeOpts.width = 200;
    resizeOpts.height = 150;
    resizeOpts.keepAspectRatio = true;
    auto resizeRes = LocalTools::Core::ImageBackend::resize(testPngPath, testResizePath, resizeOpts);
    assert(resizeRes.hasValue());
    auto infoRes = LocalTools::Core::ImageBackend::getInfo(testResizePath);
    assert(infoRes.hasValue());
    assert(infoRes.value().width == 200 && infoRes.value().height == 150);
    std::cout << "  - Image resize test passed! Result: " << infoRes.value().width << "x" << infoRes.value().height << " px" << std::endl;

    // Image Resize (Percentage scale: 50%)
    QString testScalePath = "test_image_scale_50.png";
    LocalTools::Core::ImageBackend::ResizeOptions scaleOpts;
    scaleOpts.scalePercent = 50.0;
    auto scaleRes = LocalTools::Core::ImageBackend::resize(testPngPath, testScalePath, scaleOpts);
    assert(scaleRes.hasValue());
    auto scaleInfo = LocalTools::Core::ImageBackend::getInfo(testScalePath);
    assert(scaleInfo.hasValue() && scaleInfo.value().width == 200 && scaleInfo.value().height == 150);
    std::cout << "  - Image percentage resize (%50) test passed!" << std::endl;

    // Image Crop (Explicit bounding box)
    QString testCropPath = "test_image_cropped.png";
    auto cropRes = LocalTools::Core::ImageBackend::crop(testPngPath, testCropPath, QRect(50, 50, 100, 100));
    assert(cropRes.hasValue());
    auto cropInfo = LocalTools::Core::ImageBackend::getInfo(testCropPath);
    assert(cropInfo.hasValue());
    assert(cropInfo.value().width == 100 && cropInfo.value().height == 100);
    std::cout << "  - Image crop test passed! Result: " << cropInfo.value().width << "x" << cropInfo.value().height << " px" << std::endl;

    // Image Crop (Aspect Ratio: 1:1 Square)
    QString testAspectCropPath = "test_image_aspect_1_1.png";
    auto aspectCropRes = LocalTools::Core::ImageBackend::cropAspectRatio(testPngPath, testAspectCropPath, 1.0, 1.0);
    assert(aspectCropRes.hasValue());
    auto aspectInfo = LocalTools::Core::ImageBackend::getInfo(testAspectCropPath);
    assert(aspectInfo.hasValue() && aspectInfo.value().width == 300 && aspectInfo.value().height == 300);
    std::cout << "  - Image aspect ratio crop (1:1 -> 300x300 px centered) passed!" << std::endl;

    // Image Rotate
    QString testRotatePath = "test_image_rotated.png";
    auto rotRes = LocalTools::Core::ImageBackend::rotate(testPngPath, testRotatePath, 90);
    assert(rotRes.hasValue());
    std::cout << "  - Image rotate test passed!" << std::endl;

    // Image Target-Size Compression (Target: 50 KB)
    QString testTargetCompPath = "test_image_target_compressed.jpg";
    auto targetCompRes = LocalTools::Core::ImageBackend::compressToTargetSize(testPngPath, testTargetCompPath, 50 * 1024);
    assert(targetCompRes.hasValue());
    QFileInfo targetCompFi(testTargetCompPath);
    assert(targetCompFi.exists() && targetCompFi.size() <= 55 * 1024);
    std::cout << "  - Image target-size compression test passed! Output size: " << targetCompFi.size() << " bytes (<= 50 KB target)" << std::endl;

    // Image EXIF / Metadata Strip
    QString testCleanPath = "test_image_stripped.png";
    auto stripRes = LocalTools::Core::ImageBackend::stripMetadata(testPngPath, testCleanPath);
    assert(stripRes.hasValue());
    assert(QFileInfo::exists(testCleanPath));
    std::cout << "  - Image EXIF/metadata strip test passed!" << std::endl;

    // 13. V2 DocumentBackend Engine Tests (Phase V2.2)
    std::cout << "[Test 13] Testing V2 Document & ID Backend Engine..." << std::endl;

    // A: ID Copy Sheet Generator (A4, 2 copies)
    QString idFrontSample = "test_sample_image.png";
    QString idCopyPdfOutput = "test_id_copy_a4.pdf";
    LocalTools::Core::DocumentBackend::IdCopyOptions idOpts;
    idOpts.copyCount = 2;
    idOpts.isA4 = true;
    idOpts.dpi = 150;
    auto idCopyRes = LocalTools::Core::DocumentBackend::createIdCopySheet(idFrontSample, idFrontSample, idCopyPdfOutput, idOpts);
    assert(idCopyRes.hasValue());
    LocalTools::Core::PdfiumBackend idCopyBackend;
    assert(idCopyBackend.openDocument(idCopyPdfOutput.toStdString()));
    assert(idCopyBackend.getPageCount() == 1);
    idCopyBackend.closeDocument();
    std::cout << "  - ID Copy Sheet (A4 PDF) test passed!" << std::endl;

    // B: Passport Photo Grid (6 copies, 10x15cm)
    QString passportOutput = "test_passport_grid.jpg";
    LocalTools::Core::DocumentBackend::PassportPhotoOptions passOpts;
    passOpts.photoCount = 6;
    passOpts.photoWidthMm = 35.0;
    passOpts.photoHeightMm = 45.0;
    passOpts.dpi = 150;
    auto passRes = LocalTools::Core::DocumentBackend::generatePassportPhotoGrid(idFrontSample, passportOutput, passOpts);
    assert(passRes.hasValue());
    assert(!QImage(passportOutput).isNull());
    std::cout << "  - Passport Photo Grid (35x45mm 6-photo) test passed!" << std::endl;

    // C: 4-Point Perspective Warp
    QString warpOutput = "test_deskewed_quad.png";
    auto warpRes = LocalTools::Core::DocumentBackend::correctPerspective(
        idFrontSample, warpOutput,
        QPointF(10, 10), QPointF(190, 20), QPointF(185, 180), QPointF(15, 175),
        200, 200
    );
    assert(warpRes.hasValue());
    assert(QFileInfo::exists(warpOutput));
    std::cout << "  - 4-point Perspective Warp test passed!" << std::endl;

    // D: Scan Enhancement & Contrast Boost
    QString scanEnhanceOutput = "test_scan_enhanced.jpg";
    LocalTools::Core::DocumentBackend::ScanEnhanceOptions scanOpts;
    scanOpts.autoContrast = true;
    scanOpts.removeShadows = true;
    auto scanRes = LocalTools::Core::DocumentBackend::enhanceDocumentScan(idFrontSample, scanEnhanceOutput, scanOpts);
    assert(scanRes.hasValue());
    assert(QFileInfo::exists(scanEnhanceOutput));
    std::cout << "  - Document Scan Enhancement test passed!" << std::endl;

    // 14. V2 FileUtilsBackend Engine Tests (Phase V2.3)
    std::cout << "[Test 14] Testing V2 File Utilities Backend Engine..." << std::endl;

    // A: Batch Renaming Preview & Execution
    QString sampleRenameA = "test_raw_file_A.tmp";
    QString sampleRenameB = "test_raw_file_B.tmp";
    QFile fA(sampleRenameA);
    if (fA.open(QIODevice::WriteOnly)) { fA.write("dataA"); fA.close(); }
    QFile fB(sampleRenameB);
    if (fB.open(QIODevice::WriteOnly)) { fB.write("dataB"); fB.close(); }

    LocalTools::Core::FileUtilsBackend::RenameRule rRule;
    rRule.prefix = "batch_";
    rRule.useNumbering = true;
    rRule.startNumber = 1;
    rRule.numberPadding = 3;
    auto previewList = LocalTools::Core::FileUtilsBackend::previewRename({sampleRenameA, sampleRenameB}, rRule);
    assert(previewList.size() == 2);
    assert(!previewList[0].hasConflict && !previewList[1].hasConflict);

    auto renameRes = LocalTools::Core::FileUtilsBackend::executeRename(previewList);
    assert(renameRes.hasValue() && renameRes.value() == 2);
    assert(QFile::exists(previewList[0].newPath));
    assert(QFile::exists(previewList[1].newPath));
    QFile::remove(previewList[0].newPath);
    QFile::remove(previewList[1].newPath);
    std::cout << "  - Batch file renaming test passed!" << std::endl;

    // B: Multi-algorithm Hash & Verify
    auto hashRes = LocalTools::Core::FileUtilsBackend::computeHashes(testPngPath);
    assert(hashRes.hasValue());
    assert(!hashRes.value().md5.isEmpty() && !hashRes.value().sha256.isEmpty());
    assert(LocalTools::Core::FileUtilsBackend::verifyHash(testPngPath, hashRes.value().sha256));
    std::cout << "  - Multi-algorithm hash computation & verification test passed!" << std::endl;

    // C: Directory Size & Category Analysis
    auto dirAnalysisRes = LocalTools::Core::FileUtilsBackend::analyzeDirectory(".", 5);
    assert(dirAnalysisRes.hasValue());
    assert(dirAnalysisRes.value().totalFilesCount > 0);
    assert(!dirAnalysisRes.value().categories.empty());
    std::cout << "  - Directory size & category breakdown test passed! Analyzed files: " << dirAnalysisRes.value().totalFilesCount << std::endl;

    // D: Duplicate File Finder
    QString dupFile1 = "test_dup_1.tmp";
    QString dupFile2 = "test_dup_2.tmp";
    QFile df1(dupFile1);
    if (df1.open(QIODevice::WriteOnly)) { df1.write("identical_payload_12345"); df1.close(); }
    QFile df2(dupFile2);
    if (df2.open(QIODevice::WriteOnly)) { df2.write("identical_payload_12345"); df2.close(); }

    auto dupRes = LocalTools::Core::FileUtilsBackend::findDuplicates(".");
    assert(dupRes.hasValue());
    bool foundOurDups = false;
    for (const auto& grp : dupRes.value()) {
        if (grp.filePaths.contains(QDir(".").absoluteFilePath(dupFile1)) ||
            grp.filePaths.contains(dupFile1) ||
            grp.filePaths.filter("test_dup_").size() >= 2) {
            foundOurDups = true;
            break;
        }
    }
    assert(foundOurDups);
    QFile::remove(dupFile1);
    QFile::remove(dupFile2);
    std::cout << "  - 3-stage duplicate file finder test passed!" << std::endl;

    // 15. V2 Watermark PDF Operation Test (Phase V2.4)
    std::cout << "[Test 15] Testing V2 Watermark PDF Operation..." << std::endl;
    QString watermarkedPdf = "test_watermarked.pdf";
    LocalTools::Core::Operations::WatermarkOptions wmOpts;
    wmOpts.text = "GİZLİ & ÖZEL";
    wmOpts.fontSize = 40;
    wmOpts.opacity = 0.3;
    wmOpts.rotationAngle = -45;
    auto wmRes = LocalTools::Core::Operations::WatermarkPdfOperation::execute(testPath,
                                                                             watermarkedPdf.toStdString(),
                                                                             wmOpts);
    assert(wmRes.hasValue());
    assert(QFileInfo::exists(watermarkedPdf));
    std::cout << "  - Watermark PDF operation test passed!" << std::endl;

    // 16. V2 Quick Tools Backend Engine Tests (Phase V2.7)
    std::cout << "[Test 16] Testing V2 Quick Tools Backend Engine..." << std::endl;

    // A: QR Code Generator
    QString qrCodeOutput = "test_qr_code.png";
    auto qrRes = LocalTools::Core::QuickToolsBackend::saveQrCode("https://localtools.app", qrCodeOutput);
    assert(qrRes.hasValue());
    assert(QFileInfo::exists(qrCodeOutput));
    std::cout << "  - QR Code generator test passed!" << std::endl;

    // QR Code Reader & Decoder Test (V3 Feature)
    auto qrDecodeRes = LocalTools::Core::QuickToolsBackend::decodeQrFile(qrCodeOutput);
    assert(qrDecodeRes.hasValue() && qrDecodeRes.value().success);
    assert(qrDecodeRes.value().text == "https://localtools.app");
    std::cout << "  - QR Code reader/decoder test passed! Decoded payload: " << qrDecodeRes.value().text.toStdString() << std::endl;

    // Additional multi-type QR test (Wi-Fi / Long text)
    QString qrWifiOutput = "test_qr_wifi.png";
    QString wifiPayload = "WIFI:S:MyHomeNetwork;T:WPA;P:SecretPassword123;;";
    auto qrWifiGen = LocalTools::Core::QuickToolsBackend::saveQrCode(wifiPayload, qrWifiOutput);
    assert(qrWifiGen.hasValue());
    auto qrWifiDecode = LocalTools::Core::QuickToolsBackend::decodeQrFile(qrWifiOutput);
    assert(qrWifiDecode.hasValue() && qrWifiDecode.value().success);
    assert(qrWifiDecode.value().text == wifiPayload);
    std::cout << "  - QR Code Wi-Fi format decode test passed!" << std::endl;

    // B: JSON Formatter & Minifier
    QString rawJson = "{\"app\":\"LocalTools\",\"version\":2,\"offline\":true}";
    auto formatRes = LocalTools::Core::QuickToolsBackend::formatJson(rawJson);
    assert(formatRes.hasValue() && formatRes.value().contains("\n"));
    auto minifyRes = LocalTools::Core::QuickToolsBackend::minifyJson(formatRes.value());
    assert(minifyRes.hasValue() && !minifyRes.value().contains("\n"));
    std::cout << "  - JSON Formatter & Minifier test passed!" << std::endl;

    // C: Base64 & URL Encoding
    QString secretText = "Privacy First Local Tools 2026!";
    QString b64 = LocalTools::Core::QuickToolsBackend::encodeBase64(secretText);
    auto deb64 = LocalTools::Core::QuickToolsBackend::decodeBase64(b64);
    assert(deb64.hasValue() && deb64.value() == secretText);

    QString rawUrl = "https://localtools.app/search?query=türkçe karakterler & test=1";
    QString encodedUrl = LocalTools::Core::QuickToolsBackend::urlEncode(rawUrl);
    QString decodedUrl = LocalTools::Core::QuickToolsBackend::urlDecode(encodedUrl);
    assert(decodedUrl == rawUrl);
    std::cout << "  - Base64 & URL encoding/decoding test passed!" << std::endl;

    // 17. V2 Video Backend Engine Tests (Phase V2.5 & V2.6)
    std::cout << "[Test 17] Testing V2 Video Backend Engine..." << std::endl;
    bool hasFfmpeg = LocalTools::Core::VideoBackend::isFFmpegAvailable();
    assert(hasFfmpeg);
    const QString bundledFfmpeg = LocalTools::Core::VideoBackend::getFFmpegPath();
    assert(QFileInfo(bundledFfmpeg).absolutePath() == QCoreApplication::applicationDirPath());
    std::cout << "  - Verified bundled FFmpeg runtime: " << bundledFfmpeg.toStdString() << std::endl;

    QProcess ffmpegAudit;
    ffmpegAudit.start(bundledFfmpeg, {"-hide_banner", "-version"});
    assert(ffmpegAudit.waitForStarted(5000));
    assert(ffmpegAudit.waitForFinished(10000));
    const QByteArray ffmpegBuildInfo = ffmpegAudit.readAllStandardOutput() + ffmpegAudit.readAllStandardError();
    assert(ffmpegBuildInfo.contains("--disable-libx264"));
    assert(ffmpegBuildInfo.contains("--disable-libx265"));
    assert(!ffmpegBuildInfo.contains("--enable-gpl"));
    assert(!ffmpegBuildInfo.contains("--enable-nonfree"));

    ffmpegAudit.start(bundledFfmpeg, {"-hide_banner", "-L"});
    assert(ffmpegAudit.waitForStarted(5000));
    assert(ffmpegAudit.waitForFinished(10000));
    const QByteArray ffmpegLicense = ffmpegAudit.readAllStandardOutput() + ffmpegAudit.readAllStandardError();
    assert(ffmpegLicense.contains("GNU Lesser General Public License"));

    auto nonExistentRes = LocalTools::Core::VideoBackend::probeVideo("non_existent_file.mp4");
    assert(nonExistentRes.hasError());
    assert(nonExistentRes.error().category == LocalTools::Core::PdfErrorCategory::FileNotFound);

    auto dummyProbe = LocalTools::Core::VideoBackend::probeVideo(testPngPath);
    assert(dummyProbe.hasError());

    QString fixtureVideo = "test_media_fixture.mp4";
    QProcess fixtureProcess;
    fixtureProcess.start(bundledFfmpeg,
                         {"-nostdin", "-hide_banner", "-y",
                          "-f", "lavfi", "-i", "testsrc=size=320x240:rate=10",
                          "-f", "lavfi", "-i", "sine=frequency=880:sample_rate=44100",
                          "-t", "1", "-c:v", "mpeg4", "-q:v", "5", "-c:a", "aac",
                          fixtureVideo});
    assert(fixtureProcess.waitForStarted(5000));
    assert(fixtureProcess.waitForFinished(20000));
    assert(fixtureProcess.exitStatus() == QProcess::NormalExit && fixtureProcess.exitCode() == 0);

    auto videoProbe = LocalTools::Core::VideoBackend::probeVideo(fixtureVideo);
    assert(videoProbe.hasValue());
    assert(videoProbe.value().width == 320 && videoProbe.value().height == 240);

    LocalTools::Core::VideoBackend::CompressOptions videoCompressOptions;
    videoCompressOptions.targetWidth = 160;
    videoCompressOptions.crf = 26;
    auto videoCompress = LocalTools::Core::VideoBackend::compressVideo(
        fixtureVideo, "test_media_compressed.mp4", videoCompressOptions);
    assert(videoCompress.hasValue() && QFileInfo::exists("test_media_compressed.mp4"));

    LocalTools::Core::VideoBackend::GifOptions gifOptions;
    gifOptions.width = 160;
    gifOptions.fps = 8;
    gifOptions.durationSec = 0.5;
    auto gifResult = LocalTools::Core::VideoBackend::convertToGif(
        fixtureVideo, "test_media.gif", gifOptions);
    assert(gifResult.hasValue() && QFileInfo::exists("test_media.gif"));

    auto audioExtract = LocalTools::Core::VideoBackend::extractAudio(
        fixtureVideo, "test_media_audio.mp3", "mp3", 128);
    assert(audioExtract.hasValue() && QFileInfo::exists("test_media_audio.mp3"));
    auto audioProbe = LocalTools::Core::AudioBackend::probeAudio("test_media_audio.mp3");
    assert(audioProbe.hasValue() && audioProbe.value().sampleRateHz > 0);

    for (const QString& format : {QStringLiteral("mp3"), QStringLiteral("wav"),
                                  QStringLiteral("flac"), QStringLiteral("ogg"),
                                  QStringLiteral("m4a")}) {
        LocalTools::Core::AudioBackend::ConvertOptions audioConvertOptions;
        audioConvertOptions.targetFormat = format;
        audioConvertOptions.bitrateKbps = 128;
        audioConvertOptions.stripMetadata = true;
        const QString outputPath = QStringLiteral("test_media_audio_converted.%1").arg(format);
        auto audioConvert = LocalTools::Core::AudioBackend::convertFormat(
            "test_media_audio.mp3", outputPath, audioConvertOptions);
        assert(audioConvert.hasValue() && QFileInfo::exists(outputPath));
        auto convertedProbe = LocalTools::Core::AudioBackend::probeAudio(outputPath);
        assert(convertedProbe.hasValue() && convertedProbe.value().durationSeconds > 0.0);
    }

    LocalTools::Core::AudioBackend::ConvertOptions audioCompressOptions;
    audioCompressOptions.targetFormat = "mp3";
    audioCompressOptions.bitrateKbps = 64;
    audioCompressOptions.stripMetadata = true;
    auto audioCompress = LocalTools::Core::AudioBackend::convertFormat(
        "test_media_audio.mp3", "test_media_audio_compressed.mp3", audioCompressOptions);
    assert(audioCompress.hasValue() && QFileInfo::exists("test_media_audio_compressed.mp3"));
    auto compressedAudioProbe = LocalTools::Core::AudioBackend::probeAudio(
        "test_media_audio_compressed.mp3");
    assert(compressedAudioProbe.hasValue());
    assert(QFileInfo("test_media_audio_compressed.mp3").size()
           < QFileInfo("test_media_audio.mp3").size());

    LocalTools::Core::AudioBackend::TrimOptions audioTrimOptions;
    audioTrimOptions.startTimeSec = 0.1;
    audioTrimOptions.endTimeSec = 0.6;
    audioTrimOptions.reEncode = true;
    auto audioTrim = LocalTools::Core::AudioBackend::trimAudio(
        "test_media_audio.mp3", "test_media_audio_trimmed.mp3", audioTrimOptions);
    assert(audioTrim.hasValue() && QFileInfo::exists("test_media_audio_trimmed.mp3"));
    auto trimmedAudioProbe = LocalTools::Core::AudioBackend::probeAudio(
        "test_media_audio_trimmed.mp3");
    assert(trimmedAudioProbe.hasValue());

    auto audioSourceProtection = LocalTools::Core::AudioBackend::convertFormat(
        "test_media_audio.mp3", "test_media_audio.mp3", audioCompressOptions);
    assert(audioSourceProtection.hasError() &&
           audioSourceProtection.error().category ==
               LocalTools::Core::PdfErrorCategory::PermissionDenied);

    LocalTools::Core::VideoBackend::TrimOptions trimOptions;
    trimOptions.startTimeSec = 0.1;
    trimOptions.endTimeSec = 0.8;
    trimOptions.reEncode = true;
    auto trimResult = LocalTools::Core::VideoBackend::trimVideo(
        fixtureVideo, "test_media_trimmed.mp4", trimOptions);
    assert(trimResult.hasValue() && QFileInfo::exists("test_media_trimmed.mp4"));

    auto sourceProtection = LocalTools::Core::VideoBackend::compressVideo(
        fixtureVideo, fixtureVideo, videoCompressOptions);
    assert(sourceProtection.hasError() &&
           sourceProtection.error().category == LocalTools::Core::PdfErrorCategory::PermissionDenied);
    std::cout << "  - Video/audio probe, convert, compress, GIF, trim and safety tests passed!" << std::endl;

    // 18. V4 Metadata Inspection & Cleaning Tests
    std::cout << "[Test 18] Testing V4 Metadata Inspection & Cleaning Engine..." << std::endl;
    auto metaInspectImg = LocalTools::Core::QuickToolsBackend::inspectMetadata(testPngPath);
    assert(metaInspectImg.hasValue());
    assert(!metaInspectImg.value().items.empty());
    std::cout << "  - Image metadata inspected! Items count: " << metaInspectImg.value().items.size() << std::endl;

    QString cleanImgOut = "test_image_clean_meta.png";
    auto cleanImgRes = LocalTools::Core::QuickToolsBackend::cleanMetadata(testPngPath, cleanImgOut);
    assert(cleanImgRes.hasValue());
    assert(QFileInfo::exists(cleanImgOut));
    std::cout << "  - Image metadata cleaned successfully!" << std::endl;

    auto metaInspectPdf = LocalTools::Core::QuickToolsBackend::inspectMetadata(QString::fromStdString(testPath));
    assert(metaInspectPdf.hasValue());
    assert(!metaInspectPdf.value().items.empty());
    std::cout << "  - PDF metadata inspected! Items count: " << metaInspectPdf.value().items.size() << std::endl;

    QString cleanPdfOut = "test_pdf_clean_meta.pdf";
    auto cleanPdfRes = LocalTools::Core::QuickToolsBackend::cleanMetadata(QString::fromStdString(testPath), cleanPdfOut);
    assert(cleanPdfRes.hasValue());
    assert(QFileInfo::exists(cleanPdfOut));
    std::cout << "  - PDF metadata cleaned successfully!" << std::endl;

    // 19. Release hardening: invalid input, Unicode path and source safety
    std::cout << "[Test 19] Testing release failure and path-safety matrix..." << std::endl;
    const QString sourcePdf = QString::fromStdString(testPath);
    const QString unicodePdf = QDir::current().absoluteFilePath(
        QStringLiteral("Türkçe_İçerik_şğü.pdf"));
    assert(QFile::copy(sourcePdf, unicodePdf));
    const std::string unicodePdfPath = unicodePdf.toStdString();

    LocalTools::Core::PdfiumBackend unicodeBackend;
    assert(unicodeBackend.openDocument(unicodePdfPath));
    assert(unicodeBackend.getPageCount() == totalPages);
    unicodeBackend.closeDocument();

    const QString unicodeMerged = QDir::current().absoluteFilePath(
        QStringLiteral("Türkçe_birleşik_çıktı.pdf"));
    auto unicodeMerge = LocalTools::Core::Operations::MergePdfOperation::execute(
        {unicodePdfPath, unicodePdfPath}, unicodeMerged.toStdString());
    assert(unicodeMerge.hasValue() && QFileInfo::exists(unicodeMerged));

    const QString unicodeSplitDir = QDir::current().absoluteFilePath(
        QStringLiteral("Türkçe_bölümler"));
    assert(QDir().mkpath(unicodeSplitDir));
    auto unicodeSplit = LocalTools::Core::Operations::SplitPdfOperation::execute(
        unicodePdfPath, unicodeSplitDir.toStdString(), 1);
    assert(unicodeSplit.hasValue());
    assert(QDir(unicodeSplitDir).entryList({QStringLiteral("*.pdf")}, QDir::Files).size()
           == totalPages);

    std::vector<int> reversedOrder(static_cast<size_t>(totalPages));
    std::iota(reversedOrder.begin(), reversedOrder.end(), 0);
    std::reverse(reversedOrder.begin(), reversedOrder.end());
    const QString reorderedOutput = QDir::current().absoluteFilePath(
        QStringLiteral("Türkçe_sıralı.pdf"));
    auto reorderSuccess = LocalTools::Core::Operations::ReorderPagesOperation::execute(
        unicodePdfPath, reversedOrder, reorderedOutput.toStdString());
    assert(reorderSuccess.hasValue() && QFileInfo::exists(reorderedOutput));

    const auto sourceSizeBefore = QFileInfo(unicodePdf).size();
    const QString equivalentSourcePath = QDir(QFileInfo(unicodePdf).absolutePath()).absoluteFilePath(
        QStringLiteral("./") + QFileInfo(unicodePdf).fileName());
    auto mergeSourceProtection = LocalTools::Core::Operations::MergePdfOperation::execute(
        {unicodePdfPath}, equivalentSourcePath.toStdString());
    assert(mergeSourceProtection.hasError());
    assert(mergeSourceProtection.error().category
           == LocalTools::Core::PdfErrorCategory::PermissionDenied);
    assert(QFileInfo(unicodePdf).size() == sourceSizeBefore);

    auto invalidExtract = LocalTools::Core::Operations::ExtractPagesOperation::execute(
        unicodePdfPath, {totalPages}, "invalid_extract.pdf");
    assert(invalidExtract.hasError()
           && invalidExtract.error().category
               == LocalTools::Core::PdfErrorCategory::InvalidPageRange);
    auto invalidDelete = LocalTools::Core::Operations::DeletePagesOperation::execute(
        unicodePdfPath, {}, "invalid_delete.pdf");
    assert(invalidDelete.hasError()
           && invalidDelete.error().category
               == LocalTools::Core::PdfErrorCategory::InvalidPageRange);
    auto invalidReorder = LocalTools::Core::Operations::ReorderPagesOperation::execute(
        unicodePdfPath, std::vector<int>(static_cast<size_t>(totalPages), 0),
        "invalid_reorder.pdf");
    assert(invalidReorder.hasError()
           && invalidReorder.error().category
               == LocalTools::Core::PdfErrorCategory::InvalidPageRange);
    auto invalidRotation = LocalTools::Core::Operations::RotatePagesOperation::execute(
        unicodePdfPath, {{0, 45}}, "invalid_rotation.pdf");
    assert(invalidRotation.hasError()
           && invalidRotation.error().category
               == LocalTools::Core::PdfErrorCategory::InvalidPageRange);
    auto invalidSplit = LocalTools::Core::Operations::SplitPdfOperation::execute(
        unicodePdfPath, unicodeSplitDir.toStdString(), 0);
    assert(invalidSplit.hasError()
           && invalidSplit.error().category
               == LocalTools::Core::PdfErrorCategory::InvalidPageRange);

    QFile zeroPdf("zero_byte.pdf");
    assert(zeroPdf.open(QIODevice::WriteOnly));
    zeroPdf.close();
    QFile corruptPdf("corrupt.pdf");
    assert(corruptPdf.open(QIODevice::WriteOnly));
    assert(corruptPdf.write("not a PDF document") > 0);
    corruptPdf.close();
    LocalTools::Core::PdfiumBackend badPdfBackend;
    assert(!badPdfBackend.openDocument("zero_byte.pdf"));
    assert(!badPdfBackend.openDocument("corrupt.pdf"));
    auto corruptMerge = LocalTools::Core::Operations::MergePdfOperation::execute(
        {"corrupt.pdf"}, "corrupt_output.pdf");
    assert(corruptMerge.hasError());

    const QByteArray encryptedPdfBytes = QByteArray::fromBase64(
        "JVBERi0xLjMKJeLjz9MKMSAwIG9iago8PAovUHJvZHVjZXIgPDQyYzA4YzI3MWE+Cj4+CmVuZG9iagoyIDAgb2JqCjw8Ci9UeXBlIC9QYWdlcwovQ291bnQgMQovS2lkcyBbIDQgMCBSIF0KPj4KZW5kb2JqCjMgMCBvYmoKPDwKL1R5cGUgL0NhdGFsb2cKL1BhZ2VzIDIgMCBSCj4+CmVuZG9iago0IDAgb2JqCjw8Ci9UeXBlIC9QYWdlCi9SZXNvdXJjZXMgPDwKPj4KL01lZGlhQm94IFsgMC4wIDAuMCAyMDAgMjAwIF0KL1BhcmVudCAyIDAgUgo+PgplbmRvYmoKNSAwIG9iago8PAovViAyCi9SIDMKL0xlbmd0aCAxMjgKL1AgNDI5NDk2NzI5MgovRmlsdGVyIC9TdGFuZGFyZAovTyA8ZWI0ODA2YWU4NzUyZjRhY2Q5YjhhOTg0YjYyZmIyMTBmY2UwNDk2MDA3M2Y0ZmE5MjQ1YWRjYWY5YzY2ZGY1ND4KL1UgPDI0YzdlYjg1NjYwNGQ1ZWFjMmVlYWRjNmFlMDU1ZTFiMjhiZjRlNWU0ZTc1OGE0MTY0MDA0ZTU2ZmZmYTAxMDg+Cj4+CmVuZG9iagp4cmVmCjAgNgowMDAwMDAwMDAwIDY1NTM1IGYgCjAwMDAwMDAwMTUgMDAwMDAgbiAKMDAwMDAwMDA1OSAwMDAwMCBuIAowMDAwMDAwMTE4IDAwMDAwIG4gCjAwMDAwMDAxNjcgMDAwMDAgbiAKMDAwMDAwMDI2MSAwMDAwMCBuIAp0cmFpbGVyCjw8Ci9TaXplIDYKL1Jvb3QgMyAwIFIKL0luZm8gMSAwIFIKL0lEIFsgPDM1Mzk2MzMyMzA2MjYyNjE2NTYzMzgzMjY1MzE2MjM1NjMzNjMzNjM2MTYyNjU2NjM1NjE2NjYxMzE+IDwzNTM5NjMzMjMwNjI2MjYxNjU2MzM4MzI2NTMxNjIzNTYzMzYzMzYzNjE2MjY1NjYzNTYxNjY2MTY1NjYzMTMxPiBdCi9FbmNyeXB0IDUgMCBSCj4+CnN0YXJ0eHJlZgo0NzYKJSVFT0YK");
    QFile encryptedPdf("password_protected.pdf");
    assert(encryptedPdf.open(QIODevice::WriteOnly));
    assert(encryptedPdf.write(encryptedPdfBytes) == encryptedPdfBytes.size());
    encryptedPdf.close();
    auto encryptedMerge = LocalTools::Core::Operations::MergePdfOperation::execute(
        {"password_protected.pdf"}, "encrypted_output.pdf");
    assert(encryptedMerge.hasError());
    assert(encryptedMerge.error().category
           == LocalTools::Core::PdfErrorCategory::InvalidPdf);
    assert(QString::fromStdString(encryptedMerge.error().toUserFriendlyMessage())
           .contains(QStringLiteral("password"), Qt::CaseInsensitive));

    QFile corruptImage("corrupt_image.png");
    assert(corruptImage.open(QIODevice::WriteOnly));
    assert(corruptImage.write("not image data") > 0);
    corruptImage.close();
    assert(LocalTools::Core::ImageBackend::getInfo("corrupt_image.png").hasError());
    assert(LocalTools::Core::QuickToolsBackend::decodeQrFile("corrupt_image.png").hasError());

    const qint64 sourceImageSize = QFileInfo(testPngPath).size();
    auto imageSourceProtection = LocalTools::Core::ImageBackend::compress(
        testPngPath, QDir::current().absoluteFilePath(QStringLiteral("./") + testPngPath), 70);
    assert(imageSourceProtection.hasError()
           && imageSourceProtection.error().category
               == LocalTools::Core::PdfErrorCategory::PermissionDenied);
    assert(QFileInfo(testPngPath).size() == sourceImageSize);

    QFile outputBlocker("output_blocker");
    assert(outputBlocker.open(QIODevice::WriteOnly));
    outputBlocker.write("block directory creation");
    outputBlocker.close();
    auto writeFailure = LocalTools::Core::ImageBackend::compress(
        testPngPath, "output_blocker/result.jpg", 70);
    assert(writeFailure.hasError());
    auto splitWriteFailure = LocalTools::Core::Operations::SplitPdfOperation::execute(
        unicodePdfPath, "output_blocker", 1);
    assert(splitWriteFailure.hasError());
    std::cout << "  - Unicode, corrupt/zero-byte, invalid-range, collision and write-failure tests passed!" << std::endl;

    // 20. V4 Localization & i18n Tests
    std::cout << "[Test 20] Testing V4 Localization & Dynamic Translation Engine..." << std::endl;
    auto& loc = LocalTools::Services::LocalizationService::instance();
    loc.initialize(&app);
    loc.setLanguage(LocalTools::Services::AppLanguage::English);
    assert(loc.currentLanguageCode() == "en");
    assert(loc.translate("Home") == "Home");
    assert(loc.translate("QR Code Generator") == "QR Code Generator");
    assert(loc.translate("View / Clean Metadata") == "View / Clean Metadata");

    auto englishMetadata = LocalTools::Core::QuickToolsBackend::inspectMetadata(QString::fromStdString(testPath));
    assert(!englishMetadata.hasError());
    auto containsMetadataLabel = [](const LocalTools::Core::FileMetadataReport& report,
                                    const QString& category,
                                    const QString& key) {
        return std::any_of(report.items.cbegin(), report.items.cend(),
                           [&](const LocalTools::Core::MetadataItem& item) {
            return item.category == category && item.key == key;
        });
    };
    assert(containsMetadataLabel(englishMetadata.value(), "File Information", "File Size"));
    assert(containsMetadataLabel(englishMetadata.value(), "File Information", "Last Modified"));
    assert(containsMetadataLabel(englishMetadata.value(), "Document Information", "Total Pages"));

    // Switch to Turkish
    loc.setLanguage(LocalTools::Services::AppLanguage::Turkish);
    assert(loc.currentLanguageCode() == "tr");
    auto turkishMetadata = LocalTools::Core::QuickToolsBackend::inspectMetadata(QString::fromStdString(testPath));
    assert(!turkishMetadata.hasError());
    assert(containsMetadataLabel(turkishMetadata.value(), "Dosya Bilgileri", "Dosya Boyutu"));
    assert(containsMetadataLabel(turkishMetadata.value(), "Dosya Bilgileri", "Son Değiştirilme"));
    assert(containsMetadataLabel(turkishMetadata.value(), "Belge Bilgileri", "Toplam Sayfa"));
    std::cout << "  - Language switched to Turkish." << std::endl;

    // Verify ToolRegistry has updated tool for quick.metadata
    const auto* quickMetaTool = LocalTools::Core::ToolRegistry::instance().toolById("quick.metadata");
    assert(quickMetaTool != nullptr);
    std::cout << "  - ToolRegistry 'quick.metadata' found: " << quickMetaTool->name.toStdString() << std::endl;

    // Switch back to Turkish
    loc.setLanguage(LocalTools::Services::AppLanguage::Turkish);
    assert(loc.currentLanguageCode() == "tr");
    std::cout << "  - Dual-language live switching test passed!" << std::endl;

    std::cout << "=== ALL LOCALTOOLS SUITE V4 AUTOMATED UNIT & INTEGRATION TESTS PASSED! ===" << std::endl;
    return 0;
}
