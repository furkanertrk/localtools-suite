#include "merge_pdf.h"
#include "../backend/pdfium_backend.h"

namespace LocalTools::Core::Operations {

Result<void> MergePdfOperation::execute(const std::vector<std::string>& inputPaths, const std::string& outputPath) {
    return PdfiumBackend::mergePdfs(inputPaths, outputPath);
}

} // namespace LocalTools::Core::Operations
