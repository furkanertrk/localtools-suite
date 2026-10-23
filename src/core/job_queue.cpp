#include "job_queue.h"
#include <QTimer>

namespace LocalTools::Core {

JobQueue& JobQueue::instance() {
    static JobQueue instance;
    return instance;
}

JobQueue::JobQueue() : QObject(nullptr) {}

JobQueue::~JobQueue() {
    shutdown();
}

void JobQueue::submit(std::unique_ptr<IToolOperation> operation) {
    if (m_running) {
        emit finished(false, tr("Another operation is already in progress."));
        return;
    }

    m_currentOperation = std::move(operation);
    m_running = true;

    // Set up progress callback
    m_currentOperation->setProgressCallback(
        [this](int percentage, const QString& message) {
            QMetaObject::invokeMethod(this, [this, percentage, message]() {
                emit progressChanged(percentage, message);
            }, Qt::QueuedConnection);
        }
    );

    IToolOperation* opPtr = m_currentOperation.get();

    // QThread::started connected to this QObject would queue the lambda back
    // to the UI thread because JobQueue lives there. QThread::create makes the
    // execution context explicit and keeps the operation off the event loop.
    QThread* workerThread = QThread::create([this, opPtr]() {
        Result<void> result = opPtr->execute();

        QMetaObject::invokeMethod(this, [this, result]() {
            m_running = false;
            if (result.hasValue()) {
                emit finished(true, tr("Operation completed successfully."));
            } else {
                QString errorMsg = QString::fromStdString(result.error().toUserFriendlyMessage());
                emit finished(false, errorMsg);
            }
            // Cleanup
            m_currentOperation.reset();
        }, Qt::QueuedConnection);
    });
    workerThread->setParent(this);
    m_workerThread = workerThread;

    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, this, [this, workerThread]() {
        if (m_workerThread == workerThread) m_workerThread = nullptr;
    });

    workerThread->start();
}

void JobQueue::submitBatch(std::vector<std::unique_ptr<IToolOperation>> operations) {
    if (m_running) {
        emit batchFinished(0, static_cast<int>(operations.size()), 0);
        return;
    }

    if (operations.empty()) {
        emit batchFinished(0, 0, 0);
        return;
    }

    m_running = true;
    m_cancelAllRequested = false;
    m_totalBatchJobs = static_cast<int>(operations.size());
    m_currentJobIndex = 0;
    m_successCount = 0;
    m_failedCount = 0;
    m_cancelledCount = 0;

    m_batchInfos.clear();
    m_batchInfos.reserve(m_totalBatchJobs);

    for (int i = 0; i < m_totalBatchJobs; ++i) {
        BatchJobInfo info;
        info.index = i;
        QString dName = operations[i] ? operations[i]->displayName() : QString();
        info.name = dName.isEmpty() ? QString(tr("Task %1")).arg(i + 1) : dName;
        info.status = JobStatus::Pending;
        info.progress = 0;
        m_batchInfos.push_back(info);
    }

    m_queuedOperations.clear();
    for (auto& op : operations) {
        m_queuedOperations.push_back(std::move(op));
    }

    emit batchStarted(m_totalBatchJobs);
    executeNextInBatch();
}

void JobQueue::executeNextInBatch() {
    if (m_cancelAllRequested || m_queuedOperations.empty()) {
        // Mark remaining jobs as Cancelled if cancelAll was requested
        if (m_cancelAllRequested) {
            for (size_t i = m_currentJobIndex; i < m_batchInfos.size(); ++i) {
                m_batchInfos[i].status = JobStatus::Cancelled;
                m_cancelledCount++;
            }
        }
        m_running = false;
        m_currentOperation.reset();
        emit batchFinished(m_successCount, m_failedCount, m_cancelledCount);
        return;
    }

    m_currentOperation = std::move(m_queuedOperations.front());
    m_queuedOperations.pop_front();

    int jobIdx = m_currentJobIndex;
    m_batchInfos[jobIdx].status = JobStatus::Processing;
    emit jobStarted(jobIdx, m_batchInfos[jobIdx].name);

    m_currentOperation->setProgressCallback([this, jobIdx](int percentage, const QString& message) {
        QMetaObject::invokeMethod(this, [this, jobIdx, percentage, message]() {
            if (static_cast<size_t>(jobIdx) < m_batchInfos.size()) {
                m_batchInfos[jobIdx].progress = percentage;
                m_batchInfos[jobIdx].message = message;
            }
            emit jobProgress(jobIdx, percentage, message);
            emit progressChanged(percentage, message);
        }, Qt::QueuedConnection);
    });

    IToolOperation* opPtr = m_currentOperation.get();

    QThread* workerThread = QThread::create([this, opPtr, jobIdx]() {
        Result<void> result = opPtr->execute();

        QMetaObject::invokeMethod(this, [this, result, jobIdx]() {
            bool success = result.hasValue();
            if (success) {
                m_successCount++;
                if (static_cast<size_t>(jobIdx) < m_batchInfos.size()) {
                    m_batchInfos[jobIdx].status = JobStatus::Completed;
                    m_batchInfos[jobIdx].progress = 100;
                }
                emit jobFinished(jobIdx, true, tr("Completed"));
            } else {
                if (m_cancelAllRequested) {
                    m_cancelledCount++;
                    if (static_cast<size_t>(jobIdx) < m_batchInfos.size()) {
                        m_batchInfos[jobIdx].status = JobStatus::Cancelled;
                    }
                    emit jobFinished(jobIdx, false, tr("Cancelled"));
                } else {
                    m_failedCount++;
                    QString err = QString::fromStdString(result.error().toUserFriendlyMessage());
                    if (static_cast<size_t>(jobIdx) < m_batchInfos.size()) {
                        m_batchInfos[jobIdx].status = JobStatus::Failed;
                        m_batchInfos[jobIdx].message = err;
                    }
                    emit jobFinished(jobIdx, false, err);
                }
            }

            int completed = m_successCount + m_failedCount + m_cancelledCount;
            int overallPct = (m_totalBatchJobs > 0) ? (completed * 100 / m_totalBatchJobs) : 100;
            emit batchProgress(completed, m_totalBatchJobs, overallPct);

            m_currentJobIndex++;
            m_currentOperation.reset();

        }, Qt::QueuedConnection);
    });
    workerThread->setParent(this);
    m_workerThread = workerThread;

    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, this, [this, workerThread]() {
        if (m_workerThread == workerThread) m_workerThread = nullptr;
        QTimer::singleShot(0, this, &JobQueue::executeNextInBatch);
    });

    workerThread->start();
}

void JobQueue::cancelCurrent() {
    if (m_currentOperation) {
        m_currentOperation->requestCancel();
    }
}

void JobQueue::cancelAll() {
    m_cancelAllRequested = true;
    cancelCurrent();
}

void JobQueue::shutdown() {
    cancelAll();
    m_queuedOperations.clear();
    if (m_workerThread && m_workerThread->isRunning()) {
        // QThread::quit() cannot interrupt a QThread::create() function. The
        // operation observes requestCancel(); wait prevents temp cleanup and
        // static destruction from racing the remaining worker instructions.
        m_workerThread->wait();
    }
    m_workerThread = nullptr;
    m_currentOperation.reset();
    m_running = false;
}

} // namespace LocalTools::Core
