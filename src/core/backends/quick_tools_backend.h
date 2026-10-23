#ifndef LOCALTOOLS_CORE_BACKENDS_QUICK_TOOLS_BACKEND_H
#define LOCALTOOLS_CORE_BACKENDS_QUICK_TOOLS_BACKEND_H

#include <QString>
#include <QImage>
#include <QByteArray>
#include <vector>
#include "../result.h"

namespace LocalTools::Core {

/**
 * Backend for Quick Tools:
 * - QR Code Generator (PNG / SVG)
 * - QR Code Reader / Decoder
 * - Metadata Inspector & Cleaner (EXIF, GPS, Device, PDF, Image Details)
 * - JSON Formatter & Minifier
 * - Base64 Encoder / Decoder
 * - URL Encoder / Decoder
 */

// --- 1. QR Code Options & Results ---
struct QrOptions {
    int size{512};         // Output image size in pixels
    int margin{4};         // Quiet zone margin modules
    QColor foreground{Qt::black};
    QColor background{Qt::white};
};

struct QrDecodeResult {
    bool success{false};
    QString text;
    QString errorMessage;
    int version{0};
    QString eccLevel;
};

struct MetadataItem {
    QString category; // Localized, user-facing metadata category.
    QString key;
    QString value;

    MetadataItem() = default;
    MetadataItem(const QString& c, const QString& k, const QString& v)
        : category(c), key(k), value(v) {}
};

struct FileMetadataReport {
    QString filePath;
    QString fileName;
    qint64 fileSizeBytes{0};
    QString fileType;
    bool hasMetadata{false};
    std::vector<MetadataItem> items;
};

class QuickToolsBackend {
public:
    // --- QR Code Generator ---
    static Result<QImage> generateQrImage(const QString& text, const QrOptions& options);
    static Result<QImage> generateQrImage(const QString& text);
    static Result<void> saveQrCode(const QString& text, const QString& outputPath, const QrOptions& options);
    static Result<void> saveQrCode(const QString& text, const QString& outputPath);

    // --- QR Code Reader ---
    static Result<QrDecodeResult> decodeQrImage(const QImage& img);
    static Result<QrDecodeResult> decodeQrFile(const QString& filePath);

    // --- Metadata Inspector & Cleaner ---
    static Result<FileMetadataReport> inspectMetadata(const QString& filePath);
    static Result<void> cleanMetadata(const QString& inputPath, const QString& outputPath);

    // --- JSON Formatting ---
    static Result<QString> formatJson(const QString& inputJson, int indent = 2);
    static Result<QString> minifyJson(const QString& inputJson);

    // --- Base64 & URL Utilities ---
    static QString encodeBase64(const QString& text);
    static Result<QString> decodeBase64(const QString& base64Text);

    static QString urlEncode(const QString& text);
    static QString urlDecode(const QString& text);
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_BACKENDS_QUICK_TOOLS_BACKEND_H
