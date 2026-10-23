#include "pdfium_backend.h"

#include <fpdfview.h>
#include <fpdf_doc.h>
#include <fpdf_ppo.h>
#include <fpdf_edit.h>
#include <fpdf_save.h>
#include <fpdf_text.h>

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QBuffer>

#include <iostream>
#include <fstream>
#include <mutex>
#include <vector>
#include <set>
#include <algorithm>
#include <filesystem>

#include "../../services/file_service.h"

namespace LocalTools::Core {

static std::once_flag g_pdfiumInitOnce;
static std::mutex g_pdfiumMutex;
static bool g_isLibraryInitialized = false;

struct PdfiumDocumentImpl {
    FPDF_DOCUMENT doc{nullptr};
    std::string path;
};

// Custom FPDF_FILEWRITE implementation for writing PDF bytes to std::ofstream
struct CustomFileWrite : public FPDF_FILEWRITE_ {
    QFile file;
    static int CustomWriteBlock(FPDF_FILEWRITE_* pThis, const void* pData, unsigned long size) {
        auto* pWrite = static_cast<CustomFileWrite*>(pThis);
        qint64 bytesWritten = pWrite->file.write(static_cast<const char*>(pData), size);
        return (bytesWritten == static_cast<qint64>(size)) ? 1 : 0;
    }
};

static bool SavePdfDocument(FPDF_DOCUMENT doc, const std::string& outputPath) {
    if (!doc || outputPath.empty()) return false;

    CustomFileWrite fileWrite;
    fileWrite.version = 1;
    fileWrite.WriteBlock = CustomFileWrite::CustomWriteBlock;
    fileWrite.file.setFileName(QString::fromStdString(outputPath));

    if (!fileWrite.file.open(QIODevice::WriteOnly)) {
        std::cerr << "[PDFium] Unable to open output file for writing: " << outputPath << std::endl;
        return false;
    }

    bool success = FPDF_SaveAsCopy(doc, &fileWrite, FPDF_NO_INCREMENTAL);
    fileWrite.file.close();
    return success;
}

static bool PathsReferToSameFile(const std::string& firstPath,
                                 const std::string& secondPath) {
    return Services::FileService::instance().pathsReferToSameFile(
        QString::fromUtf8(firstPath), QString::fromUtf8(secondPath));
}

static PdfError PdfLoadError() {
    switch (FPDF_GetLastError()) {
        case FPDF_ERR_PASSWORD:
            return PdfError::create(
                PdfErrorCategory::InvalidPdf,
                "The PDF is password protected. Remove the password before processing it.");
        case FPDF_ERR_FORMAT:
            return PdfError::create(
                PdfErrorCategory::InvalidPdf,
                "The PDF is invalid or corrupted.");
        case FPDF_ERR_FILE:
            return PdfError::create(
                PdfErrorCategory::FileNotFound,
                "The PDF could not be opened.");
        case FPDF_ERR_SECURITY:
            return PdfError::create(
                PdfErrorCategory::PermissionDenied,
                "The PDF security settings prevent this operation.");
        default:
            return PdfError::create(
                PdfErrorCategory::BackendError,
                "The PDF engine could not open the document.");
    }
}

void PdfiumBackend::ensureLibraryInitialized() {
    std::call_once(g_pdfiumInitOnce, []() {
        FPDF_LIBRARY_CONFIG config;
        config.version = 2;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;

        FPDF_InitLibraryWithConfig(&config);
        g_isLibraryInitialized = true;
        std::cout << "[PDFium] Library initialized successfully." << std::endl;
    });
}

void PdfiumBackend::shutdownLibrary() {
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);
    if (g_isLibraryInitialized) {
        FPDF_DestroyLibrary();
        g_isLibraryInitialized = false;
        std::cout << "[PDFium] Library shut down." << std::endl;
    }
}

PdfiumBackend::PdfiumBackend()
    : m_impl(std::make_unique<PdfiumDocumentImpl>()) {
    ensureLibraryInitialized();
}

PdfiumBackend::~PdfiumBackend() {
    closeDocument();
}

PdfiumBackend::PdfiumBackend(PdfiumBackend&&) noexcept = default;
PdfiumBackend& PdfiumBackend::operator=(PdfiumBackend&&) noexcept = default;

bool PdfiumBackend::openDocument(const std::string& filePath) {
    closeDocument();
    ensureLibraryInitialized();

    std::lock_guard<std::mutex> lock(g_pdfiumMutex);
    FPDF_DOCUMENT doc = FPDF_LoadDocument(filePath.c_str(), nullptr);
    if (!doc) {
        unsigned long err = FPDF_GetLastError();
        std::cerr << "[PDFium] Failed to load document (error code " << err << ")" << std::endl;
        return false;
    }

    m_impl->doc = doc;
    m_impl->path = filePath;
    return true;
}

void PdfiumBackend::closeDocument() {
    if (m_impl && m_impl->doc) {
        std::lock_guard<std::mutex> lock(g_pdfiumMutex);
        FPDF_CloseDocument(m_impl->doc);
        m_impl->doc = nullptr;
        m_impl->path.clear();
    }
}

bool PdfiumBackend::isDocumentOpen() const {
    return m_impl && (m_impl->doc != nullptr);
}

int PdfiumBackend::getPageCount() const {
    if (!isDocumentOpen()) return 0;
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);
    return FPDF_GetPageCount(m_impl->doc);
}

int PdfiumBackend::getPageRotation(int pageIndex) const {
    if (!isDocumentOpen()) return 0;
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);
    FPDF_PAGE page = FPDF_LoadPage(m_impl->doc, pageIndex);
    if (!page) return 0;
    int rot = FPDFPage_GetRotation(page);
    FPDF_ClosePage(page);
    return rot * 90;
}

QImage PdfiumBackend::renderPage(int pageIndex, int targetWidth, int targetHeight) {
    if (!isDocumentOpen()) return QImage();

    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    int totalPages = FPDF_GetPageCount(m_impl->doc);
    if (pageIndex < 0 || pageIndex >= totalPages) return QImage();

    FPDF_PAGE page = FPDF_LoadPage(m_impl->doc, pageIndex);
    if (!page) return QImage();

    double pageWidth = FPDF_GetPageWidthF(page);
    double pageHeight = FPDF_GetPageHeightF(page);

    int renderWidth = targetWidth;
    int renderHeight = targetHeight;

    if (pageWidth > 0 && pageHeight > 0) {
        double aspect = pageHeight / pageWidth;
        renderHeight = static_cast<int>(targetWidth * aspect);
    }

    FPDF_BITMAP bitmap = FPDFBitmap_Create(renderWidth, renderHeight, 0);
    if (!bitmap) {
        FPDF_ClosePage(page);
        return QImage();
    }

    FPDFBitmap_FillRect(bitmap, 0, 0, renderWidth, renderHeight, 0xFFFFFFFF);
    int flags = FPDF_ANNOT | FPDF_LCD_TEXT;
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, renderWidth, renderHeight, 0, flags);

    const void* buffer = FPDFBitmap_GetBuffer(bitmap);
    int stride = FPDFBitmap_GetStride(bitmap);

    QImage image(static_cast<const uchar*>(buffer), renderWidth, renderHeight, stride, QImage::Format_ARGB32);
    QImage result = image.copy();

    FPDFBitmap_Destroy(bitmap);
    FPDF_ClosePage(page);

    return result;
}

Result<void> PdfiumBackend::mergePdfs(const std::vector<std::string>& inputPaths, const std::string& outputPath) {
    if (inputPaths.empty()) {
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "No input PDF files provided for merge.");
    }
    if (outputPath.empty()) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied, "Output PDF path is empty.");
    }
    for (const auto& inputPath : inputPaths) {
        if (PathsReferToSameFile(inputPath, outputPath)) {
            return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                      "For security reasons, an input file cannot be overwritten.");
        }
    }

    ensureLibraryInitialized();
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    FPDF_DOCUMENT destDoc = FPDF_CreateNewDocument();
    if (!destDoc) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to create new PDF document.");
    }

    int currentDestPage = 0;
    for (const auto& inputPath : inputPaths) {
        FPDF_DOCUMENT srcDoc = FPDF_LoadDocument(inputPath.c_str(), nullptr);
        if (!srcDoc) {
            const PdfError loadError = PdfLoadError();
            FPDF_CloseDocument(destDoc);
            return Result<void>::fail(loadError);
        }

        int srcPageCount = FPDF_GetPageCount(srcDoc);
        if (srcPageCount > 0) {
            bool imported = FPDF_ImportPages(destDoc, srcDoc, nullptr, currentDestPage);
            if (!imported) {
                FPDF_CloseDocument(srcDoc);
                FPDF_CloseDocument(destDoc);
                return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to import pages from file: " + inputPath);
            }
            currentDestPage += srcPageCount;
        }
        FPDF_CloseDocument(srcDoc);
    }

    bool saved = SavePdfDocument(destDoc, outputPath);
    FPDF_CloseDocument(destDoc);

    if (!saved) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to save merged PDF document to: " + outputPath);
    }

    return Result<void>::ok();
}

Result<void> PdfiumBackend::splitPdf(const std::string& inputPath, const std::string& outputDir, int pagesPerFile) {
    if (inputPath.empty() || outputDir.empty()) {
        return Result<void>::fail(PdfErrorCategory::FileNotFound,
                                  "Input PDF or output directory is empty.");
    }
    if (pagesPerFile <= 0) {
        return Result<void>::fail(PdfErrorCategory::InvalidPageRange,
                                  "Pages per output file must be greater than zero.");
    }

    const QDir outputDirectory(QString::fromUtf8(outputDir));
    if (!outputDirectory.exists() && !QDir().mkpath(outputDirectory.absolutePath())) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                  "Output directory could not be created.");
    }

    ensureLibraryInitialized();
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    FPDF_DOCUMENT srcDoc = FPDF_LoadDocument(inputPath.c_str(), nullptr);
    if (!srcDoc) {
        return Result<void>::fail(PdfLoadError());
    }

    int totalPages = FPDF_GetPageCount(srcDoc);
    if (totalPages == 0) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "Input PDF has 0 pages.");
    }

    const QString stem = QFileInfo(QString::fromUtf8(inputPath)).completeBaseName();

    int fileCounter = 1;
    for (int startPage = 0; startPage < totalPages; startPage += pagesPerFile) {
        int endPage = std::min(startPage + pagesPerFile - 1, totalPages - 1);
        std::string rangeStr = std::to_string(startPage + 1) + "-" + std::to_string(endPage + 1);

        FPDF_DOCUMENT destDoc = FPDF_CreateNewDocument();
        if (!destDoc) {
            FPDF_CloseDocument(srcDoc);
            return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to create new PDF document for split.");
        }

        bool imported = FPDF_ImportPages(destDoc, srcDoc, rangeStr.c_str(), 0);
        if (!imported) {
            FPDF_CloseDocument(destDoc);
            FPDF_CloseDocument(srcDoc);
            return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to import page range: " + rangeStr);
        }

        const QString outputBaseName = stem + QStringLiteral("_part")
            + QString::number(fileCounter++);
        const QString desiredPath = outputDirectory.absoluteFilePath(
            outputBaseName + QStringLiteral(".pdf"));
        const QString outPath = Services::FileService::instance().generateUniqueOutputPath(desiredPath);
        if (outPath.isEmpty()) {
            FPDF_CloseDocument(destDoc);
            FPDF_CloseDocument(srcDoc);
            return Result<void>::fail(PdfErrorCategory::PermissionDenied,
                                      "Could not create a collision-free split PDF output path.");
        }
        bool saved = SavePdfDocument(destDoc, outPath.toStdString());
        FPDF_CloseDocument(destDoc);

        if (!saved) {
            FPDF_CloseDocument(srcDoc);
            return Result<void>::fail(PdfErrorCategory::BackendError,
                                      "Failed to save split PDF output.");
        }
    }

    FPDF_CloseDocument(srcDoc);
    return Result<void>::ok();
}

Result<void> PdfiumBackend::extractPages(const std::string& inputPath, const std::vector<int>& pageIndices, const std::string& outputPath) {
    if (inputPath.empty() || outputPath.empty()) {
        return Result<void>::fail(PdfErrorCategory::FileNotFound, "Input or output file path is empty.");
    }
    if (PathsReferToSameFile(inputPath, outputPath)) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied, "For security reasons, original file cannot be overwritten.");
    }
    if (pageIndices.empty()) {
        return Result<void>::fail(PdfErrorCategory::InvalidPageRange, "No page indices provided for extraction.");
    }

    ensureLibraryInitialized();
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    FPDF_DOCUMENT srcDoc = FPDF_LoadDocument(inputPath.c_str(), nullptr);
    if (!srcDoc) {
        return Result<void>::fail(PdfLoadError());
    }

    int totalPages = FPDF_GetPageCount(srcDoc);
    if (std::any_of(pageIndices.begin(), pageIndices.end(),
                    [totalPages](int index) { return index < 0 || index >= totalPages; })) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::InvalidPageRange,
                                  "One or more selected pages are outside the document.");
    }
    FPDF_DOCUMENT destDoc = FPDF_CreateNewDocument();
    if (!destDoc) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to create output PDF document.");
    }

    int destIndex = 0;
    for (int pageIdx : pageIndices) {
        int pageNum = pageIdx + 1;
        std::string rangeStr = std::to_string(pageNum);
        if (!FPDF_ImportPages(destDoc, srcDoc, rangeStr.c_str(), destIndex++)) {
            FPDF_CloseDocument(destDoc);
            FPDF_CloseDocument(srcDoc);
            return Result<void>::fail(PdfErrorCategory::BackendError,
                                      "A selected page could not be extracted.");
        }
    }

    bool saved = SavePdfDocument(destDoc, outputPath);
    FPDF_CloseDocument(destDoc);
    FPDF_CloseDocument(srcDoc);

    if (!saved) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to save extracted pages PDF.");
    }

    return Result<void>::ok();
}

Result<void> PdfiumBackend::deletePages(const std::string& inputPath, const std::vector<int>& pageIndicesToDelete, const std::string& outputPath) {
    if (inputPath.empty() || outputPath.empty()) {
        return Result<void>::fail(PdfErrorCategory::FileNotFound, "Input or output file path is empty.");
    }
    if (PathsReferToSameFile(inputPath, outputPath)) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied, "For security reasons, original file cannot be overwritten.");
    }

    ensureLibraryInitialized();
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    FPDF_DOCUMENT srcDoc = FPDF_LoadDocument(inputPath.c_str(), nullptr);
    if (!srcDoc) {
        return Result<void>::fail(PdfLoadError());
    }

    int totalPages = FPDF_GetPageCount(srcDoc);
    if (pageIndicesToDelete.empty()
        || std::any_of(pageIndicesToDelete.begin(), pageIndicesToDelete.end(),
                       [totalPages](int index) { return index < 0 || index >= totalPages; })) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::InvalidPageRange,
                                  "One or more pages selected for deletion are invalid.");
    }
    std::set<int> deleteSet(pageIndicesToDelete.begin(), pageIndicesToDelete.end());

    std::vector<int> keepIndices;
    for (int i = 0; i < totalPages; ++i) {
        if (deleteSet.find(i) == deleteSet.end()) {
            keepIndices.push_back(i);
        }
    }

    if (keepIndices.empty()) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::InvalidPageRange, "Cannot delete all pages of a PDF document.");
    }

    FPDF_DOCUMENT destDoc = FPDF_CreateNewDocument();
    if (!destDoc) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to create output PDF document.");
    }

    int destIdx = 0;
    for (int pageIdx : keepIndices) {
        int pageNum = pageIdx + 1;
        std::string rangeStr = std::to_string(pageNum);
        FPDF_ImportPages(destDoc, srcDoc, rangeStr.c_str(), destIdx++);
    }

    bool saved = SavePdfDocument(destDoc, outputPath);
    FPDF_CloseDocument(destDoc);
    FPDF_CloseDocument(srcDoc);

    if (!saved) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to save document after deleting pages.");
    }

    return Result<void>::ok();
}

Result<void> PdfiumBackend::reorderPages(const std::string& inputPath, const std::vector<int>& newPageOrder, const std::string& outputPath) {
    if (inputPath.empty() || outputPath.empty()) {
        return Result<void>::fail(PdfErrorCategory::FileNotFound, "Input or output file path is empty.");
    }
    if (PathsReferToSameFile(inputPath, outputPath)) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied, "For security reasons, original file cannot be overwritten.");
    }
    if (newPageOrder.empty()) {
        return Result<void>::fail(PdfErrorCategory::InvalidPageRange, "New page order vector is empty.");
    }

    ensureLibraryInitialized();
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    FPDF_DOCUMENT srcDoc = FPDF_LoadDocument(inputPath.c_str(), nullptr);
    if (!srcDoc) {
        return Result<void>::fail(PdfLoadError());
    }

    int totalPages = FPDF_GetPageCount(srcDoc);
    std::set<int> uniquePageIndices(newPageOrder.begin(), newPageOrder.end());
    if (static_cast<int>(newPageOrder.size()) != totalPages
        || static_cast<int>(uniquePageIndices.size()) != totalPages
        || *uniquePageIndices.begin() != 0
        || *uniquePageIndices.rbegin() != totalPages - 1) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::InvalidPageRange,
                                  "The new page order must contain every page exactly once.");
    }
    FPDF_DOCUMENT destDoc = FPDF_CreateNewDocument();
    if (!destDoc) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to create output PDF document.");
    }

    int destIdx = 0;
    for (int pageIdx : newPageOrder) {
        int pageNum = pageIdx + 1;
        std::string rangeStr = std::to_string(pageNum);
        if (!FPDF_ImportPages(destDoc, srcDoc, rangeStr.c_str(), destIdx++)) {
            FPDF_CloseDocument(destDoc);
            FPDF_CloseDocument(srcDoc);
            return Result<void>::fail(PdfErrorCategory::BackendError,
                                      "A page could not be added in the requested order.");
        }
    }

    bool saved = SavePdfDocument(destDoc, outputPath);
    FPDF_CloseDocument(destDoc);
    FPDF_CloseDocument(srcDoc);

    if (!saved) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to save reordered PDF document.");
    }

    return Result<void>::ok();
}

Result<void> PdfiumBackend::rotatePages(const std::string& inputPath, const std::vector<std::pair<int, int>>& pageRotations, const std::string& outputPath) {
    if (inputPath.empty() || outputPath.empty()) {
        return Result<void>::fail(PdfErrorCategory::FileNotFound, "Input or output file path is empty.");
    }
    if (PathsReferToSameFile(inputPath, outputPath)) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied, "For security reasons, original file cannot be overwritten.");
    }

    ensureLibraryInitialized();
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    FPDF_DOCUMENT srcDoc = FPDF_LoadDocument(inputPath.c_str(), nullptr);
    if (!srcDoc) {
        return Result<void>::fail(PdfLoadError());
    }

    int totalPages = FPDF_GetPageCount(srcDoc);
    if (pageRotations.empty()
        || std::any_of(pageRotations.begin(), pageRotations.end(),
                       [totalPages](const auto& rotation) {
                           const int pageIndex = rotation.first;
                           const int degrees = rotation.second;
                           return pageIndex < 0 || pageIndex >= totalPages
                               || degrees % 90 != 0;
                       })) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::InvalidPageRange,
                                  "One or more page rotations are invalid.");
    }
    for (const auto& [pageIdx, degrees] : pageRotations) {
        FPDF_PAGE page = FPDF_LoadPage(srcDoc, pageIdx);
        if (!page) {
            FPDF_CloseDocument(srcDoc);
            return Result<void>::fail(PdfErrorCategory::BackendError,
                                      "A selected page could not be loaded for rotation.");
        }
        int currentRot = FPDFPage_GetRotation(page); // 0, 1, 2, 3
        int deltaRot = (degrees / 90) % 4;
        int newRot = (currentRot + deltaRot + 4) % 4;
        FPDFPage_SetRotation(page, newRot);
        FPDF_ClosePage(page);
    }

    bool saved = SavePdfDocument(srcDoc, outputPath);
    FPDF_CloseDocument(srcDoc);

    if (!saved) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to save rotated PDF document.");
    }

    return Result<void>::ok();
}

static QString GetPdfMetaTag(FPDF_DOCUMENT doc, const char* tag) {
    if (!doc) return QString();
    unsigned long len = FPDF_GetMetaText(doc, tag, nullptr, 0);
    if (len <= 2) return QString();
    std::vector<unsigned char> buf(len);
    FPDF_GetMetaText(doc, tag, buf.data(), len);
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(buf.data()), (len / 2) - 1);
}

PdfiumBackend::PdfMetadata PdfiumBackend::getMetadata() const {
    PdfMetadata meta;
    if (m_impl && m_impl->doc) {
        meta.title = GetPdfMetaTag(m_impl->doc, "Title");
        meta.author = GetPdfMetaTag(m_impl->doc, "Author");
        meta.subject = GetPdfMetaTag(m_impl->doc, "Subject");
        meta.keywords = GetPdfMetaTag(m_impl->doc, "Keywords");
        meta.creator = GetPdfMetaTag(m_impl->doc, "Creator");
        meta.producer = GetPdfMetaTag(m_impl->doc, "Producer");
    }
    return meta;
}

// Helper: escape PDF string special characters
static std::string EscapePdfString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (char c : input) {
        if (c == '(' || c == ')' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

// Helper: encode a QString as PDF UTF-16BE hex string with BOM
static std::string EncodePdfUtf16BE(const QString& text) {
    // PDF hex string: <FEFF....>
    std::string hex = "<FEFF";
    const ushort* utf16 = text.utf16();
    int len = text.size();
    for (int i = 0; i < len; ++i) {
        ushort ch = utf16[i];
        char buf[8];
        snprintf(buf, sizeof(buf), "%02X%02X", (ch >> 8) & 0xFF, ch & 0xFF);
        hex += buf;
    }
    hex += ">";
    return hex;
}

Result<void> PdfiumBackend::saveWithMetadata(const std::string& inputPath, const std::string& outputPath,
                                              const QString& title, const QString& author,
                                              const QString& subject, const QString& keywords,
                                              const QString& creator, const QString& producer) {
    if (inputPath.empty() || outputPath.empty()) {
        return Result<void>::fail(PdfErrorCategory::FileNotFound, "Input or output file path is empty.");
    }
    if (PathsReferToSameFile(inputPath, outputPath)) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied, "For security reasons, original file cannot be overwritten.");
    }

    ensureLibraryInitialized();
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    // 1. Copy all pages from source to a new doc via PDFium, then save
    FPDF_DOCUMENT srcDoc = FPDF_LoadDocument(inputPath.c_str(), nullptr);
    if (!srcDoc) {
        return Result<void>::fail(PdfLoadError());
    }

    FPDF_DOCUMENT destDoc = FPDF_CreateNewDocument();
    if (!destDoc) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::BackendError, "Could not create new PDF document.");
    }

    int pageCount = FPDF_GetPageCount(srcDoc);
    if (pageCount > 0) {
        std::string range = "1-" + std::to_string(pageCount);
        if (!FPDF_ImportPages(destDoc, srcDoc, range.c_str(), 0)) {
            FPDF_CloseDocument(destDoc);
            FPDF_CloseDocument(srcDoc);
            return Result<void>::fail(PdfErrorCategory::BackendError, "Could not import pages.");
        }
    }

    // Save to output file first (without metadata)
    bool saved = SavePdfDocument(destDoc, outputPath);
    FPDF_CloseDocument(destDoc);
    FPDF_CloseDocument(srcDoc);

    if (!saved) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to save PDF document.");
    }

    // 2. Append incremental update with Info dictionary using QFile for Unicode compatibility
    QFile file(QString::fromStdString(outputPath));
    if (!file.open(QIODevice::ReadWrite)) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Could not open output file for metadata update.");
    }

    QByteArray contentBytes = file.readAll();
    std::string content(contentBytes.constData(), contentBytes.size());

    // Build the Info object content
    std::string infoObj;
    infoObj += "99999 0 obj\n<< /Type /Info\n";

    auto addMeta = [&](const char* key, const QString& val) {
        if (!val.isEmpty()) {
            std::string utf8 = val.toUtf8().toStdString();
            bool asciiSafe = true;
            for (unsigned char c : utf8) {
                if (c > 127) { asciiSafe = false; break; }
            }
            if (asciiSafe) {
                infoObj += "  /" + std::string(key) + " (" + EscapePdfString(utf8) + ")\n";
            } else {
                infoObj += "  /" + std::string(key) + " " + EncodePdfUtf16BE(val) + "\n";
            }
        }
    };

    addMeta("Title", title);
    addMeta("Author", author);
    addMeta("Subject", subject);
    addMeta("Keywords", keywords);
    addMeta("Creator", creator);
    addMeta("Producer", producer);
    infoObj += ">>\nendobj\n";

    size_t startXrefPos = content.rfind("startxref");
    if (startXrefPos == std::string::npos) {
        file.close();
        return Result<void>::ok();
    }

    std::string oldXrefOffsetStr;
    size_t numStart = startXrefPos + 9;
    while (numStart < content.size() && (content[numStart] == '\r' || content[numStart] == '\n' || content[numStart] == ' ')) numStart++;
    while (numStart < content.size() && content[numStart] >= '0' && content[numStart] <= '9') {
        oldXrefOffsetStr += content[numStart++];
    }

    file.seek(file.size());
    qint64 infoObjOffset = file.pos();
    file.write(infoObj.c_str(), infoObj.size());

    qint64 newXrefOffset = file.pos();
    std::string xrefData;
    xrefData += "xref\n0 1\n0000000000 65535 f \n99999 1\n";
    char xrefEntry[32];
    snprintf(xrefEntry, sizeof(xrefEntry), "%010lld 00000 n \n", (long long)infoObjOffset);
    xrefData += xrefEntry;

    size_t trailerPos = content.rfind("trailer");
    int existingSize = 0;
    if (trailerPos != std::string::npos) {
        size_t sizePos = content.find("/Size", trailerPos);
        if (sizePos != std::string::npos) {
            size_t sn = sizePos + 5;
            while (sn < content.size() && content[sn] == ' ') sn++;
            std::string sizeStr;
            while (sn < content.size() && content[sn] >= '0' && content[sn] <= '9') {
                sizeStr += content[sn++];
            }
            if (!sizeStr.empty()) existingSize = std::stoi(sizeStr);
        }
    }

    int newSize = std::max(existingSize, 100000);
    std::string trailerStr;
    trailerStr += "trailer\n<< /Size " + std::to_string(newSize) + " /Info 99999 0 R /Prev " + oldXrefOffsetStr + " >>\n";
    trailerStr += "startxref\n" + std::to_string((long long)newXrefOffset) + "\n%%EOF\n";

    file.write(xrefData.c_str(), xrefData.size());
    file.write(trailerStr.c_str(), trailerStr.size());
    file.close();

    return Result<void>::ok();
}

Result<void> PdfiumBackend::compressPdf(const std::string& inputPath,
                                       const std::string& outputPath,
                                       int compressionLevel,
                                       std::function<void(int, int)> progressCb) {
    if (PathsReferToSameFile(inputPath, outputPath)) {
        return Result<void>::fail(PdfErrorCategory::PermissionDenied, "For security reasons, original file cannot be overwritten.");
    }
    ensureLibraryInitialized();
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    FPDF_DOCUMENT srcDoc = FPDF_LoadDocument(inputPath.c_str(), nullptr);
    if (!srcDoc) {
        return Result<void>::fail(PdfLoadError());
    }

    int totalPages = FPDF_GetPageCount(srcDoc);
    if (totalPages <= 0) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "No pages found in PDF document.");
    }

    // Determine target settings based on level
    int targetDpi = 120;
    int jpegQuality = 70;
    if (compressionLevel == 0) { // Low Compression / High Quality
        targetDpi = 150;
        jpegQuality = 85;
    } else if (compressionLevel == 2) { // High Compression / Small Size
        targetDpi = 90;
        jpegQuality = 50;
    }

    // Check if the document contains selectable text
    bool hasText = false;
    for (int i = 0; i < totalPages; ++i) {
        FPDF_PAGE p = FPDF_LoadPage(srcDoc, i);
        if (p) {
            FPDF_TEXTPAGE tp = FPDFText_LoadPage(p);
            if (tp) {
                int charCount = FPDFText_CountChars(tp);
                FPDFText_ClosePage(tp);
                if (charCount > 0) {
                    hasText = true;
                    FPDF_ClosePage(p);
                    break;
                }
            }
            FPDF_ClosePage(p);
        }
    }

    FPDF_DOCUMENT destDoc = FPDF_CreateNewDocument();
    if (!destDoc) {
        FPDF_CloseDocument(srcDoc);
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to create new PDF document.");
    }

    if (hasText) {
        // --- HYBRID VECTOR-PRESERVING COMPRESSION ---
        // Import pages structure intact: preserves vector text, fonts, paths, searchability 100%!
        std::string range = "1-" + std::to_string(totalPages);
        if (!FPDF_ImportPages(destDoc, srcDoc, range.c_str(), 0)) {
            FPDF_CloseDocument(destDoc);
            FPDF_CloseDocument(srcDoc);
            return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to transfer page structure.");
        }

        // Optimize embedded image objects on each page
        for (int i = 0; i < totalPages; ++i) {
            FPDF_PAGE page = FPDF_LoadPage(destDoc, i);
            if (page) {
                int objCount = FPDFPage_CountObjects(page);
                bool pageModified = false;
                for (int j = 0; j < objCount; ++j) {
                    FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, j);
                    if (obj && FPDFPageObj_GetType(obj) == FPDF_PAGEOBJ_IMAGE) {
                        FPDF_BITMAP bmp = FPDFImageObj_GetBitmap(obj);
                        if (bmp) {
                            int w = FPDFBitmap_GetWidth(bmp);
                            int h = FPDFBitmap_GetHeight(bmp);
                            if (w > 0 && h > 0) {
                                const uchar* buffer = static_cast<const uchar*>(FPDFBitmap_GetBuffer(bmp));
                                int stride = FPDFBitmap_GetStride(bmp);
                                QImage img(buffer, w, h, stride, QImage::Format_ARGB32);

                                int newW = std::min(w, static_cast<int>(w * targetDpi / 150.0));
                                int newH = std::min(h, static_cast<int>(h * targetDpi / 150.0));
                                if (newW <= 0) newW = w;
                                if (newH <= 0) newH = h;

                                QImage scaled = img.scaled(newW, newH, Qt::KeepAspectRatio, Qt::SmoothTransformation);

                                QByteArray jpegData;
                                QBuffer bBuf(&jpegData);
                                bBuf.open(QIODevice::WriteOnly);
                                scaled.save(&bBuf, "JPEG", jpegQuality);
                                bBuf.close();

                                QImage compressedImg;
                                compressedImg.loadFromData(jpegData, "JPEG");
                                QImage converted = compressedImg.convertToFormat(QImage::Format_ARGB32);

                                FPDF_BITMAP newBmp = FPDFBitmap_CreateEx(converted.width(), converted.height(), FPDFBitmap_BGRA, const_cast<uchar*>(converted.bits()), converted.bytesPerLine());
                                FPDF_PAGE pageArr[1] = { page };
                                FPDFImageObj_SetBitmap(pageArr, 1, obj, newBmp);
                                FPDFBitmap_Destroy(newBmp);
                                pageModified = true;
                            }
                            FPDFBitmap_Destroy(bmp);
                        }
                    }
                }
                if (pageModified) {
                    FPDFPage_GenerateContent(page);
                }
                FPDF_ClosePage(page);
            }
            if (progressCb) {
                progressCb(i + 1, totalPages);
            }
        }
    } else {
        // --- SCANNED / IMAGE-ONLY COMPRESSION ---
        for (int i = 0; i < totalPages; ++i) {
            FPDF_PAGE srcPage = FPDF_LoadPage(srcDoc, i);
            if (srcPage) {
                double pointW = FPDF_GetPageWidthF(srcPage);
                double pointH = FPDF_GetPageHeightF(srcPage);
                int renderW = static_cast<int>(pointW * targetDpi / 72.0);
                int renderH = static_cast<int>(pointH * targetDpi / 72.0);
                if (renderW <= 0) renderW = 600;
                if (renderH <= 0) renderH = 800;

                FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(renderW, renderH, FPDFBitmap_BGRA, nullptr, 0);
                FPDFBitmap_FillRect(bitmap, 0, 0, renderW, renderH, 0xFFFFFFFF);
                FPDF_RenderPageBitmap(bitmap, srcPage, 0, 0, renderW, renderH, 0, FPDF_ANNOT | FPDF_LCD_TEXT);

                const uchar* buffer = static_cast<const uchar*>(FPDFBitmap_GetBuffer(bitmap));
                int stride = FPDFBitmap_GetStride(bitmap);
                QImage img(buffer, renderW, renderH, stride, QImage::Format_ARGB32);

                QByteArray jpegData;
                QBuffer bBuf(&jpegData);
                bBuf.open(QIODevice::WriteOnly);
                img.save(&bBuf, "JPEG", jpegQuality);
                bBuf.close();

                QImage compressedImg;
                compressedImg.loadFromData(jpegData, "JPEG");
                QImage converted = compressedImg.convertToFormat(QImage::Format_ARGB32);

                FPDF_PAGE destPage = FPDFPage_New(destDoc, i, pointW, pointH);
                if (destPage) {
                    FPDF_PAGEOBJECT imgObj = FPDFPageObj_NewImageObj(destDoc);
                    if (imgObj) {
                        FPDF_BITMAP imgBitmap = FPDFBitmap_CreateEx(converted.width(), converted.height(), FPDFBitmap_BGRA, const_cast<uchar*>(converted.bits()), converted.bytesPerLine());
                        FPDF_PAGE pagesArr[1] = { destPage };
                        FPDFImageObj_SetBitmap(pagesArr, 1, imgObj, imgBitmap);
                        FPDFImageObj_SetMatrix(imgObj, pointW, 0, 0, pointH, 0, 0);
                        FPDFPage_InsertObject(destPage, imgObj);
                        FPDFPage_GenerateContent(destPage);
                        FPDFBitmap_Destroy(imgBitmap);
                    }
                    FPDF_ClosePage(destPage);
                }

                FPDFBitmap_Destroy(bitmap);
                FPDF_ClosePage(srcPage);
            }

            if (progressCb) {
                progressCb(i + 1, totalPages);
            }
        }
    }

    bool saved = SavePdfDocument(destDoc, outputPath);
    FPDF_CloseDocument(destDoc);
    FPDF_CloseDocument(srcDoc);

    if (!saved) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to save compressed PDF.");
    }
    return Result<void>::ok();
}

Result<void> PdfiumBackend::imagesToPdf(const std::vector<std::string>& imagePaths, const std::string& outputPath) {
    if (imagePaths.empty()) {
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "No images selected to convert.");
    }
    ensureLibraryInitialized();
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    FPDF_DOCUMENT destDoc = FPDF_CreateNewDocument();
    if (!destDoc) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to create new PDF document.");
    }

    int pageIndex = 0;
    int successCount = 0;

    for (const auto& imgPath : imagePaths) {
        QImage img(QString::fromStdString(imgPath));
        if (img.isNull()) {
            std::cerr << "[PDFium] Visual load error, skipping invalid image: " << imgPath << std::endl;
            continue;
        }

        QImage converted = img.convertToFormat(QImage::Format_ARGB32);
        int w = converted.width();
        int h = converted.height();

        // 72 points per inch (standard PDF points)
        double pageW = w * 72.0 / 150.0;
        double pageH = h * 72.0 / 150.0;

        FPDF_PAGE page = FPDFPage_New(destDoc, pageIndex, pageW, pageH);
        if (!page) continue;

        FPDF_PAGEOBJECT imgObj = FPDFPageObj_NewImageObj(destDoc);
        if (imgObj) {
            FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRA, const_cast<uchar*>(converted.bits()), converted.bytesPerLine());
            FPDF_PAGE pagesArr[1] = { page };
            FPDFImageObj_SetBitmap(pagesArr, 1, imgObj, bitmap);
            FPDFImageObj_SetMatrix(imgObj, pageW, 0, 0, pageH, 0, 0);
            FPDFPage_InsertObject(page, imgObj);
            FPDFPage_GenerateContent(page);
            FPDFBitmap_Destroy(bitmap);
        }

        FPDF_ClosePage(page);
        pageIndex++;
        successCount++;
    }

    if (successCount == 0) {
        FPDF_CloseDocument(destDoc);
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "No valid images could be converted.");
    }

    bool saved = SavePdfDocument(destDoc, outputPath);
    FPDF_CloseDocument(destDoc);

    if (!saved) {
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to save generated PDF.");
    }
    return Result<void>::ok();
}

Result<void> PdfiumBackend::pdfToImages(const std::string& inputPath,
                                       const std::string& outputDir,
                                       const std::string& format,
                                       int dpi,
                                       std::function<void(int, int)> progressCb) {
    ensureLibraryInitialized();
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    FPDF_DOCUMENT doc = FPDF_LoadDocument(inputPath.c_str(), nullptr);
    if (!doc) {
        return Result<void>::fail(PdfLoadError());
    }

    int totalPages = FPDF_GetPageCount(doc);
    if (totalPages <= 0) {
        FPDF_CloseDocument(doc);
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "No pages found in PDF document.");
    }

    QDir outDir(QString::fromStdString(outputDir));
    if (!outDir.exists()) {
        outDir.mkpath(".");
    }

    QString ext = QString::fromStdString(format).toLower();
    if (ext != "png" && ext != "jpg" && ext != "jpeg") {
        ext = "png";
    }

    int targetDpi = (dpi > 0) ? dpi : 150;

    for (int i = 0; i < totalPages; ++i) {
        FPDF_PAGE page = FPDF_LoadPage(doc, i);
        if (page) {
            double pointW = FPDF_GetPageWidthF(page);
            double pointH = FPDF_GetPageHeightF(page);
            int renderW = static_cast<int>(pointW * targetDpi / 72.0);
            int renderH = static_cast<int>(pointH * targetDpi / 72.0);
            if (renderW <= 0) renderW = 600;
            if (renderH <= 0) renderH = 800;

            FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(renderW, renderH, FPDFBitmap_BGRA, nullptr, 0);
            FPDFBitmap_FillRect(bitmap, 0, 0, renderW, renderH, 0xFFFFFFFF);
            FPDF_RenderPageBitmap(bitmap, page, 0, 0, renderW, renderH, 0, FPDF_ANNOT | FPDF_LCD_TEXT);

            const uchar* buffer = static_cast<const uchar*>(FPDFBitmap_GetBuffer(bitmap));
            int stride = FPDFBitmap_GetStride(bitmap);
            QImage img(buffer, renderW, renderH, stride, QImage::Format_ARGB32);

            QString fileName = QString("sayfa_%1.%2").arg(i + 1, 3, 10, QChar('0')).arg(ext);
            QString filePath = outDir.filePath(fileName);
            img.save(filePath, ext.toUpper().toUtf8().constData(), 95);

            FPDFBitmap_Destroy(bitmap);
            FPDF_ClosePage(page);
        }

        if (progressCb) {
            progressCb(i + 1, totalPages);
        }
    }

    FPDF_CloseDocument(doc);
    return Result<void>::ok();
}

Result<void> PdfiumBackend::pdfToText(const std::string& inputPath,
                                     const std::string& outputPath,
                                     std::function<void(int, int)> progressCb) {
    ensureLibraryInitialized();
    std::lock_guard<std::mutex> lock(g_pdfiumMutex);

    FPDF_DOCUMENT doc = FPDF_LoadDocument(inputPath.c_str(), nullptr);
    if (!doc) {
        return Result<void>::fail(PdfLoadError());
    }

    int totalPages = FPDF_GetPageCount(doc);
    if (totalPages <= 0) {
        FPDF_CloseDocument(doc);
        return Result<void>::fail(PdfErrorCategory::InvalidPdf, "No pages found in PDF document.");
    }

    QFile outFile(QString::fromStdString(outputPath));
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        FPDF_CloseDocument(doc);
        return Result<void>::fail(PdfErrorCategory::BackendError, "Failed to create output text file.");
    }

    QTextStream outStream(&outFile);
    outStream.setEncoding(QStringConverter::Utf8);

    for (int i = 0; i < totalPages; ++i) {
        outStream << QString("--- SAYFA %1 / %2 ---\n").arg(i + 1).arg(totalPages);

        FPDF_PAGE page = FPDF_LoadPage(doc, i);
        if (page) {
            FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
            if (textPage) {
                int charCount = FPDFText_CountChars(textPage);
                if (charCount > 0) {
                    std::vector<unsigned short> buffer(charCount + 1, 0);
                    int written = FPDFText_GetText(textPage, 0, charCount, buffer.data());
                    if (written > 0) {
                        QString pageText = QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.data()), written - 1);
                        outStream << pageText << "\n";
                    }
                } else {
                    outStream << "[Bu sayfada çıkarılabilir metin bulunamadı]\n";
                }
                FPDFText_ClosePage(textPage);
            } else {
                outStream << "[Metin katmanı yüklenemedi]\n";
            }
            FPDF_ClosePage(page);
        } else {
            outStream << "[Sayfa yüklenemedi]\n";
        }
        outStream << "\n";

        if (progressCb) {
            progressCb(i + 1, totalPages);
        }
    }

    outFile.close();
    FPDF_CloseDocument(doc);
    return Result<void>::ok();
}

} // namespace LocalTools::Core

