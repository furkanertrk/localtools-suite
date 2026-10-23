#ifndef LOCALTOOLS_SERVICES_FILE_SERVICE_H
#define LOCALTOOLS_SERVICES_FILE_SERVICE_H

#include <filesystem>
#include <string>
#include <vector>
#include <QString>
#include "../core/result.h"

namespace LocalTools::Services {

class FileService {
public:
    static FileService& instance();

    // Path validation & checks
    bool isPdfFile(const std::filesystem::path& path) const;
    bool exists(const std::filesystem::path& path) const;
    uint64_t getFileSize(const std::filesystem::path& path) const;
    std::string formatFileSize(uint64_t bytes) const;

    // Output path generation & collision checks
    std::filesystem::path generateUniqueOutputPath(
        const std::filesystem::path& targetDir,
        const std::string& baseName,
        const std::string& extension = ".pdf") const;

    // Returns desiredPath when unused, otherwise appends a deterministic
    // numeric suffix without replacing an existing file.
    QString generateUniqueOutputPath(const QString& desiredPath) const;

    // Compares normalized/canonical paths using Windows' case-insensitive
    // semantics. This is the final backend guard against replacing an input.
    bool pathsReferToSameFile(const QString& firstPath, const QString& secondPath) const;

    // Application Temp Directory Management
    std::filesystem::path getTempDirectory() const;
    Core::Result<std::filesystem::path> createTempFile(const std::string& prefix = "tmp_", const std::string& ext = ".pdf") const;
    size_t cleanTempDirectory() const;

private:
    FileService();
    ~FileService() = default;

    FileService(const FileService&) = delete;
    FileService& operator=(const FileService&) = delete;
};

} // namespace LocalTools::Services

#endif // LOCALTOOLS_SERVICES_FILE_SERVICE_H
