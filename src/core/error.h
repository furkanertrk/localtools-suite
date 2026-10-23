#ifndef LOCALTOOLS_CORE_ERROR_H
#define LOCALTOOLS_CORE_ERROR_H

#include <string>

namespace LocalTools::Core {

enum class PdfErrorCategory {
    None = 0,
    FileNotFound,
    InvalidInput,
    InvalidPdf,
    PermissionDenied,
    InvalidPageRange,
    OutputExists,
    InsufficientDiskSpace,
    BackendError,
    UnsupportedFeature,
    Cancelled,
    Unknown
};

struct PdfError {
    PdfErrorCategory category{PdfErrorCategory::None};
    std::string message;
    std::string details;

    bool isSuccess() const { return category == PdfErrorCategory::None; }
    bool isError() const { return category != PdfErrorCategory::None; }

    static PdfError none() {
        return PdfError{PdfErrorCategory::None, "", ""};
    }

    static PdfError create(PdfErrorCategory category, const std::string& message, const std::string& details = "") {
        return PdfError{category, message, details};
    }

    std::string categoryToString() const;
    std::string toUserFriendlyMessage() const;
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_ERROR_H
