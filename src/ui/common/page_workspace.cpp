#include "page_workspace.h"

namespace LocalTools::UI {

PageWorkspace::PageWorkspace(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

void PageWorkspace::setupUi() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(8);

    m_thumbnailGrid = new ThumbnailGridWidget(this);
    m_thumbnailGrid->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_mainLayout->addWidget(m_thumbnailGrid, 1);

    connect(m_thumbnailGrid, &ThumbnailGridWidget::selectionCountChanged, this, &PageWorkspace::selectionCountChanged);
}

void PageWorkspace::reloadFromState() {
    m_thumbnailGrid->loadFromState();
}

void PageWorkspace::clear() {
    m_thumbnailGrid->clear();
}

void PageWorkspace::selectAllPages(bool select) {
    if (select) {
        m_thumbnailGrid->selectAll();
    } else {
        m_thumbnailGrid->deselectAll();
    }
}

std::vector<int> PageWorkspace::getSelectedPageIndices() const {
    return m_thumbnailGrid->getSelectedPageIndices();
}

} // namespace LocalTools::UI
