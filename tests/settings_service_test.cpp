#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

#include <iostream>

#include "services/settings_service.h"

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
    QTemporaryDir settingsRoot;
    if (!check(settingsRoot.isValid(), "temporary settings directory must be available")) return 1;

    QCoreApplication::setOrganizationName(QStringLiteral("LocalToolsReleaseTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SettingsServiceRegression"));
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsRoot.path());

    auto& settings = LocalTools::Services::SettingsService::instance();
    settings.clearRecentFiles();
    settings.setKeepRecentFiles(true);

    const QString firstPath = QDir(settingsRoot.path()).absoluteFilePath(QStringLiteral("first.pdf"));
    const QString secondPath = QDir(settingsRoot.path()).absoluteFilePath(QStringLiteral("second.pdf"));
    settings.addRecentFile(firstPath);
    settings.addRecentFile(secondPath);
    settings.addRecentFile(firstPath);

    const QStringList recent = settings.getRecentFiles();
    if (!check(recent.size() == 2, "recent history must de-duplicate paths")) return 1;
    if (!check(recent.first() == QDir::cleanPath(firstPath),
               "reopened file must move to the front of recent history")) return 1;

    settings.setKeepRecentFiles(false);
    if (!check(settings.getRecentFiles().isEmpty(),
               "disabling recent history must erase existing entries")) return 1;
    settings.addRecentFile(secondPath);
    if (!check(settings.getRecentFiles().isEmpty(),
               "disabled recent history must not collect new paths")) return 1;

    settings.setAutoCleanTemp(false);
    if (!check(!settings.getAutoCleanTemp(), "automatic temp cleanup setting must persist false")) return 1;
    settings.setAutoCleanTemp(true);
    if (!check(settings.getAutoCleanTemp(), "automatic temp cleanup setting must persist true")) return 1;

    settings.setConfirmOverwrite(false);
    if (!check(!settings.getConfirmOverwrite(), "overwrite confirmation setting must persist false")) return 1;
    settings.setConfirmOverwrite(true);
    if (!check(settings.getConfirmOverwrite(), "overwrite confirmation setting must persist true")) return 1;

    std::cout << "[PASS] Settings privacy and persistence behavior is active in Release builds.\n";
    return 0;
}
