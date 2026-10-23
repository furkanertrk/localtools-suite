#include "extract_pages.h"
#include "../backend/pdfium_backend.h"

namespace LocalTools::Core::Operations {

Result<void> ExtractPagesOperation::execute(const std::string& inputPath, const std::vector<int>& pageIndices, const std::string& outputPath) {
    return PdfiumBackend::extractPages(inputPath, pageIndices, outputPath);
}

} // namespace LocalTools::Core::Operations
