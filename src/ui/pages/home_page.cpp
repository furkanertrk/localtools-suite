#include "home_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QScrollArea>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <algorithm>

#include "../../services/settings_service.h"
#include "../../services/file_service.h"
#include "../../core/backends/image_backend.h"
#include "../../core/tool_registry.h"
#include "../theme.h"

namespace LocalTools::UI {

namespace {
constexpr int kMinCardWidth = 236;
constexpr int kMaxColumns = 4;
constexpr int kRecentFileLimit = 5;
} // namespace

HomePage::HomePage(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

void HomePage::setupUi() {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *contentWidget = new QWidget();
    m_contentLayout = new QVBoxLayout(contentWidget);
    const int pad = Theme::space(Space::L);
    m_contentLayout->setContentsMargins(pad, pad, pad, pad);
    m_contentLayout->setSpacing(Theme::space(Space::L));

    // ---- Header ----
    m_header = new PageHeader(contentWidget);

    m_themeToggleBtn = new QPushButton();
    m_themeToggleBtn->setObjectName("IconButton");
    m_themeToggleBtn->setFixedSize(36, 36);
    m_themeToggleBtn->setIconSize(QSize(20, 20));
    m_themeToggleBtn->setFocusPolicy(Qt::StrongFocus);
    m_themeToggleBtn->setCursor(Qt::PointingHandCursor);
    connect(m_themeToggleBtn, &QPushButton::clicked, this, &HomePage::themeToggleRequested);
    m_header->addAction(m_themeToggleBtn);

    m_contentLayout->addWidget(m_header);

    // ---- Search ----
    m_search = new SearchField(contentWidget);
    connect(m_search, &SearchField::searchChanged, this, &HomePage::applySearch);
    m_contentLayout->addWidget(m_search);

    // ---- Search results (hidden until the user types) ----
    m_results = createSection(m_contentLayout, QString());
    m_results.container->hide();

    m_noResults = new QWidget(contentWidget);
    auto *noResLayout = new QVBoxLayout(m_noResults);
    noResLayout->setContentsMargins(0, Theme::space(Space::XL), 0, Theme::space(Space::XL));
    noResLayout->setSpacing(Theme::space(Space::XS));
    m_noResultsTitle = new QLabel(m_noResults);
    m_noResultsTitle->setObjectName("EmptyStateTitle");
    m_noResultsTitle->setAlignment(Qt::AlignCenter);
    m_noResultsBody = new QLabel(m_noResults);
    m_noResultsBody->setObjectName("EmptyStateBody");
    m_noResultsBody->setAlignment(Qt::AlignCenter);
    m_noResultsBody->setWordWrap(true);
    noResLayout->addWidget(m_noResultsTitle);
    noResLayout->addWidget(m_noResultsBody);
    m_noResults->hide();
    m_contentLayout->addWidget(m_noResults);

    // ---- Favourites + per-category grids ----
    buildToolSections();

    // ---- Drop zone & quick actions ----
    m_dropZone = new DropZoneWidget(contentWidget);
    m_dropZone->setAcceptedFileExtensions({"pdf", "jpg", "jpeg", "png", "webp", "bmp", "tiff", "tif"},
                                          tr("Select File"),
                                          tr("PDF or Image Files Accepted"));
    connect(m_dropZone, &DropZoneWidget::filesDropped, this, &HomePage::handleFilesDropped);
    m_contentLayout->addWidget(m_dropZone);

    m_actionSheetContainer = createQuickActionSheet();
    m_actionSheetContainer->hide();
    m_contentLayout->addWidget(m_actionSheetContainer);

    // ---- Recent files ----
    m_recentSection = createRecentFilesSection();
    m_contentLayout->addWidget(m_recentSection);

    m_contentLayout->addStretch();

    scrollArea->setWidget(contentWidget);
    rootLayout->addWidget(scrollArea);

    retranslateUi();
    relayoutAllSections();
}

// ---------------------------------------------------------------------------
// Sections
// ---------------------------------------------------------------------------

HomePage::CardSection HomePage::createSection(QVBoxLayout* parent, const QString& headingText) {
    CardSection section;

    section.container = new QWidget();
    auto* layout = new QVBoxLayout(section.container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Theme::space(Space::S));

    auto* headingRow = new QHBoxLayout();
    headingRow->setContentsMargins(0, 0, 0, 0);
    headingRow->setSpacing(Theme::space(Space::XS));

    section.heading = new QLabel(headingText, section.container);
    section.heading->setObjectName("CategoryHeading");
    headingRow->addWidget(section.heading);

    section.count = new QLabel(section.container);
    section.count->setObjectName("CategoryCount");
    headingRow->addWidget(section.count);
    headingRow->addStretch();

    layout->addLayout(headingRow);

    section.grid = new QGridLayout();
    section.grid->setHorizontalSpacing(Theme::space(Space::S));
    section.grid->setVerticalSpacing(Theme::space(Space::S));
    layout->addLayout(section.grid);

    parent->addWidget(section.container);
    return section;
}

void HomePage::buildToolSections() {
    auto& registry = Core::ToolRegistry::instance();

    // Favourites first: the whole point of pinning is a zero-scroll path back.
    m_favorites = createSection(m_contentLayout, tr("Favorites"));
    populateSection(m_favorites, registry.favorites());

    const QStringList categories = registry.categories();
    m_categories.reserve(static_cast<size_t>(categories.size()));
    for (const QString& category : categories) {
        CardSection section = createSection(m_contentLayout, category);
        populateSection(section, registry.toolsByCategory(category));
        m_categories.push_back(section);
    }
}

void HomePage::populateSection(CardSection& section, const std::vector<const Core::ToolDescriptor*>& tools) {
    for (ToolCard* card : section.cards) {
        section.grid->removeWidget(card);
        card->deleteLater();
    }
    section.cards.clear();

    auto& registry = Core::ToolRegistry::instance();
    for (const Core::ToolDescriptor* tool : tools) {
        // Unimplemented tools are not advertised: a launcher tile the user
        // cannot act on reads as a broken feature, not as a roadmap.
        if (!tool->isAvailable) continue;

        auto* card = new ToolCard(tool, section.container);
        card->setFavorite(registry.isFavorite(tool->id));
        connectCard(card);
        section.cards.push_back(card);
    }

    section.count->setText(section.cards.empty()
                               ? QString()
                               : QStringLiteral("%1").arg(section.cards.size()));
    section.container->setVisible(!section.cards.empty());
    layoutSection(section, m_currentColumns);
}

void HomePage::connectCard(ToolCard* card) {
    connect(card, &ToolCard::activated, this, &HomePage::activateTool);
    connect(card, &ToolCard::favoriteToggled, this,
            [this](const QString& toolId, bool favorite) {
                Core::ToolRegistry::instance().setFavorite(toolId, favorite);

                // Mirror the new state onto every other card showing this tool,
                // so the category grid and the favourites row never disagree.
                const auto syncSection = [&](CardSection& section) {
                    for (ToolCard* other : section.cards) {
                        if (other->toolId() == toolId && other->isFavorite() != favorite) {
                            other->setFavorite(favorite);
                        }
                    }
                };
                for (CardSection& section : m_categories) syncSection(section);
                syncSection(m_results);

                populateSection(m_favorites, Core::ToolRegistry::instance().favorites());
                retranslateUi();
            });
}

void HomePage::activateTool(const QString& toolId) {
    const Core::ToolDescriptor* tool = Core::ToolRegistry::instance().toolById(toolId);
    if (!tool || !tool->isAvailable) return;

    if (tool->legacyToolId >= 0) {
        emit toolSelected(tool->legacyToolId);
    }
}

void HomePage::layoutSection(CardSection& section, int columns) {
    if (!section.grid) return;

    for (ToolCard* card : section.cards) {
        section.grid->removeWidget(card);
    }
    for (int c = 0; c < kMaxColumns; ++c) {
        section.grid->setColumnStretch(c, c < columns ? 1 : 0);
    }
    for (size_t i = 0; i < section.cards.size(); ++i) {
        const int row = static_cast<int>(i) / columns;
        const int col = static_cast<int>(i) % columns;
        section.grid->addWidget(section.cards[i], row, col);
    }
}

int HomePage::columnsForWidth() const {
    const int available = width() - 2 * Theme::space(Space::L);
    return std::max(1, std::min(kMaxColumns, available / kMinCardWidth));
}

void HomePage::relayoutAllSections() {
    const int columns = columnsForWidth();
    if (columns == m_currentColumns && !m_categories.empty()) {
        return;
    }
    m_currentColumns = columns;

    layoutSection(m_favorites, columns);
    layoutSection(m_results, columns);
    for (CardSection& section : m_categories) {
        layoutSection(section, columns);
    }
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

void HomePage::applySearch(const QString& query) {
    const QString trimmed = query.trimmed();

    if (trimmed.isEmpty()) {
        setBrowseMode(true);
        m_results.container->hide();
        m_noResults->hide();
        return;
    }

    setBrowseMode(false);

    const auto matches = Core::ToolRegistry::instance().search(trimmed);
    populateSection(m_results, matches);
    layoutSection(m_results, m_currentColumns);

    const bool hasMatches = !matches.empty();
    m_results.container->setVisible(hasMatches);
    m_noResults->setVisible(!hasMatches);

    if (hasMatches) {
        m_results.heading->setText(tr("Search results"));
    } else {
        // A dead end with no way forward is the worst search outcome; name the
        // query back and suggest what does work.
        m_noResultsTitle->setText(tr("No tools match \"%1\"").arg(trimmed));
        m_noResultsBody->setText(
            tr("Try a file extension such as \"pdf\" or \"jpg\", or a task word "
               "such as \"compress\", \"convert\" or \"merge\"."));
    }
}

void HomePage::setBrowseMode(bool browsing) {
    m_favorites.container->setVisible(browsing && !m_favorites.cards.empty());
    for (CardSection& section : m_categories) {
        section.container->setVisible(browsing && !section.cards.empty());
    }
    if (m_recentSection) m_recentSection->setVisible(browsing);

    // The drop zone stays hidden while the quick action sheet is up, so only
    // restore it when no drop is currently being resolved.
    const bool sheetVisible = m_actionSheetContainer && m_actionSheetContainer->isVisible();
    if (m_dropZone) m_dropZone->setVisible(browsing && !sheetVisible);
}

// ---------------------------------------------------------------------------
// Layout / lifecycle
// ---------------------------------------------------------------------------

void HomePage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    relayoutAllSections();
}

void HomePage::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        // ToolRegistry rebuilds its descriptors on a language change, so the
        // pointers held by existing cards are dangling - rebuild the grids.
        for (CardSection& section : m_categories) {
            for (ToolCard* card : section.cards) {
                section.grid->removeWidget(card);
                card->deleteLater();
            }
            section.cards.clear();
            section.container->deleteLater();
        }
        m_categories.clear();

        for (ToolCard* card : m_results.cards) card->deleteLater();
        m_results.cards.clear();
        for (ToolCard* card : m_favorites.cards) card->deleteLater();
        m_favorites.cards.clear();
        m_favorites.container->deleteLater();

        buildToolSections();
        retranslateUi();
        relayoutAllSections();
    }
    QWidget::changeEvent(event);
}

void HomePage::retranslateUi() {
    if (m_header) {
        m_header->setText(tr("Tools"),
                          tr("Fast, secure, and fully local desktop tools. "
                             "All processing happens on your computer."));
    }
    if (m_search) {
        m_search->setPlaceholderText(
            tr("Search tools — e.g. merge PDF, compress image, QR, hash"));
        m_search->setAccessibleName(tr("Search tools"));
    }
    if (m_favorites.heading) m_favorites.heading->setText(tr("Favorites"));
    if (m_results.heading) m_results.heading->setText(tr("Search results"));
    if (m_recentSectionTitle) m_recentSectionTitle->setText(tr("Recent Files"));
    if (m_dropZone) {
        m_dropZone->setInstruction(tr("Drop a PDF or Image File Here for Quick Actions"),
                                   tr("or click to select file"));
    }
    if (m_promptLabel) {
        m_promptLabel->setText(tr("What would you like to do with this file?"));
    }
}

void HomePage::updateThemeIcon(bool isDark) {
    if (!m_themeToggleBtn) return;

    const IconType icon = isDark ? IconType::Sun : IconType::Moon;
    const QString label = isDark ? tr("Switch to Light Mode") : tr("Switch to Dark Mode");

    m_themeToggleBtn->setIcon(IconUtils::getIcon(icon, Theme::color(ColorToken::TextSecondary), 20));
    m_themeToggleBtn->setToolTip(label);
    // Icon-only control: without this it is unnamed to assistive technology.
    m_themeToggleBtn->setAccessibleName(label);

    if (m_search) m_search->refreshIcon();
}

// ---------------------------------------------------------------------------
// Drop handling
// ---------------------------------------------------------------------------

void HomePage::handleFilesDropped(const QStringList& files) {
    if (files.isEmpty()) return;
    m_droppedFiles = files;

    const QString firstFile = files.first();
    const QFileInfo fi(firstFile);
    const bool isImage = Core::ImageBackend::isImageFile(firstFile);

    QString labelText;
    if (files.size() == 1) {
        labelText = isImage ? tr("Selected Image: %1").arg(fi.fileName())
                            : tr("Selected Document: %1").arg(fi.fileName());
    } else {
        labelText = isImage ? tr("%1 image file(s) selected").arg(files.size())
                            : tr("%1 PDF file(s) selected").arg(files.size());
    }

    m_actionSheetFileLabel->setText(labelText);
    m_promptLabel->setText(isImage ? tr("What would you like to do with this image?")
                                   : tr("What would you like to do with this PDF?"));

    if (m_pdfActionsWidget && m_imageActionsWidget) {
        m_pdfActionsWidget->setVisible(!isImage);
        m_imageActionsWidget->setVisible(isImage);
    }

    m_dropZone->hide();
    m_actionSheetContainer->show();
}

void HomePage::hideActionSheet() {
    m_actionSheetContainer->hide();
    m_dropZone->show();
    m_droppedFiles.clear();
}

QWidget* HomePage::createQuickActionSheet() {
    auto *container = new QFrame();
    container->setObjectName("QuickActionSheet");

    auto *layout = new QVBoxLayout(container);
    const int padX = Theme::space(Space::M);
    const int padY = Theme::space(Space::S);
    layout->setContentsMargins(padX, padY, padX, padY);
    layout->setSpacing(Theme::space(Space::S));

    auto *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);

    m_actionSheetFileLabel = new QLabel(container);
    m_actionSheetFileLabel->setObjectName("DroppedFileChip");
    headerRow->addWidget(m_actionSheetFileLabel);
    headerRow->addStretch();

    auto *cancelBtn = new QPushButton(tr("Cancel"), container);
    cancelBtn->setObjectName("SecondaryButton");
    cancelBtn->setIcon(IconUtils::getIcon(IconType::Close, Theme::color(ColorToken::TextSecondary), 14));
    cancelBtn->setIconSize(QSize(14, 14));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(cancelBtn);
    connect(cancelBtn, &QPushButton::clicked, this, &HomePage::hideActionSheet);

    layout->addLayout(headerRow);

    m_promptLabel = new QLabel(tr("What would you like to do with this file?"), container);
    m_promptLabel->setObjectName("PageSubtitle");
    layout->addWidget(m_promptLabel);

    auto createActionBtn = [this](QWidget* parentWidget, QHBoxLayout* targetLayout,
                                  const QString& text, IconType iconType, int toolId) {
        auto *btn = new QPushButton(text, parentWidget);
        btn->setObjectName("SecondaryButton");
        btn->setIcon(IconUtils::getIcon(iconType, Theme::color(ColorToken::Accent), 16));
        btn->setIconSize(QSize(16, 16));
        btn->setCursor(Qt::PointingHandCursor);
        targetLayout->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, toolId]() {
            QStringList filesToPass = m_droppedFiles;
            hideActionSheet();
            emit toolRequestedWithFiles(toolId, filesToPass);
        });
    };

    m_pdfActionsWidget = new QWidget(container);
    auto *pdfLayout = new QHBoxLayout(m_pdfActionsWidget);
    pdfLayout->setContentsMargins(0, 0, 0, 0);
    pdfLayout->setSpacing(Theme::space(Space::XS));
    createActionBtn(m_pdfActionsWidget, pdfLayout, tr("Organize Pages"), IconType::Organize, 1);
    createActionBtn(m_pdfActionsWidget, pdfLayout, tr("Compress"), IconType::Compress, 10);
    createActionBtn(m_pdfActionsWidget, pdfLayout, tr("Convert to Images"), IconType::PdfToImages, 8);
    createActionBtn(m_pdfActionsWidget, pdfLayout, tr("Convert to Text"), IconType::PdfToText, 9);
    createActionBtn(m_pdfActionsWidget, pdfLayout, tr("Metadata"), IconType::Metadata, 11);
    pdfLayout->addStretch();
    layout->addWidget(m_pdfActionsWidget);

    m_imageActionsWidget = new QWidget(container);
    auto *imgLayout = new QHBoxLayout(m_imageActionsWidget);
    imgLayout->setContentsMargins(0, 0, 0, 0);
    imgLayout->setSpacing(Theme::space(Space::XS));
    createActionBtn(m_imageActionsWidget, imgLayout, tr("Compress Image"), IconType::Compress, 101);
    createActionBtn(m_imageActionsWidget, imgLayout, tr("Convert"), IconType::Convert, 102);
    createActionBtn(m_imageActionsWidget, imgLayout, tr("Strip EXIF"), IconType::Delete, 104);
    imgLayout->addStretch();
    m_imageActionsWidget->hide();
    layout->addWidget(m_imageActionsWidget);

    return container;
}

// ---------------------------------------------------------------------------
// Recent files
// ---------------------------------------------------------------------------

QWidget* HomePage::createRecentFilesSection() {
    auto *container = new QWidget();
    auto *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(Theme::space(Space::S));

    m_recentSectionTitle = new QLabel(tr("Recent Files"), container);
    m_recentSectionTitle->setObjectName("CategoryHeading");
    mainLayout->addWidget(m_recentSectionTitle);

    auto *cardFrame = new QFrame(container);
    cardFrame->setObjectName("CardFrame");
    m_recentFilesLayout = new QVBoxLayout(cardFrame);
    const int padX = Theme::space(Space::S);
    m_recentFilesLayout->setContentsMargins(padX, padX, padX, padX);
    m_recentFilesLayout->setSpacing(Theme::space(Space::XS));

    mainLayout->addWidget(cardFrame);
    refreshRecentFiles();

    return container;
}

void HomePage::refreshRecentFiles() {
    if (!m_recentFilesLayout) return;

    QLayoutItem *child;
    while ((child = m_recentFilesLayout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }

    const QStringList recentFiles = Services::SettingsService::instance().getRecentFiles();
    if (recentFiles.isEmpty()) {
        auto *emptyLabel = new QLabel(tr("No recent file history yet."));
        emptyLabel->setObjectName("CaptionLabel");
        emptyLabel->setAlignment(Qt::AlignCenter);
        m_recentFilesLayout->addWidget(emptyLabel);
        return;
    }

    int count = 0;
    for (const QString& filePath : recentFiles) {
        if (count++ >= kRecentFileLimit) break;

        const QFileInfo fi(filePath);
        auto *rowFrame = new QFrame();
        rowFrame->setObjectName("RecentFileRow");
        auto *rowLayout = new QHBoxLayout(rowFrame);
        rowLayout->setContentsMargins(Theme::space(Space::XS), Theme::space(Space::XS),
                                      Theme::space(Space::XS), Theme::space(Space::XS));
        rowLayout->setSpacing(Theme::space(Space::S));

        auto *iconLabel = new QLabel(rowFrame);
        iconLabel->setFixedSize(24, 24);
        iconLabel->setPixmap(IconUtils::getPixmap(IconType::FileText,
                                                  Theme::color(ColorToken::TextSecondary), 20));
        iconLabel->setAlignment(Qt::AlignCenter);
        rowLayout->addWidget(iconLabel);

        auto *textBox = new QVBoxLayout();
        textBox->setSpacing(0);
        auto *nameLabel = new QLabel(fi.fileName(), rowFrame);
        nameLabel->setObjectName("ToolCardName");

        const double sizeMb = fi.exists() ? (fi.size() / (1024.0 * 1024.0)) : 0.0;
        const QString detailsStr = fi.exists()
            ? tr("%1 · %2 MB").arg(fi.suffix().toUpper(), QString::number(sizeMb, 'f', 2))
            : tr("File not found");
        auto *detailsLabel = new QLabel(detailsStr, rowFrame);
        detailsLabel->setObjectName("CaptionLabel");

        textBox->addWidget(nameLabel);
        textBox->addWidget(detailsLabel);
        rowLayout->addLayout(textBox, 1);

        auto *openBtn = new QPushButton(tr("Open"), rowFrame);
        openBtn->setObjectName("SecondaryButton");
        openBtn->setIcon(IconUtils::getIcon(IconType::View, Theme::color(ColorToken::TextSecondary), 14));
        openBtn->setIconSize(QSize(14, 14));
        openBtn->setCursor(Qt::PointingHandCursor);
        openBtn->setEnabled(fi.exists());

        connect(openBtn, &QPushButton::clicked, this, [filePath]() {
            if (QFileInfo::exists(filePath)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
            }
        });

        rowLayout->addWidget(openBtn);
        m_recentFilesLayout->addWidget(rowFrame);
    }
}

} // namespace LocalTools::UI
