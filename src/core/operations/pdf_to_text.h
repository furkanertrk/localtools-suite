#ifndef LOCALTOOLS_CORE_OPERATIONS_PDF_TO_TEXT_H
#define LOCALTOOLS_CORE_OPERATIONS_PDF_TO_TEXT_H

#include <string>
#include <functional>
#include "../result.h"

namespace LocalTools::Core::Operations {

class PdfToTextOperation {
public:
    static Result<void> execute(const std::string& inputPath,
                                const std::string& outputPath,
                                std::function<void(int, int)> progressCb = nullptr);
};

} // namespace LocalTools::Core::Operations

#endif // LOCALTOOLS_CORE_OPERATIONS_PDF_TO_TEXT_H
