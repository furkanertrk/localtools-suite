#ifndef LOCALTOOLS_SERVICES_SETTINGS_SERVICE_H
#define LOCALTOOLS_SERVICES_SETTINGS_SERVICE_H

#include <QSettings>
#include <QString>
#include <QStringList>
#include <memory>

namespace LocalTools::Services {

enum class AppTheme {
    System,
    Light,
    Dark
};

class SettingsService {
public:
    static SettingsService& instance();

    AppTheme getTheme() const;
    void setTheme(AppTheme theme);

    QString getLanguage() const;
    void setLanguage(const QString& lang);

    QString getDefaultOutputDir() const;
    void setDefaultOutputDir(const QString& dirPath);

    bool getConfirmOverwrite() const;
    void setConfirmOverwrite(bool confirm);

    bool getAutoOpenOutput() const;
    void setAutoOpenOutput(bool autoOpen);

    QStringList getRecentFiles() const;
    void addRecentFile(const QString& filePath);
    void clearRecentFiles();

    // Favorites
    QStringList getFavoriteTools() const;
    void setFavoriteTools(const QStringList& toolIds);

    // Recent files privacy controls
    bool getKeepRecentFiles() const;
    void setKeepRecentFiles(bool keep);

    // Temp file auto-cleanup
    bool getAutoCleanTemp() const;
    void setAutoCleanTemp(bool autoClean);

private:
    SettingsService();
    ~SettingsService() = default;

    SettingsService(const SettingsService&) = delete;
    SettingsService& operator=(const SettingsService&) = delete;

    std::unique_ptr<QSettings> m_settings;
};

} // namespace LocalTools::Services

#endif // LOCALTOOLS_SERVICES_SETTINGS_SERVICE_H
