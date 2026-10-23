#include "pdf_to_text.h"
#include "../backend/pdfium_backend.h"

namespace LocalTools::Core::Operations {

Result<void> PdfToTextOperation::execute(const std::string& inputPath,
                                         const std::string& outputPath,
                                         std::function<void(int, int)> progressCb) {
    return PdfiumBackend::pdfToText(inputPath, outputPath, progressCb);
}

} // namespace LocalTools::Core::Operations
