#ifndef LOCALTOOLS_CORE_BACKENDS_DOCUMENT_BACKEND_H
#define LOCALTOOLS_CORE_BACKENDS_DOCUMENT_BACKEND_H

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <vector>
#include "../result.h"

namespace LocalTools::Core {

/**
 * Backend engine for Document & ID preparation tools.
 * Handles A4 ID copy sheet generation, passport/biometric photo grid layout,
 * perspective transformation (deskewing), and document scan contrast enhancement.
 */
class DocumentBackend {
public:
    // --- ID Copy Sheet ---
    struct IdCopyOptions {
        int copyCount{2};          // 1, 2, or 4 copies
        bool isA4{true};           // true = A4 (210x297mm), false = Letter (215.9x279.4mm)
        int dpi{300};              // 300 DPI for high-quality printing
        int marginMm{20};          // Page margin in millimeters
        int spacingMm{15};         // Spacing between front and back
        bool drawCutLines{true};   // Draw subtle dashed lines around cards
    };

    static Result<void> createIdCopySheet(const QString& frontPath, const QString& backPath,
                                          const QString& outputPath, const IdCopyOptions& options);

    // --- Passport / Biometric Photo Grid ---
    struct PassportPhotoOptions {
        double photoWidthMm{35.0};   // e.g. 35mm (standard Turkish passport/visa)
        double photoHeightMm{45.0};  // e.g. 45mm
        int photoCount{6};           // 4, 6, or 8 photos
        bool isA4Paper{false};       // false = 10x15 cm photo paper, true = A4
        int dpi{300};
        bool drawBorder{true};       // 1px subtle guide border for cutting
    };

    static Result<void> generatePassportPhotoGrid(const QString& photoPath, const QString& outputPath,
                                                  const PassportPhotoOptions& options);

    // --- Perspective Correction (Deskew) ---
    static Result<void> correctPerspective(const QString& inputPath, const QString& outputPath,
                                           const QPointF& topLeft, const QPointF& topRight,
                                           const QPointF& bottomRight, const QPointF& bottomLeft,
                                           int targetWidth = 0, int targetHeight = 0);

    // --- Document Scan Enhancement ---
    struct ScanEnhanceOptions {
        bool autoContrast{true};
        bool removeShadows{true};
        bool toGrayscale{false};
        double brightness{0.1};
        double contrast{1.3};
    };

    static Result<void> enhanceDocumentScan(const QString& inputPath, const QString& outputPath,
                                            const ScanEnhanceOptions& options);

private:
    static Result<QImage> loadImage(const QString& path);
    static Result<void> saveImageOrPdf(const QImage& image, const QString& outputPath, int dpi = 300);
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_BACKENDS_DOCUMENT_BACKEND_H
