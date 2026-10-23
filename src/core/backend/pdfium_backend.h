#ifndef LOCALTOOLS_CORE_BACKEND_PDFIUM_BACKEND_H
#define LOCALTOOLS_CORE_BACKEND_PDFIUM_BACKEND_H

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <functional>
#include <QImage>
#include "../result.h"

namespace LocalTools::Core {

struct PdfiumDocumentImpl;

class PdfiumBackend {
public:
    PdfiumBackend();
    ~PdfiumBackend();

    // Disable copy
    PdfiumBackend(const PdfiumBackend&) = delete;
    PdfiumBackend& operator=(const PdfiumBackend&) = delete;

    // Enable move
    PdfiumBackend(PdfiumBackend&&) noexcept;
    PdfiumBackend& operator=(PdfiumBackend&&) noexcept;

    // Document loading & inspection
    bool openDocument(const std::string& filePath);
    void closeDocument();
    bool isDocumentOpen() const;

    int getPageCount() const;
    QImage renderPage(int pageIndex, int targetWidth = 600, int targetHeight = 800);
    int getPageRotation(int pageIndex) const;

    struct PdfMetadata {
        QString title;
        QString author;
        QString subject;
        QString keywords;
        QString creator;
        QString producer;
    };
    PdfMetadata getMetadata() const;

    // Static PDF Operations implemented via PDFium C API
    static Result<void> mergePdfs(const std::vector<std::string>& inputPaths, const std::string& outputPath);
    static Result<void> splitPdf(const std::string& inputPath, const std::string& outputDir, int pagesPerFile);
    static Result<void> extractPages(const std::string& inputPath, const std::vector<int>& pageIndices, const std::string& outputPath);
    static Result<void> deletePages(const std::string& inputPath, const std::vector<int>& pageIndicesToDelete, const std::string& outputPath);
    static Result<void> reorderPages(const std::string& inputPath, const std::vector<int>& newPageOrder, const std::string& outputPath);
    static Result<void> rotatePages(const std::string& inputPath, const std::vector<std::pair<int, int>>& pageRotations, const std::string& outputPath);
    static Result<void> saveWithMetadata(const std::string& inputPath, const std::string& outputPath,
                                         const QString& title, const QString& author,
                                         const QString& subject, const QString& keywords,
                                         const QString& creator = "", const QString& producer = "");

    // Optimize Operations
    static Result<void> compressPdf(const std::string& inputPath, const std::string& outputPath, int compressionLevel = 1, std::function<void(int, int)> progressCb = nullptr);

    // Convert Operations
    static Result<void> imagesToPdf(const std::vector<std::string>& imagePaths, const std::string& outputPath);
    static Result<void> pdfToImages(const std::string& inputPath, const std::string& outputDir, const std::string& format = "png", int dpi = 150, std::function<void(int, int)> progressCb = nullptr);
    static Result<void> pdfToText(const std::string& inputPath, const std::string& outputPath, std::function<void(int, int)> progressCb = nullptr);

private:
    static void ensureLibraryInitialized();
    static void shutdownLibrary();

    std::unique_ptr<PdfiumDocumentImpl> m_impl;
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_BACKEND_PDFIUM_BACKEND_H
