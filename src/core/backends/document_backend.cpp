#include "document_backend.h"

#include <QPainter>
#include <QPen>
#include <QFileInfo>
#include <QTransform>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <cmath>
#include <algorithm>

#include "image_backend.h"
#include "../operations/images_to_pdf.h"
#include "../../services/file_service.h"

namespace LocalTools::Core {

Result<QImage> DocumentBackend::loadImage(const QString& path) {
    QFileInfo fi(path);
    if (!fi.exists()) {
        return Result<QImage>::fail(PdfErrorCategory::FileNotFound,
                                    "File not found: " + path.toStdString());
    }

    QImage image(path);
    if (image.isNull()) {
        return Result<QImage>::fail(PdfErrorCategory::BackendError,
                                    "Failed to load image: " + path.toStdString());
    }
    image = image.convertToFormat(QImage::Format_ARGB32);
    return Result<QImage>::ok(std::move(image));
}

Result<void> DocumentBackend::saveImageOrPdf(const QImage& image, const QString& outputPath, int dpi) {
    if (image.isNull() || outputPath.trimmed().isEmpty()) {
        return Result<void>::fail(PdfErrorCategory::InvalidInput,
                                  "Output document image or path is invalid.");
    }
    QFileInfo fi(outputPath);
    QString ext = fi.suffix().toLower();

    if (ext == "pdf") {
        // Save image to temporary PNG, then convert to PDF
        const QString tempRoot = QDir::temp().absoluteFilePath(QStringLiteral("LocalTools"));
        if (!QDir().mkpath(tempRoot)) {
            return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                      "Temporary document directory could not be created.");
        }
        QString tempPng = Services::FileService::instance().generateUniqueOutputPath(
            QDir(tempRoot).absoluteFilePath(
                QStringLiteral("temp_doc_sheet_")
                + QString::number(QDateTime::currentMSecsSinceEpoch())
                + QStringLiteral(".png")));
        
        if (!image.save(tempPng, "PNG")) {
            return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to create temporary image.");
        }

        auto pdfRes = Operations::ImagesToPdfOperation::execute({tempPng.toStdString()}, outputPath.toStdString());
        QFile::remove(tempPng);
        return pdfRes;
    }

    QImage toSave = image;
    toSave.setDotsPerMeterX(static_cast<int>(dpi * 39.3701));
    toSave.setDotsPerMeterY(static_cast<int>(dpi * 39.3701));

    if (!toSave.save(outputPath)) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to save output document: " + outputPath.toStdString());
    }
    return Result<void>::ok();
}

Result<void> DocumentBackend::createIdCopySheet(const QString& frontPath, const QString& backPath,
                                                const QString& outputPath, const IdCopyOptions& options) {
    auto& fileService = Services::FileService::instance();
    if (fileService.pathsReferToSameFile(frontPath, outputPath)
        || (!backPath.isEmpty() && fileService.pathsReferToSameFile(backPath, outputPath))) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                  "The output cannot replace an input image.");
    }
    if (options.copyCount <= 0 || options.dpi <= 0) {
        return Result<void>::fail(PdfErrorCategory::InvalidInput,
                                  "Copy count and resolution must be greater than zero.");
    }
    auto frontRes = loadImage(frontPath);
    if (frontRes.hasError()) return Result<void>::fail(frontRes.error());

    QImage frontImg = frontRes.value();
    QImage backImg;
    bool hasBack = !backPath.isEmpty();
    if (hasBack) {
        auto backRes = loadImage(backPath);
        if (backRes.hasError()) return Result<void>::fail(backRes.error());
        backImg = backRes.value();
    }

    // Page dimensions at given DPI
    // A4: 210 x 297 mm, Letter: 215.9 x 279.4 mm
    double pageWMm = options.isA4 ? 210.0 : 215.9;
    double pageHMm = options.isA4 ? 297.0 : 279.4;

    double dpmm = options.dpi / 25.4; // dots per mm
    int pageW = static_cast<int>(pageWMm * dpmm);
    int pageH = static_cast<int>(pageHMm * dpmm);

    // Standard ID card dimensions: 85.60 × 53.98 mm (ISO/IEC 7810 ID-1)
    int cardW = static_cast<int>(85.60 * dpmm);
    int cardH = static_cast<int>(53.98 * dpmm);

    QImage canvas(pageW, pageH, QImage::Format_RGB32);
    canvas.fill(Qt::white);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QImage frontScaled = frontImg.scaled(cardW, cardH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage backScaled = hasBack ? backImg.scaled(cardW, cardH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation) : QImage();

    // Calculate layout positions
    int spacingPx = static_cast<int>(options.spacingMm * dpmm);

    QPen cutPen(QColor(200, 200, 200), 1, Qt::DashLine);

    auto drawCard = [&](const QImage& img, int x, int y, const QString& label) {
        Q_UNUSED(label);
        QRect cardRect(x, y, cardW, cardH);
        painter.drawImage(cardRect, img);
        
        if (options.drawCutLines) {
            painter.setPen(cutPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(cardRect, 8, 8);
        }
    };

    int centerX = (pageW - cardW) / 2;

    if (options.copyCount == 1) {
        // Single copy: Front and Back centered vertically
        int totalBlockH = hasBack ? (cardH * 2 + spacingPx) : cardH;
        int startY = (pageH - totalBlockH) / 2;

        drawCard(frontScaled, centerX, startY, "FRONT SIDE");
        if (hasBack) {
            drawCard(backScaled, centerX, startY + cardH + spacingPx, "BACK SIDE");
        }
    } else if (options.copyCount == 2) {
        // 2 Copies: Top half and Bottom half
        int blockH = hasBack ? (cardH * 2 + spacingPx) : cardH;
        int firstY = (pageH / 2 - blockH) / 2;
        int secondY = (pageH / 2) + (pageH / 2 - blockH) / 2;

        // Set 1
        drawCard(frontScaled, centerX, firstY, "1st COPY - FRONT SIDE");
        if (hasBack) {
            drawCard(backScaled, centerX, firstY + cardH + spacingPx, "1st COPY - BACK SIDE");
        }

        // Set 2
        drawCard(frontScaled, centerX, secondY, "2nd COPY - FRONT SIDE");
        if (hasBack) {
            drawCard(backScaled, centerX, secondY + cardH + spacingPx, "2nd COPY - BACK SIDE");
        }
    } else {
        // 4 Copies: 2x2 grid
        int col1X = (pageW / 2 - cardW) / 2;
        int col2X = (pageW / 2) + (pageW / 2 - cardW) / 2;
        int row1Y = static_cast<int>(pageH * 0.15);
        int row2Y = static_cast<int>(pageH * 0.55);

        drawCard(frontScaled, col1X, row1Y, "1st COPY - FRONT SIDE");
        if (hasBack) drawCard(backScaled, col1X, row1Y + cardH + spacingPx / 2, "1st COPY - BACK SIDE");

        drawCard(frontScaled, col2X, row1Y, "2nd COPY - FRONT SIDE");
        if (hasBack) drawCard(backScaled, col2X, row1Y + cardH + spacingPx / 2, "2nd COPY - BACK SIDE");

        drawCard(frontScaled, col1X, row2Y, "3rd COPY - FRONT SIDE");
        if (hasBack) drawCard(backScaled, col1X, row2Y + cardH + spacingPx / 2, "3rd COPY - BACK SIDE");

        drawCard(frontScaled, col2X, row2Y, "4th COPY - FRONT SIDE");
        if (hasBack) drawCard(backScaled, col2X, row2Y + cardH + spacingPx / 2, "4th COPY - BACK SIDE");
    }

    painter.end();
    return saveImageOrPdf(canvas, outputPath, options.dpi);
}

Result<void> DocumentBackend::generatePassportPhotoGrid(const QString& photoPath, const QString& outputPath,
                                                        const PassportPhotoOptions& options) {
    if (Services::FileService::instance().pathsReferToSameFile(photoPath, outputPath)) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                  "The output cannot replace the source photo.");
    }
    if (options.photoCount <= 0 || options.photoWidthMm <= 0.0
        || options.photoHeightMm <= 0.0 || options.dpi <= 0) {
        return Result<void>::fail(PdfErrorCategory::InvalidInput,
                                  "Photo grid dimensions and count must be greater than zero.");
    }
    auto imgRes = loadImage(photoPath);
    if (imgRes.hasError()) return Result<void>::fail(imgRes.error());

    QImage srcPhoto = imgRes.value();

    // Canvas size
    double canvasWMm = options.isA4Paper ? 210.0 : 150.0; // 10x15 cm standard photo paper
    double canvasHMm = options.isA4Paper ? 297.0 : 100.0;

    double dpmm = options.dpi / 25.4;
    int canvasW = static_cast<int>(canvasWMm * dpmm);
    int canvasH = static_cast<int>(canvasHMm * dpmm);

    int photoW = static_cast<int>(options.photoWidthMm * dpmm);
    int photoH = static_cast<int>(options.photoHeightMm * dpmm);

    QImage scaledPhoto = srcPhoto.scaled(photoW, photoH, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    QImage canvas(canvasW, canvasH, QImage::Format_RGB32);
    canvas.fill(Qt::white);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Determine grid rows and cols based on photoCount
    int cols = (options.photoCount >= 6) ? 3 : 2;
    int rows = (options.photoCount + cols - 1) / cols;

    int spacingPx = static_cast<int>(4.0 * dpmm); // 4mm between photos

    int gridTotalW = cols * photoW + (cols - 1) * spacingPx;
    int gridTotalH = rows * photoH + (rows - 1) * spacingPx;

    int startX = (canvasW - gridTotalW) / 2;
    int startY = (canvasH - gridTotalH) / 2;

    QPen guidePen(QColor(210, 210, 210), 1, Qt::DashLine);

    int placed = 0;
    for (int r = 0; r < rows && placed < options.photoCount; ++r) {
        for (int c = 0; c < cols && placed < options.photoCount; ++c) {
            int px = startX + c * (photoW + spacingPx);
            int py = startY + r * (photoH + spacingPx);

            QRect targetRect(px, py, photoW, photoH);
            painter.drawImage(targetRect, scaledPhoto, QRect((scaledPhoto.width() - photoW) / 2, 0, photoW, photoH));

            if (options.drawBorder) {
                painter.setPen(guidePen);
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(targetRect);
            }
            placed++;
        }
    }

    painter.end();
    return saveImageOrPdf(canvas, outputPath, options.dpi);
}

Result<void> DocumentBackend::correctPerspective(const QString& inputPath, const QString& outputPath,
                                                 const QPointF& topLeft, const QPointF& topRight,
                                                 const QPointF& bottomRight, const QPointF& bottomLeft,
                                                 int targetWidth, int targetHeight) {
    if (Services::FileService::instance().pathsReferToSameFile(inputPath, outputPath)) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                  "The output cannot replace the source image.");
    }
    auto imgRes = loadImage(inputPath);
    if (imgRes.hasError()) return Result<void>::fail(imgRes.error());

    QImage srcImg = imgRes.value();

    if (targetWidth <= 0 || targetHeight <= 0) {
        // Estimate width/height from point distances
        double topW = std::hypot(topRight.x() - topLeft.x(), topRight.y() - topLeft.y());
        double botW = std::hypot(bottomRight.x() - bottomLeft.x(), bottomRight.y() - bottomLeft.y());
        double leftH = std::hypot(bottomLeft.x() - topLeft.x(), bottomLeft.y() - topLeft.y());
        double rightH = std::hypot(bottomRight.x() - topRight.x(), bottomRight.y() - topRight.y());

        targetWidth = static_cast<int>(std::max(topW, botW));
        targetHeight = static_cast<int>(std::max(leftH, rightH));
    }

    if (targetWidth < 50) targetWidth = 800;
    if (targetHeight < 50) targetHeight = 1000;

    QPolygonF srcQuad;
    srcQuad << topLeft << topRight << bottomRight << bottomLeft;

    QPolygonF dstQuad;
    dstQuad << QPointF(0, 0)
            << QPointF(targetWidth, 0)
            << QPointF(targetWidth, targetHeight)
            << QPointF(0, targetHeight);

    QTransform transform;
    if (!QTransform::quadToQuad(srcQuad, dstQuad, transform)) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Perspective matrix calculation failed.");
    }

    QImage dstImg(targetWidth, targetHeight, QImage::Format_RGB32);
    dstImg.fill(Qt::white);

    QPainter painter(&dstImg);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setTransform(transform);
    painter.drawImage(0, 0, srcImg);
    painter.end();

    return saveImageOrPdf(dstImg, outputPath);
}

Result<void> DocumentBackend::enhanceDocumentScan(const QString& inputPath, const QString& outputPath,
                                                  const ScanEnhanceOptions& options) {
    if (Services::FileService::instance().pathsReferToSameFile(inputPath, outputPath)) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                  "The output cannot replace the source image.");
    }
    auto imgRes = loadImage(inputPath);
    if (imgRes.hasError()) return Result<void>::fail(imgRes.error());

    QImage srcImg = imgRes.value().convertToFormat(QImage::Format_RGB32);
    int w = srcImg.width();
    int h = srcImg.height();

    QImage enhanced(w, h, QImage::Format_RGB32);

    for (int y = 0; y < h; ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(srcImg.constScanLine(y));
        QRgb* dstLine = reinterpret_cast<QRgb*>(enhanced.scanLine(y));

        for (int x = 0; x < w; ++x) {
            QRgb pixel = srcLine[x];
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            // Compute luminance
            int lum = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b);

            if (options.toGrayscale) {
                r = g = b = lum;
            }

            if (options.autoContrast || options.removeShadows) {
                // Adaptive threshold: push near-white/gray background directly to pure white (#FFFFFF)
                if (lum > 185) {
                    r = 255;
                    g = 255;
                    b = 255;
                } else {
                    // Darken text for high crisp readability
                    double cFactor = options.contrast;
                    r = std::clamp(static_cast<int>((r - 128) * cFactor + 128 - (options.brightness * 255)), 0, 255);
                    g = std::clamp(static_cast<int>((g - 128) * cFactor + 128 - (options.brightness * 255)), 0, 255);
                    b = std::clamp(static_cast<int>((b - 128) * cFactor + 128 - (options.brightness * 255)), 0, 255);
                }
            }

            dstLine[x] = qRgb(r, g, b);
        }
    }

    return saveImageOrPdf(enhanced, outputPath);
}

} // namespace LocalTools::Core
