#include "logging_service.h"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace LocalTools::Services {

LoggingService& LoggingService::instance() {
    static LoggingService instance;
    return instance;
}

LoggingService::LoggingService() = default;

LoggingService::~LoggingService() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

void LoggingService::initialize(const std::string& logDirectory) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_initialized) return;

    try {
        std::filesystem::create_directories(logDirectory);
        std::filesystem::path logPath = std::filesystem::path(logDirectory) / "app.log";
        constexpr std::uintmax_t kMaxLogSize = 512 * 1024;
        std::error_code ec;
        if (std::filesystem::exists(logPath, ec) &&
            std::filesystem::file_size(logPath, ec) > kMaxLogSize) {
            const std::filesystem::path previousLogPath =
                std::filesystem::path(logDirectory) / "app.previous.log";
            std::filesystem::remove(previousLogPath, ec);
            ec.clear();
            std::filesystem::rename(logPath, previousLogPath, ec);
        }
        m_logFile.open(logPath, std::ios::app);
        m_initialized = m_logFile.is_open();
    } catch (const std::exception& ex) {
        std::cerr << "[LoggingService] Failed to initialize log file: " << ex.what() << std::endl;
    }
}

std::string LoggingService::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string LoggingService::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        default:                return "LOG  ";
    }
}

void LoggingService::log(LogLevel level, const std::string& category, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    std::string timestamp = getTimestamp();
    std::string levelStr = levelToString(level);

    std::string safeCategory = category;
    std::replace(safeCategory.begin(), safeCategory.end(), '\r', ' ');
    std::replace(safeCategory.begin(), safeCategory.end(), '\n', ' ');
    std::string safeMessage = message;
    std::replace(safeMessage.begin(), safeMessage.end(), '\r', ' ');
    std::replace(safeMessage.begin(), safeMessage.end(), '\n', ' ');

    std::stringstream ss;
    ss << "[" << timestamp << "] [" << levelStr << "] [" << safeCategory << "] " << safeMessage;
    std::string line = ss.str();

    std::cout << line << std::endl;

    if (m_logFile.is_open()) {
        m_logFile << line << "\n";
        m_logFile.flush();
    }
}

void LoggingService::info(const std::string& category, const std::string& message) {
    log(LogLevel::Info, category, message);
}

void LoggingService::warning(const std::string& category, const std::string& message) {
    log(LogLevel::Warning, category, message);
}

void LoggingService::error(const std::string& category, const std::string& message, const Core::PdfError* pdfErr) {
    std::string fullMsg = message;
    if (pdfErr && pdfErr->isError()) {
        // PdfError messages frequently contain full paths or extracted data.
        // Persist only the bounded category; user-facing details remain in UI.
        fullMsg += " [Category: " + pdfErr->categoryToString() + "]";
    }
    log(LogLevel::Error, category, fullMsg);
}

void LoggingService::debug(const std::string& category, const std::string& message) {
    log(LogLevel::Debug, category, message);
}

} // namespace LocalTools::Services
