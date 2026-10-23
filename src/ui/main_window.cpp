#include "main_window.h"

#include <QHBoxLayout>
#include <QApplication>
#include <QGuiApplication>
#include <QStyleHints>

#include "theme.h"
#include "../services/settings_service.h"
#include "../services/logging_service.h"
#include "../core/tool_registry.h"

namespace LocalTools::UI {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setupUi();
    applyTheme(Services::SettingsService::instance().getTheme());

    // Automatically update theme when Windows 11 / OS color scheme changes live
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
        Services::AppTheme currentSetting = Services::SettingsService::instance().getTheme();
        if (currentSetting == Services::AppTheme::System) {
            Services::LoggingService::instance().info("UI", "System ColorScheme changed live. Updating window theme.");
            applyTheme(Services::AppTheme::System);
        }
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        setWindowTitle(tr("LocalTools Suite - Offline File Toolkit"));
        Core::ToolRegistry::instance().retranslateTools();
        if (m_sidebar) {
            QEvent ev(QEvent::LanguageChange);
            QCoreApplication::sendEvent(m_sidebar, &ev);
        }
        if (m_pageStack) {
            for (int i = 0; i < m_pageStack->count(); ++i) {
                if (QWidget* w = m_pageStack->widget(i)) {
                    QEvent ev(QEvent::LanguageChange);
                    QCoreApplication::sendEvent(w, &ev);
                }
            }
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::setupUi() {
    setWindowTitle(tr("LocalTools Suite - Offline File Toolkit"));
    setWindowIcon(QIcon(":/icons/icons/LocalTools.ico"));
    setMinimumSize(700, 480);
    resize(1180, 750);

    auto *centralWidget = new QWidget(this);
    centralWidget->setObjectName("CentralWidget");

    auto *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Primary product navigation on the left.
    m_sidebar = new SidebarWidget(this);
    mainLayout->addWidget(m_sidebar);

    // Stacked widget for pages on the right (full height, no header bar gap)
    m_pageStack = new QStackedWidget(this);
    // A QStackedWidget reports the widest page's minimum as its own, so the
    // single widest tool page would otherwise force every other page - Home
    // included - to overflow the window and clip its right-hand column.
    m_pageStack->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    m_homePage = new HomePage(this);
    m_organizePage = new OrganizePage(this);
    m_convertPage = new ConvertPage(this);
    m_optimizePage = new OptimizePage(this);
    m_metadataPage = new MetadataPage(this);
    m_settingsPage = new SettingsPage(this);
    m_imagesPage = new ImagesPage(this);
    m_documentsPage = new DocumentsPage(this);
    m_filesPage = new FilesPage(this);
    m_quickToolsPage = new QuickToolsPage(this);
    m_videoPage = new VideoPage(this);
    m_audioPage = new AudioPage(this);

    // Keep these indexes aligned with NavItem enum (Home=0, Organize=1, Convert=2, Optimize=3, Metadata=4, Settings=5, AllTools=6, Images=7, Documents=8, Files=9, QuickTools=10, Video=11).
    m_pageStack->addWidget(m_homePage);       // 0
    m_pageStack->addWidget(m_organizePage);   // 1
    m_pageStack->addWidget(m_convertPage);    // 2
    m_pageStack->addWidget(m_optimizePage);   // 3
    m_pageStack->addWidget(m_metadataPage);   // 4
    m_pageStack->addWidget(m_settingsPage);   // 5
    m_pageStack->addWidget(new QWidget(this)); // 6 (Placeholder)
    m_pageStack->addWidget(m_imagesPage);     // 7 (Images)
    m_pageStack->addWidget(m_documentsPage);  // 8 (Documents)
    m_pageStack->addWidget(m_filesPage);      // 9 (Files)
    m_pageStack->addWidget(m_quickToolsPage); // 10 (QuickTools)
    m_pageStack->addWidget(m_videoPage);      // 11 (Video)
    m_pageStack->addWidget(m_audioPage);      // 12 (Audio)

    mainLayout->addWidget(m_pageStack, 1);
    setCentralWidget(centralWidget);

    // Connect HomePage Theme Toggle Button
    connect(m_homePage, &HomePage::themeToggleRequested, this, [this]() {
        Services::AppTheme current = Services::SettingsService::instance().getTheme();
        if (current == Services::AppTheme::System) {
            current = Theme::getSystemColorScheme();
        }
        Services::AppTheme next = (current == Services::AppTheme::Dark) ? Services::AppTheme::Light : Services::AppTheme::Dark;
        Services::SettingsService::instance().setTheme(next);
        applyTheme(next);
    });

    // Connect Settings clear recent files to HomePage refresh
    connect(m_settingsPage, &SettingsPage::recentFilesCleared, m_homePage, &HomePage::refreshRecentFiles);

    // Images dropped on the Metadata page no longer bounce to Quick Tools: the
    // Inspect & Clean tab handles them on the same page.

    // Connect Sidebar item selection to Page Stack
    connect(m_sidebar, &SidebarWidget::navItemSelected, this, [this](NavItem item) {
        int index = static_cast<int>(item);
        if (index == 0 && m_homePage) {
            m_homePage->refreshRecentFiles();
        }
        m_pageStack->setCurrentIndex(index);
        Services::LoggingService::instance().info("UI", "Navigated via sidebar to item: " + std::to_string(static_cast<int>(item)));
    });

    // Home cards are shortcuts into the same category pages exposed by the sidebar.
    connect(m_homePage, &HomePage::toolSelected, this, [this](int toolId) {
        Services::LoggingService::instance().info("UI", "Selected tool ID: " + std::to_string(toolId));
        if (toolId == 7) { // Images to PDF
            m_convertPage->setMode(ConvertMode::ImagesToPdf);
            m_sidebar->setCurrentItem(NavItem::Convert);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Convert));
        } else if (toolId == 8) { // PDF'ten Imagese
            m_convertPage->setMode(ConvertMode::PdfToImages);
            m_sidebar->setCurrentItem(NavItem::Convert);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Convert));
        } else if (toolId == 9) { // Metin Çıkar
            m_convertPage->setMode(ConvertMode::PdfToText);
            m_sidebar->setCurrentItem(NavItem::Convert);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Convert));
        } else if (toolId == 10) { // Compress
            m_sidebar->setCurrentItem(NavItem::Optimize);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Optimize));
        } else if (toolId == 11) { // Metadata
            m_metadataPage->setMode(MetadataMode::View);
            m_sidebar->setCurrentItem(NavItem::Metadata);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Metadata));
        } else if (toolId == 101) { // Görsel Compress
            m_imagesPage->setMode(ImageTabMode::CompressMode);
            m_sidebar->setCurrentItem(NavItem::Images);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Images));
        } else if (toolId == 102) { // Görsel Convert
            m_imagesPage->setMode(ImageTabMode::Convert);
            m_sidebar->setCurrentItem(NavItem::Images);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Images));
        } else if (toolId == 104) { // Metadata Görüntüle
            m_metadataPage->setMode(MetadataMode::View);
            m_sidebar->setCurrentItem(NavItem::Metadata);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Metadata));
        } else if (toolId == 404) { // Metadata Temizle
            m_metadataPage->setMode(MetadataMode::Clean);
            m_sidebar->setCurrentItem(NavItem::Metadata);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Metadata));
        } else if (toolId == 201) { // ID Card Fotokopisi
            m_documentsPage->setMode(DocumentTabMode::IdCopy);
            m_sidebar->setCurrentItem(NavItem::Documents);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Documents));
        } else if (toolId == 202) { // Passport Fotoğraf
            m_documentsPage->setMode(DocumentTabMode::PassportPhoto);
            m_sidebar->setCurrentItem(NavItem::Documents);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Documents));
        } else if (toolId == 302) { // Hash & Verification
            m_filesPage->setMode(FilesTabMode::HashCheck);
            m_sidebar->setCurrentItem(NavItem::Files);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Files));
        } else if (toolId == 401) { // QR Kod Generator
            m_quickToolsPage->setMode(QuickTabMode::QrCode);
            m_sidebar->setCurrentItem(NavItem::QuickTools);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::QuickTools));
        } else if (toolId == 405) { // QR Kod Reader
            m_quickToolsPage->setMode(QuickTabMode::QrReader);
            m_sidebar->setCurrentItem(NavItem::QuickTools);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::QuickTools));
        } else if (toolId == 501) { // Video Compress
            m_videoPage->setMode(VideoTabMode::Compress);
            m_sidebar->setCurrentItem(NavItem::Video);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Video));
        } else if (toolId == 502) { // GIF'e Convert
            m_videoPage->setMode(VideoTabMode::ToGif);
            m_sidebar->setCurrentItem(NavItem::Video);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Video));
        } else if (toolId == 503) { // Audio Çıkar
            m_videoPage->setMode(VideoTabMode::ExtractAudio);
            m_sidebar->setCurrentItem(NavItem::Video);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Video));
        } else if (toolId == 504) { // Video Kes
            m_videoPage->setMode(VideoTabMode::Trim);
            m_sidebar->setCurrentItem(NavItem::Video);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Video));
        } else if (toolId == 601) { // Audio Convert
            m_audioPage->setMode(AudioPage::Mode::Convert);
            m_sidebar->setCurrentItem(NavItem::Audio);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Audio));
        } else if (toolId == 602) { // Compress Audio
            m_audioPage->setMode(AudioPage::Mode::Compress);
            m_sidebar->setCurrentItem(NavItem::Audio);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Audio));
        } else if (toolId == 603) { // Trim Audio
            m_audioPage->setMode(AudioPage::Mode::Trim);
            m_sidebar->setCurrentItem(NavItem::Audio);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Audio));
        } else {
            m_organizePage->setActiveTab(toolId);
            m_sidebar->setCurrentItem(NavItem::Organize);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Organize));
        }
    });

    // Connect Quick Action Sheet from Home to launch specific tool with pre-loaded files
    connect(m_homePage, &HomePage::toolRequestedWithFiles, this, [this](int toolId, const QStringList& files) {
        Services::LoggingService::instance().info("UI", "Tool requested from Home action sheet with files, tool ID: " + std::to_string(toolId));
        if (toolId == 8) { // PDF'ten Imagese
            m_convertPage->setMode(ConvertMode::PdfToImages);
            m_convertPage->loadFiles(files);
            m_sidebar->setCurrentItem(NavItem::Convert);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Convert));
        } else if (toolId == 9) { // PDF to Text
            m_convertPage->setMode(ConvertMode::PdfToText);
            m_convertPage->loadFiles(files);
            m_sidebar->setCurrentItem(NavItem::Convert);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Convert));
        } else if (toolId == 10) { // Compress
            m_optimizePage->loadPdfFile(files);
            m_sidebar->setCurrentItem(NavItem::Optimize);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Optimize));
        } else if (toolId == 11) { // Metadata
            m_metadataPage->setMode(MetadataMode::View);
            m_metadataPage->loadPdfFile(files);
            m_sidebar->setCurrentItem(NavItem::Metadata);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Metadata));
        } else if (toolId == 101) { // Görsel Compress
            m_imagesPage->setMode(ImageTabMode::CompressMode);
            m_imagesPage->loadImages(files);
            m_sidebar->setCurrentItem(NavItem::Images);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Images));
        } else if (toolId == 102) { // Görsel Convert
            m_imagesPage->setMode(ImageTabMode::Convert);
            m_imagesPage->loadImages(files);
            m_sidebar->setCurrentItem(NavItem::Images);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Images));
        } else if (toolId == 104) { // Metadata Görüntüle
            m_metadataPage->setMode(MetadataMode::View);
            m_metadataPage->loadPdfFile(files);
            m_sidebar->setCurrentItem(NavItem::Metadata);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Metadata));
        } else if (toolId == 404) { // Metadata Temizle
            if (!files.isEmpty()) {
                m_metadataPage->loadMetadataFile(files.first());
            } else {
                m_metadataPage->setMode(MetadataMode::Clean);
            }
            m_sidebar->setCurrentItem(NavItem::Metadata);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Metadata));
        } else if (toolId == 201 || toolId == 202) {
            if (toolId == 201) m_documentsPage->setMode(DocumentTabMode::IdCopy);
            else if (toolId == 202) m_documentsPage->setMode(DocumentTabMode::PassportPhoto);
            m_sidebar->setCurrentItem(NavItem::Documents);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Documents));
        } else { // Organize tools
            m_organizePage->loadPdfFiles(files);
            m_organizePage->setActiveTab(toolId);
            m_sidebar->setCurrentItem(NavItem::Organize);
            m_pageStack->setCurrentIndex(static_cast<int>(NavItem::Organize));
        }
    });

    // Connect Settings theme change
    connect(m_settingsPage, &SettingsPage::themeChanged, this, &MainWindow::applyTheme);
}

void MainWindow::applyTheme(Services::AppTheme theme) {
    // Cached pixmaps are tinted for the outgoing palette; drop them before any
    // widget rebuilds its icons, otherwise the old tint is reused.
    IconUtils::clearCache();

    Theme::applyThemeToWindow(this, theme);

    Services::AppTheme effective = theme;
    if (theme == Services::AppTheme::System) {
        effective = Theme::getSystemColorScheme();
    }
    bool isDark = (effective == Services::AppTheme::Dark);
    if (m_homePage) {
        m_homePage->updateThemeIcon(isDark);
    }
    if (m_sidebar) {
        m_sidebar->updateTheme(isDark);
    }
    if (m_organizePage) {
        m_organizePage->refreshThemeIcons();
    }

    // Keep the Settings combo in step when the theme is changed from Home,
    // otherwise the dropdown keeps showing the previous selection.
    if (m_settingsPage) {
        m_settingsPage->syncThemeSelection(theme);
    }
}

} // namespace LocalTools::UI
