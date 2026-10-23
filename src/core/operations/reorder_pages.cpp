#include "reorder_pages.h"
#include "../backend/pdfium_backend.h"

namespace LocalTools::Core::Operations {

Result<void> ReorderPagesOperation::execute(const std::string& inputPath, const std::vector<int>& newPageOrder, const std::string& outputPath) {
    return PdfiumBackend::reorderPages(inputPath, newPageOrder, outputPath);
}

} // namespace LocalTools::Core::Operations
