#ifndef LOCALTOOLS_CORE_BACKENDS_FFMPEG_PROCESS_H
#define LOCALTOOLS_CORE_BACKENDS_FFMPEG_PROCESS_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

#include "../result.h"

namespace LocalTools::Core {

class FFmpegProcess {
public:
    using CancelCallback = std::function<bool()>;
    using OutputCallback = std::function<void(const QByteArray& standardErrorChunk)>;

    static Result<QByteArray> run(const QString& executable,
                                  const QStringList& arguments,
                                  CancelCallback cancelRequested = {},
                                  OutputCallback outputReceived = {});
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_BACKENDS_FFMPEG_PROCESS_H
