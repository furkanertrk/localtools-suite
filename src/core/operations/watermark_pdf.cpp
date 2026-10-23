#include "watermark_pdf.h"

#include <QPainter>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QFile>

#include "../backend/pdfium_backend.h"
#include "images_to_pdf.h"
#include "../../services/file_service.h"

namespace LocalTools::Core::Operations {

Result<void> WatermarkPdfOperation::execute(const std::string& inputPath,
                                            const std::string& outputPath,
                                            const WatermarkOptions& options) {
    if (inputPath.empty()) {
        return Result<void>::fail(PdfErrorCategory::FileNotFound, "Input file not specified.");
    }
    if (outputPath.empty()
        || Services::FileService::instance().pathsReferToSameFile(
            QString::fromUtf8(inputPath), QString::fromUtf8(outputPath))) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied, "Input and output files cannot be the same.");
    }

    PdfiumBackend backend;
    if (!backend.openDocument(inputPath)) {
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "Failed to open PDF document: " + inputPath);
    }

    int pageCount = backend.getPageCount();
    if (pageCount <= 0) {
        backend.closeDocument();
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "PDF contains no pages.");
    }

    QString tempDir = QString::fromStdString(Services::FileService::instance().getTempDirectory().string());
    std::vector<std::string> tempImagePaths;
    tempImagePaths.reserve(pageCount);

    int targetW = (options.dpi == 300) ? 2480 : 1240;
    int targetH = (options.dpi == 300) ? 3508 : 1754;

    for (int i = 0; i < pageCount; ++i) {
        QImage pageImg = backend.renderPage(i, targetW, targetH);
        if (pageImg.isNull()) {
            backend.closeDocument();
            return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to process page: " + std::to_string(i + 1));
        }

        // Apply watermark with QPainter
        QPainter painter(&pageImg);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        painter.save();
        painter.translate(pageImg.width() / 2.0, pageImg.height() / 2.0);
        painter.rotate(options.rotationAngle);

        QFont font = painter.font();
        font.setPointSize(options.fontSize * (options.dpi / 72.0));
        font.setBold(true);
        painter.setFont(font);

        QColor textColor = options.color;
        textColor.setAlphaF(static_cast<float>(options.opacity));
        painter.setPen(textColor);

        QFontMetrics fm(font);
        int textW = fm.horizontalAdvance(options.text);
        int textH = fm.height();

        painter.drawText(-textW / 2, textH / 4, options.text);
        painter.restore();
        painter.end();

        QString tempPath = tempDir + "/temp_wm_page_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" + QString::number(i) + ".png";
        pageImg.save(tempPath, "PNG");
        tempImagePaths.push_back(tempPath.toStdString());
    }

    backend.closeDocument();

    auto pdfRes = ImagesToPdfOperation::execute(tempImagePaths, outputPath);

    // Clean temp images
    for (const auto& path : tempImagePaths) {
        QFile::remove(QString::fromStdString(path));
    }

    return pdfRes;
}

} // namespace LocalTools::Core::Operations
