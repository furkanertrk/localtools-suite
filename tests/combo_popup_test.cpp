#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QListView>
#include <QScrollBar>

#include <iostream>

#include "ui/theme.h"

namespace {

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

bool verifyCombo(QComboBox& combo, int expectedVisibleRows) {
    combo.ensurePolished();
    QApplication::sendPostedEvents();
    QApplication::processEvents();

    auto* view = qobject_cast<QListView*>(combo.view());
    if (!check(view != nullptr, "QComboBox popup must use QListView")) return false;
    if (!check(view->uniformItemSizes(), "popup rows must use uniform item sizing")) return false;
    if (!check(view->frameShape() == QFrame::NoFrame,
               "popup view frame must not consume row height")) return false;
    if (!check(combo.maxVisibleItems() == 10, "visible row cap must remain 10")) return false;
    if (!check(view->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded,
               "popup must scroll only when needed")) return false;
    if (!check(view->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
               "popup must not gain a horizontal scrollbar")) return false;
    if (!check(view->contentsMargins().isNull(),
               "popup view margins must not consume row height")) return false;
    if (!check(view->viewport()->contentsMargins().isNull(),
               "popup viewport margins must not consume row height")) return false;

    for (int row = 0; row < combo.count(); ++row) {
        if (!check(view->sizeHintForRow(row) == 32, "every popup row must be exactly 32 px")) {
            return false;
        }
    }

    const int expectedContentHeight = expectedVisibleRows * 32;
    int measuredContentHeight = 0;
    for (int row = 0; row < expectedVisibleRows; ++row) {
        measuredContentHeight += view->sizeHintForRow(row);
    }
    if (!check(measuredContentHeight == expectedContentHeight,
               "visible rows must add up to an unclipped popup content height")) return false;

    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("LocalToolsTests"));
    app.setApplicationName(QStringLiteral("ComboPopupRegression"));
    LocalTools::UI::Theme::initializeApplication();
    app.setStyleSheet(LocalTools::UI::Theme::getStyleSheet(
        LocalTools::Services::AppTheme::Dark));

    QComboBox settingsLikeCombo;
    settingsLikeCombo.addItems({
        QStringLiteral("System Default (Windows 11)"),
        QStringLiteral("Light Theme (Light Mode)"),
        QStringLiteral("Dark Theme (Dark Mode)")
    });
    if (!verifyCombo(settingsLikeCombo, 3)) return 1;

    QComboBox longCombo;
    for (int i = 1; i <= 14; ++i) {
        longCombo.addItem(QStringLiteral("Option %1").arg(i));
    }
    if (!verifyCombo(longCombo, 10)) return 1;
    std::cout << "[PASS] QComboBox popup rows are fully visible application-wide.\n";
    return 0;
}
