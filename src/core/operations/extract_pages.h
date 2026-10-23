#ifndef LOCALTOOLS_CORE_OPERATIONS_EXTRACT_PAGES_H
#define LOCALTOOLS_CORE_OPERATIONS_EXTRACT_PAGES_H

#include <string>
#include <vector>
#include "../result.h"

namespace LocalTools::Core::Operations {

class ExtractPagesOperation {
public:
    static Result<void> execute(const std::string& inputPath, const std::vector<int>& pageIndices, const std::string& outputPath);
};

} // namespace LocalTools::Core::Operations

#endif // LOCALTOOLS_CORE_OPERATIONS_EXTRACT_PAGES_H
