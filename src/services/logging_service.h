#ifndef LOCALTOOLS_SERVICES_LOGGING_SERVICE_H
#define LOCALTOOLS_SERVICES_LOGGING_SERVICE_H

#include <string>
#include <fstream>
#include <mutex>
#include "../core/error.h"

namespace LocalTools::Services {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class LoggingService {
public:
    static LoggingService& instance();

    void initialize(const std::string& logDirectory);
    void log(LogLevel level, const std::string& category, const std::string& message);

    void info(const std::string& category, const std::string& message);
    void warning(const std::string& category, const std::string& message);
    void error(const std::string& category, const std::string& message, const Core::PdfError* pdfErr = nullptr);
    void debug(const std::string& category, const std::string& message);

private:
    LoggingService();
    ~LoggingService();

    LoggingService(const LoggingService&) = delete;
    LoggingService& operator=(const LoggingService&) = delete;

    std::string getTimestamp() const;
    std::string levelToString(LogLevel level) const;

    std::mutex m_logMutex;
    std::ofstream m_logFile;
    bool m_initialized{false};
};

} // namespace LocalTools::Services

#endif // LOCALTOOLS_SERVICES_LOGGING_SERVICE_H
