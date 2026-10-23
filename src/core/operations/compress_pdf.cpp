#include "compress_pdf.h"
#include "../backend/pdfium_backend.h"

namespace LocalTools::Core::Operations {

Result<void> CompressPdfOperation::execute(const std::string& inputPath,
                                           const std::string& outputPath,
                                           CompressionLevel level,
                                           std::function<void(int, int)> progressCb) {
    return PdfiumBackend::compressPdf(inputPath, outputPath, static_cast<int>(level), progressCb);
}

} // namespace LocalTools::Core::Operations
