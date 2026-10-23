#ifndef LOCALTOOLS_UI_COMMON_ASYNC_TASK_H
#define LOCALTOOLS_UI_COMMON_ASYNC_TASK_H

#include <QFutureWatcher>
#include <QObject>
#include <QtConcurrentRun>

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

namespace LocalTools::UI::AsyncTask {

using CancelCheck = std::function<bool()>;

template <typename ResultType>
void run(QObject* context,
         std::function<ResultType(CancelCheck)> work,
         std::function<void(ResultType)> completed) {
    auto cancelRequested = std::make_shared<std::atomic_bool>(false);
    QObject::connect(context, &QObject::destroyed, [cancelRequested]() {
        cancelRequested->store(true, std::memory_order_relaxed);
    });

    auto* watcher = new QFutureWatcher<ResultType>(context);
    QObject::connect(watcher, &QFutureWatcher<ResultType>::finished, context,
                     [watcher, completed = std::move(completed)]() mutable {
        ResultType result = watcher->result();
        watcher->deleteLater();
        completed(std::move(result));
    });

    watcher->setFuture(QtConcurrent::run(
        [work = std::move(work), cancelRequested]() mutable {
            return work([cancelRequested]() {
                return cancelRequested->load(std::memory_order_relaxed);
            });
        }));
}

} // namespace LocalTools::UI::AsyncTask

#endif // LOCALTOOLS_UI_COMMON_ASYNC_TASK_H
