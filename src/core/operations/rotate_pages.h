#ifndef LOCALTOOLS_CORE_OPERATIONS_ROTATE_PAGES_H
#define LOCALTOOLS_CORE_OPERATIONS_ROTATE_PAGES_H

#include <string>
#include <vector>
#include <utility>
#include "../result.h"

namespace LocalTools::Core::Operations {

class RotatePagesOperation {
public:
    static Result<void> execute(const std::string& inputPath, const std::vector<std::pair<int, int>>& pageRotations, const std::string& outputPath);
};

} // namespace LocalTools::Core::Operations

#endif // LOCALTOOLS_CORE_OPERATIONS_ROTATE_PAGES_H
