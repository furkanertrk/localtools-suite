#ifndef LOCALTOOLS_CORE_JOB_QUEUE_H
#define LOCALTOOLS_CORE_JOB_QUEUE_H

#include <QObject>
#include <QThread>
#include <memory>
#include <vector>
#include <deque>
#include <atomic>
#include <functional>
#include "tool_operation.h"
#include "result.h"

namespace LocalTools::Core {

enum class JobStatus {
    Pending,
    Processing,
    Completed,
    Failed,
    Cancelled
};

struct BatchJobInfo {
    int index{0};
    QString name;
    QString inputPath;
    QString outputPath;
    JobStatus status{JobStatus::Pending};
    int progress{0};
    QString message;
};

/**
 * Manages background execution of single and multi-file (batch) IToolOperation instances.
 * Provides granular progress reporting, per-item status tracking, cancellation, and cleanup.
 */
class JobQueue : public QObject {
    Q_OBJECT

public:
    static JobQueue& instance();

    /**
     * Submit a single operation for background execution.
     */
    void submit(std::unique_ptr<IToolOperation> operation);

    /**
     * Submit a batch of operations for sequential background execution.
     */
    void submitBatch(std::vector<std::unique_ptr<IToolOperation>> operations);

    /**
     * Cancel the currently running operation.
     */
    void cancelCurrent();

    /**
     * Cancel all remaining jobs in the batch.
     */
    void cancelAll();

    /**
     * Stop accepting queued work, request cooperative cancellation and wait
     * until the current worker no longer touches application or temp data.
     * Intended for orderly application shutdown.
     */
    void shutdown();

    /**
     * Check if an operation or batch is currently running.
     */
    bool isRunning() const { return m_running; }

    /**
     * Get snapshot of current batch items.
     */
    const std::vector<BatchJobInfo>& currentBatchJobs() const { return m_batchInfos; }

signals:
    // Single-job signals (backward-compatible)
    void progressChanged(int percentage, const QString& message);
    void finished(bool success, const QString& message);

    // Batch execution signals (Phase V2.3)
    void batchStarted(int totalJobs);
    void jobStarted(int jobIndex, const QString& jobName);
    void jobProgress(int jobIndex, int percentage, const QString& message);
    void jobFinished(int jobIndex, bool success, const QString& message);
    void batchProgress(int completedJobs, int totalJobs, int overallPercentage);
    void batchFinished(int successCount, int failedCount, int cancelledCount);

private slots:
    void executeNextInBatch();

private:
    JobQueue();
    ~JobQueue() override;

    JobQueue(const JobQueue&) = delete;
    JobQueue& operator=(const JobQueue&) = delete;

    std::unique_ptr<IToolOperation> m_currentOperation;
    std::deque<std::unique_ptr<IToolOperation>> m_queuedOperations;
    std::vector<BatchJobInfo> m_batchInfos;
    
    QThread* m_workerThread{nullptr};
    bool m_running{false};
    std::atomic<bool> m_cancelAllRequested{false};

    int m_totalBatchJobs{0};
    int m_currentJobIndex{0};
    int m_successCount{0};
    int m_failedCount{0};
    int m_cancelledCount{0};
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_JOB_QUEUE_H
