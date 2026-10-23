#include "thumbnail_grid.h"
#include "../../core/state/workspace_state.h"

#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <algorithm>

namespace LocalTools::UI {

ThumbnailGridWidget::ThumbnailGridWidget(QWidget *parent)
    : QWidget(parent) {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet("QScrollArea { background-color: transparent; }");

    auto *container = new QWidget(m_scrollArea);
    m_gridLayout = new QGridLayout(container);
    m_gridLayout->setContentsMargins(16, 16, 16, 16);
    m_gridLayout->setHorizontalSpacing(14);
    m_gridLayout->setVerticalSpacing(14);

    m_scrollArea->setWidget(container);
    rootLayout->addWidget(m_scrollArea);

    // Connect to State
    auto& state = Core::State::WorkspaceState::instance();
    connect(&state, &Core::State::WorkspaceState::pageSelectionChanged, this, &ThumbnailGridWidget::onPageSelectionChanged);
    connect(&state, &Core::State::WorkspaceState::allPagesSelectionChanged, this, &ThumbnailGridWidget::onAllPagesSelectionChanged);
    connect(&state, &Core::State::WorkspaceState::pageRotationChanged, this, &ThumbnailGridWidget::onPageRotationChanged);
    connect(&state, &Core::State::WorkspaceState::pagesDeleted, this, &ThumbnailGridWidget::loadFromState);
    connect(&state, &Core::State::WorkspaceState::pagesReordered, this, &ThumbnailGridWidget::loadFromState);
    connect(&state, &Core::State::WorkspaceState::workspaceCleared, this, &ThumbnailGridWidget::clear);
}

void ThumbnailGridWidget::clear() {
    for (auto* card : m_cards) {
        m_gridLayout->removeWidget(card);
        delete card;
    }
    m_cards.clear();
    emit selectionCountChanged(0, 0);
}

void ThumbnailGridWidget::loadFromState() {
    clear();

    const auto& state = Core::State::WorkspaceState::instance();
    auto allPages = state.getAllPages();
    
    m_cards.reserve(allPages.size());

    int displayIndex = 0;
    for (const auto& page : allPages) {
        auto *card = new ThumbnailCardWidget(page.id, displayIndex++, this);
        card->setPageData(page.thumbnail, page.rotation);
        card->setSelected(page.isSelected);
        card->setActionMode(m_actionMode);

        connect(card, &ThumbnailCardWidget::selectionChanged, this, [](const QString& pageId, bool selected) {
            Core::State::WorkspaceState::instance().setPageSelection(pageId, selected);
        });

        connect(card, &ThumbnailCardWidget::rotateRequested, this, [](const QString& pageId, int delta) {
            Core::State::WorkspaceState::instance().rotatePage(pageId, delta);
        });

        connect(card, &ThumbnailCardWidget::deleteRequested, this, [](const QString& pageId) {
            Core::State::WorkspaceState::instance().removePage(pageId);
        });

        m_cards.push_back(card);
    }

    for (size_t i = 0; i < m_cards.size(); ++i) {
        m_cards[i]->setMoveAvailability(i > 0, i + 1 < m_cards.size());
    }

    updateGridColumns();

    int selectedCount = static_cast<int>(state.getSelectedPages().size());
    emit selectionCountChanged(selectedCount, static_cast<int>(m_cards.size()));
}

void ThumbnailGridWidget::onPageSelectionChanged(const QString& pageId, bool isSelected) {
    for (auto* card : m_cards) {
        if (card->getPageId() == pageId) {
            card->setSelected(isSelected);
            break;
        }
    }
    
    int selectedCount = static_cast<int>(Core::State::WorkspaceState::instance().getSelectedPages().size());
    emit selectionCountChanged(selectedCount, static_cast<int>(m_cards.size()));
}

void ThumbnailGridWidget::onAllPagesSelectionChanged(bool isSelected) {
    for (auto* card : m_cards) {
        card->setSelected(isSelected);
    }
    int selectedCount = isSelected ? static_cast<int>(m_cards.size()) : 0;
    emit selectionCountChanged(selectedCount, static_cast<int>(m_cards.size()));
}

void ThumbnailGridWidget::onPageRotationChanged(const QString& pageId, int newRotation) {
    for (auto* card : m_cards) {
        if (card->getPageId() == pageId) {
            card->setRotation(newRotation);
            break;
        }
    }
}

void ThumbnailGridWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateGridColumns();
}

void ThumbnailGridWidget::updateGridColumns() {
    if (!m_gridLayout || m_cards.empty()) return;

    const int scrollbarWidth = m_scrollArea && m_scrollArea->verticalScrollBar()->isVisible()
        ? m_scrollArea->verticalScrollBar()->sizeHint().width() : 0;
    int availableWidth = (m_scrollArea ? m_scrollArea->viewport()->width() : width()) - 32 - scrollbarWidth;
    int cardWidth = 174;
    int columns = std::max(1, availableWidth / cardWidth);

    if (columns == m_currentColumns && m_gridLayout->count() == static_cast<int>(m_cards.size())) {
        return;
    }
    const int previousColumns = m_currentColumns;
    m_currentColumns = columns;

    for (auto* card : m_cards) {
        m_gridLayout->removeWidget(card);
    }
    for (int column = 0; column < columns; ++column) {
        m_gridLayout->setColumnStretch(column, 1);
    }
    for (int column = columns; column < previousColumns; ++column) {
        m_gridLayout->setColumnStretch(column, 0);
    }

    for (size_t i = 0; i < m_cards.size(); ++i) {
        int row = static_cast<int>(i) / columns;
        int col = static_cast<int>(i) % columns;
        m_gridLayout->addWidget(m_cards[i], row, col);
    }
}

std::vector<int> ThumbnailGridWidget::getSelectedPageIndices() const {
    std::vector<int> selected;
    for (const auto* card : m_cards) {
        if (card->isSelected()) {
            selected.push_back(card->getPageIndex());
        }
    }
    return selected;
}

std::vector<std::pair<int, int>> ThumbnailGridWidget::getPageRotations() const {
    std::vector<std::pair<int, int>> rotations;
    for (const auto* card : m_cards) {
        if (card->getRotation() != 0) {
            rotations.push_back({card->getPageIndex(), card->getRotation()});
        }
    }
    return rotations;
}

std::vector<int> ThumbnailGridWidget::getPageOrder() const {
    std::vector<int> pageOrder;
    pageOrder.reserve(m_cards.size());
    for (const auto* card : m_cards) {
        pageOrder.push_back(card->getPageIndex());
    }
    return pageOrder;
}

bool ThumbnailGridWidget::moveSelectedPage(int offset) {
    const auto selectedPages = Core::State::WorkspaceState::instance().getSelectedPages();
    if (selectedPages.size() != 1) {
        return false;
    }
    return Core::State::WorkspaceState::instance().movePage(selectedPages.front().id, offset);
}

void ThumbnailGridWidget::rotateSelectedPages(int degreesDelta) {
    Core::State::WorkspaceState::instance().rotateSelectedPages(degreesDelta);
    for (auto* card : m_cards) {
        if (card->isSelected()) {
            int currentRot = card->getRotation();
            int newRot = (currentRot + degreesDelta) % 360;
            if (newRot < 0) newRot += 360;
            card->setRotation(newRot);
        }
    }
}

void ThumbnailGridWidget::selectAll() {
    Core::State::WorkspaceState::instance().selectAllPages(true);
}

void ThumbnailGridWidget::deselectAll() {
    Core::State::WorkspaceState::instance().selectAllPages(false);
}

void ThumbnailGridWidget::setActionMode(ThumbnailActionMode mode) {
    if (m_actionMode == mode) return;
    m_actionMode = mode;
    for (size_t i = 0; i < m_cards.size(); ++i) {
        m_cards[i]->setActionMode(mode);
        m_cards[i]->setMoveAvailability(i > 0, i + 1 < m_cards.size());
    }
}

} // namespace LocalTools::UI
