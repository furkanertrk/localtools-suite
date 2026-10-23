#ifndef LOCALTOOLS_CORE_OPERATIONS_PDF_TO_IMAGES_H
#define LOCALTOOLS_CORE_OPERATIONS_PDF_TO_IMAGES_H

#include <string>
#include <functional>
#include "../result.h"

namespace LocalTools::Core::Operations {

class PdfToImagesOperation {
public:
    static Result<void> execute(const std::string& inputPath,
                                const std::string& outputDir,
                                const std::string& format = "png",
                                int dpi = 150,
                                std::function<void(int, int)> progressCb = nullptr);
};

} // namespace LocalTools::Core::Operations

#endif // LOCALTOOLS_CORE_OPERATIONS_PDF_TO_IMAGES_H
