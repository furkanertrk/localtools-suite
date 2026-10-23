#ifndef LOCALTOOLS_CORE_OPERATIONS_COMPRESS_PDF_H
#define LOCALTOOLS_CORE_OPERATIONS_COMPRESS_PDF_H

#include <string>
#include <functional>
#include "../result.h"

namespace LocalTools::Core::Operations {

enum class CompressionLevel {
    Low = 0,     // High Quality (150 DPI, 85% JPEG)
    Medium = 1,  // Standard Compression (120 DPI, 70% JPEG)
    High = 2     // Maximum Compression / Smallest Size (90 DPI, 50% JPEG)
};

class CompressPdfOperation {
public:
    static Result<void> execute(const std::string& inputPath,
                                const std::string& outputPath,
                                CompressionLevel level = CompressionLevel::Medium,
                                std::function<void(int, int)> progressCb = nullptr);
};

} // namespace LocalTools::Core::Operations

#endif // LOCALTOOLS_CORE_OPERATIONS_COMPRESS_PDF_H
