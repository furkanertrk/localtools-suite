#include "delete_pages.h"
#include "../backend/pdfium_backend.h"

namespace LocalTools::Core::Operations {

Result<void> DeletePagesOperation::execute(const std::string& inputPath, const std::vector<int>& pageIndicesToDelete, const std::string& outputPath) {
    return PdfiumBackend::deletePages(inputPath, pageIndicesToDelete, outputPath);
}

} // namespace LocalTools::Core::Operations
