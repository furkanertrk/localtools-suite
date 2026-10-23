#ifndef LOCALTOOLS_UI_THEME_H
#define LOCALTOOLS_UI_THEME_H

#include <QString>
#include <QColor>
#include <QFont>
#include <QMainWindow>
#include "../services/settings_service.h"

namespace LocalTools::UI {

/**
 * Semantic color tokens.
 *
 * Each token resolves to a different value in light and dark mode; callers ask
 * for the *role* and never for a mode-specific constant. This is what allows a
 * widget to be styled once and stay correct across theme switches.
 *
 * Palette basis: Windows 11 Fluent surfaces + system accent, so the app reads as
 * a native desktop utility rather than a web app.
 */
enum class ColorToken {
    // Surfaces
    SurfaceCanvas,      // window background
    SurfaceCard,        // cards, panels, tool tiles
    SurfaceMuted,       // inset / secondary fills
    SurfaceHover,
    SurfacePressed,

    // Text
    TextPrimary,
    TextSecondary,
    TextTertiary,       // metadata only - lowest contrast that still passes AA
    TextDisabled,

    // Lines
    Border,
    BorderStrong,

    // Accent
    Accent,
    OnAccent,
    AccentHover,
    AccentPressed,
    AccentSubtle,       // selected nav row fill
    AccentSubtleBorder,

    // Focus
    FocusRing,

    // Semantic
    SuccessBg, SuccessText, SuccessBorder,
    WarningBg, WarningText, WarningBorder,
    ErrorBg,   ErrorText,   ErrorBorder,
    InfoBg,    InfoText,    InfoBorder
};

/** 4px-based spacing scale. Replaces ad-hoc 2/6/7/9/14/18/26 literals. */
enum class Space {
    XXS,    // 4
    XS,     // 8
    S,      // 12
    M,      // 16
    ML,     // 20
    L,      // 24  - standard page margin / section gap
    XL,     // 32
    XXL,    // 40
    XXXL    // 48
};

/** Fluent radii: controls / cards / dialogs. */
enum class Radius {
    Small,   // 4  - buttons, inputs
    Medium,  // 8  - cards, flyouts
    Large    // 12 - dialogs, drop zones
};

/** Windows 11 Fluent type ramp. All sizes in px (never pt). */
enum class FontRole {
    Caption,     // 12/16 400 - helper text, timestamps
    Body,        // 14/20 400 - default UI size
    BodyStrong,  // 14/20 600 - labels, list titles
    BodyLarge,   // 18/24 400 - lead paragraphs
    Subtitle,    // 20/28 600 - section headers
    Title,       // 28/36 600 - page titles
    TitleLarge   // 40/52 600 - Home hero only
};

class Theme {
public:
    // ---- Application ----
    static void applyThemeToWindow(QMainWindow* window, Services::AppTheme theme);
    static Services::AppTheme getSystemColorScheme();
    static QString getStyleSheet(Services::AppTheme effectiveTheme);

    /** Installs the app font + base style. Call once, before any widget is built. */
    static void initializeApplication();

    // ---- Mode (single source of truth) ----

    /** The *resolved* mode (never returns System). */
    static Services::AppTheme currentMode();
    static bool isDark();

    // ---- Token accessors ----

    static QColor color(ColorToken token);
    static QColor color(ColorToken token, bool dark);
    /** Hex string form, for embedding in stylesheets. */
    static QString hex(ColorToken token);
    static QString hex(ColorToken token, bool dark);

    static int space(Space s);
    static int radius(Radius r);

    static QFont font(FontRole role);
    static int fontSizePx(FontRole role);
    static int fontWeight(FontRole role);
    /** Segoe UI Variable stack, falling back to Inter off-Windows. */
    static QString fontFamilyCss();
    static QStringList fontFamilies();

    // ---- Motion ----
    static constexpr int DurationFast = 150;
    static constexpr int DurationNormal = 250;

    // ---- Minimum interaction target (WCAG 2.5.8) ----
    static constexpr int MinTargetSize = 28;

    // ------------------------------------------------------------------
    // Deprecated string constants.
    //
    // Retained so the existing ~21 call sites keep compiling while pages are
    // migrated to the token API above. Values now point at the new palette, so
    // legacy call sites pick up the redesign automatically.
    //
    // Do not use in new code - prefer Theme::hex(ColorToken::...).
    // ------------------------------------------------------------------
    static const char* PrimaryColor;
    static const char* PrimaryHover;

    static const char* BackgroundLight;
    static const char* SurfaceLight;
    static const char* BorderLight;
    static const char* TextPrimaryLight;
    static const char* TextSecondaryLight;

    static const char* BackgroundDark;
    static const char* SurfaceDark;
    static const char* BorderDark;
    static const char* TextPrimaryDark;
    static const char* TextSecondaryDark;

    static const char* SuccessBgDark;
    static const char* SuccessTextDark;
    static const char* WarningBgDark;
    static const char* WarningTextDark;
    static const char* ErrorBgDark;
    static const char* ErrorTextDark;
    static const char* InfoBgDark;
    static const char* InfoTextDark;

    static const char* SuccessBgLight;
    static const char* SuccessTextLight;
    static const char* WarningBgLight;
    static const char* WarningTextLight;
    static const char* ErrorBgLight;
    static const char* ErrorTextLight;
    static const char* InfoBgLight;
    static const char* InfoTextLight;

    static const char* RadiusSmall;
    static const char* RadiusMedium;
    static const char* RadiusLarge;

private:
    /** Expands {{token}} placeholders in the single shared stylesheet template. */
    static QString expandTokens(const QString& templateSheet, bool dark);
    static const char* styleSheetTemplate();

    static Services::AppTheme s_currentMode;
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_THEME_H
