#include "settings_service.h"

#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>

namespace LocalTools::Services {

SettingsService& SettingsService::instance() {
    static SettingsService instance;
    return instance;
}

SettingsService::SettingsService() {
    m_settings = std::make_unique<QSettings>(
        QSettings::IniFormat,
        QSettings::UserScope,
        QCoreApplication::organizationName(),
        QCoreApplication::applicationName()
    );
}

AppTheme SettingsService::getTheme() const {
    int val = m_settings->value("UI/Theme", static_cast<int>(AppTheme::System)).toInt();
    return static_cast<AppTheme>(val);
}

void SettingsService::setTheme(AppTheme theme) {
    m_settings->setValue("UI/Theme", static_cast<int>(theme));
}

QString SettingsService::getLanguage() const {
    return m_settings->value("General/Language", "auto").toString();
}

void SettingsService::setLanguage(const QString& lang) {
    m_settings->setValue("General/Language", lang);
}

QString SettingsService::getDefaultOutputDir() const {
    QString fallback = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/LocalTools";
    return m_settings->value("General/DefaultOutputDir", fallback).toString();
}

void SettingsService::setDefaultOutputDir(const QString& dirPath) {
    m_settings->setValue("General/DefaultOutputDir", dirPath);
}

bool SettingsService::getConfirmOverwrite() const {
    return m_settings->value("General/ConfirmOverwrite", true).toBool();
}

void SettingsService::setConfirmOverwrite(bool confirm) {
    m_settings->setValue("General/ConfirmOverwrite", confirm);
}

bool SettingsService::getAutoOpenOutput() const {
    return m_settings->value("General/AutoOpenOutput", false).toBool();
}

void SettingsService::setAutoOpenOutput(bool autoOpen) {
    m_settings->setValue("General/AutoOpenOutput", autoOpen);
}

QStringList SettingsService::getRecentFiles() const {
    if (!getKeepRecentFiles()) {
        return {};
    }
    return m_settings->value("Recent/Files").toStringList();
}

void SettingsService::addRecentFile(const QString& filePath) {
    if (!getKeepRecentFiles() || filePath.trimmed().isEmpty()) {
        return;
    }

    const QString normalizedPath = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    QStringList files = getRecentFiles();
    files.removeIf([&normalizedPath](const QString& existingPath) {
        return QString::compare(QDir::cleanPath(QFileInfo(existingPath).absoluteFilePath()),
                                normalizedPath,
                                Qt::CaseInsensitive) == 0;
    });
    files.prepend(normalizedPath);

    while (files.size() > 10) {
        files.removeLast();
    }

    m_settings->setValue("Recent/Files", files);
}

void SettingsService::clearRecentFiles() {
    m_settings->remove("Recent/Files");
}

QStringList SettingsService::getFavoriteTools() const {
    return m_settings->value("Favorites/Tools").toStringList();
}

void SettingsService::setFavoriteTools(const QStringList& toolIds) {
    m_settings->setValue("Favorites/Tools", toolIds);
}

bool SettingsService::getKeepRecentFiles() const {
    return m_settings->value("Privacy/KeepRecentFiles", true).toBool();
}

void SettingsService::setKeepRecentFiles(bool keep) {
    m_settings->setValue("Privacy/KeepRecentFiles", keep);
    if (!keep) {
        clearRecentFiles();
    }
}

bool SettingsService::getAutoCleanTemp() const {
    return m_settings->value("Privacy/AutoCleanTemp", true).toBool();
}

void SettingsService::setAutoCleanTemp(bool autoClean) {
    m_settings->setValue("Privacy/AutoCleanTemp", autoClean);
}

} // namespace LocalTools::Services
