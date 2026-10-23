#include "file_service.h"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <limits>

namespace LocalTools::Services {

FileService& FileService::instance() {
    static FileService instance;
    return instance;
}

FileService::FileService() = default;

bool FileService::isPdfFile(const std::filesystem::path& path) const {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".pdf";
}

bool FileService::exists(const std::filesystem::path& path) const {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

uint64_t FileService::getFileSize(const std::filesystem::path& path) const {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    return ec ? 0 : size;
}

std::string FileService::formatFileSize(uint64_t bytes) const {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    double kb = bytes / 1024.0;
    if (kb < 1024) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << kb << " KB";
        return ss.str();
    }
    double mb = kb / 1024.0;
    if (mb < 1024) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << mb << " MB";
        return ss.str();
    }
    double gb = mb / 1024.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << gb << " GB";
    return ss.str();
}

std::filesystem::path FileService::generateUniqueOutputPath(
    const std::filesystem::path& targetDir,
    const std::string& baseName,
    const std::string& extension) const {

    std::filesystem::path candidate = targetDir / (baseName + extension);
    if (!exists(candidate)) {
        return candidate;
    }

    int counter = 1;
    while (counter < std::numeric_limits<int>::max()) {
        std::string filename = baseName + "_" + std::to_string(counter) + extension;
        candidate = targetDir / filename;
        if (!exists(candidate)) {
            return candidate;
        }
        counter++;
    }

    return {};
}

QString FileService::generateUniqueOutputPath(const QString& desiredPath) const {
    const QFileInfo desiredInfo(desiredPath);
    const QString absoluteDesiredPath = desiredInfo.absoluteFilePath();
    if (!QFileInfo::exists(absoluteDesiredPath)) {
        return absoluteDesiredPath;
    }

    const QString extension = desiredInfo.suffix().isEmpty()
        ? QString{}
        : QStringLiteral(".") + desiredInfo.suffix();
    const QDir targetDir(desiredInfo.absolutePath());
    for (int counter = 1; counter < std::numeric_limits<int>::max(); ++counter) {
        const QString candidate = targetDir.absoluteFilePath(
            desiredInfo.completeBaseName() + QStringLiteral("_") + QString::number(counter) + extension);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

bool FileService::pathsReferToSameFile(const QString& firstPath,
                                       const QString& secondPath) const {
    if (firstPath.trimmed().isEmpty() || secondPath.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo firstInfo(firstPath);
    const QFileInfo secondInfo(secondPath);
    const QString firstCanonical = firstInfo.canonicalFilePath();
    const QString secondCanonical = secondInfo.canonicalFilePath();
    const QString first = QDir::cleanPath(
        firstCanonical.isEmpty() ? firstInfo.absoluteFilePath() : firstCanonical);
    const QString second = QDir::cleanPath(
        secondCanonical.isEmpty() ? secondInfo.absoluteFilePath() : secondCanonical);
#ifdef Q_OS_WIN
    return first.compare(second, Qt::CaseInsensitive) == 0;
#else
    return first == second;
#endif
}

std::filesystem::path FileService::getTempDirectory() const {
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/LocalTools";
    std::filesystem::path p(tempPath.toStdString());
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
}

Core::Result<std::filesystem::path> FileService::createTempFile(const std::string& prefix, const std::string& ext) const {
    static uint64_t counter = 0;
    auto tempDir = getTempDirectory();
    std::string filename = prefix + std::to_string(counter++) + "_" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()) + ext;
    std::filesystem::path p = tempDir / filename;
    return Core::Result<std::filesystem::path>::ok(p);
}

size_t FileService::cleanTempDirectory() const {
    auto tempDir = getTempDirectory();
    std::error_code ec;
    size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(tempDir, ec)) {
        if (entry.is_regular_file()) {
            if (std::filesystem::remove(entry.path(), ec)) {
                count++;
            }
        }
    }
    return count;
}

} // namespace LocalTools::Services
