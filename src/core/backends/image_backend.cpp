#include "image_backend.h"
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QTransform>
#include <QPainter>
#include <QBuffer>
#include <QFile>

#include "../../services/file_service.h"

namespace LocalTools::Core {

namespace {

Result<void> validateImageOutputPath(const QString& inputPath, const QString& outputPath) {
    if (outputPath.trimmed().isEmpty()) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                  "Output image path is empty.");
    }
    if (Services::FileService::instance().pathsReferToSameFile(inputPath, outputPath)) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                  "For security reasons, the source image cannot be overwritten.");
    }
    return Result<void>::ok();
}

} // namespace

QStringList ImageBackend::supportedReadFormats() {
    QStringList formats;
    for (const QByteArray& fmt : QImageReader::supportedImageFormats()) {
        formats.append(QString::fromLatin1(fmt).toLower());
    }
    return formats;
}

QStringList ImageBackend::supportedWriteFormats() {
    QStringList formats;
    for (const QByteArray& fmt : QImageWriter::supportedImageFormats()) {
        formats.append(QString::fromLatin1(fmt).toLower());
    }
    return formats;
}

bool ImageBackend::isImageFile(const QString& filePath) {
    QFileInfo fi(filePath);
    if (!fi.exists()) return false;
    QString ext = fi.suffix().toLower();
    static const QStringList imageExts = {"jpg", "jpeg", "png", "bmp", "webp", "tiff", "tif", "gif", "ico", "svg"};
    return imageExts.contains(ext);
}

Result<QImage> ImageBackend::loadImage(const QString& path) {
    QFileInfo fi(path);
    if (!fi.exists()) {
        return Result<QImage>::fail(PdfErrorCategory::FileNotFound,
                                    "File not found: " + path.toStdString());
    }

    QImage image(path);
    if (image.isNull()) {
        return Result<QImage>::fail(PdfErrorCategory::BackendError,
                                    "Failed to load image file: " + path.toStdString());
    }
    return Result<QImage>::ok(std::move(image));
}

Result<void> ImageBackend::saveImage(const QImage& image, const QString& path,
                                      const QString& format, int quality) {
    QByteArray fmt;
    if (!format.isEmpty()) {
        fmt = format.toLatin1().toLower();
    } else {
        // Detect from extension
        QFileInfo fi(path);
        fmt = fi.suffix().toLower().toLatin1();
    }

    if (fmt == "jpg") fmt = "jpeg";

    bool ok = image.save(path, fmt.constData(), quality);
    if (!ok) {
        return Result<void>::fail(PdfErrorCategory::BackendError,
                                  "Failed to save image (" + fmt.toUpper().toStdString() + "): " + path.toStdString());
    }
    return Result<void>::ok();
}

Result<void> ImageBackend::convertFormat(const QString& inputPath, const QString& outputPath,
                                          const QString& format, int quality) {
    auto pathCheck = validateImageOutputPath(inputPath, outputPath);
    if (pathCheck.hasError()) return pathCheck;
    auto imgResult = loadImage(inputPath);
    if (imgResult.hasError()) return Result<void>::fail(imgResult.error());

    QImage image = imgResult.value();

    // Handle alpha channel: JPEG doesn't support transparency
    QString fmtLower = format.toLower();
    if ((fmtLower == "jpg" || fmtLower == "jpeg") && image.hasAlphaChannel()) {
        QImage noAlpha(image.size(), QImage::Format_RGB32);
        noAlpha.fill(Qt::white);
        QPainter painter(&noAlpha);
        painter.drawImage(0, 0, image);
        painter.end();
        image = noAlpha;
    }

    return saveImage(image, outputPath, format, quality);
}

Result<void> ImageBackend::compress(const QString& inputPath, const QString& outputPath,
                                     int quality) {
    auto pathCheck = validateImageOutputPath(inputPath, outputPath);
    if (pathCheck.hasError()) return pathCheck;
    auto imgResult = loadImage(inputPath);
    if (imgResult.hasError()) return Result<void>::fail(imgResult.error());

    return saveImage(imgResult.value(), outputPath, {}, quality);
}

Result<void> ImageBackend::compressToTargetSize(const QString& inputPath, const QString& outputPath,
                                                qint64 targetSizeBytes) {
    auto pathCheck = validateImageOutputPath(inputPath, outputPath);
    if (pathCheck.hasError()) return pathCheck;
    if (targetSizeBytes <= 0) {
        return Result<void>::fail(PdfErrorCategory::InvalidInput,
                                  "Target image size must be greater than zero.");
    }
    auto imgResult = loadImage(inputPath);
    if (imgResult.hasError()) return Result<void>::fail(imgResult.error());

    QImage originalImage = imgResult.value();
    QFileInfo inputFi(inputPath);

    if (inputFi.size() <= targetSizeBytes) {
        // Already smaller than target size, just copy or re-save
        return saveImage(originalImage, outputPath, {}, 90);
    }

    // Binary search on quality between 15 and 90
    int lowQ = 15;
    int highQ = 90;
    QByteArray buffer;

    while (lowQ <= highQ) {
        int midQ = (lowQ + highQ) / 2;
        QByteArray tempBuf;
        QBuffer qbuf(&tempBuf);
        qbuf.open(QIODevice::WriteOnly);
        originalImage.save(&qbuf, "JPEG", midQ);

        if (tempBuf.size() <= targetSizeBytes) {
            buffer = tempBuf;
            lowQ = midQ + 1; // Try higher quality
        } else {
            highQ = midQ - 1; // Try lower quality
        }
    }

    // If even lowest quality is larger than target size, scale down resolution
    QImage currentImage = originalImage;
    double scaleFactor = 0.85;
    while ((buffer.isEmpty() || buffer.size() > targetSizeBytes) && currentImage.width() > 300) {
        int newW = static_cast<int>(currentImage.width() * scaleFactor);
        int newH = static_cast<int>(currentImage.height() * scaleFactor);
        currentImage = currentImage.scaled(newW, newH, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        QByteArray tempBuf;
        QBuffer qbuf(&tempBuf);
        qbuf.open(QIODevice::WriteOnly);
        currentImage.save(&qbuf, "JPEG", 70);

        if (tempBuf.size() <= targetSizeBytes || scaleFactor < 0.3) {
            buffer = tempBuf;
            break;
        }
        scaleFactor -= 0.15;
    }

    if (buffer.isEmpty()) {
        // Fallback: save current image with low quality
        return saveImage(currentImage, outputPath, "JPEG", 30);
    }

    QFile outFile(outputPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                  "Failed to write output file: " + outputPath.toStdString());
    }
    outFile.write(buffer);
    outFile.close();
    return Result<void>::ok();
}

Result<void> ImageBackend::stripMetadata(const QString& inputPath, const QString& outputPath) {
    auto pathCheck = validateImageOutputPath(inputPath, outputPath);
    if (pathCheck.hasError()) return pathCheck;
    auto imgResult = loadImage(inputPath);
    if (imgResult.hasError()) return Result<void>::fail(imgResult.error());

    QImage rawImage = imgResult.value();

    // Create a pristine image copy to discard all text metadata, EXIF, and ICC profile chunks
    QImage cleanImage = rawImage.copy();
    for (const QString& key : cleanImage.textKeys()) {
        cleanImage.setText(key, QString());
    }

    return saveImage(cleanImage, outputPath, {}, 95);
}

Result<void> ImageBackend::resize(const QString& inputPath, const QString& outputPath,
                                   const ResizeOptions& options) {
    auto pathCheck = validateImageOutputPath(inputPath, outputPath);
    if (pathCheck.hasError()) return pathCheck;
    auto imgResult = loadImage(inputPath);
    if (imgResult.hasError()) return Result<void>::fail(imgResult.error());

    QImage image = imgResult.value();
    QImage scaled;

    if (options.scalePercent > 0.0) {
        int newW = static_cast<int>(image.width() * options.scalePercent / 100.0);
        int newH = static_cast<int>(image.height() * options.scalePercent / 100.0);
        scaled = image.scaled(newW, newH, Qt::IgnoreAspectRatio, options.mode);
    } else if (options.width > 0 && options.height > 0) {
        Qt::AspectRatioMode ratio = options.keepAspectRatio ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio;
        scaled = image.scaled(options.width, options.height, ratio, options.mode);
    } else if (options.width > 0) {
        double factor = static_cast<double>(options.width) / image.width();
        int newH = static_cast<int>(image.height() * factor);
        scaled = image.scaled(options.width, newH, Qt::IgnoreAspectRatio, options.mode);
    } else if (options.height > 0) {
        double factor = static_cast<double>(options.height) / image.height();
        int newW = static_cast<int>(image.width() * factor);
        scaled = image.scaled(newW, options.height, Qt::IgnoreAspectRatio, options.mode);
    } else {
        return Result<void>::fail(PdfErrorCategory::BackendError,
                                  "Invalid resize options.");
    }

    if (scaled.isNull()) {
        return Result<void>::fail(PdfErrorCategory::BackendError,
                                  "Image resizing failed.");
    }

    return saveImage(scaled, outputPath);
}

Result<void> ImageBackend::crop(const QString& inputPath, const QString& outputPath,
                                 const QRect& cropRect) {
    auto pathCheck = validateImageOutputPath(inputPath, outputPath);
    if (pathCheck.hasError()) return pathCheck;
    auto imgResult = loadImage(inputPath);
    if (imgResult.hasError()) return Result<void>::fail(imgResult.error());

    QImage image = imgResult.value();

    if (!QRect(0, 0, image.width(), image.height()).contains(cropRect)) {
        return Result<void>::fail(PdfErrorCategory::InvalidPageRange,
                                  "Crop area is outside image bounds.");
    }

    QImage cropped = image.copy(cropRect);
    if (cropped.isNull()) {
        return Result<void>::fail(PdfErrorCategory::BackendError,
                                  "Image crop failed.");
    }

    return saveImage(cropped, outputPath);
}

QRect ImageBackend::calculateAspectCropRect(int imgW, int imgH, double aspectW, double aspectH) {
    if (imgW <= 0 || imgH <= 0 || aspectW <= 0 || aspectH <= 0) {
        return QRect(0, 0, imgW, imgH);
    }
    double targetRatio = aspectW / aspectH;
    double currentRatio = static_cast<double>(imgW) / imgH;

    int cropW = imgW;
    int cropH = imgH;

    if (currentRatio > targetRatio) {
        // Image is wider than target: trim left and right
        cropW = static_cast<int>(std::round(imgH * targetRatio));
    } else {
        // Image is taller than target: trim top and bottom
        cropH = static_cast<int>(std::round(imgW / targetRatio));
    }

    // Safety clamp
    cropW = std::clamp(cropW, 1, imgW);
    cropH = std::clamp(cropH, 1, imgH);

    int cropX = std::max(0, (imgW - cropW) / 2);
    int cropY = std::max(0, (imgH - cropH) / 2);

    return QRect(cropX, cropY, cropW, cropH);
}

Result<void> ImageBackend::cropAspectRatio(const QString& inputPath, const QString& outputPath,
                                           double aspectW, double aspectH) {
    auto pathCheck = validateImageOutputPath(inputPath, outputPath);
    if (pathCheck.hasError()) return pathCheck;
    if (aspectW <= 0.0 || aspectH <= 0.0) {
        return Result<void>::fail(PdfErrorCategory::InvalidInput,
                                  "Crop aspect ratio must be greater than zero.");
    }
    auto imgResult = loadImage(inputPath);
    if (imgResult.hasError()) return Result<void>::fail(imgResult.error());

    QImage image = imgResult.value();
    QRect cropRect = calculateAspectCropRect(image.width(), image.height(), aspectW, aspectH);
    QImage cropped = image.copy(cropRect);
    if (cropped.isNull()) {
        return Result<void>::fail(PdfErrorCategory::BackendError,
                                  "Image crop failed.");
    }

    return saveImage(cropped, outputPath);
}

Result<void> ImageBackend::rotate(const QString& inputPath, const QString& outputPath,
                                   int degrees) {
    auto pathCheck = validateImageOutputPath(inputPath, outputPath);
    if (pathCheck.hasError()) return pathCheck;
    if (degrees % 90 != 0) {
        return Result<void>::fail(PdfErrorCategory::InvalidInput,
                                  "Image rotation must be a multiple of 90 degrees.");
    }
    auto imgResult = loadImage(inputPath);
    if (imgResult.hasError()) return Result<void>::fail(imgResult.error());

    QTransform transform;
    transform.rotate(degrees);
    QImage rotated = imgResult.value().transformed(transform, Qt::SmoothTransformation);

    if (rotated.isNull()) {
        return Result<void>::fail(PdfErrorCategory::BackendError,
                                  "Image rotation failed.");
    }

    return saveImage(rotated, outputPath);
}

Result<void> ImageBackend::flipHorizontal(const QString& inputPath, const QString& outputPath) {
    auto pathCheck = validateImageOutputPath(inputPath, outputPath);
    if (pathCheck.hasError()) return pathCheck;
    auto imgResult = loadImage(inputPath);
    if (imgResult.hasError()) return Result<void>::fail(imgResult.error());

    QTransform transform;
    transform.scale(-1, 1);
    QImage flipped = imgResult.value().transformed(transform);
    return saveImage(flipped, outputPath);
}

Result<void> ImageBackend::flipVertical(const QString& inputPath, const QString& outputPath) {
    auto pathCheck = validateImageOutputPath(inputPath, outputPath);
    if (pathCheck.hasError()) return pathCheck;
    auto imgResult = loadImage(inputPath);
    if (imgResult.hasError()) return Result<void>::fail(imgResult.error());

    QTransform transform;
    transform.scale(1, -1);
    QImage flipped = imgResult.value().transformed(transform);
    return saveImage(flipped, outputPath);
}

Result<ImageBackend::ImageInfo> ImageBackend::getInfo(const QString& filePath) {
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        return Result<ImageInfo>::fail(PdfErrorCategory::FileNotFound,
                                       "File not found: " + filePath.toStdString());
    }

    QImageReader reader(filePath);
    if (!reader.canRead()) {
        return Result<ImageInfo>::fail(PdfErrorCategory::BackendError,
                                       "Image file cannot be read: " + filePath.toStdString());
    }

    QSize size = reader.size();
    QImage image = reader.read();

    ImageInfo info;
    info.width = size.width();
    info.height = size.height();
    info.depth = image.isNull() ? 0 : image.depth();
    info.dotsPerMeterX = image.isNull() ? 0 : image.dotsPerMeterX();
    info.dotsPerMeterY = image.isNull() ? 0 : image.dotsPerMeterY();
    info.fileSize = fi.size();
    info.format = QString::fromLatin1(reader.format());

    info.hasAlpha = !image.isNull() && image.hasAlphaChannel();

    return Result<ImageInfo>::ok(info);
}

QImage ImageBackend::smartCropIdCard(const QImage& img) {
    if (img.isNull()) return img;
    
    int w = img.width();
    int h = img.height();
    
    int top = 0, bottom = h - 1, left = 0, right = w - 1;
    
    // Very simple margin detection by comparing with corners
    int bgThreshold = 30; // difference threshold
    int cornerPixels[4] = {
        qGray(img.pixel(0, 0)),
        qGray(img.pixel(w-1, 0)),
        qGray(img.pixel(0, h-1)),
        qGray(img.pixel(w-1, h-1))
    };
    int bgGray = (cornerPixels[0] + cornerPixels[1] + cornerPixels[2] + cornerPixels[3]) / 4;
    
    auto isBackground = [&](int x, int y) {
        return std::abs(qGray(img.pixel(x, y)) - bgGray) < bgThreshold;
    };
    
    // Scan Top
    for (int y = 0; y < h / 3; ++y) {
        int bgCount = 0;
        for (int x = 0; x < w; ++x) { if (isBackground(x, y)) bgCount++; }
        if (bgCount < w * 0.85) { top = y; break; }
    }
    // Scan Bottom
    for (int y = h - 1; y > h * 2 / 3; --y) {
        int bgCount = 0;
        for (int x = 0; x < w; ++x) { if (isBackground(x, y)) bgCount++; }
        if (bgCount < w * 0.85) { bottom = y; break; }
    }
    // Scan Left
    for (int x = 0; x < w / 3; ++x) {
        int bgCount = 0;
        for (int y = 0; y < h; ++y) { if (isBackground(x, y)) bgCount++; }
        if (bgCount < h * 0.85) { left = x; break; }
    }
    // Scan Right
    for (int x = w - 1; x > w * 2 / 3; --x) {
        int bgCount = 0;
        for (int y = 0; y < h; ++y) { if (isBackground(x, y)) bgCount++; }
        if (bgCount < h * 0.85) { right = x; break; }
    }
    
    QRect cropRect(left, top, right - left + 1, bottom - top + 1);
    
    // Ensure it's not too small (if detection failed)
    if (cropRect.width() < w * 0.4 || cropRect.height() < h * 0.4) {
        // Fallback to center 85%
        cropRect = QRect(w * 0.075, h * 0.075, w * 0.85, h * 0.85);
    }
    
    return img.copy(cropRect);
}

} // namespace LocalTools::Core
