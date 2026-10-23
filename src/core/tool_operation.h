#ifndef LOCALTOOLS_CORE_TOOL_OPERATION_H
#define LOCALTOOLS_CORE_TOOL_OPERATION_H

#include <QObject>
#include <QString>
#include <QAtomicInt>
#include <functional>
#include "result.h"

namespace LocalTools::Core {

/**
 * Abstract interface for all tool operations.
 * Every tool (PDF, Image, Video, etc.) implements this interface.
 * Operations run on worker threads via JobQueue.
 */
class IToolOperation {
public:
    using ProgressCallback = std::function<void(int percentage, const QString& statusMessage)>;

    virtual ~IToolOperation() = default;

    /**
     * Execute the operation. Called on a worker thread.
     * Implementations should periodically check isCancelRequested()
     * and return early with an appropriate error if cancelled.
     */
    virtual Result<void> execute() = 0;

    /**
     * Optional user-friendly display name for this operation (e.g. filename).
     */
    virtual QString displayName() const { return {}; }

    /**
     * Request cooperative cancellation.
     * The operation should stop at the next safe checkpoint.
     */
    void requestCancel() { m_cancelRequested.storeRelaxed(1); }

    /**
     * Check if cancellation has been requested.
     */
    bool isCancelRequested() const { return m_cancelRequested.loadRelaxed() != 0; }

    /**
     * Set the progress callback. Called by JobQueue before execution.
     */
    void setProgressCallback(ProgressCallback cb) { m_progressCallback = std::move(cb); }

protected:
    /**
     * Report progress from within execute().
     * @param percentage 0-100
     * @param message Status message for the UI
     */
    void reportProgress(int percentage, const QString& message = {}) {
        if (m_progressCallback) {
            m_progressCallback(percentage, message);
        }
    }

private:
    QAtomicInt m_cancelRequested{0};
    ProgressCallback m_progressCallback;
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_TOOL_OPERATION_H
