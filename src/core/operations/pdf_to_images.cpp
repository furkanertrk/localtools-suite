#include "pdf_to_images.h"
#include "../backend/pdfium_backend.h"

namespace LocalTools::Core::Operations {

Result<void> PdfToImagesOperation::execute(const std::string& inputPath,
                                           const std::string& outputDir,
                                           const std::string& format,
                                           int dpi,
                                           std::function<void(int, int)> progressCb) {
    return PdfiumBackend::pdfToImages(inputPath, outputDir, format, dpi, progressCb);
}

} // namespace LocalTools::Core::Operations
