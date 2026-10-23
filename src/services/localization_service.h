#ifndef LOCALTOOLS_SERVICES_LOCALIZATION_SERVICE_H
#define LOCALTOOLS_SERVICES_LOCALIZATION_SERVICE_H

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QCoreApplication>
#include <memory>

namespace LocalTools::Services {

enum class AppLanguage {
    Turkish,
    English
};

class LocalizationService : public QObject {
    Q_OBJECT

public:
    static LocalizationService& instance();

    void initialize(QCoreApplication* app = nullptr);

    AppLanguage currentLanguage() const { return m_currentLanguage; }
    QString currentLanguageCode() const;
    QString currentLanguageDisplayName() const;

    void setLanguage(AppLanguage lang);
    void setLanguageCode(const QString& code);

    // Dynamic translation lookup helper (English is base/fallback or Turkish)
    QString translate(const QString& sourceText, const char* disambiguation = nullptr) const;

signals:
    void languageChanged(AppLanguage newLanguage);

private:
    LocalizationService();
    ~LocalizationService() override = default;

    LocalizationService(const LocalizationService&) = delete;
    LocalizationService& operator=(const LocalizationService&) = delete;

    void loadAndInstallTranslator();
    void notifyLanguageChanged();

    QCoreApplication* m_app{nullptr};
    AppLanguage m_currentLanguage{AppLanguage::Turkish};
    std::unique_ptr<QTranslator> m_translator;
    std::unique_ptr<QTranslator> m_qtTranslator;
};

} // namespace LocalTools::Services

#endif // LOCALTOOLS_SERVICES_LOCALIZATION_SERVICE_H
