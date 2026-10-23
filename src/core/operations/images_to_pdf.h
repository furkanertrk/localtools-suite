#ifndef LOCALTOOLS_CORE_OPERATIONS_IMAGES_TO_PDF_H
#define LOCALTOOLS_CORE_OPERATIONS_IMAGES_TO_PDF_H

#include <string>
#include <vector>
#include "../result.h"

namespace LocalTools::Core::Operations {

class ImagesToPdfOperation {
public:
    static Result<void> execute(const std::vector<std::string>& imagePaths, const std::string& outputPath);
};

} // namespace LocalTools::Core::Operations

#endif // LOCALTOOLS_CORE_OPERATIONS_IMAGES_TO_PDF_H
