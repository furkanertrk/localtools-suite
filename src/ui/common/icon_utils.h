#ifndef LOCALTOOLS_UI_COMMON_ICON_UTILS_H
#define LOCALTOOLS_UI_COMMON_ICON_UTILS_H

#include <QIcon>
#include <QPixmap>
#include <QColor>

namespace LocalTools::UI {

enum class IconType {
    // Navigation / chrome
    Home,
    Settings,
    Folder,
    View,
    Edit,
    Close,
    Search,
    Grid,
    Star,
    StarFilled,
    Sun,
    Moon,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,

    // Status
    Success,
    Warning,
    Error,
    Info,

    // PDF
    Organize,
    Merge,
    Split,
    Extract,
    Delete,
    Reorder,
    Rotate,
    RotateLeft,
    RotateRight,
    Convert,
    ImagesToPdf,
    PdfToImages,
    PdfToText,
    Optimize,
    Compress,
    Metadata,
    Watermark,

    // Media
    Image,
    Crop,
    Resize,
    Video,
    Music,
    Trim,
    Play,
    Pause,
    Clock,
    Monitor,
    Drive,
    Film,

    // Files / utilities
    FileText,
    Archive,
    Rename,
    Duplicate,
    Hash,
    QrCode
};

class IconUtils {
public:
    /**
     * Renders a themed, DPI-correct pixmap.
     *
     * The pixmap is rasterized at size * devicePixelRatio and tagged accordingly,
     * so it stays sharp at the fractional Windows scale factors (125% / 150%)
     * that PassThrough rounding preserves.
     *
     * An invalid @p color resolves to the current theme's secondary text color.
     */
    static QPixmap getPixmap(IconType type, const QColor& color = QColor(), int size = 20);

    /**
     * Builds a multi-state QIcon (normal / active / selected x on / off).
     * An invalid @p color or @p checkedColor resolves against the current theme.
     */
    static QIcon getIcon(IconType type, const QColor& color = QColor(), int size = 20,
                         const QColor& checkedColor = QColor());

    /** Drops cached renderers and pixmaps. Call whenever the theme changes. */
    static void clearCache();
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_ICON_UTILS_H
