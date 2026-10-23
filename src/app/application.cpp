#include "application.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThreadPool>
#include <iostream>

#include "../services/logging_service.h"
#include "../services/localization_service.h"
#include "../services/file_service.h"
#include "../services/settings_service.h"
#include "../core/job_queue.h"
#include "../ui/theme.h"

namespace LocalTools::App {

Application::Application(int& argc, char** argv)
    : m_qapp(std::make_unique<QApplication>(argc, argv)) {
    QCoreApplication::setOrganizationName("LocalTools Suite");
    QCoreApplication::setApplicationName("LocalTools Suite");
    QCoreApplication::setApplicationVersion("4.0.0");
    m_qapp->setWindowIcon(QIcon(":/icons/icons/LocalTools.ico"));

    // Install the base style and app font before any widget is constructed.
    // Widgets cache font metrics at build time, so this must precede MainWindow.
    LocalTools::UI::Theme::initializeApplication();

    // Initialize Localization Service before any UI is loaded
    Services::LocalizationService::instance().initialize(m_qapp.get());

    // Remove working files left by an interrupted previous run before any new
    // operation can reuse the application temp directory.
    if (Services::SettingsService::instance().getAutoCleanTemp()) {
        Services::FileService::instance().cleanTempDirectory();
    }

    // Initialize privacy-respecting LoggingService
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    Services::LoggingService::instance().initialize(appDataDir.toStdString());
    Services::LoggingService::instance().info("App", "LocalTools Suite Application starting...");

    m_mainWindow = std::make_unique<LocalTools::UI::MainWindow>();
}

Application::~Application() {
    // MainWindow (and therefore its workers) must be destroyed before temp
    // cleanup. Explicitly release it here because member destruction happens
    // after the destructor body.
    m_mainWindow.reset();
    // QWidget-owned AsyncTask contexts request cancellation during the reset
    // above. Wait for the global pool and JobQueue before deleting temp files;
    // otherwise a late worker could still read/write a path being cleaned.
    QThreadPool::globalInstance()->clear();
    QThreadPool::globalInstance()->waitForDone();
    Core::JobQueue::instance().shutdown();
    if (Services::SettingsService::instance().getAutoCleanTemp()) {
        Services::FileService::instance().cleanTempDirectory();
    }
}

int Application::run() {
    m_mainWindow->show();
    return m_qapp->exec();
}

} // namespace LocalTools::App
