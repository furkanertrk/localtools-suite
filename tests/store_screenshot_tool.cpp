#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QSettings>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QThread>

#include <iostream>
#include <functional>
#include <vector>

#include "core/backend/pdfium_backend.h"
#include "services/localization_service.h"
#include "services/settings_service.h"
#include "ui/common/sidebar.h"
#include "ui/common/thumbnail_grid.h"
#include "ui/main_window.h"
#include "ui/pages/images_page.h"
#include "ui/pages/metadata_page.h"
#include "ui/pages/organize_page.h"
#include "ui/theme.h"

namespace {

bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 15000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(20);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    return condition();
}

bool saveWindow(LocalTools::UI::MainWindow& window, const QString& path) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QThread::msleep(120);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    // Render into a logical-pixel image instead of QWidget::grab(), whose
    // physical dimensions inherit the host display DPR (for example 120%).
    // Store artifacts must be deterministic across capture machines.
    if (window.size() != QSize(1600, 900)) return false;
    QImage capture(window.size(), QImage::Format_RGB32);
    capture.fill(Qt::black);
    QPainter painter(&capture);
    window.render(&painter, QPoint(), QRegion(), QWidget::DrawWindowBackground | QWidget::DrawChildren);
    painter.end();
    return capture.save(path, "PNG");
}

QStackedWidget* primaryStack(LocalTools::UI::MainWindow& window) {
    QStackedWidget* result = nullptr;
    for (QStackedWidget* stack : window.findChildren<QStackedWidget*>()) {
        if (!result || stack->count() > result->count()) result = stack;
    }
    return result;
}

bool createSampleImage(const QString& path) {
    QImage image(1920, 1080, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient background(0, 0, image.width(), image.height());
    background.setColorAt(0.0, QColor(15, 62, 98));
    background.setColorAt(0.55, QColor(26, 120, 153));
    background.setColorAt(1.0, QColor(72, 187, 174));
    painter.fillRect(image.rect(), background);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 28));
    painter.drawEllipse(QPointF(1540, 230), 360, 360);
    painter.drawEllipse(QPointF(260, 960), 430, 430);

    painter.setPen(QColor(255, 255, 255));
    QFont titleFont(QStringLiteral("Segoe UI"), 58, QFont::DemiBold);
    painter.setFont(titleFont);
    painter.drawText(QRect(150, 345, 1350, 100), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("LocalTools Suite"));
    QFont bodyFont(QStringLiteral("Segoe UI"), 27);
    painter.setFont(bodyFont);
    painter.setPen(QColor(230, 247, 250));
    painter.drawText(QRect(155, 455, 1200, 70), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Offline file processing • Sample image"));
    painter.end();
    return image.save(path, "PNG");
}

bool createSamplePdf(const QString& path, const QString& workingDirectory, const QString& language) {
    std::vector<std::string> pagePaths;
    for (int pageNumber = 1; pageNumber <= 2; ++pageNumber) {
        QImage page(1240, 1754, QImage::Format_RGB32);
        page.fill(QColor(248, 250, 252));
        QPainter painter(&page);
        painter.setRenderHint(QPainter::Antialiasing);

        painter.fillRect(QRect(0, 0, page.width(), 210), QColor(15, 99, 135));
        painter.setPen(Qt::white);
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 34, QFont::DemiBold));
        painter.drawText(QRect(85, 55, 900, 65), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("LocalTools Suite"));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 18));
        painter.setPen(QColor(215, 240, 246));
        painter.drawText(QRect(88, 125, 900, 45), Qt::AlignLeft | Qt::AlignVCenter,
                         language == QStringLiteral("tr")
                             ? QStringLiteral("Çevrimdışı belge örneği")
                             : QStringLiteral("Offline document sample"));

        painter.setPen(QColor(30, 41, 59));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 25, QFont::DemiBold));
        painter.drawText(QRect(85, 285, 1000, 55), Qt::AlignLeft | Qt::AlignVCenter,
                         language == QStringLiteral("tr")
                             ? QStringLiteral("Belge özeti — Sayfa %1").arg(pageNumber)
                             : QStringLiteral("Document overview — Page %1").arg(pageNumber));

        const QColor panelColors[] = {QColor(224, 242, 254), QColor(236, 253, 245), QColor(245, 243, 255)};
        for (int panel = 0; panel < 3; ++panel) {
            const QRect panelRect(85, 390 + panel * 310, 1070, 240);
            painter.setPen(Qt::NoPen);
            painter.setBrush(panelColors[panel]);
            painter.drawRoundedRect(panelRect, 18, 18);
            painter.setBrush(QColor(14, 116, 144));
            painter.drawRoundedRect(QRect(panelRect.x() + 35, panelRect.y() + 42, 145, 18), 9, 9);
            painter.setBrush(QColor(100, 116, 139, 115));
            for (int line = 0; line < 4; ++line) {
                const int width = 780 - line * 75;
                painter.drawRoundedRect(QRect(panelRect.x() + 35, panelRect.y() + 88 + line * 31,
                                               width, 10), 5, 5);
            }
        }
        painter.end();

        const QString pagePath = QDir(workingDirectory).filePath(
            QStringLiteral("store-sample-page-%1.png").arg(pageNumber));
        if (!page.save(pagePath, "PNG")) return false;
        pagePaths.push_back(pagePath.toStdString());
    }

    const auto result = LocalTools::Core::PdfiumBackend::imagesToPdf(pagePaths, path.toStdString());
    return !result.hasError() && QFileInfo::exists(path);
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: store_screenshot_tool <output-dir> <en|tr>\n";
        return 2;
    }

    qputenv("QT_MEDIA_BACKEND", "windows");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QTemporaryDir settingsRoot;
    if (!settingsRoot.isValid()) return 3;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsRoot.path());

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("LocalTools Store Capture"));
    QCoreApplication::setApplicationName(QStringLiteral("LocalTools Store Capture"));

    const QString outputDir = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    const QString language = QString::fromLocal8Bit(argv[2]).toLower() == QStringLiteral("tr")
        ? QStringLiteral("tr") : QStringLiteral("en");
    if (!QDir().mkpath(outputDir)) return 4;

    const QString samplePdf = QDir(settingsRoot.path()).filePath(QStringLiteral("LocalTools_Sample.pdf"));
    const QString sampleImage = QDir(settingsRoot.path()).filePath(QStringLiteral("Sample_Landscape.png"));
    if (!createSamplePdf(samplePdf, settingsRoot.path(), language)
        || !createSampleImage(sampleImage)) return 5;

    auto& settings = LocalTools::Services::SettingsService::instance();
    settings.setTheme(LocalTools::Services::AppTheme::Dark);
    settings.setKeepRecentFiles(false);
    LocalTools::UI::Theme::initializeApplication();
    auto& localization = LocalTools::Services::LocalizationService::instance();
    localization.initialize(&app);
    localization.setLanguageCode(language);

    LocalTools::UI::MainWindow window;
    // Keep the capture widget outside the desktop compositor so Windows does
    // not clamp it to the taskbar-reduced work area on a 1536x864 display.
    window.setAttribute(Qt::WA_DontShowOnScreen, true);
    window.resize(1600, 900);
    window.show();
    if (!waitUntil([&window]() { return window.isVisible(); }, 3000)) return 6;

    auto* stack = primaryStack(window);
    auto* sidebar = window.findChild<LocalTools::UI::SidebarWidget*>();
    auto* organize = window.findChild<LocalTools::UI::OrganizePage*>();
    auto* images = window.findChild<LocalTools::UI::ImagesPage*>();
    auto* metadata = window.findChild<LocalTools::UI::MetadataPage*>();
    if (!stack || !sidebar || !organize || !images || !metadata) return 7;

    auto showPage = [&](LocalTools::UI::NavItem item) {
        sidebar->setCurrentItem(item);
        stack->setCurrentIndex(static_cast<int>(item));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    };

    showPage(LocalTools::UI::NavItem::Home);
    if (!saveWindow(window, QDir(outputDir).filePath(QStringLiteral("01-home.png")))) return 8;

    showPage(LocalTools::UI::NavItem::Organize);
    organize->setActiveTab(1);
    organize->loadPdfFiles({samplePdf});
    auto* grid = organize->findChild<LocalTools::UI::ThumbnailGridWidget*>();
    if (!grid || !waitUntil([grid]() { return grid->getPageCount() > 0; })) return 9;
    if (!saveWindow(window, QDir(outputDir).filePath(QStringLiteral("02-pdf-organize.png")))) return 10;

    showPage(LocalTools::UI::NavItem::Images);
    images->setMode(LocalTools::UI::ImageTabMode::CompressMode);
    images->loadImages({sampleImage});
    if (!waitUntil([images]() {
            for (QLabel* label : images->findChildren<QLabel*>()) {
                if (label->text().contains(QStringLiteral("1920"))
                    && label->text().contains(QStringLiteral("1080"))) return true;
            }
            return false;
        })) return 11;
    if (!saveWindow(window, QDir(outputDir).filePath(QStringLiteral("03-image-compress.png")))) return 12;

    showPage(LocalTools::UI::NavItem::Metadata);
    metadata->setMode(LocalTools::UI::MetadataMode::View);
    metadata->loadPdfFile({samplePdf});
    auto* table = metadata->findChild<QTableWidget*>();
    if (!table || !waitUntil([table]() { return table->isVisible() && table->rowCount() > 0; })) return 13;
    if (!saveWindow(window, QDir(outputDir).filePath(QStringLiteral("04-metadata.png")))) return 14;

    window.close();
    std::cout << "Store screenshots generated in " << outputDir.toStdString() << '\n';
    return 0;
}
