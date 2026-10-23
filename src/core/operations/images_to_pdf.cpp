#include "images_to_pdf.h"
#include "../backend/pdfium_backend.h"

namespace LocalTools::Core::Operations {

Result<void> ImagesToPdfOperation::execute(const std::vector<std::string>& imagePaths, const std::string& outputPath) {
    return PdfiumBackend::imagesToPdf(imagePaths, outputPath);
}

} // namespace LocalTools::Core::Operations
