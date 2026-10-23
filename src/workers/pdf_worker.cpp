#include "pdf_worker.h"

#include "../core/operations/merge_pdf.h"
#include "../core/operations/split_pdf.h"
#include "../core/operations/extract_pages.h"
#include "../core/operations/delete_pages.h"
#include "../core/operations/reorder_pages.h"
#include "../core/operations/rotate_pages.h"
#include "../core/operations/images_to_pdf.h"
#include "../core/operations/pdf_to_images.h"
#include "../core/operations/pdf_to_text.h"
#include "../core/operations/compress_pdf.h"
#include "../services/logging_service.h"

namespace LocalTools::Workers {

PdfWorker::PdfWorker(QObject *parent)
    : QObject(parent) {}

PdfWorker::~PdfWorker() = default;

void PdfWorker::prepareMerge(const QStringList& inputFiles, const QString& outputFile) {
    m_opType = OperationType::Merge;
    m_inputFiles = inputFiles;
    m_outputFile = outputFile;
}

void PdfWorker::prepareSplit(const QString& inputFile, const QString& outputDir, int pagesPerFile) {
    m_opType = OperationType::Split;
    m_inputFile = inputFile;
    m_outputDir = outputDir;
    m_pagesPerFile = pagesPerFile;
}

void PdfWorker::prepareExtract(const QString& inputFile, const QList<int>& pageIndices, const QString& outputFile) {
    m_opType = OperationType::Extract;
    m_inputFile = inputFile;
    m_pageIndices = pageIndices;
    m_outputFile = outputFile;
}

void PdfWorker::prepareDelete(const QString& inputFile, const QList<int>& pageIndicesToDelete, const QString& outputFile) {
    m_opType = OperationType::Delete;
    m_inputFile = inputFile;
    m_pageIndices = pageIndicesToDelete;
    m_outputFile = outputFile;
}

void PdfWorker::prepareReorder(const QString& inputFile, const QList<int>& pageOrder, const QString& outputFile) {
    m_opType = OperationType::Reorder;
    m_inputFile = inputFile;
    m_pageOrder = pageOrder;
    m_outputFile = outputFile;
}

void PdfWorker::prepareRotate(const QString& inputFile, const QList<QPair<int, int>>& rotations, const QString& outputFile) {
    m_opType = OperationType::Rotate;
    m_inputFile = inputFile;
    m_rotations = rotations;
    m_outputFile = outputFile;
}

void PdfWorker::prepareImagesToPdf(const QStringList& imageFiles, const QString& outputFile) {
    m_opType = OperationType::ImagesToPdf;
    m_inputFiles = imageFiles;
    m_outputFile = outputFile;
}

void PdfWorker::preparePdfToImages(const QString& inputFile, const QString& outputDir, const QString& format, int dpi) {
    m_opType = OperationType::PdfToImages;
    m_inputFile = inputFile;
    m_outputDir = outputDir;
    m_imageFormat = format;
    m_dpi = dpi;
}

void PdfWorker::preparePdfToText(const QString& inputFile, const QString& outputFile) {
    m_opType = OperationType::PdfToText;
    m_inputFile = inputFile;
    m_outputFile = outputFile;
}

void PdfWorker::prepareCompress(const QString& inputFile, const QString& outputFile, int compressionLevel) {
    m_opType = OperationType::Compress;
    m_inputFile = inputFile;
    m_outputFile = outputFile;
    m_dpi = compressionLevel;
}

void PdfWorker::process() {
    emit progressChanged(10);
    emit statusMessage(tr("Starting operation..."));

    Core::Result<void> res = Core::Result<void>::ok();

    switch (m_opType) {
        case OperationType::Merge: {
            emit statusMessage(tr("Merging PDF files..."));
            std::vector<std::string> inputs;
            for (const auto& f : m_inputFiles) {
                inputs.push_back(f.toStdString());
            }
            emit progressChanged(40);
            res = Core::Operations::MergePdfOperation::execute(inputs, m_outputFile.toStdString());
            break;
        }
        case OperationType::Split: {
            emit statusMessage(tr("Splitting PDF document..."));
            emit progressChanged(40);
            res = Core::Operations::SplitPdfOperation::execute(m_inputFile.toStdString(), m_outputDir.toStdString(), m_pagesPerFile);
            break;
        }
        case OperationType::Extract: {
            emit statusMessage(tr("Extracting selected pages..."));
            std::vector<int> idx(m_pageIndices.begin(), m_pageIndices.end());
            emit progressChanged(40);
            res = Core::Operations::ExtractPagesOperation::execute(m_inputFile.toStdString(), idx, m_outputFile.toStdString());
            break;
        }
        case OperationType::Delete: {
            emit statusMessage(tr("Deleting selected pages..."));
            std::vector<int> idx(m_pageIndices.begin(), m_pageIndices.end());
            emit progressChanged(40);
            res = Core::Operations::DeletePagesOperation::execute(m_inputFile.toStdString(), idx, m_outputFile.toStdString());
            break;
        }
        case OperationType::Reorder: {
            emit statusMessage(tr("Reordering pages..."));
            std::vector<int> pageOrder(m_pageOrder.begin(), m_pageOrder.end());
            emit progressChanged(40);
            res = Core::Operations::ReorderPagesOperation::execute(m_inputFile.toStdString(), pageOrder, m_outputFile.toStdString());
            break;
        }
        case OperationType::Rotate: {
            emit statusMessage(tr("Rotating pages..."));
            std::vector<std::pair<int, int>> rots;
            for (const auto& p : m_rotations) {
                rots.push_back({p.first, p.second});
            }
            emit progressChanged(40);
            res = Core::Operations::RotatePagesOperation::execute(m_inputFile.toStdString(), rots, m_outputFile.toStdString());
            break;
        }
        case OperationType::ImagesToPdf: {
            emit statusMessage(tr("Converting images to PDF..."));
            std::vector<std::string> imgs;
            for (const auto& img : m_inputFiles) {
                imgs.push_back(img.toStdString());
            }
            emit progressChanged(30);
            res = Core::Operations::ImagesToPdfOperation::execute(imgs, m_outputFile.toStdString());
            break;
        }
        case OperationType::PdfToImages: {
            emit statusMessage(tr("Converting PDF pages to images..."));
            auto progFunc = [this](int current, int total) {
                int pct = 10 + (current * 85 / total);
                emit progressChanged(pct);
                emit statusMessage(tr("Rendering image: Page %1 / %2").arg(current).arg(total));
            };
            res = Core::Operations::PdfToImagesOperation::execute(
                m_inputFile.toStdString(), m_outputDir.toStdString(),
                m_imageFormat.toStdString(), m_dpi, progFunc
            );
            break;
        }
        case OperationType::PdfToText: {
            emit statusMessage(tr("Extracting PDF text..."));
            auto progFunc = [this](int current, int total) {
                int pct = 10 + (current * 85 / total);
                emit progressChanged(pct);
                emit statusMessage(tr("Extracting text: Page %1 / %2").arg(current).arg(total));
            };
            res = Core::Operations::PdfToTextOperation::execute(
                m_inputFile.toStdString(), m_outputFile.toStdString(), progFunc
            );
            break;
        }
        case OperationType::Compress: {
            emit statusMessage(tr("Compress PDF..."));
            auto progFunc = [this](int current, int total) {
                int pct = 10 + (current * 85 / total);
                emit progressChanged(pct);
                emit statusMessage(tr("Compressing page: Page %1 / %2").arg(current).arg(total));
            };
            Core::Operations::CompressionLevel lvl = static_cast<Core::Operations::CompressionLevel>(m_dpi);
            res = Core::Operations::CompressPdfOperation::execute(
                m_inputFile.toStdString(), m_outputFile.toStdString(), lvl, progFunc
            );
            break;
        }
        default:
            break;
    }

    emit progressChanged(100);
    emit finished(res);
}

} // namespace LocalTools::Workers
