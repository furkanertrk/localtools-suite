#include "localization_service.h"
#include "settings_service.h"
#include "logging_service.h"
#include "../core/tool_registry.h"

#include <QLocale>
#include <QEvent>
#include <QMap>
#include <iostream>
#include <QLibraryInfo>

namespace LocalTools::Services {

LocalizationService& LocalizationService::instance() {
    static LocalizationService instance;
    return instance;
}

LocalizationService::LocalizationService() {
    m_translator = std::make_unique<QTranslator>();
    m_qtTranslator = std::make_unique<QTranslator>();
}

void LocalizationService::initialize(QCoreApplication* app) {
    m_app = app;

    // Determine initial language preference
    QString savedLang = SettingsService::instance().getLanguage();

    if (savedLang == "auto" || savedLang.isEmpty()) {
        QLocale systemLocale = QLocale::system();
        if (systemLocale.language() == QLocale::Turkish) {
            m_currentLanguage = AppLanguage::Turkish;
        } else {
            // Default fallback for English, German, French, Spanish, etc. is English
            m_currentLanguage = AppLanguage::English;
        }
        SettingsService::instance().setLanguage(currentLanguageCode());
    } else if (savedLang == "en") {
        m_currentLanguage = AppLanguage::English;
    } else {
        m_currentLanguage = AppLanguage::Turkish;
    }

    loadAndInstallTranslator();
}

QString LocalizationService::currentLanguageCode() const {
    return (m_currentLanguage == AppLanguage::Turkish) ? "tr" : "en";
}

QString LocalizationService::currentLanguageDisplayName() const {
    return (m_currentLanguage == AppLanguage::Turkish) ? "Türkçe (Turkish)" : "English";
}

void LocalizationService::setLanguage(AppLanguage lang) {
    if (m_currentLanguage == lang) return;

    m_currentLanguage = lang;
    SettingsService::instance().setLanguage(currentLanguageCode());
    loadAndInstallTranslator();
    notifyLanguageChanged();
}

void LocalizationService::setLanguageCode(const QString& code) {
    if (code.toLower() == "en") {
        setLanguage(AppLanguage::English);
    } else {
        setLanguage(AppLanguage::Turkish);
    }
}

void LocalizationService::loadAndInstallTranslator() {
    if (m_app) {
        m_app->removeTranslator(m_translator.get());
        m_app->removeTranslator(m_qtTranslator.get());
    }

    if (m_currentLanguage == AppLanguage::Turkish) {
        // Load custom application translations (from resource)
        if (m_translator->load(":/translations/tr_TR.qm")) {
            if (m_app) m_app->installTranslator(m_translator.get());
        } else {
            LoggingService::instance().warning("Localization", "Failed to load tr_TR.qm from resources");
        }
    } else {
        // English is the base language, so no translator is needed.
    }

    LoggingService::instance().info("Localization", 
        QString("Language set to %1 (%2)").arg(currentLanguageDisplayName(), currentLanguageCode()).toStdString());
}

void LocalizationService::notifyLanguageChanged() {
    // Notify ToolRegistry to retranslate all tools
    Core::ToolRegistry::instance().retranslateTools();

    // Broadcast QEvent::LanguageChange
    if (m_app) {
        QEvent event(QEvent::LanguageChange);
        QCoreApplication::sendEvent(m_app, &event);
    }

    emit languageChanged(m_currentLanguage);
}

QString LocalizationService::translate(const QString& sourceText, const char* disambiguation) const {
    // With standard Qt i18n, QObject::tr() should be used directly in classes.
    // This fallback method is kept for legacy use cases but delegates to standard QCoreApplication translation.
    if (m_app) {
        return QCoreApplication::translate("Global", sourceText.toUtf8().constData(), disambiguation);
    }
    return sourceText;
}

} // namespace LocalTools::Services
