#include "tool_registry.h"
#include "../services/settings_service.h"
#include <algorithm>

namespace LocalTools::Core {

ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry instance;
    return instance;
}

ToolRegistry::ToolRegistry() {
    // Load favorites from settings
    m_favoriteIds = LocalTools::Services::SettingsService::instance().getFavoriteTools();

    // Register all built-in tools
    retranslateTools();
}

void ToolRegistry::retranslateTools() {
    m_tools.clear();
    registerPdfTools();
    registerImageTools();
    registerDocumentTools();
    registerFileTools();
    registerVideoTools();
    registerAudioTools();
    registerQuickTools();
}

void ToolRegistry::registerTool(ToolDescriptor descriptor) {
    m_tools.push_back(std::move(descriptor));
}

std::vector<const ToolDescriptor*> ToolRegistry::toolsByCategory(const QString& category) const {
    std::vector<const ToolDescriptor*> result;
    for (const auto& tool : m_tools) {
        if (tool.category == category) {
            result.push_back(&tool);
        }
    }
    std::sort(result.begin(), result.end(), [](const ToolDescriptor* a, const ToolDescriptor* b) {
        return a->sortOrder < b->sortOrder;
    });
    return result;
}

std::vector<const ToolDescriptor*> ToolRegistry::toolsBySubcategory(const QString& category, const QString& subcategory) const {
    std::vector<const ToolDescriptor*> result;
    for (const auto& tool : m_tools) {
        if (tool.category == category && tool.subcategory == subcategory) {
            result.push_back(&tool);
        }
    }
    std::sort(result.begin(), result.end(), [](const ToolDescriptor* a, const ToolDescriptor* b) {
        return a->sortOrder < b->sortOrder;
    });
    return result;
}

const ToolDescriptor* ToolRegistry::toolById(const QString& id) const {
    for (const auto& tool : m_tools) {
        if (tool.id == id) return &tool;
    }
    return nullptr;
}

const ToolDescriptor* ToolRegistry::toolByLegacyId(int legacyToolId) const {
    for (const auto& tool : m_tools) {
        if (tool.legacyToolId == legacyToolId) return &tool;
    }
    return nullptr;
}

std::vector<const ToolDescriptor*> ToolRegistry::search(const QString& query) const {
    if (query.trimmed().isEmpty()) return {};

    QString lowerQuery = query.toLower().trimmed();
    std::vector<std::pair<const ToolDescriptor*, int>> scored;

    for (const auto& tool : m_tools) {
        int score = 0;

        // Exact name match (highest priority)
        if (tool.name.toLower().contains(lowerQuery)) {
            score += 100;
        }

        // Keyword match
        for (const QString& kw : tool.keywords) {
            if (kw.toLower().contains(lowerQuery)) {
                score += 50;
            }
        }

        // Description match
        if (tool.description.toLower().contains(lowerQuery)) {
            score += 20;
        }

        // Category/subcategory match
        if (tool.category.toLower().contains(lowerQuery) ||
            tool.subcategory.toLower().contains(lowerQuery)) {
            score += 30;
        }

        // Extension match (e.g. user types "jpg")
        QString dotQuery = lowerQuery.startsWith('.') ? lowerQuery : ("." + lowerQuery);
        for (const QString& ext : tool.inputExtensions) {
            if (ext.toLower() == dotQuery) {
                score += 40;
            }
        }
        for (const QString& ext : tool.outputExtensions) {
            if (ext.toLower() == dotQuery) {
                score += 40;
            }
        }

        if (score > 0) {
            scored.push_back({&tool, score});
        }
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    std::vector<const ToolDescriptor*> result;
    result.reserve(scored.size());
    for (const auto& [tool, score] : scored) {
        (void)score;
        result.push_back(tool);
    }
    return result;
}

QStringList ToolRegistry::categories() const {
    QStringList cats;
    for (const auto& tool : m_tools) {
        if (!cats.contains(tool.category)) {
            cats.append(tool.category);
        }
    }
    return cats;
}

QStringList ToolRegistry::subcategories(const QString& category) const {
    QStringList subs;
    for (const auto& tool : m_tools) {
        if (tool.category == category && !tool.subcategory.isEmpty() && !subs.contains(tool.subcategory)) {
            subs.append(tool.subcategory);
        }
    }
    return subs;
}

void ToolRegistry::setFavorite(const QString& toolId, bool isFavorite) {
    if (isFavorite) {
        if (!m_favoriteIds.contains(toolId)) {
            m_favoriteIds.append(toolId);
        }
    } else {
        m_favoriteIds.removeAll(toolId);
    }
    LocalTools::Services::SettingsService::instance().setFavoriteTools(m_favoriteIds);
}

bool ToolRegistry::isFavorite(const QString& toolId) const {
    return m_favoriteIds.contains(toolId);
}

std::vector<const ToolDescriptor*> ToolRegistry::favorites() const {
    std::vector<const ToolDescriptor*> result;
    for (const QString& favId : m_favoriteIds) {
        if (const ToolDescriptor* tool = toolById(favId)) {
            result.push_back(tool);
        }
    }
    return result;
}

// ─── Built-in Tool Registration ────────────────────────────────────────────────

void ToolRegistry::registerPdfTools() {
    using IT = LocalTools::UI::IconType;

    registerTool({
        .id = "pdf.merge",
        .name = QObject::tr("Merge PDF"),
        .description = QObject::tr("Combine multiple PDF documents into a single file."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Organize"),
        .iconType = IT::Merge,
        .keywords = {"birleştir", "merge", "combine", "pdf", "topla"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".pdf"},
        .supportsBatch = true,
        .isAvailable = true,
        .sortOrder = 1,
        .legacyToolId = 1,
        .createOperation = nullptr // Uses legacy PdfWorker path
    });

    registerTool({
        .id = "pdf.split",
        .name = QObject::tr("Split PDF"),
        .description = QObject::tr("Split PDF document into parts by page ranges."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Organize"),
        .iconType = IT::Split,
        .keywords = {"böl", "split", "ayır", "parçala", "pdf"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".pdf"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 2,
        .legacyToolId = 2,
        .createOperation = nullptr
    });

    registerTool({
        .id = "pdf.extract",
        .name = QObject::tr("Extract Pages"),
        .description = QObject::tr("Extract selected pages into a new PDF document."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Organize"),
        .iconType = IT::Extract,
        .keywords = {"çıkar", "extract", "sayfa", "page", "pdf"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".pdf"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 3,
        .legacyToolId = 3,
        .createOperation = nullptr
    });

    registerTool({
        .id = "pdf.delete",
        .name = QObject::tr("Delete Pages"),
        .description = QObject::tr("Remove unwanted pages from document."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Organize"),
        .iconType = IT::Delete,
        .keywords = {"sil", "delete", "kaldır", "remove", "sayfa", "pdf"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".pdf"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 4,
        .legacyToolId = 4,
        .createOperation = nullptr
    });

    registerTool({
        .id = "pdf.reorder",
        .name = QObject::tr("Reorder Pages"),
        .description = QObject::tr("Rearrange page order by dragging."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Organize"),
        .iconType = IT::Reorder,
        .keywords = {"sırala", "reorder", "düzenle", "sort", "pdf"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".pdf"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 5,
        .legacyToolId = 5,
        .createOperation = nullptr
    });

    registerTool({
        .id = "pdf.rotate",
        .name = QObject::tr("Rotate Pages"),
        .description = QObject::tr("Rotate page orientations by 90/180/270 degrees."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Organize"),
        .iconType = IT::Rotate,
        .keywords = {"döndür", "rotate", "çevir", "yön", "pdf"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".pdf"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 6,
        .legacyToolId = 6,
        .createOperation = nullptr
    });

    registerTool({
        .id = "pdf.images-to-pdf",
        .name = QObject::tr("Images to PDF"),
        .description = QObject::tr("Convert PNG, JPG, WEBP images into a PDF document."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Convert"),
        .iconType = IT::ImagesToPdf,
        .keywords = {"görsel", "images", "pdf", "dönüştür", "convert", "jpg", "png"},
        .inputExtensions = {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tiff"},
        .outputExtensions = {".pdf"},
        .supportsBatch = true,
        .isAvailable = true,
        .sortOrder = 7,
        .legacyToolId = 7,
        .createOperation = nullptr
    });

    registerTool({
        .id = "pdf.pdf-to-images",
        .name = QObject::tr("PDF to Images"),
        .description = QObject::tr("Convert PDF pages to PNG/JPEG images."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Convert"),
        .iconType = IT::PdfToImages,
        .keywords = {"görsel", "images", "pdf", "dönüştür", "convert", "png", "jpeg"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".png", ".jpg"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 8,
        .legacyToolId = 8,
        .createOperation = nullptr
    });

    registerTool({
        .id = "pdf.pdf-to-text",
        .name = QObject::tr("PDF to Text"),
        .description = QObject::tr("Export PDF text content as a .txt file."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Convert"),
        .iconType = IT::PdfToText,
        .keywords = {"metin", "text", "pdf", "çıkar", "extract", "txt"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".txt"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 9,
        .legacyToolId = 9,
        .createOperation = nullptr
    });

    registerTool({
        .id = "pdf.compress",
        .name = QObject::tr("Compress PDF"),
        .description = QObject::tr("Reduce PDF file size without noticeable quality loss."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Optimize"),
        .iconType = IT::Compress,
        .keywords = {"sıkıştır", "compress", "küçült", "reduce", "optimize", "pdf"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".pdf"},
        .supportsBatch = true,
        .isAvailable = true,
        .sortOrder = 10,
        .legacyToolId = 10,
        .createOperation = nullptr
    });

    registerTool({
        .id = "pdf.metadata",
        .name = QObject::tr("Metadata"),
        .description = QObject::tr("Edit document title, author, and subject details."),
        .category = QObject::tr("Metadata"),
        .subcategory = QObject::tr("Metadata"),
        .iconType = IT::Metadata,
        .keywords = {"metaveri", "metadata", "başlık", "yazar", "title", "author", "pdf"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".pdf"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 11,
        .legacyToolId = 11,
        .createOperation = nullptr
    });

    // Future PDF tools (coming soon)
    registerTool({
        .id = "pdf.watermark",
        .name = QObject::tr("Add Watermark"),
        .description = QObject::tr("Add text or image watermark to PDF pages."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Organize"),
        .iconType = IT::Edit,
        .keywords = {"filigran", "watermark", "damga", "overlay", "pdf"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".pdf"},
        .supportsBatch = true,
        .isAvailable = false,
        .sortOrder = 20,
        .legacyToolId = -1,
        .createOperation = nullptr
    });

    registerTool({
        .id = "pdf.protect",
        .name = QObject::tr("Protect PDF"),
        .description = QObject::tr("Add password protection to PDF document."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Security"),
        .iconType = IT::Success,
        .keywords = {"koru", "protect", "şifre", "password", "encrypt", "güvenlik", "pdf"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".pdf"},
        .supportsBatch = false,
        .isAvailable = false,
        .sortOrder = 21,
        .legacyToolId = -1,
        .createOperation = nullptr
    });

    registerTool({
        .id = "pdf.unlock",
        .name = QObject::tr("Unlock PDF"),
        .description = QObject::tr("Unlock password-protected PDF document."),
        .category = QObject::tr("PDF"),
        .subcategory = QObject::tr("Security"),
        .iconType = IT::View,
        .keywords = {"kilit", "unlock", "şifre", "password", "decrypt", "pdf"},
        .inputExtensions = {".pdf"},
        .outputExtensions = {".pdf"},
        .supportsBatch = false,
        .isAvailable = false,
        .sortOrder = 22,
        .legacyToolId = -1,
        .createOperation = nullptr
    });
}

void ToolRegistry::registerImageTools() {
    using IT = LocalTools::UI::IconType;

    registerTool({
        .id = "image.convert",
        .name = QObject::tr("Convert Image"),
        .description = QObject::tr("Convert images to various formats (JPG, PNG, WEBP, BMP, TIFF)."),
        .category = QObject::tr("Images"),
        .subcategory = QObject::tr("Convert"),
        .iconType = IT::Convert,
        .keywords = {"dönüştür", "convert", "jpg", "png", "webp", "bmp", "tiff", "format", "görsel", "image"},
        .inputExtensions = {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tiff", ".tif"},
        .outputExtensions = {".jpg", ".png", ".webp", ".bmp", ".tiff"},
        .supportsBatch = true,
        .isAvailable = true, // V2.1
        .sortOrder = 1,
        .legacyToolId = 102,
        .createOperation = nullptr
    });

    registerTool({
        .id = "image.compress",
        .name = QObject::tr("Compress Image"),
        .description = QObject::tr("Reduce image file size with quality control."),
        .category = QObject::tr("Images"),
        .subcategory = QObject::tr("Compress"),
        .iconType = IT::Compress,
        .keywords = {"sıkıştır", "compress", "küçült", "reduce", "görsel", "image", "quality"},
        .inputExtensions = {".jpg", ".jpeg", ".png", ".webp"},
        .outputExtensions = {".jpg", ".png", ".webp"},
        .supportsBatch = true,
        .isAvailable = true, // V2.1
        .sortOrder = 2,
        .legacyToolId = 101,
        .createOperation = nullptr
    });

    registerTool({
        .id = "image.rotate",
        .name = QObject::tr("Rotate Image"),
        .description = QObject::tr("Rotate or flip images by 90/180/270 degrees."),
        .category = QObject::tr("Images"),
        .subcategory = QObject::tr("Organize"),
        .iconType = IT::Rotate,
        .keywords = {"döndür", "rotate", "çevir", "flip", "görsel", "image"},
        .inputExtensions = {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tiff"},
        .outputExtensions = {".jpg", ".png", ".webp", ".bmp", ".tiff"},
        .supportsBatch = true,
        .isAvailable = false, // no UI panel exists yet - routed to the wrong page via the Organize fallback // V2.1
        .sortOrder = 5,
        .legacyToolId = -1,
        .createOperation = nullptr
    });

}

void ToolRegistry::registerDocumentTools() {
    using IT = LocalTools::UI::IconType;

    registerTool({
        .id = "document.id-copy",
        .name = QObject::tr("ID Card Copy"),
        .description = QObject::tr("Place front and back ID photos on standard A4 print template."),
        .category = QObject::tr("Documents"),
        .subcategory = QObject::tr("ID Card"),
        .iconType = IT::ImagesToPdf,
        .keywords = {"kimlik", "id", "fotokopi", "ehliyet", "a4", "baskı", "print", "belge"},
        .inputExtensions = {".jpg", ".jpeg", ".png", ".webp", ".bmp"},
        .outputExtensions = {".pdf", ".png"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 1,
        .legacyToolId = 201,
        .createOperation = nullptr
    });

    registerTool({
        .id = "document.passport-photo",
        .name = QObject::tr("Passport & Biometric"),
        .description = QObject::tr("Convert portrait photos into official size multi-print templates."),
        .category = QObject::tr("Documents"),
        .subcategory = QObject::tr("Portrait"),
        .iconType = IT::Organize,
        .keywords = {"vesikalık", "biyometrik", "pasaport", "passport", "vize", "visa", "fotoğraf", "photo"},
        .inputExtensions = {".jpg", ".jpeg", ".png", ".webp", ".bmp"},
        .outputExtensions = {".pdf", ".jpg"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 2,
        .legacyToolId = 202,
        .createOperation = nullptr
    });

    registerTool({
        .id = "document.scanner",
        .name = QObject::tr("Document Scanner"),
        .description = QObject::tr("Scan document photos with perspective correction and enhancement."),
        .category = QObject::tr("Documents"),
        .subcategory = QObject::tr("Scanning"),
        .iconType = IT::View,
        .keywords = {"tarayıcı", "scanner", "tara", "düzelt", "deskew", "perspektif", "kontrast", "belge"},
        .inputExtensions = {".jpg", ".jpeg", ".png", ".webp", ".bmp", ".tiff"},
        .outputExtensions = {".pdf", ".png", ".jpg"},
        .supportsBatch = false,
        .isAvailable = false, // no UI panel exists yet - routed to the wrong page via the Organize fallback
        .sortOrder = 3,
        .legacyToolId = -1,
        .createOperation = nullptr
    });
}

void ToolRegistry::registerFileTools() {
    using IT = LocalTools::UI::IconType;

    registerTool({
        .id = "file.hash",
        .name = QObject::tr("Calculate Hash"),
        .description = QObject::tr("Calculate and verify MD5, SHA-1, SHA-256, and SHA-512 checksums."),
        .category = QObject::tr("Files"),
        .subcategory = QObject::tr("Verification"),
        .iconType = IT::Success,
        .keywords = {"hash", "hesapla", "md5", "sha1", "sha256", "sha512", "checksum", "doğrula", "bütünlük"},
        .inputExtensions = {"*"},
        .outputExtensions = {},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 1,
        .legacyToolId = 302,
        .createOperation = nullptr
    });

    registerTool({
        .id = "file.batch-rename",
        .name = QObject::tr("Batch Rename"),
        .description = QObject::tr("Rename multiple files using numbering, dates, and pattern rules."),
        .category = QObject::tr("Files"),
        .subcategory = QObject::tr("Organize"),
        .iconType = IT::Edit,
        .keywords = {"adlandır", "rename", "toplu", "batch", "numaralandır", "dosya"},
        .inputExtensions = {"*"},
        .outputExtensions = {},
        .supportsBatch = true,
        .isAvailable = false, // no UI panel exists yet - routed to the wrong page via the Organize fallback
        .sortOrder = 2,
        .legacyToolId = -1,
        .createOperation = nullptr
    });

    registerTool({
        .id = "file.duplicates",
        .name = QObject::tr("Find Duplicate Files"),
        .description = QObject::tr("Detect identical duplicate files in folders to free up storage."),
        .category = QObject::tr("Files"),
        .subcategory = QObject::tr("Cleanup"),
        .iconType = IT::Delete,
        .keywords = {"yinelenen", "duplicate", "kopya", "aynı", "temizle", "bul"},
        .inputExtensions = {},
        .outputExtensions = {},
        .supportsBatch = false,
        .isAvailable = false, // no UI panel exists yet - routed to the wrong page via the Organize fallback
        .sortOrder = 3,
        .legacyToolId = -1,
        .createOperation = nullptr
    });

    registerTool({
        .id = "file.analyzer",
        .name = QObject::tr("Folder Size Analyzer"),
        .description = QObject::tr("Visualize disk usage and file type distribution in folders."),
        .category = QObject::tr("Files"),
        .subcategory = QObject::tr("Analysis"),
        .iconType = IT::Info,
        .keywords = {"analiz", "boyut", "klasör", "disk", "usage", "alan"},
        .inputExtensions = {},
        .outputExtensions = {},
        .supportsBatch = false,
        .isAvailable = false, // no UI panel exists yet - routed to the wrong page via the Organize fallback
        .sortOrder = 4,
        .legacyToolId = -1,
        .createOperation = nullptr
    });
}

void ToolRegistry::registerVideoTools() {
    using IT = LocalTools::UI::IconType;

    registerTool({
        .id = "video.compress",
        .name = QObject::tr("Compress Video"),
        .description = QObject::tr("Reduce video file size with quality and resolution controls."),
        .category = QObject::tr("Video"),
        .subcategory = QObject::tr("Compress"),
        .iconType = IT::Compress,
        .keywords = {"video", "sıkıştır", "compress", "küçült", "mp4", "boyut"},
        .inputExtensions = {".mp4", ".mkv", ".mov", ".avi", ".webm", ".flv"},
        .outputExtensions = {".mp4"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 1,
        .legacyToolId = 501,
        .createOperation = nullptr
    });

    registerTool({
        .id = "video.to-gif",
        .name = QObject::tr("Convert to GIF"),
        .description = QObject::tr("Create high quality animated GIF from selected video segments."),
        .category = QObject::tr("Video"),
        .subcategory = QObject::tr("Convert"),
        .iconType = IT::Convert,
        .keywords = {"gif", "video", "hareketli", "animasyon", "dönüştür"},
        .inputExtensions = {".mp4", ".mkv", ".mov", ".avi", ".webm"},
        .outputExtensions = {".gif"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 2,
        .legacyToolId = 502,
        .createOperation = nullptr
    });

    registerTool({
        .id = "video.extract-audio",
        .name = QObject::tr("Extract Audio from Video"),
        .description = QObject::tr("Extract and save audio tracks from video files as MP3 or WAV."),
        .category = QObject::tr("Video"),
        .subcategory = QObject::tr("Convert"),
        .iconType = IT::Extract,
        .keywords = {"ses", "audio", "mp3", "wav", "çıkar", "extract", "video"},
        .inputExtensions = {".mp4", ".mkv", ".mov", ".avi", ".webm"},
        .outputExtensions = {".mp3", ".wav"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 3,
        .legacyToolId = 503,
        .createOperation = nullptr
    });

    registerTool({
        .id = "video.trim",
        .name = QObject::tr("Trim Video"),
        .description = QObject::tr("Cut and save selected start and end ranges of videos."),
        .category = QObject::tr("Video"),
        .subcategory = QObject::tr("Organize"),
        .iconType = IT::Split,
        .keywords = {"kes", "trim", "kırp", "aralık", "video", "bölüm"},
        .inputExtensions = {".mp4", ".mkv", ".mov", ".avi", ".webm"},
        .outputExtensions = {".mp4", ".mkv", ".mov"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 4,
        .legacyToolId = 504,
        .createOperation = nullptr
    });
}

void ToolRegistry::registerQuickTools() {
    using IT = LocalTools::UI::IconType;

    registerTool({
        .id = "quick.qrcode",
        .name = QObject::tr("QR Code Generator"),
        .description = QObject::tr("Generate high-resolution QR codes for text, URLs, or Wi-Fi."),
        .category = QObject::tr("Quick Tools"),
        .subcategory = QObject::tr("Generator"),
        .iconType = IT::ImagesToPdf,
        .keywords = {"qr", "qrcode", "karekod", "üret", "generate", "wifi", "link"},
        .inputExtensions = {},
        .outputExtensions = {".png", ".svg"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 1,
        .legacyToolId = 401,
        .createOperation = nullptr
    });

    registerTool({
        .id = "quick.qrread",
        .name = QObject::tr("QR Code Reader"),
        .description = QObject::tr("Read and decode QR codes from images completely offline."),
        .category = QObject::tr("Quick Tools"),
        .subcategory = QObject::tr("Reader"),
        .iconType = IT::View,
        .keywords = {"qr", "qrcode", "karekod", "oku", "read", "decode", "tara", "scan"},
        .inputExtensions = {".png", ".jpg", ".jpeg", ".webp", ".bmp"},
        .outputExtensions = {},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 2,
        .legacyToolId = 405,
        .createOperation = nullptr
    });

    registerTool({
        .id = "quick.metadata",
        .name = QObject::tr("View / Clean Metadata"),
        .description = QObject::tr("Inspect and remove EXIF, GPS, and camera metadata from files."),
        .category = QObject::tr("Metadata"),
        .subcategory = QObject::tr("Privacy"),
        .iconType = IT::Delete,
        .keywords = {"metadata", "exif", "gps", "konum", "location", "temizle", "görüntüle", "kamera", "bilgi", "view", "strip", "clean"},
        .inputExtensions = {".jpg", ".jpeg", ".png", ".webp", ".bmp", ".tiff", ".pdf"},
        .outputExtensions = {".jpg", ".png", ".webp", ".pdf"},
        .supportsBatch = true,
        .isAvailable = true,
        .sortOrder = 3,
        .legacyToolId = 404,
        .createOperation = nullptr
    });

    // Developer helpers in QuickToolsBackend are intentionally not registered
    // until they have a real page and route. Advertising one here would create a
    // Home card that either does nothing or opens an unrelated QR workspace.
}

void ToolRegistry::registerAudioTools() {
    using IT = LocalTools::UI::IconType;

    registerTool({
        .id = "audio.convert",
        .name = QObject::tr("Convert Audio"),
        .description = QObject::tr("Convert audio files to various formats (MP3, WAV, FLAC, OGG, M4A)."),
        .category = QObject::tr("Audio"),
        .subcategory = QObject::tr("Convert"),
        .iconType = IT::Music,
        .keywords = {"ses", "audio", "mp3", "dönüştür"},
        .inputExtensions = {".mp3", ".wav", ".flac", ".ogg", ".m4a", ".aac"},
        .outputExtensions = {".mp3", ".wav", ".flac", ".ogg", ".m4a"},
        .supportsBatch = true,
        .isAvailable = true,
        .sortOrder = 1,
        .legacyToolId = 601,
        .createOperation = nullptr
    });

    registerTool({
        .id = "audio.compress",
        .name = QObject::tr("Compress Audio"),
        .description = QObject::tr("Reduce audio file size with quality control."),
        .category = QObject::tr("Audio"),
        .subcategory = QObject::tr("Compress"),
        .iconType = IT::Compress,
        .keywords = {"ses", "audio", "mp3", "sıkıştır"},
        .inputExtensions = {".mp3", ".wav", ".flac", ".ogg", ".m4a", ".aac"},
        .outputExtensions = {".mp3", ".wav", ".flac", ".ogg", ".m4a"},
        .supportsBatch = true,
        .isAvailable = true,
        .sortOrder = 2,
        .legacyToolId = 602,
        .createOperation = nullptr
    });

    registerTool({
        .id = "audio.trim",
        .name = QObject::tr("Trim Audio"),
        .description = QObject::tr("Trim and cut audio files with precise timestamps."),
        .category = QObject::tr("Audio"),
        .subcategory = QObject::tr("Organize"),
        .iconType = IT::Trim,
        .keywords = {"ses", "audio", "kırp", "kes", "trim"},
        .inputExtensions = {".mp3", ".wav", ".flac", ".ogg", ".m4a", ".aac"},
        .outputExtensions = {".mp3", ".wav", ".flac", ".ogg", ".m4a"},
        .supportsBatch = false,
        .isAvailable = true,
        .sortOrder = 3,
        .legacyToolId = 603,
        .createOperation = nullptr
    });
}

} // namespace LocalTools::Core
