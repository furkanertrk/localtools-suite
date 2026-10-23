#include <QAbstractButton>
#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QLabel>
#include <QStackedWidget>

#include <iostream>

#include "ui/main_window.h"
#include "services/localization_service.h"
#include "services/settings_service.h"

namespace {

bool check(bool condition, const QString& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message.toStdString() << '\n';
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    QTemporaryDir settingsRoot;
    if (!settingsRoot.isValid()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsRoot.path());

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("LocalTools Suite Test"));
    QCoreApplication::setApplicationName(QStringLiteral("UI Regression"));
    auto& settings = LocalTools::Services::SettingsService::instance();
    const QString requestedTheme = qEnvironmentVariable("LOCALTOOLS_TEST_THEME", "system");
    if (requestedTheme == QStringLiteral("light")) {
        settings.setTheme(LocalTools::Services::AppTheme::Light);
    } else if (requestedTheme == QStringLiteral("dark")) {
        settings.setTheme(LocalTools::Services::AppTheme::Dark);
    } else {
        settings.setTheme(LocalTools::Services::AppTheme::System);
    }
    auto& localization = LocalTools::Services::LocalizationService::instance();
    localization.initialize(&app);
    localization.setLanguageCode(qEnvironmentVariable("LOCALTOOLS_TEST_LANGUAGE", "en"));
    LocalTools::UI::MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1600, 900);
    window.show();
    app.processEvents();

    const auto buttons = window.findChildren<QAbstractButton*>();
    if (!check(buttons.size() >= 40, QStringLiteral("main UI button inventory is unexpectedly small"))) {
        return 1;
    }

    for (QAbstractButton* button : buttons) {
        if (!button || button->icon().isNull() || !button->text().trimmed().isEmpty()) continue;
        // Qt creates this private decoration for QLineEdit clear actions; the
        // owning line edit exposes the accessible value and action.
        if (qobject_cast<QLineEdit*>(button->parentWidget())) continue;
        if (!check(!button->accessibleName().trimmed().isEmpty(),
                   QStringLiteral("icon-only button has no accessible name: %1")
                       .arg(button->objectName().isEmpty()
                                ? QString::fromLatin1(button->metaObject()->className())
                                : button->objectName()))) {
            return 1;
        }
    }

    QStackedWidget* primaryStack = nullptr;
    for (QStackedWidget* stack : window.findChildren<QStackedWidget*>()) {
        if (!primaryStack || stack->count() > primaryStack->count()) primaryStack = stack;
    }
    if (!check(primaryStack && primaryStack->count() >= 13,
               QStringLiteral("primary page stack was not found"))) {
        return 1;
    }

    int geometryChecks = 0;
    for (int pageIndex = 0; pageIndex < primaryStack->count(); ++pageIndex) {
        primaryStack->setCurrentIndex(pageIndex);
        app.processEvents();

        for (QAbstractButton* button : window.findChildren<QAbstractButton*>()) {
            if (!button || !button->isVisibleTo(&window) || button->text().trimmed().isEmpty()) continue;
            ++geometryChecks;
            if (!check(button->sizeHint().width() <= button->width() + 2,
                       QStringLiteral("button text is clipped on page %1: %2 (%3 > %4)")
                           .arg(pageIndex)
                           .arg(button->text())
                           .arg(button->sizeHint().width())
                           .arg(button->width()))) {
                return 1;
            }
        }

        for (QLabel* label : window.findChildren<QLabel*>()) {
            if (!label || !label->isVisibleTo(&window) || label->wordWrap()
                || label->text().trimmed().isEmpty() || label->pixmap().isNull() == false) {
                continue;
            }
            ++geometryChecks;
            const int required = label->fontMetrics().horizontalAdvance(label->text());
            if (!check(required <= label->contentsRect().width() + 2,
                       QStringLiteral("label text is clipped on page %1: %2 (%3 > %4)")
                           .arg(pageIndex)
                           .arg(label->text())
                           .arg(required)
                           .arg(label->contentsRect().width()))) {
                return 1;
            }
        }
    }

    std::cout << "[PASS] " << buttons.size()
              << " buttons audited; every icon-only control has an accessible name; "
              << geometryChecks << " visible text geometries fit across all pages; theme="
              << requestedTheme.toStdString() << ", language="
              << localization.currentLanguageCode().toStdString() << ".\n";
    return 0;
}
