#ifndef LOCALTOOLS_CORE_OPERATIONS_DELETE_PAGES_H
#define LOCALTOOLS_CORE_OPERATIONS_DELETE_PAGES_H

#include <string>
#include <vector>
#include "../result.h"

namespace LocalTools::Core::Operations {

class DeletePagesOperation {
public:
    static Result<void> execute(const std::string& inputPath, const std::vector<int>& pageIndicesToDelete, const std::string& outputPath);
};

} // namespace LocalTools::Core::Operations

#endif // LOCALTOOLS_CORE_OPERATIONS_DELETE_PAGES_H
