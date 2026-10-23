#ifndef LOCALTOOLS_CORE_BACKENDS_IMAGE_BACKEND_H
#define LOCALTOOLS_CORE_BACKENDS_IMAGE_BACKEND_H

#include <QImage>
#include <QRect>
#include <QString>
#include <QStringList>
#include <vector>
#include "../result.h"

namespace LocalTools::Core {

/**
 * Image processing backend using Qt QImage.
 * Handles format conversion, resize, crop, rotate, flip, and compression.
 * No external dependency beyond Qt 6.
 */
class ImageBackend {
public:
    // Supported format detection
    static QStringList supportedReadFormats();
    static QStringList supportedWriteFormats();
    static bool isImageFile(const QString& filePath);

    // Format conversion
    static Result<void> convertFormat(const QString& inputPath, const QString& outputPath, 
                                       const QString& format, int quality = -1);

    // Compression (save with specified quality)
    static Result<void> compress(const QString& inputPath, const QString& outputPath,
                                  int quality = 85);

    // Smart compression to reach a target file size (e.g. 500 KB, 1 MB)
    static Result<void> compressToTargetSize(const QString& inputPath, const QString& outputPath,
                                            qint64 targetSizeBytes);

    // Strip EXIF and privacy metadata from image
    static Result<void> stripMetadata(const QString& inputPath, const QString& outputPath);

    // Resize
    struct ResizeOptions {
        int width{0};
        int height{0};
        bool keepAspectRatio{true};
        double scalePercent{0.0}; // If > 0, width/height are ignored
        Qt::TransformationMode mode{Qt::SmoothTransformation};
    };
    static Result<void> resize(const QString& inputPath, const QString& outputPath,
                                const ResizeOptions& options);

    // Crop
    static Result<void> crop(const QString& inputPath, const QString& outputPath,
                              const QRect& cropRect);
    static Result<void> cropAspectRatio(const QString& inputPath, const QString& outputPath,
                                         double aspectW, double aspectH);
    static QRect calculateAspectCropRect(int imageW, int imageH, double aspectW, double aspectH);
    static QImage smartCropIdCard(const QImage& img);

    // Rotate (degrees: 90, 180, 270)
    static Result<void> rotate(const QString& inputPath, const QString& outputPath,
                                int degrees);

    // Flip
    static Result<void> flipHorizontal(const QString& inputPath, const QString& outputPath);
    static Result<void> flipVertical(const QString& inputPath, const QString& outputPath);

    // Image info
    struct ImageInfo {
        int width{0};
        int height{0};
        int depth{0};         // bits per pixel
        int dotsPerMeterX{0};
        int dotsPerMeterY{0};
        qint64 fileSize{0};
        QString format;
        bool hasAlpha{false};
    };
    static Result<ImageInfo> getInfo(const QString& filePath);

private:
    static Result<QImage> loadImage(const QString& path);
    static Result<void> saveImage(const QImage& image, const QString& path, 
                                   const QString& format = {}, int quality = -1);
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_BACKENDS_IMAGE_BACKEND_H
