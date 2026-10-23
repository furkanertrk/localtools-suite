#include "error.h"

namespace LocalTools::Core {

std::string PdfError::categoryToString() const {
    switch (category) {
        case PdfErrorCategory::None:                  return "Success";
        case PdfErrorCategory::FileNotFound:          return "File Not Found";
        case PdfErrorCategory::InvalidInput:          return "Invalid Input";
        case PdfErrorCategory::InvalidPdf:            return "Invalid or Corrupted PDF";
        case PdfErrorCategory::PermissionDenied:      return "Permission Denied";
        case PdfErrorCategory::InvalidPageRange:      return "Invalid Page Range";
        case PdfErrorCategory::OutputExists:          return "Output File Already Exists";
        case PdfErrorCategory::InsufficientDiskSpace: return "Insufficient Disk Space";
        case PdfErrorCategory::BackendError:          return "Processing Backend Error";
        case PdfErrorCategory::UnsupportedFeature:    return "Unsupported Feature";
        case PdfErrorCategory::Cancelled:             return "Cancelled";
        case PdfErrorCategory::Unknown:
        default:                                      return "Unknown Error";
    }
}

std::string PdfError::toUserFriendlyMessage() const {
    switch (category) {
        case PdfErrorCategory::FileNotFound:
            return "The selected file was not found. It may have been moved or deleted.";
        case PdfErrorCategory::InvalidInput:
            return "The selected input is invalid or could not be read.";
        case PdfErrorCategory::InvalidPdf:
            return "Failed to open PDF document. The file might be corrupted or password protected.";
        case PdfErrorCategory::PermissionDenied:
            return "Permission denied. The original file cannot be overwritten or is locked.";
        case PdfErrorCategory::InvalidPageRange:
            return "Invalid page number or range specified.";
        case PdfErrorCategory::OutputExists:
            return "Output file already exists. Please choose a different file name.";
        case PdfErrorCategory::InsufficientDiskSpace:
            return "Insufficient disk space. Please free up space on your drive.";
        case PdfErrorCategory::BackendError:
            return message.empty() ? "The processing engine could not complete the request." : message;
        case PdfErrorCategory::Cancelled:
            return "Operation was cancelled.";
        default:
            return message.empty() ? "An unexpected error occurred during the operation." : message;
    }
}

} // namespace LocalTools::Core
