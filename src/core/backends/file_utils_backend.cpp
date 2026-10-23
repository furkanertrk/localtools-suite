#include "file_utils_backend.h"

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QCryptographicHash>
#include <algorithm>
#include <set>

namespace LocalTools::Core {

std::vector<FileUtilsBackend::RenamePreviewItem> FileUtilsBackend::previewRename(
    const QStringList& filePaths, const RenameRule& rule) {
    
    std::vector<RenamePreviewItem> results;
    results.reserve(filePaths.size());

    std::set<QString> plannedDestinations;

    int currentNum = rule.startNumber;

    for (int i = 0; i < filePaths.size(); ++i) {
        const QString& srcPath = filePaths[i];
        QFileInfo fi(srcPath);
        QString baseName = fi.completeBaseName();
        QString ext = fi.suffix();

        // 1. Find and Replace
        if (!rule.findText.isEmpty()) {
            baseName.replace(rule.findText, rule.replaceText, Qt::CaseInsensitive);
        }

        // 2. Case Mode
        if (rule.caseMode == RenameRule::CaseMode::Lowercase) {
            baseName = baseName.toLower();
            ext = ext.toLower();
        } else if (rule.caseMode == RenameRule::CaseMode::Uppercase) {
            baseName = baseName.toUpper();
            ext = ext.toUpper();
        } else if (rule.caseMode == RenameRule::CaseMode::TitleCase && !baseName.isEmpty()) {
            QStringList words = baseName.split(' ', Qt::SkipEmptyParts);
            for (QString& w : words) {
                if (!w.isEmpty()) {
                    w = w.left(1).toUpper() + w.mid(1).toLower();
                }
            }
            baseName = words.join(' ');
        }

        // 3. Numbering
        if (rule.useNumbering) {
            QString numStr = QString("%1").arg(currentNum, rule.numberPadding, 10, QChar('0'));
            baseName = baseName + "_" + numStr;
            currentNum++;
        }

        // 4. Prefix & Suffix
        baseName = rule.prefix + baseName + rule.suffix;

        QString newFileName = ext.isEmpty() ? baseName : (baseName + "." + ext);
        QString newPath = fi.dir().absoluteFilePath(newFileName);

        RenamePreviewItem item;
        item.originalPath = srcPath;
        item.originalName = fi.fileName();
        item.newName = newFileName;
        item.newPath = newPath;

        // Check conflicts
        if (srcPath != newPath && (QFile::exists(newPath) || plannedDestinations.count(newPath) > 0)) {
            item.hasConflict = true;
        } else {
            item.hasConflict = false;
        }

        plannedDestinations.insert(newPath);
        results.push_back(std::move(item));
    }

    return results;
}

Result<int> FileUtilsBackend::executeRename(const std::vector<RenamePreviewItem>& items) {
    int count = 0;
    for (const auto& item : items) {
        if (item.originalPath == item.newPath) continue;
        if (item.hasConflict) continue;

        if (QFile::rename(item.originalPath, item.newPath)) {
            count++;
        }
    }
    return Result<int>::ok(count);
}

Result<FileUtilsBackend::FileHashes> FileUtilsBackend::computeHashes(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return Result<FileHashes>::fail(PdfErrorCategory::FileNotFound,
                                        "Failed to open file: " + filePath.toStdString());
    }

    QCryptographicHash md5Hash(QCryptographicHash::Md5);
    QCryptographicHash sha1Hash(QCryptographicHash::Sha1);
    QCryptographicHash sha256Hash(QCryptographicHash::Sha256);
    QCryptographicHash sha512Hash(QCryptographicHash::Sha512);

    char buffer[65536];
    qint64 bytesRead = 0;
    while ((bytesRead = file.read(buffer, sizeof(buffer))) > 0) {
        QByteArrayView chunk(buffer, bytesRead);
        md5Hash.addData(chunk);
        sha1Hash.addData(chunk);
        sha256Hash.addData(chunk);
        sha512Hash.addData(chunk);
    }

    FileHashes hashes;
    hashes.filePath = filePath;
    hashes.fileSize = file.size();
    hashes.md5 = QString::fromLatin1(md5Hash.result().toHex());
    hashes.sha1 = QString::fromLatin1(sha1Hash.result().toHex());
    hashes.sha256 = QString::fromLatin1(sha256Hash.result().toHex());
    hashes.sha512 = QString::fromLatin1(sha512Hash.result().toHex());

    return Result<FileHashes>::ok(hashes);
}

bool FileUtilsBackend::verifyHash(const QString& filePath, const QString& expectedHash) {
    QString trimmed = expectedHash.trimmed().toLower();
    auto hashRes = computeHashes(filePath);
    if (hashRes.hasError()) return false;

    const auto& h = hashRes.value();
    return (h.md5.toLower() == trimmed ||
            h.sha1.toLower() == trimmed ||
            h.sha256.toLower() == trimmed ||
            h.sha512.toLower() == trimmed);
}

Result<FileUtilsBackend::DirectoryAnalysisResult> FileUtilsBackend::analyzeDirectory(
    const QString& dirPath, int maxLargeFiles) {
    
    QDir dir(dirPath);
    if (!dir.exists()) {
        return Result<DirectoryAnalysisResult>::fail(PdfErrorCategory::FileNotFound,
                                                    "Folder not found: " + dirPath.toStdString());
    }

    DirectoryAnalysisResult result;
    result.directoryPath = dirPath;

    std::map<QString, CategoryBreakdown> catMap;
    catMap["Images"] = {"Images", 0, 0};
    catMap["Videolar"] = {"Videolar", 0, 0};
    catMap["PDF Documentsi"] = {"PDF Documentsi", 0, 0};
    catMap["Ofis & Metin"] = {"Ofis & Metin", 0, 0};
    catMap["Archives"] = {"Archives", 0, 0};
    catMap["Audio"] = {"Audio", 0, 0};
    catMap["Kod & Veri"] = {"Kod & Veri", 0, 0};
    catMap["Other"] = {"Other", 0, 0};

    std::vector<LargeFileInfo> allFiles;

    static const std::set<QString> imgExts = {"jpg", "jpeg", "png", "webp", "bmp", "gif", "tiff", "tif", "svg", "ico", "heic", "avif"};
    static const std::set<QString> vidExts = {"mp4", "mkv", "mov", "avi", "webm", "flv", "wmv", "m4v"};
    static const std::set<QString> docExts = {"docx", "doc", "xlsx", "xls", "pptx", "ppt", "txt", "md", "rtf", "csv", "odt", "ods"};
    static const std::set<QString> arcExts = {"zip", "rar", "7z", "tar", "gz", "bz2", "xz", "iso"};
    static const std::set<QString> audExts = {"mp3", "wav", "flac", "aac", "ogg", "m4a", "wma"};
    static const std::set<QString> codeExts = {"cpp", "h", "c", "py", "js", "html", "css", "json", "xml", "sql", "ts", "rs", "go", "java"};

    QDirIterator it(dirPath, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        qint64 size = fi.size();
        QString ext = fi.suffix().toLower();

        result.totalSizeBytes += size;
        result.totalFilesCount++;

        QString cat = "Other";
        if (ext == "pdf") cat = "PDF Documentsi";
        else if (imgExts.count(ext)) cat = "Images";
        else if (vidExts.count(ext)) cat = "Videolar";
        else if (docExts.count(ext)) cat = "Ofis & Metin";
        else if (arcExts.count(ext)) cat = "Archives";
        else if (audExts.count(ext)) cat = "Audio";
        else if (codeExts.count(ext)) cat = "Kod & Veri";

        catMap[cat].totalBytes += size;
        catMap[cat].fileCount++;

        allFiles.push_back({fi.absoluteFilePath(), fi.fileName(), size});
    }

    // Count folders
    QDirIterator folderIt(dirPath, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (folderIt.hasNext()) {
        folderIt.next();
        result.totalFoldersCount++;
    }

    for (const auto& [name, breakdown] : catMap) {
        if (breakdown.fileCount > 0) {
            result.categories.push_back(breakdown);
        }
    }

    // Sort categories by total size descending
    std::sort(result.categories.begin(), result.categories.end(), [](const auto& a, const auto& b) {
        return a.totalBytes > b.totalBytes;
    });

    // Sort top largest files descending
    std::sort(allFiles.begin(), allFiles.end(), [](const auto& a, const auto& b) {
        return a.sizeBytes > b.sizeBytes;
    });

    int topN = std::min(maxLargeFiles, static_cast<int>(allFiles.size()));
    result.largestFiles.assign(allFiles.begin(), allFiles.begin() + topN);

    return Result<DirectoryAnalysisResult>::ok(result);
}

Result<std::vector<FileUtilsBackend::DuplicateGroup>> FileUtilsBackend::findDuplicates(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) {
        return Result<std::vector<DuplicateGroup>>::fail(PdfErrorCategory::FileNotFound,
                                                        "Folder not found: " + dirPath.toStdString());
    }

    // Stage 1: Group by file size
    std::map<qint64, QStringList> sizeMap;
    QDirIterator it(dirPath, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        if (fi.size() > 0) {
            sizeMap[fi.size()].append(fi.absoluteFilePath());
        }
    }

    // Filter candidate sizes with >= 2 files
    std::vector<QStringList> stage1Candidates;
    for (auto& [size, list] : sizeMap) {
        if (list.size() >= 2) {
            stage1Candidates.push_back(std::move(list));
        }
    }

    // Stage 2 & 3: Compute Full SHA-256 for identical size files
    std::map<QString, QStringList> hashMap;
    for (const auto& candidateList : stage1Candidates) {
        for (const QString& fPath : candidateList) {
            QFile file(fPath);
            if (file.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha256);
                if (hash.addData(&file)) {
                    QString digest = QString::fromLatin1(hash.result().toHex());
                    hashMap[digest].append(fPath);
                }
            }
        }
    }

    std::vector<DuplicateGroup> duplicates;
    for (const auto& [digest, paths] : hashMap) {
        if (paths.size() >= 2) {
            DuplicateGroup grp;
            grp.sha256 = digest;
            grp.filePaths = paths;
            grp.fileSize = QFileInfo(paths.first()).size();
            duplicates.push_back(std::move(grp));
        }
    }

    // Sort duplicates by file size descending
    std::sort(duplicates.begin(), duplicates.end(), [](const auto& a, const auto& b) {
        return a.fileSize > b.fileSize;
    });

    return Result<std::vector<DuplicateGroup>>::ok(duplicates);
}

} // namespace LocalTools::Core
