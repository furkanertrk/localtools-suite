#include "rotate_pages.h"
#include "../backend/pdfium_backend.h"

namespace LocalTools::Core::Operations {

Result<void> RotatePagesOperation::execute(const std::string& inputPath, const std::vector<std::pair<int, int>>& pageRotations, const std::string& outputPath) {
    return PdfiumBackend::rotatePages(inputPath, pageRotations, outputPath);
}

} // namespace LocalTools::Core::Operations
