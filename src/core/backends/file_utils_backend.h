#ifndef LOCALTOOLS_CORE_BACKENDS_FILE_UTILS_BACKEND_H
#define LOCALTOOLS_CORE_BACKENDS_FILE_UTILS_BACKEND_H

#include <QString>
#include <QStringList>
#include <QCryptographicHash>
#include <vector>
#include <map>
#include "../result.h"

namespace LocalTools::Core {

/**
 * Backend engine for File Utilities:
 * - Rule-based batch renaming with conflict detection
 * - Multi-algorithm file hashing & verification (MD5, SHA-1, SHA-256, SHA-512)
 * - Directory size & file category analysis
 * - 3-stage duplicate file detection (Size -> Header hash -> Full hash)
 */
class FileUtilsBackend {
public:
    // --- 1. Batch Renaming ---
    struct RenameRule {
        QString prefix;
        QString suffix;
        QString findText;
        QString replaceText;
        bool useNumbering{false};
        int startNumber{1};
        int numberPadding{3}; // 3 => 001, 002
        enum class CaseMode { None, Lowercase, Uppercase, TitleCase } caseMode{CaseMode::None};
    };

    struct RenamePreviewItem {
        QString originalPath;
        QString originalName;
        QString newName;
        QString newPath;
        bool hasConflict{false};
    };

    static std::vector<RenamePreviewItem> previewRename(const QStringList& filePaths, const RenameRule& rule);
    static Result<int> executeRename(const std::vector<RenamePreviewItem>& items);

    // --- 2. Hash Generation & Verification ---
    struct FileHashes {
        QString filePath;
        qint64 fileSize{0};
        QString md5;
        QString sha1;
        QString sha256;
        QString sha512;
    };

    static Result<FileHashes> computeHashes(const QString& filePath);
    static bool verifyHash(const QString& filePath, const QString& expectedHash);

    // --- 3. Directory Size & Category Analysis ---
    struct CategoryBreakdown {
        QString categoryName;
        qint64 totalBytes{0};
        int fileCount{0};
    };

    struct LargeFileInfo {
        QString filePath;
        QString fileName;
        qint64 sizeBytes{0};
    };

    struct DirectoryAnalysisResult {
        QString directoryPath;
        qint64 totalSizeBytes{0};
        int totalFilesCount{0};
        int totalFoldersCount{0};
        std::vector<CategoryBreakdown> categories;
        std::vector<LargeFileInfo> largestFiles;
    };

    static Result<DirectoryAnalysisResult> analyzeDirectory(const QString& dirPath, int maxLargeFiles = 10);

    // --- 4. Duplicate Finder ---
    struct DuplicateGroup {
        qint64 fileSize{0};
        QString sha256;
        QStringList filePaths;
    };

    static Result<std::vector<DuplicateGroup>> findDuplicates(const QString& dirPath);
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_BACKENDS_FILE_UTILS_BACKEND_H
