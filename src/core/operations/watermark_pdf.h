#ifndef LOCALTOOLS_CORE_OPERATIONS_WATERMARK_PDF_H
#define LOCALTOOLS_CORE_OPERATIONS_WATERMARK_PDF_H

#include <string>
#include <vector>
#include <QString>
#include <QColor>
#include "../result.h"

namespace LocalTools::Core::Operations {

struct WatermarkOptions {
    QString text{"CONFIDENTIAL"};
    int fontSize{48};
    double opacity{0.25};      // 0.0 to 1.0
    int rotationAngle{-45};    // degrees (e.g. -45 for diagonal)
    QColor color{QColor(180, 0, 0)};
    int dpi{150};              // Render resolution
};

class WatermarkPdfOperation {
public:
    static Result<void> execute(const std::string& inputPath,
                                const std::string& outputPath,
                                const WatermarkOptions& options);
};

} // namespace LocalTools::Core::Operations

#endif // LOCALTOOLS_CORE_OPERATIONS_WATERMARK_PDF_H
