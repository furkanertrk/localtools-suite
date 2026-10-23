#ifndef LOCALTOOLS_CORE_OPERATIONS_SPLIT_PDF_H
#define LOCALTOOLS_CORE_OPERATIONS_SPLIT_PDF_H

#include <string>
#include "../result.h"

namespace LocalTools::Core::Operations {

class SplitPdfOperation {
public:
    static Result<void> execute(const std::string& inputPath, const std::string& outputDir, int pagesPerFile = 1);
};

} // namespace LocalTools::Core::Operations

#endif // LOCALTOOLS_CORE_OPERATIONS_SPLIT_PDF_H
