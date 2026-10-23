#include "tool_card.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QEvent>

#include "icon_utils.h"
#include "../theme.h"
#include "../../core/tool_registry.h"

namespace LocalTools::UI {

ToolCard::ToolCard(const Core::ToolDescriptor* tool, QWidget* parent)
    : QFrame(parent), m_tool(tool) {
    setObjectName("ToolCard");
    setCursor(Qt::PointingHandCursor);
    // Keyboard reachable: the stylesheet paints the focus ring on QFrame#ToolCard:focus.
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(112);
    // The grid decides the width; the card must not push back or a wrapped
    // description would widen the column past the viewport.
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(Theme::space(Space::M), Theme::space(Space::S),
                             Theme::space(Space::M), Theme::space(Space::S));
    root->setSpacing(Theme::space(Space::XS));

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(Theme::space(Space::XS));

    m_icon = new QLabel(this);
    m_icon->setFixedSize(24, 24);
    topRow->addWidget(m_icon, 0, Qt::AlignTop);
    topRow->addStretch();

    m_favoriteBtn = new QPushButton(this);
    m_favoriteBtn->setObjectName("IconButton");
    m_favoriteBtn->setFixedSize(Theme::MinTargetSize, Theme::MinTargetSize);
    m_favoriteBtn->setIconSize(QSize(16, 16));
    m_favoriteBtn->setCursor(Qt::PointingHandCursor);
    m_favoriteBtn->setFocusPolicy(Qt::StrongFocus);
    connect(m_favoriteBtn, &QPushButton::clicked, this, [this]() {
        setFavorite(!m_favorite);
        emit favoriteToggled(toolId(), m_favorite);
    });
    topRow->addWidget(m_favoriteBtn, 0, Qt::AlignTop);

    root->addLayout(topRow);

    m_name = new QLabel(this);
    m_name->setObjectName("ToolCardName");
    m_name->setWordWrap(true);
    // Ignored horizontally: a wrapped QLabel otherwise reports a minimum width
    // wide enough for its longest line, which is what made the grid overflow.
    m_name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    root->addWidget(m_name);

    m_description = new QLabel(this);
    m_description->setObjectName("ToolCardDescription");
    m_description->setWordWrap(true);
    m_description->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    root->addWidget(m_description);

    root->addStretch();

    retranslateUi();
}

QString ToolCard::toolId() const {
    return m_tool ? m_tool->id : QString();
}

void ToolCard::setFavorite(bool favorite) {
    m_favorite = favorite;
    refreshIcons();
    if (m_favoriteBtn) {
        const QString label = favorite ? tr("Remove from favorites") : tr("Add to favorites");
        m_favoriteBtn->setToolTip(label);
        // Icon-only control: without this the button is unnamed to screen readers.
        m_favoriteBtn->setAccessibleName(label);
    }
}

void ToolCard::refreshIcons() {
    if (m_icon && m_tool) {
        m_icon->setPixmap(IconUtils::getPixmap(m_tool->iconType,
                                               Theme::color(ColorToken::Accent), 24));
    }
    if (m_favoriteBtn) {
        m_favoriteBtn->setIcon(IconUtils::getIcon(
            m_favorite ? IconType::StarFilled : IconType::Star,
            m_favorite ? Theme::color(ColorToken::WarningText)
                       : Theme::color(ColorToken::TextTertiary),
            16));
    }
}

void ToolCard::retranslateUi() {
    if (!m_tool) return;

    m_name->setText(m_tool->name);
    m_description->setText(m_tool->description);

    // The card itself is the control; give it the tool name so assistive tech
    // announces something more useful than "frame".
    setAccessibleName(m_tool->name);
    setAccessibleDescription(m_tool->description);
    setToolTip(m_tool->description);

    setFavorite(m_favorite);
}

void ToolCard::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        emit activated(toolId());
    }
    QFrame::mouseReleaseEvent(event);
}

void ToolCard::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Space) {
        emit activated(toolId());
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

void ToolCard::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    } else if (event->type() == QEvent::PaletteChange) {
        refreshIcons();
    }
    QFrame::changeEvent(event);
}

} // namespace LocalTools::UI
