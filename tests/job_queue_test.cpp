#include <QCoreApplication>
#include <QEventLoop>
#include <QEvent>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>

#include <iostream>
#include <atomic>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

#include "core/job_queue.h"

namespace {

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

class ThreadProbeOperation final : public LocalTools::Core::IToolOperation {
public:
    explicit ThreadProbeOperation(std::atomic<QThread*>* executedThread,
                                  bool waitForCancellation = false)
        : m_executedThread(executedThread), m_waitForCancellation(waitForCancellation) {}

    LocalTools::Core::Result<void> execute() override {
        m_executedThread->store(QThread::currentThread(), std::memory_order_release);
        reportProgress(10, QStringLiteral("started"));
        for (int i = 0; i < (m_waitForCancellation ? 100 : 5); ++i) {
            if (isCancelRequested()) {
                return LocalTools::Core::Result<void>::fail(
                    LocalTools::Core::PdfErrorCategory::Cancelled,
                    "Operation was cancelled.");
            }
            QThread::msleep(10);
        }
        reportProgress(100, QStringLiteral("finished"));
        return LocalTools::Core::Result<void>::ok();
    }

private:
    std::atomic<QThread*>* m_executedThread;
    bool m_waitForCancellation;
};

bool waitForFinished(LocalTools::Core::JobQueue& queue,
                     std::unique_ptr<LocalTools::Core::IToolOperation> operation,
                     bool* success,
                     QString* message,
                     bool cancel) {
    QEventLoop loop;
    QMetaObject::Connection connection = QObject::connect(
        &queue, &LocalTools::Core::JobQueue::finished, &loop,
        [&](bool ok, const QString& text) {
            *success = ok;
            *message = text;
            loop.quit();
        });
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(3000);
    queue.submit(std::move(operation));
    if (cancel) QTimer::singleShot(40, &queue, &LocalTools::Core::JobQueue::cancelCurrent);
    loop.exec();
    QObject::disconnect(connection);
    return deadline.isActive();
}

#ifdef Q_OS_WIN
DWORD currentHandleCount() {
    DWORD count = 0;
    return GetProcessHandleCount(GetCurrentProcess(), &count) ? count : 0;
}

int currentProcessThreadCount() {
    const DWORD currentPid = GetCurrentProcessId();
    int count = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return -1;
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID == currentPid) ++count;
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return count;
}
#endif

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    auto& queue = LocalTools::Core::JobQueue::instance();

    std::atomic<QThread*> executionThread{nullptr};
    bool success = false;
    QString message;
    if (!check(waitForFinished(queue,
                               std::make_unique<ThreadProbeOperation>(&executionThread),
                               &success, &message, false),
               "background job must finish before timeout")) return 1;
    if (!check(success, "normal background job must report success")) return 1;
    if (!check(executionThread.load(std::memory_order_acquire) != nullptr
               && executionThread.load(std::memory_order_acquire)
                   != QCoreApplication::instance()->thread(),
               "operation execute() must not run on the application thread")) return 1;

    executionThread.store(nullptr, std::memory_order_release);
    success = true;
    message.clear();
    if (!check(waitForFinished(queue,
                               std::make_unique<ThreadProbeOperation>(&executionThread, true),
                               &success, &message, true),
               "cancelled background job must finish before timeout")) return 1;
    if (!check(!success && message.contains(QStringLiteral("cancel"), Qt::CaseInsensitive),
               "cooperative cancellation must be reported as cancellation, not success")) return 1;

#ifdef Q_OS_WIN
    const DWORD baselineHandles = currentHandleCount();
    const int baselineThreads = currentProcessThreadCount();
#endif
    for (int iteration = 0; iteration < 20; ++iteration) {
        executionThread.store(nullptr, std::memory_order_release);
        success = false;
        message.clear();
        if (!check(waitForFinished(queue,
                                   std::make_unique<ThreadProbeOperation>(&executionThread),
                                   &success, &message, false),
                   "repeated background operation must finish before timeout")) return 1;
        if (!check(success, "repeated background operation must report success")) return 1;
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
#ifdef Q_OS_WIN
    if (!check(currentProcessThreadCount() <= baselineThreads + 1,
               "twenty queue runs must not continuously leak worker threads")) return 1;
    if (!check(currentHandleCount() <= baselineHandles + 8,
               "twenty queue runs must not continuously leak handles")) return 1;
#endif

    executionThread.store(nullptr, std::memory_order_release);
    queue.submit(std::make_unique<ThreadProbeOperation>(&executionThread, true));
    QThread::msleep(40);
    QElapsedTimer shutdownTimer;
    shutdownTimer.start();
    queue.shutdown();
    if (!check(!queue.isRunning(), "shutdown must leave the queue idle")) return 1;
    if (!check(shutdownTimer.elapsed() < 2500,
               "shutdown must cooperatively stop the active job within budget")) return 1;

    std::cout << "[PASS] JobQueue executes off the UI thread, cancels, and passes 20-run leak checks.\n";
    return 0;
}
