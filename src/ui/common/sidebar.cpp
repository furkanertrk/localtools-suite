#include "sidebar.h"
#include "icon_utils.h"

#include <QLabel>
#include <QFrame>
#include <QEvent>
#include "../../services/settings_service.h"
#include "../theme.h"

namespace LocalTools::UI {

SidebarWidget::SidebarWidget(QWidget *parent)
    : QWidget(parent) {
    setObjectName("Sidebar");
    setFixedWidth(220);
    setupUi();
}

void SidebarWidget::setupUi() {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(Theme::space(Space::S), Theme::space(Space::M),
                                 Theme::space(Space::S), Theme::space(Space::M));
    m_layout->setSpacing(Theme::space(Space::XXS));

    // App Branding Header (Icon + LocalTools Suite Title)
    auto *brandLayout = new QHBoxLayout();
    brandLayout->setContentsMargins(6, 2, 6, 12);
    brandLayout->setSpacing(8);
    brandLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *brandIcon = new QLabel(this);
    brandIcon->setFixedSize(26, 26);
    QIcon appIcon(":/icons/icons/LocalTools.ico");
    brandIcon->setPixmap(appIcon.pixmap(24, 24));
    brandIcon->setStyleSheet("background: transparent; border: none;");
    brandIcon->setAlignment(Qt::AlignCenter);

    m_brandLabel = new QLabel("LocalTools Suite", this);
    m_brandLabel->setFont(Theme::font(FontRole::BodyLarge));

    brandLayout->addWidget(brandIcon);
    brandLayout->addWidget(m_brandLabel);
    brandLayout->addStretch();
    m_layout->addLayout(brandLayout);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    // Primary Nav Item
    createNavButton(tr("Home"), NavItem::Home, IconType::Home);

    m_layout->addSpacing(12);

    // Section Divider / Header
    // Pre-uppercased in the source string rather than via text-transform: the
    // Turkish i -> Ii mapping makes CSS uppercasing unsafe for this locale.
    m_categoryLabel = new QLabel(tr("CATEGORIES"), this);
    m_categoryLabel->setObjectName("SidebarSectionLabel");
    m_layout->addWidget(m_categoryLabel);

    createNavButton(tr("Organize"), NavItem::Organize, IconType::Organize);
    createNavButton(tr("Convert"), NavItem::Convert, IconType::Convert);
    createNavButton(tr("Optimize"), NavItem::Optimize, IconType::Optimize);
    createNavButton(tr("Images"), NavItem::Images, IconType::ImagesToPdf);
    createNavButton(tr("Documents"), NavItem::Documents, IconType::View);
    createNavButton(tr("Files"), NavItem::Files, IconType::Folder);
    createNavButton(tr("Video"), NavItem::Video, IconType::PdfToImages);
    createNavButton(tr("Audio"), NavItem::Audio, IconType::Music);
    createNavButton(tr("Quick Tools"), NavItem::QuickTools, IconType::Compress);
    createNavButton(tr("Metadata"), NavItem::Metadata, IconType::Metadata);

    m_layout->addStretch();

    // Divider
    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("color: palette(mid); margin: 6px 0;");
    m_layout->addWidget(line);

    createNavButton(tr("Settings"), NavItem::Settings, IconType::Settings);

    m_layout->addSpacing(8);

    // Bottom Offline Privacy Badge
    auto *privacyCard = new QFrame(this);
    privacyCard->setObjectName("PrivacyBadge");
    auto *privacyLayout = new QHBoxLayout(privacyCard);
    privacyLayout->setContentsMargins(8, 8, 8, 8);
    privacyLayout->setSpacing(8);

    m_privacyIcon = new QLabel(privacyCard);
    m_privacyIcon->setFixedSize(20, 20);
    m_privacyIcon->setAlignment(Qt::AlignCenter);

    m_privacyText = new QLabel(tr("Files are processed locally on your device."), privacyCard);
    m_privacyText->setObjectName("PrivacyBadgeText");
    m_privacyText->setWordWrap(true);

    privacyLayout->addWidget(m_privacyIcon, 0, Qt::AlignTop);
    privacyLayout->addWidget(m_privacyText, 1);
    m_layout->addWidget(privacyCard);

    // Apply initial theme icons
    Services::AppTheme currentTheme = Services::SettingsService::instance().getTheme();
    if (currentTheme == Services::AppTheme::System) {
        currentTheme = Theme::getSystemColorScheme();
    }
    updateTheme(currentTheme == Services::AppTheme::Dark);

    setCurrentItem(NavItem::Home);
}

QPushButton* SidebarWidget::createNavButton(const QString& text, NavItem item, IconType iconType) {
    auto *btn = new QPushButton(text, this);
    btn->setObjectName("NavButton");
    btn->setIconSize(QSize(18, 18));
    btn->setCheckable(true);
    // Navigation must be keyboard reachable; the stylesheet draws the focus ring.
    btn->setFocusPolicy(Qt::StrongFocus);

    int id = static_cast<int>(item);
    m_buttonGroup->addButton(btn, id);
    m_layout->addWidget(btn);

    m_navButtons.push_back({btn, item, iconType});

    connect(btn, &QPushButton::clicked, this, [this, item]() {
        emit navItemSelected(item);
    });

    return btn;
}

void SidebarWidget::updateTheme(bool isDark) {
    // Icon state colors come from tokens, so both modes stay in sync with the
    // palette instead of being two hand-maintained hex lists.
    const QColor rest     = Theme::color(ColorToken::TextSecondary, isDark);
    const QColor selected = Theme::color(ColorToken::Accent, isDark);
    const QColor hovered  = Theme::color(ColorToken::TextPrimary, isDark);

    if (m_privacyIcon) {
        m_privacyIcon->setPixmap(IconUtils::getPixmap(
            IconType::Success, Theme::color(ColorToken::SuccessText, isDark), 16));
    }

    for (const auto& item : m_navButtons) {
        QIcon icon;
        icon.addPixmap(IconUtils::getPixmap(item.iconType, rest, 18),     QIcon::Normal,   QIcon::Off);
        icon.addPixmap(IconUtils::getPixmap(item.iconType, selected, 18), QIcon::Normal,   QIcon::On);
        icon.addPixmap(IconUtils::getPixmap(item.iconType, hovered, 18),  QIcon::Active,   QIcon::Off);
        icon.addPixmap(IconUtils::getPixmap(item.iconType, selected, 18), QIcon::Active,   QIcon::On);
        icon.addPixmap(IconUtils::getPixmap(item.iconType, hovered, 18),  QIcon::Selected, QIcon::Off);
        icon.addPixmap(IconUtils::getPixmap(item.iconType, selected, 18), QIcon::Selected, QIcon::On);
        item.btn->setIcon(icon);
    }
}

void SidebarWidget::setCurrentItem(NavItem item) {
    int id = static_cast<int>(item);
    if (QAbstractButton* btn = m_buttonGroup->button(id)) {
        btn->setChecked(true);
    }
}

void SidebarWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void SidebarWidget::retranslateUi() {
    m_brandLabel->setText("LocalTools Suite");
    if (m_categoryLabel) m_categoryLabel->setText(tr("CATEGORIES"));
    if (m_privacyText) m_privacyText->setText(tr("Files are processed locally on your device."));

    for (const auto& item : m_navButtons) {
        switch (item.item) {
            case NavItem::Home: item.btn->setText(tr("Home")); break;
            case NavItem::Organize: item.btn->setText(tr("Organize")); break;
            case NavItem::Convert: item.btn->setText(tr("Convert")); break;
            case NavItem::Optimize: item.btn->setText(tr("Optimize")); break;
            case NavItem::Images: item.btn->setText(tr("Images")); break;
            case NavItem::Documents: item.btn->setText(tr("Documents")); break;
            case NavItem::Files: item.btn->setText(tr("Files")); break;
            case NavItem::Video: item.btn->setText(tr("Video")); break;
            case NavItem::Audio: item.btn->setText(tr("Audio")); break;
            case NavItem::QuickTools: item.btn->setText(tr("Quick Tools")); break;
            case NavItem::Metadata: item.btn->setText(tr("Metadata")); break;
            case NavItem::Settings: item.btn->setText(tr("Settings")); break;
            default: break;
        }
    }
}

} // namespace LocalTools::UI
