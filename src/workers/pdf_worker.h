#ifndef LOCALTOOLS_WORKERS_PDF_WORKER_H
#define LOCALTOOLS_WORKERS_PDF_WORKER_H

#include <QObject>
#include <QThread>
#include <QString>
#include <QStringList>
#include <functional>
#include <vector>
#include <string>
#include "../core/result.h"

namespace LocalTools::Workers {

enum class OperationType {
    Merge,
    Split,
    Extract,
    Delete,
    Reorder,
    Rotate,
    ImagesToPdf,
    PdfToImages,
    PdfToText,
    Compress
};

class PdfWorker : public QObject {
    Q_OBJECT

public:
    explicit PdfWorker(QObject *parent = nullptr);
    ~PdfWorker() override;

    // Operation Setters
    void prepareMerge(const QStringList& inputFiles, const QString& outputFile);
    void prepareSplit(const QString& inputFile, const QString& outputDir, int pagesPerFile);
    void prepareExtract(const QString& inputFile, const QList<int>& pageIndices, const QString& outputFile);
    void prepareDelete(const QString& inputFile, const QList<int>& pageIndicesToDelete, const QString& outputFile);
    void prepareReorder(const QString& inputFile, const QList<int>& pageOrder, const QString& outputFile);
    void prepareRotate(const QString& inputFile, const QList<QPair<int, int>>& rotations, const QString& outputFile);
    void prepareImagesToPdf(const QStringList& imageFiles, const QString& outputFile);
    void preparePdfToImages(const QString& inputFile, const QString& outputDir, const QString& format, int dpi);
    void preparePdfToText(const QString& inputFile, const QString& outputFile);
    void prepareCompress(const QString& inputFile, const QString& outputFile, int compressionLevel);

public slots:
    void process();

signals:
    void progressChanged(int percentage);
    void statusMessage(const QString& message);
    void finished(const LocalTools::Core::Result<void>& result);

private:
    OperationType m_opType{OperationType::Merge};
    QStringList m_inputFiles;
    QString m_inputFile;
    QString m_outputFile;
    QString m_outputDir;
    QString m_imageFormat{"png"};
    int m_dpi{150};
    int m_pagesPerFile{1};
    QList<int> m_pageIndices;
    QList<int> m_pageOrder;
    QList<QPair<int, int>> m_rotations;
};

} // namespace LocalTools::Workers

#endif // LOCALTOOLS_WORKERS_PDF_WORKER_H
