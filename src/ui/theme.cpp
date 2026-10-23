#include "theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include <QFontDatabase>
#include <QStringList>
#include <QComboBox>
#include <QListView>
#include <QStyledItemDelegate>
#include <QSlider>
#include <QEvent>
#include <QTimer>
#include <QScreen>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#endif

namespace LocalTools::UI {

namespace {

// ---------------------------------------------------------------------------
// Palette
//
// Light surfaces/text come from the neutral "documentation" scale; the accent is
// the Windows 11 system accent so the app matches native chrome. Dark surfaces
// are the Fluent dark ramp (#202020/#2B2B2B/#323232) rather than a tinted navy,
// which is what made the previous dark mode read as a web app.
//
// Contrast verified: OnAccent/Accent = 5.5:1 light, 11.2:1 dark (both pass AA).
// ---------------------------------------------------------------------------

struct TokenPair {
    const char* light;
    const char* dark;
};

TokenPair tokenValue(ColorToken t) {
    switch (t) {
    case ColorToken::SurfaceCanvas:      return { "#f5f6f8", "#202020" };
    case ColorToken::SurfaceCard:        return { "#ffffff", "#2b2b2b" };
    case ColorToken::SurfaceMuted:       return { "#eaeff3", "#323232" };
    case ColorToken::SurfaceHover:       return { "#e8edf2", "#333333" };
    case ColorToken::SurfacePressed:     return { "#dfe5ec", "#3a3a3a" };

    case ColorToken::TextPrimary:        return { "#1e293b", "#ffffff" };
    case ColorToken::TextSecondary:      return { "#475569", "#c5c5c5" };
    case ColorToken::TextTertiary:       return { "#64748b", "#9a9a9a" };
    case ColorToken::TextDisabled:       return { "#94a3b8", "#6d6d6d" };

    case ColorToken::Border:             return { "#e2e8f0", "#3d3d3d" };
    case ColorToken::BorderStrong:       return { "#cbd5e1", "#4a4a4a" };

    case ColorToken::Accent:             return { "#0067c0", "#4cc2ff" };
    case ColorToken::OnAccent:           return { "#ffffff", "#00243d" };
    case ColorToken::AccentHover:        return { "#0078d4", "#60cdff" };
    case ColorToken::AccentPressed:      return { "#005494", "#3aa8e0" };
    case ColorToken::AccentSubtle:       return { "#eaf3fb", "#12303f" };
    case ColorToken::AccentSubtleBorder: return { "#cfe4f7", "#1d4a61" };

    case ColorToken::FocusRing:          return { "#0067c0", "#4cc2ff" };

    case ColorToken::SuccessBg:          return { "#eff6ef", "#1b2e1a" };
    case ColorToken::SuccessText:        return { "#0f7b0f", "#6ccb5f" };
    case ColorToken::SuccessBorder:      return { "#c9e5c9", "#2f5c2c" };

    case ColorToken::WarningBg:          return { "#fdf6e9", "#332a12" };
    case ColorToken::WarningText:        return { "#9d5d00", "#fce100" };
    case ColorToken::WarningBorder:      return { "#f0dcb4", "#5c4a15" };

    case ColorToken::ErrorBg:            return { "#fdeeec", "#3a1d1b" };
    case ColorToken::ErrorText:          return { "#c42b1c", "#ff99a4" };
    case ColorToken::ErrorBorder:        return { "#f3c9c4", "#6b2f2b" };

    case ColorToken::InfoBg:             return { "#eaf3fb", "#12303f" };
    case ColorToken::InfoText:           return { "#0067c0", "#4cc2ff" };
    case ColorToken::InfoBorder:         return { "#cfe4f7", "#1d4a61" };
    }
    return { "#000000", "#ffffff" };
}

/** Placeholder name used inside styleSheetTemplate(), e.g. {{surfaceCard}}. */
const char* tokenName(ColorToken t) {
    switch (t) {
    case ColorToken::SurfaceCanvas:      return "surfaceCanvas";
    case ColorToken::SurfaceCard:        return "surfaceCard";
    case ColorToken::SurfaceMuted:       return "surfaceMuted";
    case ColorToken::SurfaceHover:       return "surfaceHover";
    case ColorToken::SurfacePressed:     return "surfacePressed";
    case ColorToken::TextPrimary:        return "textPrimary";
    case ColorToken::TextSecondary:      return "textSecondary";
    case ColorToken::TextTertiary:       return "textTertiary";
    case ColorToken::TextDisabled:       return "textDisabled";
    case ColorToken::Border:             return "border";
    case ColorToken::BorderStrong:       return "borderStrong";
    case ColorToken::Accent:             return "accent";
    case ColorToken::OnAccent:           return "onAccent";
    case ColorToken::AccentHover:        return "accentHover";
    case ColorToken::AccentPressed:      return "accentPressed";
    case ColorToken::AccentSubtle:       return "accentSubtle";
    case ColorToken::AccentSubtleBorder: return "accentSubtleBorder";
    case ColorToken::FocusRing:          return "focusRing";
    case ColorToken::SuccessBg:          return "successBg";
    case ColorToken::SuccessText:        return "successText";
    case ColorToken::SuccessBorder:      return "successBorder";
    case ColorToken::WarningBg:          return "warningBg";
    case ColorToken::WarningText:        return "warningText";
    case ColorToken::WarningBorder:      return "warningBorder";
    case ColorToken::ErrorBg:            return "errorBg";
    case ColorToken::ErrorText:          return "errorText";
    case ColorToken::ErrorBorder:        return "errorBorder";
    case ColorToken::InfoBg:             return "infoBg";
    case ColorToken::InfoText:           return "infoText";
    case ColorToken::InfoBorder:         return "infoBorder";
    }
    return "unknown";
}

class ComboBoxItemDelegate : public QStyledItemDelegate {
public:
    explicit ComboBoxItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        s.setHeight(32);
        return s;
    }
};

/**
 * Gives every QComboBox and QSlider the setup needed for robust UI behavior.
 *
 * For QComboBox:
 * A stylesheet alone cannot fix the popup: the native/default view ignores
 * `::item` rules (so min-height never applies) and the popup is sized from the
 * item count without accounting for view padding, which clips the last row.
 * Swapping in a QListView with uniform item sizes, NoFrame, and a fixed-height
 * delegate ensures the popup height calculation matches the painted rows exactly.
 *
 * For QSlider:
 * Setting TabFocus policy ensures mouse clicks/drags do not trigger a keyboard focus
 * ring outline while keyboard navigation remains fully accessible.
 */
class ComboBoxPolisher : public QObject {
public:
    explicit ComboBoxPolisher(QObject* parent = nullptr) : QObject(parent) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Polish || event->type() == QEvent::Show) {
            if (auto* combo = qobject_cast<QComboBox*>(watched)) {
                auto* lv = qobject_cast<QListView*>(combo->view());
                if (!lv) {
                    lv = new QListView(combo);
                    combo->setView(lv);
                }
                combo->setMaxVisibleItems(10);
                lv->setUniformItemSizes(true);
                lv->setFrameShape(QFrame::NoFrame);
                lv->setFrameShadow(QFrame::Plain);
                lv->setLineWidth(0);
                if (!dynamic_cast<ComboBoxItemDelegate*>(lv->itemDelegate())) {
                    lv->setItemDelegate(new ComboBoxItemDelegate(lv));
                }
                lv->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                lv->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                lv->setContentsMargins(0, 0, 0, 0);
                // Used below to recognize Qt's private popup container without
                // depending on its private (and version-specific) class name.
                lv->setProperty("localToolsComboPopupView", true);
                if (auto* vp = lv->viewport()) {
                    vp->setContentsMargins(0, 0, 0, 0);
                }
            } else if (auto* slider = qobject_cast<QSlider*>(watched)) {
                slider->setFocusPolicy(Qt::TabFocus);
            }
        }

        // QComboBox sizes its private popup from the sum of its row size hints.
        // A QSS border on the popup container is applied afterwards and steals
        // a couple of pixels from the QListView viewport.  That tiny mismatch is
        // enough for QAbstractItemView to expose a scroll bar and paint the last
        // row only partially.  It is especially visible with fractional Windows
        // display scaling.  Once the popup layout has settled, restore exactly
        // the viewport height promised by the visible row size hints.
        if (event->type() == QEvent::Show) {
            auto* popup = qobject_cast<QWidget*>(watched);
            if (popup && popup->isWindow() && (popup->windowType() == Qt::Popup)) {
                auto* view = popup->findChild<QListView*>();
                if (view && view->property("localToolsComboPopupView").toBool()) {
                    QTimer::singleShot(0, popup, [popup, view]() {
                        if (!popup->isVisible() || !view->model() || !view->viewport()) {
                            return;
                        }

                        view->doItemsLayout();
                        const int rowCount = view->model()->rowCount(view->rootIndex());
                        if (rowCount <= 0) {
                            return;
                        }

                        auto* combo = qobject_cast<QComboBox*>(popup->parentWidget());
                        int maxVisibleItems = 10;
                        if (combo) {
                            maxVisibleItems = combo->maxVisibleItems();
                        }
                        const int visibleRows = qMin(rowCount, qMax(1, maxVisibleItems));

                        int requiredViewportHeight = 0;
                        for (int row = 0; row < visibleRows; ++row) {
                            requiredViewportHeight += qMax(1, view->sizeHintForRow(row));
                        }

                        const int popupChrome = qMax(0,
                            popup->height() - view->viewport()->height());
                        int targetHeight = requiredViewportHeight + popupChrome;

                        // The default popup width follows the closed combo's
                        // width, even when a translated option is longer.  Use
                        // the delegate's real column hint so text is never
                        // elided merely because the field itself is compact.
                        const int horizontalChrome = qMax(0,
                            popup->width() - view->viewport()->width());
                        const int requiredContentWidth = qMax(0, view->sizeHintForColumn(0));
                        int targetWidth = qMax(popup->width(),
                            requiredContentWidth + horizontalChrome);
                        if (combo) {
                            targetWidth = qMax(targetWidth, combo->width());
                        }

                        // Respect the usable screen area.  When a genuinely long
                        // list cannot fit, Qt's normal vertical scrolling remains.
                        const QRect available = popup->screen()->availableGeometry();
                        targetHeight = qMin(targetHeight, available.height());
                        targetWidth = qMin(targetWidth, available.width());
                        if (targetHeight == popup->height() && targetWidth == popup->width()) {
                            return;
                        }

                        QRect geometry = popup->geometry();
                        geometry.setSize(QSize(targetWidth, targetHeight));

                        // Preserve the popup's relationship to the field after
                        // growing it. Near the bottom of the screen, prefer the
                        // larger side and anchor the popup above the combo so it
                        // never expands across the control itself.
                        if (combo) {
                            const QRect comboGeometry(
                                combo->mapToGlobal(QPoint(0, 0)), combo->size());
                            const int roomBelow = qMax(0, available.bottom() - comboGeometry.bottom());
                            const int roomAbove = qMax(0, comboGeometry.top() - available.top());
                            const bool placeBelow = roomBelow >= targetHeight || roomBelow >= roomAbove;
                            const int sideHeight = placeBelow ? roomBelow : roomAbove;
                            geometry.setHeight(qMin(geometry.height(), qMax(1, sideHeight)));
                            if (placeBelow) {
                                geometry.moveTop(comboGeometry.bottom() + 1);
                            } else {
                                geometry.moveBottom(comboGeometry.top() - 1);
                            }
                        }
                        if (geometry.right() > available.right()) {
                            geometry.moveRight(available.right());
                        }
                        if (geometry.left() < available.left()) {
                            geometry.moveLeft(available.left());
                        }
                        if (geometry.bottom() > available.bottom()) {
                            geometry.moveBottom(available.bottom());
                        }
                        if (geometry.top() < available.top()) {
                            geometry.moveTop(available.top());
                        }
                        popup->setGeometry(geometry);
                    });
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

constexpr ColorToken kAllTokens[] = {
    ColorToken::SurfaceCanvas, ColorToken::SurfaceCard, ColorToken::SurfaceMuted,
    ColorToken::SurfaceHover, ColorToken::SurfacePressed,
    ColorToken::TextPrimary, ColorToken::TextSecondary, ColorToken::TextTertiary,
    ColorToken::TextDisabled,
    ColorToken::Border, ColorToken::BorderStrong,
    ColorToken::Accent, ColorToken::OnAccent, ColorToken::AccentHover,
    ColorToken::AccentPressed, ColorToken::AccentSubtle, ColorToken::AccentSubtleBorder,
    ColorToken::FocusRing,
    ColorToken::SuccessBg, ColorToken::SuccessText, ColorToken::SuccessBorder,
    ColorToken::WarningBg, ColorToken::WarningText, ColorToken::WarningBorder,
    ColorToken::ErrorBg, ColorToken::ErrorText, ColorToken::ErrorBorder,
    ColorToken::InfoBg, ColorToken::InfoText, ColorToken::InfoBorder
};

} // namespace

// ---------------------------------------------------------------------------
// Deprecated aliases - repointed at the new palette so unmigrated call sites
// inherit the redesign instead of pinning the old blue.
// ---------------------------------------------------------------------------

const char* Theme::PrimaryColor = "#0067c0";
const char* Theme::PrimaryHover = "#0078d4";

const char* Theme::BackgroundLight = "#f5f6f8";
const char* Theme::SurfaceLight = "#ffffff";
const char* Theme::BorderLight = "#e2e8f0";
const char* Theme::TextPrimaryLight = "#1e293b";
const char* Theme::TextSecondaryLight = "#475569";

const char* Theme::BackgroundDark = "#202020";
const char* Theme::SurfaceDark = "#2b2b2b";
const char* Theme::BorderDark = "#3d3d3d";
const char* Theme::TextPrimaryDark = "#ffffff";
const char* Theme::TextSecondaryDark = "#c5c5c5";

const char* Theme::SuccessBgDark = "#1b2e1a";
const char* Theme::SuccessTextDark = "#6ccb5f";
const char* Theme::WarningBgDark = "#332a12";
const char* Theme::WarningTextDark = "#fce100";
const char* Theme::ErrorBgDark = "#3a1d1b";
const char* Theme::ErrorTextDark = "#ff99a4";
const char* Theme::InfoBgDark = "#12303f";
const char* Theme::InfoTextDark = "#4cc2ff";

const char* Theme::SuccessBgLight = "#eff6ef";
const char* Theme::SuccessTextLight = "#0f7b0f";
const char* Theme::WarningBgLight = "#fdf6e9";
const char* Theme::WarningTextLight = "#9d5d00";
const char* Theme::ErrorBgLight = "#fdeeec";
const char* Theme::ErrorTextLight = "#c42b1c";
const char* Theme::InfoBgLight = "#eaf3fb";
const char* Theme::InfoTextLight = "#0067c0";

const char* Theme::RadiusSmall = "4px";
const char* Theme::RadiusMedium = "8px";
const char* Theme::RadiusLarge = "12px";

Services::AppTheme Theme::s_currentMode = Services::AppTheme::Light;

// ---------------------------------------------------------------------------
// Mode
// ---------------------------------------------------------------------------

Services::AppTheme Theme::getSystemColorScheme() {
    auto scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark) {
        return Services::AppTheme::Dark;
    }
    return Services::AppTheme::Light;
}

Services::AppTheme Theme::currentMode() {
    return s_currentMode;
}

bool Theme::isDark() {
    return s_currentMode == Services::AppTheme::Dark;
}

// ---------------------------------------------------------------------------
// Token accessors
// ---------------------------------------------------------------------------

QColor Theme::color(ColorToken token, bool dark) {
    const TokenPair p = tokenValue(token);
    return QColor(QString::fromLatin1(dark ? p.dark : p.light));
}

QColor Theme::color(ColorToken token) {
    return color(token, isDark());
}

QString Theme::hex(ColorToken token, bool dark) {
    const TokenPair p = tokenValue(token);
    return QString::fromLatin1(dark ? p.dark : p.light);
}

QString Theme::hex(ColorToken token) {
    return hex(token, isDark());
}

int Theme::space(Space s) {
    switch (s) {
    case Space::XXS:  return 4;
    case Space::XS:   return 8;
    case Space::S:    return 12;
    case Space::M:    return 16;
    case Space::ML:   return 20;
    case Space::L:    return 24;
    case Space::XL:   return 32;
    case Space::XXL:  return 40;
    case Space::XXXL: return 48;
    }
    return 16;
}

int Theme::radius(Radius r) {
    switch (r) {
    case Radius::Small:  return 4;
    case Radius::Medium: return 8;
    case Radius::Large:  return 12;
    }
    return 8;
}

int Theme::fontSizePx(FontRole role) {
    switch (role) {
    case FontRole::Caption:    return 12;
    case FontRole::Body:       return 14;
    case FontRole::BodyStrong: return 14;
    case FontRole::BodyLarge:  return 18;
    case FontRole::Subtitle:   return 20;
    case FontRole::Title:      return 28;
    case FontRole::TitleLarge: return 40;
    }
    return 14;
}

int Theme::fontWeight(FontRole role) {
    switch (role) {
    case FontRole::Caption:
    case FontRole::Body:
    case FontRole::BodyLarge:
        return QFont::Normal;      // 400
    case FontRole::BodyStrong:
    case FontRole::Subtitle:
    case FontRole::Title:
    case FontRole::TitleLarge:
        return QFont::DemiBold;    // 600
    }
    return QFont::Normal;
}

QStringList Theme::fontFamilies() {
    // Segoe UI Variable ships with Windows 11; Segoe UI covers Windows 10.
    // Inter is only a fallback for non-Windows builds - nothing is downloaded.
    return { QStringLiteral("Segoe UI Variable Text"),
             QStringLiteral("Segoe UI Variable"),
             QStringLiteral("Segoe UI"),
             QStringLiteral("Inter"),
             QStringLiteral("sans-serif") };
}

QString Theme::fontFamilyCss() {
    QStringList quoted;
    const auto families = fontFamilies();
    for (const QString& f : families) {
        quoted << (f == QStringLiteral("sans-serif") ? f : QStringLiteral("'%1'").arg(f));
    }
    return quoted.join(QStringLiteral(", "));
}

QFont Theme::font(FontRole role) {
    QFont f;
    f.setFamilies(fontFamilies());
    f.setPixelSize(fontSizePx(role));
    f.setWeight(static_cast<QFont::Weight>(fontWeight(role)));
    return f;
}

// ---------------------------------------------------------------------------
// Application bootstrap
// ---------------------------------------------------------------------------

void Theme::initializeApplication() {
    // Fusion is a predictable substrate for QSS. Qt 6.7+ defaults to the native
    // "windows11" style, which overrides several stylesheet properties and makes
    // controls render inconsistently against a custom-themed shell.
    if (QStyleFactory::keys().contains(QStringLiteral("Fusion"), Qt::CaseInsensitive)) {
        QApplication::setStyle(QStringLiteral("Fusion"));
    }

    // The font-family declaration in QSS does not reliably propagate to widgets
    // that set their own stylesheet, so the real app font must be set here.
    QFont appFont = font(FontRole::Body);
    QApplication::setFont(appFont);

    // Resolve the mode NOW. MainWindow builds every page before it calls
    // applyThemeToWindow(), so without this every construction-time
    // Theme::color() lookup would silently resolve against the light palette.
    Services::AppTheme stored = Services::SettingsService::instance().getTheme();
    s_currentMode = (stored == Services::AppTheme::System) ? getSystemColorScheme() : stored;

    // Combo box popups need per-widget setup that a stylesheet cannot express
    // (a QListView so ::item rules apply, and a visible-item cap). Doing it on
    // polish covers every combo in the app, including ones created later,
    // without 24 call sites having to remember.
    if (auto* app = QCoreApplication::instance()) {
        // Parented to the app so it is torn down with it, rather than as a
        // static outliving QApplication.
        app->installEventFilter(new ComboBoxPolisher(app));
    }
}

void Theme::applyThemeToWindow(QMainWindow* window, Services::AppTheme theme) {
    if (!window) return;

    Services::AppTheme effective = theme;
    if (theme == Services::AppTheme::System) {
        effective = getSystemColorScheme();
    }

    // Publish the resolved mode before anything reads a token.
    s_currentMode = effective;
    const bool dark = (effective == Services::AppTheme::Dark);

    // 1. Windows 11 dark title bar
#ifdef Q_OS_WIN
    HWND hwnd = (HWND)window->winId();
    if (hwnd) {
        BOOL useDarkMode = dark ? TRUE : FALSE;
        ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
    }
#endif

    // 2. QPalette so native-drawn controls and palette(...) references follow
    QPalette palette;
    palette.setColor(QPalette::Window,           color(ColorToken::SurfaceCanvas, dark));
    palette.setColor(QPalette::WindowText,       color(ColorToken::TextPrimary, dark));
    palette.setColor(QPalette::Base,             color(ColorToken::SurfaceCard, dark));
    palette.setColor(QPalette::AlternateBase,    color(ColorToken::SurfaceMuted, dark));
    palette.setColor(QPalette::ToolTipBase,      color(ColorToken::SurfaceCard, dark));
    palette.setColor(QPalette::ToolTipText,      color(ColorToken::TextPrimary, dark));
    palette.setColor(QPalette::Text,             color(ColorToken::TextPrimary, dark));
    palette.setColor(QPalette::PlaceholderText,  color(ColorToken::TextTertiary, dark));
    palette.setColor(QPalette::Button,           color(ColorToken::SurfaceCard, dark));
    palette.setColor(QPalette::ButtonText,       color(ColorToken::TextPrimary, dark));
    palette.setColor(QPalette::BrightText,       color(ColorToken::OnAccent, dark));
    palette.setColor(QPalette::Link,             color(ColorToken::Accent, dark));
    palette.setColor(QPalette::Highlight,        color(ColorToken::Accent, dark));
    palette.setColor(QPalette::HighlightedText,  color(ColorToken::OnAccent, dark));
    palette.setColor(QPalette::Disabled, QPalette::Text,       color(ColorToken::TextDisabled, dark));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, color(ColorToken::TextDisabled, dark));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, color(ColorToken::TextDisabled, dark));

    // QApplication, not QGuiApplication: the widget layer keeps its own palette
    // propagation, and setting only the GUI-level palette leaves already-built
    // widgets on the default (light) one.
    QApplication::setPalette(palette);
    window->setPalette(palette);

    // 3. Stylesheet on qApp, not the window, so dialogs, popups and combo box
    //    views inherit it too.
    if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        app->setStyleSheet(getStyleSheet(effective));
    } else {
        window->setStyleSheet(getStyleSheet(effective));
    }
}

QString Theme::expandTokens(const QString& templateSheet, bool dark) {
    QString out = templateSheet;
    for (ColorToken t : kAllTokens) {
        out.replace(QStringLiteral("{{%1}}").arg(QString::fromLatin1(tokenName(t))),
                    hex(t, dark));
    }
    out.replace(QStringLiteral("{{fontFamily}}"), fontFamilyCss());
    out.replace(QStringLiteral("{{radiusSmall}}"),  QString::number(radius(Radius::Small)));
    out.replace(QStringLiteral("{{radiusMedium}}"), QString::number(radius(Radius::Medium)));
    out.replace(QStringLiteral("{{radiusLarge}}"),  QString::number(radius(Radius::Large)));
    out.replace(QStringLiteral("{{arrowUp}}"),
                dark ? QStringLiteral("arrow_up_dark") : QStringLiteral("arrow_up_light"));
    out.replace(QStringLiteral("{{arrowDown}}"),
                dark ? QStringLiteral("arrow_down_dark") : QStringLiteral("arrow_down_light"));
    return out;
}

QString Theme::getStyleSheet(Services::AppTheme effectiveTheme) {
    const bool dark = (effectiveTheme == Services::AppTheme::Dark);
    return expandTokens(QString::fromLatin1(styleSheetTemplate()), dark);
}

// ---------------------------------------------------------------------------
// The single stylesheet template.
//
// One source for both modes - a rule written here cannot drift between light and
// dark, which is what previously left dark #ToolCard:hover a visual no-op.
//
// Focus policy: there is deliberately NO global `* { outline: 0 }` reset. Every
// focusable control keeps a transparent border in its rest state and colours it
// on :focus, so padding never shifts and keyboard focus is always visible
// (WCAG 2.4.7).
// ---------------------------------------------------------------------------

const char* Theme::styleSheetTemplate() {
    return R"QSS(
QMainWindow, QWidget#CentralWidget {
    background-color: {{surfaceCanvas}};
    color: {{textPrimary}};
    font-family: {{fontFamily}};
}
QLabel {
    color: {{textPrimary}};
    background: transparent;
}
QLabel#PageTitle {
    font-size: 28px;
    font-weight: 600;
    color: {{textPrimary}};
}
QLabel#PageSubtitle {
    font-size: 14px;
    color: {{textSecondary}};
}
QLabel#SectionLabel {
    font-size: 20px;
    font-weight: 600;
    color: {{textPrimary}};
}
QLabel#CaptionLabel {
    font-size: 12px;
    color: {{textTertiary}};
}

/* ---------- Sidebar ---------- */
QWidget#Sidebar {
    background-color: {{surfaceCard}};
    border-right: 1px solid {{border}};
}
QPushButton#NavButton {
    background-color: transparent;
    color: {{textSecondary}};
    border: 2px solid transparent;
    border-radius: {{radiusMedium}}px;
    padding: 7px 10px;
    text-align: left;
    font-size: 14px;
    font-weight: 500;
}
QPushButton#NavButton:hover {
    background-color: {{surfaceHover}};
    color: {{textPrimary}};
}
QPushButton#NavButton:checked {
    background-color: {{accentSubtle}};
    color: {{textPrimary}};
    font-weight: 600;
}
QPushButton#NavButton:focus {
    border: 2px solid {{focusRing}};
    outline: none;
}
QLabel#SidebarSectionLabel {
    font-size: 11px;
    font-weight: 600;
    color: {{textTertiary}};
    margin-left: 10px;
    margin-bottom: 2px;
}
QFrame#PrivacyBadge {
    background-color: {{successBg}};
    border: 1px solid {{successBorder}};
    border-radius: {{radiusMedium}}px;
}
QLabel#PrivacyBadgeText {
    font-size: 12px;
    color: {{successText}};
}

/* ---------- Buttons ---------- */
QPushButton#PrimaryButton, QPushButton#PrimaryActionButton {
    background-color: {{accent}};
    color: {{onAccent}};
    border: 2px solid transparent;
    border-radius: {{radiusSmall}}px;
    padding: 6px 16px;
    font-size: 14px;
    font-weight: 600;
    min-height: 20px;
}
QPushButton#PrimaryButton:hover, QPushButton#PrimaryActionButton:hover {
    background-color: {{accentHover}};
}
QPushButton#PrimaryButton:pressed, QPushButton#PrimaryActionButton:pressed {
    background-color: {{accentPressed}};
}
QPushButton#PrimaryButton:focus, QPushButton#PrimaryActionButton:focus {
    border: 2px solid {{onAccent}};
    outline: none;
}
QPushButton#SecondaryButton, QPushButton#SecondaryActionButton {
    background-color: {{surfaceCard}};
    color: {{textPrimary}};
    border: 1px solid {{borderStrong}};
    border-radius: {{radiusSmall}}px;
    padding: 7px 16px;
    font-size: 14px;
    font-weight: 500;
    min-height: 20px;
}
QPushButton#SecondaryButton:hover, QPushButton#SecondaryActionButton:hover {
    background-color: {{surfaceHover}};
}
QPushButton#SecondaryButton:pressed, QPushButton#SecondaryActionButton:pressed {
    background-color: {{surfacePressed}};
}
QPushButton#SecondaryButton:focus, QPushButton#SecondaryActionButton:focus {
    border: 2px solid {{focusRing}};
    padding: 6px 15px;
    outline: none;
}
QPushButton#PrimaryButton:disabled, QPushButton#PrimaryActionButton:disabled,
QPushButton#SecondaryButton:disabled, QPushButton#SecondaryActionButton:disabled {
    background-color: {{surfaceMuted}};
    color: {{textDisabled}};
    border: 1px solid {{border}};
}
QPushButton#IconButton {
    background-color: transparent;
    border: 2px solid transparent;
    border-radius: {{radiusSmall}}px;
    padding: 4px;
}
QPushButton#IconButton:hover {
    background-color: {{surfaceHover}};
}
QPushButton#IconButton:pressed {
    background-color: {{surfacePressed}};
}
QPushButton#IconButton:focus {
    border: 2px solid {{focusRing}};
    outline: none;
}

/* ---------- Segmented tab control ---------- */
QPushButton#QuickModeTabButton {
    background-color: {{surfaceCard}};
    color: {{textSecondary}};
    border: 1px solid {{borderStrong}};
    border-radius: {{radiusMedium}}px;
    padding: 7px 16px;
    font-size: 14px;
    font-weight: 500;
}
QPushButton#QuickModeTabButton:hover:!checked {
    background-color: {{surfaceHover}};
    color: {{textPrimary}};
    border: 1px solid {{accent}};
}
QPushButton#QuickModeTabButton:checked {
    background-color: {{accent}};
    color: {{onAccent}};
    border: 2px solid {{accent}};
    padding: 6px 15px;
    font-weight: 700;
}
QPushButton#QuickModeTabButton:focus {
    border: 2px solid {{focusRing}};
    padding: 6px 15px;
    outline: none;
}
QPushButton#ToolTabButton {
    background-color: {{surfaceCard}};
    color: {{textSecondary}};
    border: 1px solid {{border}};
    border-radius: {{radiusSmall}}px;
    padding: 7px 14px;
    font-size: 14px;
    font-weight: 500;
    min-height: 20px;
}
QPushButton#ToolTabButton:hover {
    background-color: {{surfaceHover}};
    color: {{textPrimary}};
    border: 1px solid {{borderStrong}};
}
QPushButton#ToolTabButton:checked {
    background-color: {{accentSubtle}};
    color: {{textPrimary}};
    border: 1px solid {{accent}};
    font-weight: 600;
}
QPushButton#ToolTabButton:pressed {
    background-color: {{surfacePressed}};
}
QPushButton#ToolTabButton:focus {
    border: 2px solid {{focusRing}};
    padding: 6px 13px;
    outline: none;
}
QPushButton#ToolTabButtonCompact {
    background-color: {{surfaceCard}};
    color: {{textSecondary}};
    border: 1px solid {{border}};
    border-radius: {{radiusSmall}}px;
    padding: 4px 10px;
    font-size: 12px;
    font-weight: 500;
    min-height: 18px;
}
QPushButton#ToolTabButtonCompact:hover {
    background-color: {{surfaceHover}};
    color: {{textPrimary}};
    border: 1px solid {{borderStrong}};
}
QPushButton#ToolTabButtonCompact:checked {
    background-color: {{accentSubtle}};
    color: {{textPrimary}};
    border: 1px solid {{accent}};
    font-weight: 600;
}
QPushButton#ToolTabButtonCompact:pressed {
    background-color: {{surfacePressed}};
}
QPushButton#ToolTabButtonCompact:focus {
    border: 2px solid {{focusRing}};
    padding: 3px 9px;
    outline: none;
}

QLabel#AccentValueLabel {
    font-weight: 600;
    color: {{accent}};
}

/* ---------- Cards ---------- */
QFrame#ToolCard {
    background-color: {{surfaceCard}};
    border: 2px solid {{border}};
    border-radius: {{radiusMedium}}px;
}
QFrame#ToolCard:hover {
    background-color: {{surfaceHover}};
    border: 2px solid {{accent}};
}
QFrame#ToolCard:focus {
    border: 2px solid {{focusRing}};
    background-color: {{accentSubtle}};
}
QFrame#ToolCard:disabled {
    background-color: {{surfaceMuted}};
    border: 2px dashed {{border}};
}
/* PDF split controls use a compact, bounded card. The scoped radio rules keep
   keyboard focus on the indicator instead of drawing a window-wide rectangle. */
QFrame#SplitOptionsCard {
    background-color: {{surfaceCard}};
    border: 1px solid {{border}};
    border-radius: {{radiusMedium}}px;
}
QFrame#SplitOptionsCard QWidget#SplitOptionRow {
    background-color: transparent;
    border: none;
    border-radius: {{radiusSmall}}px;
}
QFrame#SplitOptionsCard QWidget#SplitOptionRow:hover {
    background-color: {{surfaceHover}};
}
QFrame#SplitOptionsCard QRadioButton {
    background-color: transparent;
    border: none;
    border-radius: 0px;
    padding: 0px;
    spacing: 10px;
}
QFrame#SplitOptionsCard QRadioButton:focus {
    background-color: transparent;
    border: none;
    outline: none;
}
QFrame#SplitOptionsCard QRadioButton::indicator {
    width: 18px;
    height: 18px;
    background-color: {{surfaceCard}};
    border: 2px solid {{borderStrong}};
    border-radius: 10px;
}
QFrame#SplitOptionsCard QRadioButton::indicator:hover,
QFrame#SplitOptionsCard QRadioButton::indicator:focus {
    border: 2px solid {{focusRing}};
}
QFrame#SplitOptionsCard QRadioButton::indicator:checked {
    width: 18px;
    height: 18px;
    background-color: {{accent}};
    border: 5px solid {{surfaceCard}};
    border-radius: 10px;
}
/* No `QFrame#ToolCard:disabled QLabel { ... }` rule here on purpose: Qt's style
   sheet engine does not correctly scope a pseudo-state that sits on a
   non-rightmost selector in a descendant chain, so a rule of that shape matched
   every QLabel inside every #ToolCard - enabled or not - and rendered every
   card title in {{textDisabled}} regardless of theme. That single rule was the
   root cause of card text reading as neither full white (dark mode) nor near-
   black (light mode). If disabled-card label styling is needed again, set it
   from C++ (QPalette or an explicit property on the label itself), not via a
   compound `:disabled` descendant selector. */
QLabel#ToolCardName {
    font-size: 14px;
    font-weight: 600;
    color: {{textPrimary}};
}
QLabel#ToolCardDescription {
    font-size: 12px;
    color: {{textSecondary}};
}
QLabel#DroppedFileChip {
    background-color: {{accentSubtle}};
    color: {{accent}};
    border: 1px solid {{accentSubtleBorder}};
    border-radius: {{radiusSmall}}px;
    padding: 4px 10px;
    font-size: 13px;
    font-weight: 600;
}
QLabel#CategoryHeading {
    font-size: 12px;
    font-weight: 600;
    color: {{textTertiary}};
}
QLabel#CategoryCount {
    font-size: 12px;
    color: {{textTertiary}};
}
QLabel#EmptyStateTitle {
    font-size: 18px;
    font-weight: 600;
    color: {{textPrimary}};
}
QLabel#EmptyStateBody {
    font-size: 14px;
    color: {{textSecondary}};
}
/* Class-agnostic ids: some of these cards are plain QWidgets, not QFrames, so a
   QFrame#Name selector would silently not match. */
QFrame#CardFrame, QFrame#SectionCard, QFrame#PresetInfoCard, #PdfFileCard {
    background-color: {{surfaceCard}};
    border: 1px solid {{border}};
    border-radius: {{radiusMedium}}px;
}
#PdfFileCard QLabel, QFrame#CardFrame QLabel, #ImageRowWidget QLabel {
    background: transparent;
    border: none;
    color: {{textPrimary}};
}
/* Document ID front/back pickers.  These are quiet, solid upload cards rather
   than dashed drop zones: selection is button-driven and the stable outline is
   clearer in both light and dark themes. */
QFrame#DocumentUploadCard {
    background-color: {{surfaceMuted}};
    border: 1px solid {{border}};
    border-radius: {{radiusMedium}}px;
}
QFrame#DocumentUploadCard:hover {
    background-color: {{surfaceHover}};
    border-color: {{accentSubtleBorder}};
}
QLabel#DocumentUploadTitle {
    background: transparent;
    border: none;
    color: {{textPrimary}};
    font-size: 14px;
    font-weight: 600;
}
/* The parent card rule above also targets every nested QLabel.  Include both
   object ids here so the preview surface intentionally wins that cascade. */
QFrame#CardFrame QFrame#DocumentUploadCard QLabel#DocumentPreviewTile {
    background-color: {{surfaceCard}};
    border: 1px solid {{accentSubtleBorder}};
    border-radius: {{radiusMedium}}px;
}
/* Status chip. State comes from a `chipState` property so the colour follows the
   theme rather than being baked in at the call site. */
QLabel#StatusChip {
    padding: 4px 10px;
    border-radius: {{radiusSmall}}px;
    font-size: 12px;
    font-weight: 600;
    background-color: {{surfaceMuted}};
    color: {{textSecondary}};
    border: 1px solid {{border}};
}
QLabel#StatusChip[chipState="risk"] {
    background-color: {{errorBg}};
    color: {{errorText}};
    border: 1px solid {{errorBorder}};
}
QLabel#StatusChip[chipState="clean"] {
    background-color: {{successBg}};
    color: {{successText}};
    border: 1px solid {{successBorder}};
}
QLabel#ThumbTile {
    background-color: {{surfaceMuted}};
    border: 1px solid {{border}};
    border-radius: {{radiusMedium}}px;
}
/* Tables */
QTableWidget, QTreeWidget {
    background-color: {{surfaceCard}};
    alternate-background-color: {{surfaceMuted}};
    border: 1px solid {{border}};
    border-radius: {{radiusMedium}}px;
    gridline-color: {{border}};
    font-size: 13px;
    color: {{textPrimary}};
}
QTableWidget::item, QTreeWidget::item {
    padding: 4px 8px;
    color: {{textPrimary}};
}
QTableWidget::item:selected, QTreeWidget::item:selected {
    background-color: {{accentSubtle}};
    color: {{textPrimary}};
}
QHeaderView::section {
    background-color: {{surfaceMuted}};
    color: {{textSecondary}};
    padding: 6px 10px;
    font-weight: 600;
    border: none;
    border-bottom: 1px solid {{border}};
}

/* Card internals: eyebrow, divider, icon badge and key/value info rows. */
QLabel#CardEyebrow {
    font-size: 11px;
    font-weight: 700;
    color: {{accent}};
}
QFrame#CardDivider {
    background-color: {{border}};
    border: none;
    max-height: 1px;
}
QLabel#IconBadge {
    background-color: {{accentSubtle}};
    border: 1px solid {{accentSubtleBorder}};
    border-radius: {{radiusMedium}}px;
}
QLabel#InfoRowKey {
    font-size: 13px;
    font-weight: 600;
    color: {{textSecondary}};
}
QLabel#InfoRowValue {
    font-size: 13px;
    font-weight: 600;
    color: {{textPrimary}};
}
QLabel#PlaybackTime {
    font-size: 13px;
    font-weight: 600;
    color: {{textSecondary}};
}
/* Destructive action: same geometry as SecondaryButton, error colouring. */
QPushButton#DangerButton {
    background-color: {{errorBg}};
    color: {{errorText}};
    border: 1px solid {{errorBorder}};
    border-radius: {{radiusSmall}}px;
    padding: 7px 16px;
    font-size: 14px;
    font-weight: 600;
}
QPushButton#DangerButton:hover {
    border-color: {{errorText}};
}
QPushButton#DangerButton:focus {
    border: 2px solid {{focusRing}};
    padding: 6px 15px;
    outline: none;
}
QFrame#FooterBar {
    background-color: {{surfaceCard}};
    border-top: 1px solid {{border}};
}
QFrame#RecentFileRow, #ImageRowWidget {
    background-color: {{surfaceCard}};
    border: 1px solid {{border}};
    border-radius: {{radiusSmall}}px;
}
QFrame#RecentFileRow:hover, #ImageRowWidget:hover {
    background-color: {{surfaceHover}};
}
QWidget#QuickActionSheet, QWidget#QrReaderWorkspace, QWidget#MetaWorkspace {
    background-color: {{surfaceCard}};
    border: 1px solid {{border}};
    border-radius: {{radiusMedium}}px;
}
QWidget#GifPlayerContainer, QWidget#VideoPlayerContainer {
    background-color: #101010;
    border: 1px solid {{border}};
    border-radius: {{radiusMedium}}px;
}
/* Drop zone.
   Styled here rather than from DropZoneWidget itself: a widget-level stylesheet
   takes precedence over the application one for its whole subtree, and the local
   `QFrame#DropZoneFrame QLabel` rule it used to install specified no color, so
   the labels dropped back to the default palette text (black) in dark mode. */
QFrame#DropZoneFrame {
    border: 2px dashed {{borderStrong}};
    background-color: {{surfaceCard}};
    border-radius: {{radiusLarge}}px;
}
QFrame#DropZoneFrame:hover {
    border-color: {{accent}};
    background-color: {{accentSubtle}};
}
QFrame#DropZoneFrame[dropState="valid"] {
    border-color: {{accent}};
    background-color: {{accentSubtle}};
}
QFrame#DropZoneFrame[dropState="invalid"] {
    border-color: {{errorText}};
    background-color: {{errorBg}};
}
QFrame#DropZoneFrame QLabel {
    background: transparent;
    border: none;
    color: {{textPrimary}};
}
QLabel#DropZoneTitle {
    font-size: 16px;
    font-weight: 600;
    color: {{textPrimary}};
}
QLabel#DropZoneSubtitle {
    font-size: 13px;
    color: {{textSecondary}};
}
QFrame#DropZoneFrame[dropState="invalid"] QLabel#DropZoneTitle {
    color: {{errorText}};
}

/* ---------- Inputs ---------- */
QLineEdit, QPlainTextEdit, QTextEdit {
    background-color: {{surfaceCard}};
    color: {{textPrimary}};
    border: 1px solid {{borderStrong}};
    border-radius: {{radiusSmall}}px;
    padding: 6px 10px;
    selection-background-color: {{accent}};
    selection-color: {{onAccent}};
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {
    border: 2px solid {{focusRing}};
    padding: 5px 9px;
    outline: none;
}
QLineEdit:disabled, QPlainTextEdit:disabled, QTextEdit:disabled {
    background-color: {{surfaceMuted}};
    color: {{textDisabled}};
}
QLineEdit#SearchField {
    padding: 9px 12px;
    font-size: 14px;
    border-radius: {{radiusMedium}}px;
}
QLineEdit#SearchField:focus {
    padding: 8px 11px;
}

QGroupBox {
    font-weight: 600;
    color: {{textPrimary}};
    border: 1px solid {{border}};
    border-radius: {{radiusMedium}}px;
    margin-top: 12px;
    padding-top: 14px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6px;
    color: {{textSecondary}};
}

QCheckBox, QRadioButton {
    color: {{textPrimary}};
    spacing: 8px;
    padding: 2px;
    border: 2px solid transparent;
    border-radius: {{radiusSmall}}px;
}
QCheckBox:focus, QRadioButton:focus {
    border: 2px solid {{focusRing}};
    outline: none;
}
QCheckBox::indicator, QRadioButton::indicator {
    width: 18px;
    height: 18px;
    border: 1px solid {{borderStrong}};
    background-color: {{surfaceCard}};
}
QCheckBox::indicator {
    border-radius: {{radiusSmall}}px;
}
QRadioButton::indicator {
    border-radius: 9px;
}
QCheckBox::indicator:hover, QRadioButton::indicator:hover {
    border: 1px solid {{accent}};
}
QCheckBox::indicator:checked {
    background-color: {{accent}};
    border: 1px solid {{accent}};
    image: url(:/icons/icons/checkmark.svg);
}
QRadioButton::indicator:checked {
    background-color: {{accent}};
    border: 5px solid {{surfaceCard}};
    /* Keep the inner dot circular after the checked-state border is applied. */
    border-radius: 9px;
}
QCheckBox:disabled, QRadioButton:disabled {
    color: {{textDisabled}};
}

/* ---------- Combo box ---------- */
QComboBox {
    background-color: {{surfaceCard}};
    color: {{textPrimary}};
    border: 1px solid {{borderStrong}};
    border-radius: {{radiusSmall}}px;
    padding: 6px 12px;
    min-width: 140px;
    min-height: 20px;
}
QComboBox:hover {
    background-color: {{surfaceHover}};
}
QComboBox:focus {
    border: 2px solid {{focusRing}};
    padding: 5px 11px;
    outline: none;
}
QComboBox:disabled {
    background-color: {{surfaceMuted}};
    color: {{textDisabled}};
}
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: center right;
    width: 22px;
    border: none;
    background: transparent;
    margin-right: 2px;
}
QComboBox::down-arrow {
    image: url(:/icons/icons/{{arrowDown}}.svg);
    width: 10px;
    height: 10px;
}
QComboBox::down-arrow:on {
    image: url(:/icons/icons/{{arrowUp}}.svg);
}
/* Popup view.
   No border, padding or margin on the view itself: the outer frame is owned
   by QComboBoxPrivateContainer so viewport height is not reduced. Item row
   height is 32px fixed by ComboBoxItemDelegate. */
QComboBox QAbstractItemView {
    background-color: {{surfaceCard}};
    color: {{textPrimary}};
    border: none;
    border-radius: 0px;
    padding: 0px;
    margin: 0px;
    outline: none;
    selection-background-color: {{accentSubtle}};
    selection-color: {{textPrimary}};
}
QComboBox QAbstractItemView::item {
    padding: 0px 10px;
    border: none;
    color: {{textPrimary}};
}
QComboBox QAbstractItemView::item:hover {
    background-color: {{surfaceHover}};
}
QComboBox QAbstractItemView::item:selected {
    background-color: {{accentSubtle}};
    color: {{textPrimary}};
}

/* Popup container frame: holds the 1px visible border and card background */
QComboBoxPrivateContainer {
    background-color: {{surfaceCard}};
    border: 1px solid {{borderStrong}};
    border-radius: {{radiusSmall}}px;
    padding: 0px;
}

/* ---------- Spin boxes ---------- */
QSpinBox, QDoubleSpinBox {
    background-color: {{surfaceCard}};
    color: {{textPrimary}};
    border: 1px solid {{borderStrong}};
    border-radius: {{radiusSmall}}px;
    padding: 6px 26px 6px 10px;
    min-width: 90px;
    min-height: 20px;
    font-size: 14px;
}
QSpinBox:focus, QDoubleSpinBox:focus {
    border: 2px solid {{focusRing}};
    padding: 5px 25px 5px 9px;
    outline: none;
}
QSpinBox:disabled, QDoubleSpinBox:disabled {
    background-color: {{surfaceMuted}};
    color: {{textDisabled}};
}
QSpinBox::up-button, QDoubleSpinBox::up-button {
    subcontrol-origin: border;
    subcontrol-position: top right;
    width: 22px;
    height: 16px;
    background-color: transparent;
    border-left: 1px solid {{border}};
    border-bottom: 1px solid {{border}};
}
QSpinBox::down-button, QDoubleSpinBox::down-button {
    subcontrol-origin: border;
    subcontrol-position: bottom right;
    width: 22px;
    height: 16px;
    background-color: transparent;
    border-left: 1px solid {{border}};
}
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
    background-color: {{surfaceHover}};
}
QSpinBox::up-button:pressed, QDoubleSpinBox::up-button:pressed,
QSpinBox::down-button:pressed, QDoubleSpinBox::down-button:pressed {
    background-color: {{surfacePressed}};
}
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
    image: url(:/icons/icons/{{arrowUp}}.svg);
    width: 10px;
    height: 10px;
}
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
    image: url(:/icons/icons/{{arrowDown}}.svg);
    width: 10px;
    height: 10px;
}

/* ---------- Progress ---------- */
QProgressBar {
    background-color: {{surfaceMuted}};
    border: none;
    border-radius: 3px;
    height: 6px;
    text-align: center;
    color: {{textSecondary}};
    font-size: 12px;
}
QProgressBar::chunk {
    background-color: {{accent}};
    border-radius: 3px;
}

/* ---------- Sliders ---------- */
QSlider::groove:horizontal {
    height: 4px;
    background-color: {{surfaceMuted}};
    border: none;
    border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background-color: {{accent}};
    border: none;
    border-radius: 2px;
}
QSlider::add-page:horizontal {
    background-color: {{surfaceMuted}};
    border: none;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background: {{accent}};
    border: 2px solid {{surfaceCard}};
    width: 14px;
    height: 14px;
    margin: -6px 0;
    border-radius: 9px;
}
QSlider::handle:horizontal:hover {
    background: {{accentHover}};
}
QSlider:focus {
    outline: none;
}
QSlider:focus::handle:horizontal {
    border: 2px solid {{focusRing}};
}

/* ---------- Scroll bars ---------- */
QScrollBar:vertical {
    background: transparent;
    width: 12px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: {{borderStrong}};
    border-radius: 4px;
    min-height: 28px;
    margin: 2px 4px;
}
QScrollBar::handle:vertical:hover {
    background: {{textTertiary}};
}
QScrollBar:horizontal {
    background: transparent;
    height: 12px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: {{borderStrong}};
    border-radius: 4px;
    min-width: 28px;
    margin: 4px 2px;
}
QScrollBar::handle:horizontal:hover {
    background: {{textTertiary}};
}
QScrollBar::add-line, QScrollBar::sub-line {
    width: 0;
    height: 0;
    border: none;
    background: none;
}
QScrollBar::add-page, QScrollBar::sub-page {
    background: none;
}
QAbstractScrollArea {
    background: transparent;
    border: none;
}

/* ---------- Menus & tooltips ---------- */
QMenu {
    background-color: {{surfaceCard}};
    color: {{textPrimary}};
    border: 1px solid {{border}};
    border-radius: {{radiusMedium}}px;
    padding: 4px;
}
QMenu::item {
    padding: 6px 24px 6px 12px;
    border-radius: {{radiusSmall}}px;
}
QMenu::item:selected {
    background-color: {{accentSubtle}};
}
QMenu::separator {
    height: 1px;
    background: {{border}};
    margin: 4px 8px;
}
QToolTip {
    background-color: {{surfaceCard}};
    color: {{textPrimary}};
    border: 1px solid {{border}};
    border-radius: {{radiusSmall}}px;
    padding: 6px 10px;
}

/* ---------- Dialogs ---------- */
QDialog, QMessageBox {
    background-color: {{surfaceCanvas}};
    color: {{textPrimary}};
}
QDialog QPushButton, QMessageBox QPushButton, QDialogButtonBox QPushButton {
    background-color: {{accent}};
    color: {{onAccent}};
    border: 2px solid transparent;
    border-radius: {{radiusSmall}}px;
    padding: 6px 20px;
    font-size: 14px;
    font-weight: 600;
    min-width: 70px;
}
QDialog QPushButton:hover, QMessageBox QPushButton:hover, QDialogButtonBox QPushButton:hover {
    background-color: {{accentHover}};
}
QDialog QPushButton:focus, QMessageBox QPushButton:focus, QDialogButtonBox QPushButton:focus {
    border: 2px solid {{onAccent}};
    outline: none;
}
)QSS";
}

} // namespace LocalTools::UI
