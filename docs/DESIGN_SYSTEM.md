# LocalTools Design System

Single source of truth for the LocalTools Suite UI. Everything here is enforced in
code by `src/ui/theme.h` / `theme.cpp` — this document describes intent, the code
holds the values.

Design direction: **Minimalism / Swiss** applied to a **Windows 11 Fluent** shell.
Minimal but not generic, native rather than SaaS. No decorative motion, no glass,
no neumorphism.

---

## 1. Golden rule

**New features must not look like an afterthought.** A user should not be able to
tell which screen was built last.

Before building any UI:
- Reuse the components in `src/ui/common/`.
- Never write a raw hex colour, font size, or spacing number in a page.

---

## 2. Tokens — the only source of colour, spacing and type

All tokens live in `src/ui/theme.h` and resolve **at runtime** against the current
theme. Ask for the *role*, never for a mode-specific constant:

```cpp
Theme::color(ColorToken::Accent)        // QColor, correct for light or dark
Theme::hex(ColorToken::SurfaceCard)     // QString, for stylesheets
Theme::space(Space::L)                  // 24
Theme::radius(Radius::Medium)           // 8
Theme::font(FontRole::Subtitle)         // QFont
Theme::isDark()                         // single source of truth for the mode
```

### Colour tokens
Surfaces `SurfaceCanvas · SurfaceCard · SurfaceMuted · SurfaceHover · SurfacePressed`
Text `TextPrimary · TextSecondary · TextTertiary · TextDisabled`
Lines `Border · BorderStrong`
Accent `Accent · OnAccent · AccentHover · AccentPressed · AccentSubtle · AccentSubtleBorder`
Focus `FocusRing`
Semantic `Success/Warning/Error/Info` each with `Bg · Text · Border`

The accent is the **Windows 11 system accent** (`#0067C0` light, `#4CC2FF` dark).
`OnAccent` against `Accent` is contrast-verified: 5.5:1 light, 11.2:1 dark — both
pass WCAG AA.

### Spacing — 4px base
`XXS 4 · XS 8 · S 12 · M 16 · ML 20 · L 24 · XL 32 · XXL 40 · XXXL 48`
Page margin `L`. Section gap `L`. Group gap `S`. Label to control `XS`.

### Radii
`Small 4` controls · `Medium 8` cards and flyouts · `Large 12` dialogs and drop zones.

### Typography — Windows 11 Fluent ramp
| Role | Size / line | Weight |
|---|---|---|
| `Caption` | 12 / 16 | 400 |
| `Body` | 14 / 20 | 400 |
| `BodyStrong` | 14 / 20 | 600 |
| `BodyLarge` | 18 / 24 | 400 |
| `Subtitle` | 20 / 28 | 600 |
| `Title` | 28 / 36 | 600 |
| `TitleLarge` | 40 / 52 | 600 |

Family: Segoe UI Variable Text → Segoe UI Variable → Segoe UI → Inter → sans-serif.
Installed once via `Theme::initializeApplication()`.

**Sizes are always px, never pt.** `setPointSize()` is banned: at 96 DPI `13pt` is
~17px, so a "13" heading and a "13" label silently differ by 4px.

**Never uppercase Turkish text with a stylesheet transform** — the `i` → `İ`/`I`
mapping is locale-dependent. Pre-uppercase in the source string instead, as the
sidebar `CATEGORIES` label does.

---

## 3. One stylesheet, both themes

`Theme::styleSheetTemplate()` is a **single** template using `{{token}}`
placeholders, expanded per mode by `Theme::expandTokens()`. There is no separate
light and dark copy, so a rule cannot drift between modes.

It is installed on `qApp`, so dialogs, popups and combo box views inherit it.

To style something new: add a rule to that template using tokens, and give the
widget an `objectName`. Do **not** call `setStyleSheet()` in a page.

### Why a local `setStyleSheet()` is not just untidy

A widget-level stylesheet **replaces** the application one for that widget's
whole subtree. The pattern that kept recurring here was:

```cpp
card->setStyleSheet(R"(
    QFrame#Card { background-color: palette(base); ... }
    QLabel { background: transparent; border: none; }   // no color!
)");
```

That `QLabel` rule wins over the app sheet's `QLabel { color: ... }`, and because
it sets no colour of its own, every label inside the card falls back to the raw
palette text colour — which rendered them **black on a dark background**. Six
widgets shipped this bug (the drop zone, the video card, three PDF file cards and
an image row).

Rules that follow from it:
- Never call `setStyleSheet()` on a container. Give it an `objectName` and style
  it in `theme.cpp`.
- If you must scope a descendant rule, always state `color` explicitly.
- Prefer a bare `#Name` selector over `QFrame#Name` — several of these "cards"
  are plain `QWidget`s, and a type-qualified selector silently fails to match.
- Express visual states with a dynamic property plus
  `style()->unpolish(w); style()->polish(w);` (see `DropZoneWidget::applyDropState`
  and the `StatusChip[chipState=...]` rules), not by swapping stylesheets.

### Never put a pseudo-state on a non-rightmost selector

`AncestorType#Name:pseudo DescendantType { ... }` does not scope correctly in
Qt's style sheet engine — the `:pseudo` on the ancestor is not reliably
respected, so the rule can match the descendant in *every* instance of the
ancestor, not just the ones in that state. This shipped as a real bug: a rule
meant only for disabled tool cards —

```css
QFrame#ToolCard:disabled QLabel { color: {{textDisabled}}; }
```

— matched every `QLabel` inside every `#ToolCard`, enabled or not, which is why
every Home card title rendered muted instead of full-contrast text in both
themes. `QFrame#ToolCard:disabled { ... }` (pseudo-state directly on the
target widget, no descendant) is fine — the bug is specific to the descendant
form. If a descendant needs to react to an ancestor's state, drive it from C++
(a dynamic property plus `style()->unpolish/polish()`, as `DropZoneWidget` and
the `StatusChip[chipState=...]` rules do) instead.

### Combo boxes

Do not hand-tune combo popups. `Theme::initializeApplication()` installs a
polish-time filter that gives every `QComboBox` a `QListView` (so `::item` rules
apply at all), uniform row sizing and a shared 32px row-delegate policy. It also
sets a visible-item cap. The popup's height is computed from item sizeHints
alone, so the view carries **no padding, margin, border or border-radius** —
adding any is what clips the last row. The visible card border belongs to the
`QComboBoxPrivateContainer`, outside the viewport.

---

## 4. Accessibility — non-negotiable

1. **Every control is keyboard reachable.** Never `setFocusPolicy(Qt::NoFocus)` on
   anything actionable, including navigation and tab strips.
2. **Focus is always visible.** Controls carry a transparent 2px border at rest and
   colour it with `FocusRing` on `:focus`, so padding never shifts. There is no
   global `outline: 0` reset and none may be reintroduced (WCAG 2.4.7).
   Sliders use `Qt::TabFocus`, so their handle ring is shown for keyboard focus
   without appearing after a mouse drag.
3. **Icon-only controls need `setAccessibleName()`** — a tooltip is not a name.
4. **Interaction targets are at least `Theme::MinTargetSize` (28px).**
5. Vector icons only, rendered DPI-aware, so nothing blurs at 125% / 150%.

---

## 5. Components

Reuse from `src/ui/common/`; refactor them rather than writing a variant.

| Component | Use for |
|---|---|
| `PageHeader` | Page title + subtitle + trailing actions. Every page. |
| `SearchField` | Any filter input. Debounced, clearable, Escape resets. |
| `ToolCard` | A tool tile, built from a `Core::ToolDescriptor`. |
| `DropZoneWidget` | All file drag-and-drop. |
| `StatusBanner` | Inline info / success / warning / error. Not dialogs. |
| `PillSlider` | Quality and compression sliders. |
| `FileCardWidget` | A selected file with its status and actions. |
| `ThumbnailCard` / `ThumbnailGrid` / `PageWorkspace` | Page-level PDF operations. |
| `ResultScreen` | Terminal success state of a tool run. |

### Icons
`IconUtils::getIcon()` / `getPixmap()` with an `IconType`. Renders at
`size * devicePixelRatio`, antialiased and cached. Pass a token colour, never a
hex literal. Call `IconUtils::clearCache()` when the theme changes.

Icons are 20×20 filled SVGs in `resources/icons/`, registered in `resources.qrc`.
Match the existing geometry when adding one — do not mix filled and outline.

---

## 6. Home is generated, not written

Home renders **every** tool from `Core::ToolRegistry`: favourites first, then one
section per category, with live search over name, description, keywords and file
extensions. Adding a tool to the registry is all that is required for it to appear.

Never hardcode a tool list in the UI. Routing goes through
`ToolDescriptor::legacyToolId`; tools with `isAvailable = false` render disabled.

---

## 7. Tool page layout

1. `PageHeader` (title + subtitle)
2. Input / `DropZoneWidget`
3. Options
4. Primary action — `objectName("PrimaryButton")`
5. Progress — `QProgressBar`
6. Result — `StatusBanner` or `ResultScreen`

Use `setMinimumWidth` / `setMinimumHeight` on buttons, never `setFixedSize`:
Turkish strings run 1.3–1.6× longer than English and will clip.

---

## 8. Localisation

Every visible string goes through `tr()` and into `resources/translations/tr_TR.ts`.

Never call `tr()` on a runtime variable — `tr(someQString.toUtf8().constData())`
cannot be extracted by `lupdate` and will silently ship untranslated.

New widgets must implement `changeEvent()` handling `QEvent::LanguageChange` and
re-apply their text in `retranslateUi()`.
