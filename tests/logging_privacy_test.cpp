#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>

#include "core/error.h"
#include "services/logging_service.h"

namespace {

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir logRoot;
    if (!check(logRoot.isValid(), "temporary log directory must be available")) return 1;

    QFile oversizedLog(logRoot.filePath(QStringLiteral("app.log")));
    if (!check(oversizedLog.open(QIODevice::WriteOnly), "oversized log fixture must open")) return 1;
    oversizedLog.write(QByteArray(512 * 1024 + 1, 'x'));
    oversizedLog.close();

    auto& logging = LocalTools::Services::LoggingService::instance();
    logging.initialize(logRoot.path().toStdString());
    logging.info("Privacy\nTest", "one line\r\ntwo");
    const auto sensitiveError = LocalTools::Core::PdfError::create(
        LocalTools::Core::PdfErrorCategory::InvalidPdf,
        "C:/Users/Person/Documents/private-medical-record.pdf");
    logging.error("Backend", "Operation failed", &sensitiveError);

    QFile activeLog(logRoot.filePath(QStringLiteral("app.log")));
    if (!check(activeLog.open(QIODevice::ReadOnly), "active log must be readable")) return 1;
    const QByteArray content = activeLog.readAll();
    if (!check(content.contains("[Privacy Test] one line  two"),
               "log lines and categories must not permit line injection")) return 1;
    if (!check(content.contains("[Category: Invalid or Corrupted PDF]"),
               "bounded error category must remain diagnostic")) return 1;
    if (!check(!content.contains("private-medical-record"),
               "sensitive error details must not be persisted")) return 1;
    if (!check(QFile::exists(logRoot.filePath(QStringLiteral("app.previous.log"))),
               "oversized logs must rotate to a single bounded previous file")) return 1;

    std::cout << "[PASS] Logs are bounded and exclude sensitive error details.\n";
    return 0;
}
