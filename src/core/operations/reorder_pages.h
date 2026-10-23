#ifndef LOCALTOOLS_CORE_OPERATIONS_REORDER_PAGES_H
#define LOCALTOOLS_CORE_OPERATIONS_REORDER_PAGES_H

#include <string>
#include <vector>
#include "../result.h"

namespace LocalTools::Core::Operations {

class ReorderPagesOperation {
public:
    static Result<void> execute(const std::string& inputPath, const std::vector<int>& newPageOrder, const std::string& outputPath);
};

} // namespace LocalTools::Core::Operations

#endif // LOCALTOOLS_CORE_OPERATIONS_REORDER_PAGES_H
