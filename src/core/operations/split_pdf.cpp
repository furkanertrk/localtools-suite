#include "split_pdf.h"
#include "../backend/pdfium_backend.h"

namespace LocalTools::Core::Operations {

Result<void> SplitPdfOperation::execute(const std::string& inputPath, const std::string& outputDir, int pagesPerFile) {
    return PdfiumBackend::splitPdf(inputPath, outputDir, pagesPerFile);
}

} // namespace LocalTools::Core::Operations
