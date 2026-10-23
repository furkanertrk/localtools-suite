#include "icon_utils.h"

#include <QSvgRenderer>
#include <QPainter>
#include <QHash>
#include <QGuiApplication>
#include <QScreen>

#include "../theme.h"

namespace LocalTools::UI {

namespace {

QString resourcePath(IconType type) {
    switch (type) {
    // Navigation / chrome
    case IconType::Home:        return ":/icons/icons/home.svg";
    case IconType::Settings:    return ":/icons/icons/settings.svg";
    case IconType::Folder:      return ":/icons/icons/folder.svg";
    case IconType::View:        return ":/icons/icons/view.svg";
    case IconType::Edit:        return ":/icons/icons/edit.svg";
    case IconType::Close:       return ":/icons/icons/close.svg";
    case IconType::Search:      return ":/icons/icons/search.svg";
    case IconType::Grid:        return ":/icons/icons/grid.svg";
    case IconType::Star:        return ":/icons/icons/star.svg";
    case IconType::StarFilled:  return ":/icons/icons/star_filled.svg";
    case IconType::Sun:         return ":/icons/icons/sun.svg";
    case IconType::Moon:        return ":/icons/icons/moon.svg";
    case IconType::ArrowUp:     return ":/icons/icons/arrow_up.svg";
    case IconType::ArrowDown:   return ":/icons/icons/arrow_down.svg";
    case IconType::ArrowLeft:   return ":/icons/icons/arrow_left.svg";
    case IconType::ArrowRight:  return ":/icons/icons/arrow_right.svg";

    // Status
    case IconType::Success:     return ":/icons/icons/checkmark.svg";
    case IconType::Warning:     return ":/icons/icons/warning.svg";
    case IconType::Error:       return ":/icons/icons/error.svg";
    case IconType::Info:        return ":/icons/icons/info.svg";

    // PDF
    case IconType::Organize:    return ":/icons/icons/reorder.svg";
    case IconType::Merge:       return ":/icons/icons/merge.svg";
    case IconType::Split:       return ":/icons/icons/split.svg";
    case IconType::Extract:     return ":/icons/icons/extract.svg";
    case IconType::Delete:      return ":/icons/icons/delete.svg";
    case IconType::Reorder:     return ":/icons/icons/reorder.svg";
    case IconType::Rotate:      return ":/icons/icons/rotate.svg";
    case IconType::RotateLeft:  return ":/icons/icons/rotate_left.svg";
    case IconType::RotateRight: return ":/icons/icons/rotate_right.svg";
    case IconType::Convert:     return ":/icons/icons/convert.svg";
    case IconType::ImagesToPdf: return ":/icons/icons/image.svg";
    case IconType::PdfToImages: return ":/icons/icons/pdf.svg";
    case IconType::PdfToText:   return ":/icons/icons/text.svg";
    case IconType::Optimize:
    case IconType::Compress:    return ":/icons/icons/compress.svg";
    case IconType::Metadata:    return ":/icons/icons/metadata.svg";
    case IconType::Watermark:   return ":/icons/icons/watermark.svg";

    // Media
    case IconType::Image:       return ":/icons/icons/image.svg";
    case IconType::Crop:        return ":/icons/icons/crop.svg";
    case IconType::Resize:      return ":/icons/icons/resize.svg";
    case IconType::Video:       return ":/icons/icons/video.svg";
    case IconType::Music:       return ":/icons/icons/music.svg";
    case IconType::Trim:        return ":/icons/icons/trim.svg";
    case IconType::Play:        return ":/icons/icons/play.svg";
    case IconType::Pause:       return ":/icons/icons/pause.svg";
    case IconType::Clock:       return ":/icons/icons/clock.svg";
    case IconType::Monitor:     return ":/icons/icons/monitor.svg";
    case IconType::Drive:       return ":/icons/icons/drive.svg";
    case IconType::Film:        return ":/icons/icons/film.svg";

    // Files / utilities
    case IconType::FileText:    return ":/icons/icons/text.svg";
    case IconType::Archive:     return ":/icons/icons/archive.svg";
    case IconType::Rename:      return ":/icons/icons/rename.svg";
    case IconType::Duplicate:   return ":/icons/icons/duplicate.svg";
    case IconType::Hash:        return ":/icons/icons/hash.svg";
    case IconType::QrCode:      return ":/icons/icons/qrcode.svg";
    }
    return ":/icons/icons/home.svg";
}

/**
 * Cached renderers: a QSvgRenderer re-parses the XML on every construction, and
 * getIcon() asks for six pixmaps of the same glyph. Parsing once per file turns
 * a theme toggle over the sidebar from ~70 parses into ~12.
 */
QSvgRenderer* rendererFor(const QString& path) {
    static QHash<QString, QSvgRenderer*> cache;
    auto it = cache.find(path);
    if (it != cache.end()) {
        return it.value();
    }
    auto* renderer = new QSvgRenderer(path);
    if (!renderer->isValid()) {
        delete renderer;
        cache.insert(path, nullptr);
        return nullptr;
    }
    cache.insert(path, renderer);
    return renderer;
}

QHash<QString, QPixmap>& pixmapCache() {
    static QHash<QString, QPixmap> cache;
    return cache;
}

qreal currentDpr() {
    if (auto* screen = QGuiApplication::primaryScreen()) {
        return screen->devicePixelRatio();
    }
    return 1.0;
}

} // namespace

void IconUtils::clearCache() {
    pixmapCache().clear();
}

QPixmap IconUtils::getPixmap(IconType type, const QColor& color, int size) {
    const QColor tint = color.isValid() ? color : Theme::color(ColorToken::TextSecondary);
    const qreal dpr = currentDpr();

    const QString key = QStringLiteral("%1|%2|%3|%4")
                            .arg(static_cast<int>(type))
                            .arg(tint.name(QColor::HexArgb))
                            .arg(size)
                            .arg(dpr);

    auto& cache = pixmapCache();
    auto cached = cache.find(key);
    if (cached != cache.end()) {
        return cached.value();
    }

    QPixmap pixmap(qRound(size * dpr), qRound(size * dpr));
    pixmap.fill(Qt::transparent);

    QSvgRenderer* renderer = rendererFor(resourcePath(type));
    if (!renderer) {
        // Missing resource: return a blank pixmap rather than a broken glyph.
        pixmap.setDevicePixelRatio(dpr);
        cache.insert(key, pixmap);
        return pixmap;
    }

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer->render(&painter, QRectF(0, 0, size * dpr, size * dpr));

    // Flatten to a single tint. The source SVGs use fill="currentColor", which
    // Qt's SVG renderer does not resolve, so the composite is what colors them.
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), tint);
    painter.end();

    // Tagging the ratio is what makes Qt draw it at the right logical size
    // instead of upscaling a small bitmap.
    pixmap.setDevicePixelRatio(dpr);

    cache.insert(key, pixmap);
    return pixmap;
}

QIcon IconUtils::getIcon(IconType type, const QColor& color, int size, const QColor& checkedColor) {
    const QColor normalColor = color.isValid() ? color : Theme::color(ColorToken::TextSecondary);
    const QColor onColor = checkedColor.isValid() ? checkedColor : Theme::color(ColorToken::Accent);
    const QColor activeColor = Theme::color(ColorToken::TextPrimary);

    QIcon icon;
    icon.addPixmap(getPixmap(type, normalColor, size), QIcon::Normal,   QIcon::Off);
    icon.addPixmap(getPixmap(type, onColor, size),     QIcon::Normal,   QIcon::On);
    icon.addPixmap(getPixmap(type, activeColor, size), QIcon::Active,   QIcon::Off);
    icon.addPixmap(getPixmap(type, onColor, size),     QIcon::Active,   QIcon::On);
    icon.addPixmap(getPixmap(type, activeColor, size), QIcon::Selected, QIcon::Off);
    icon.addPixmap(getPixmap(type, onColor, size),     QIcon::Selected, QIcon::On);
    return icon;
}

} // namespace LocalTools::UI
