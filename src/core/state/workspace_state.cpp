#include "workspace_state.h"
#include <algorithm>

namespace LocalTools::Core::State {

void WorkspaceState::addDocument(const DocumentState& doc) {
    // Check if already exists
    auto it = std::find_if(m_documents.begin(), m_documents.end(),
                           [&doc](const DocumentState& d) { return d.id == doc.id; });
    if (it == m_documents.end()) {
        m_documents.push_back(doc);
        emit documentAdded(doc.id);
    }
}

void WorkspaceState::removeDocument(const QString& docId) {
    auto it = std::remove_if(m_documents.begin(), m_documents.end(),
                             [&docId](const DocumentState& d) { return d.id == docId; });
    if (it != m_documents.end()) {
        m_documents.erase(it, m_documents.end());
        emit documentRemoved(docId);
    }
}

void WorkspaceState::clear() {
    m_documents.clear();
    emit workspaceCleared();
}

PageState* WorkspaceState::findPagePtr(const QString& pageId) {
    for (auto& doc : m_documents) {
        for (auto& page : doc.pages) {
            if (page.id == pageId) {
                return &page;
            }
        }
    }
    return nullptr;
}

std::optional<PageState> WorkspaceState::getPage(const QString& pageId) const {
    for (const auto& doc : m_documents) {
        for (const auto& page : doc.pages) {
            if (page.id == pageId) {
                return page;
            }
        }
    }
    return std::nullopt;
}

void WorkspaceState::togglePageSelection(const QString& pageId) {
    if (PageState* p = findPagePtr(pageId)) {
        p->isSelected = !p->isSelected;
        emit pageSelectionChanged(pageId, p->isSelected);
    }
}

void WorkspaceState::setPageSelection(const QString& pageId, bool selected) {
    if (PageState* p = findPagePtr(pageId)) {
        if (p->isSelected != selected) {
            p->isSelected = selected;
            emit pageSelectionChanged(pageId, selected);
        }
    }
}

void WorkspaceState::selectAllPages(bool selected) {
    for (auto& doc : m_documents) {
        for (auto& page : doc.pages) {
            if (page.isSelected != selected) {
                page.isSelected = selected;
                // Emit for each, or just rely on allPagesSelectionChanged?
                // Emitting for each allows fine-grained UI updates.
                emit pageSelectionChanged(page.id, selected);
            }
        }
    }
    emit allPagesSelectionChanged(selected);
}

void WorkspaceState::rotatePage(const QString& pageId, int delta) {
    if (PageState* p = findPagePtr(pageId)) {
        p->rotation = (p->rotation + delta) % 360;
        if (p->rotation < 0) p->rotation += 360;
        emit pageRotationChanged(pageId, p->rotation);
    }
}

void WorkspaceState::rotateSelectedPages(int delta) {
    for (auto& doc : m_documents) {
        for (auto& page : doc.pages) {
            if (page.isSelected) {
                page.rotation = (page.rotation + delta) % 360;
                if (page.rotation < 0) page.rotation += 360;
                emit pageRotationChanged(page.id, page.rotation);
            }
        }
    }
}

void WorkspaceState::deleteSelectedPages() {
    bool anyDeleted = false;
    for (auto& doc : m_documents) {
        auto it = std::remove_if(doc.pages.begin(), doc.pages.end(),
                                 [](const PageState& p) { return p.isSelected; });
        if (it != doc.pages.end()) {
            doc.pages.erase(it, doc.pages.end());
            anyDeleted = true;
        }
    }
    if (anyDeleted) {
        emit pagesDeleted();
    }
}

void WorkspaceState::removePage(const QString& pageId) {
    bool found = false;
    for (auto& doc : m_documents) {
        auto it = std::remove_if(doc.pages.begin(), doc.pages.end(),
                                 [&pageId](const PageState& p) { return p.id == pageId; });
        if (it != doc.pages.end()) {
            doc.pages.erase(it, doc.pages.end());
            found = true;
            break;
        }
    }
    if (found) {
        emit pagesDeleted();
    }
}

bool WorkspaceState::movePage(const QString& pageId, int offset) {
    if (offset == 0) {
        return false;
    }

    for (auto& document : m_documents) {
        const auto pageIt = std::find_if(document.pages.begin(), document.pages.end(),
            [&pageId](const PageState& page) { return page.id == pageId; });
        if (pageIt == document.pages.end()) {
            continue;
        }

        const auto currentIndex = std::distance(document.pages.begin(), pageIt);
        const auto destinationIndex = currentIndex + offset;
        const auto pageCount = static_cast<std::ptrdiff_t>(document.pages.size());
        if (destinationIndex < 0 || destinationIndex >= pageCount) {
            return false;
        }

        std::iter_swap(pageIt, document.pages.begin() + destinationIndex);
        emit pagesReordered();
        return true;
    }

    return false;
}

bool WorkspaceState::movePageTo(const QString& pageId, int targetGlobalIndex) {
    const int totalPages = static_cast<int>(getAllPages().size());
    if (targetGlobalIndex < 0 || targetGlobalIndex >= totalPages) return false;

    int sourceDocument = -1;
    int sourcePage = -1;
    for (int documentIndex = 0; documentIndex < static_cast<int>(m_documents.size()); ++documentIndex) {
        auto& pages = m_documents[documentIndex].pages;
        const auto pageIt = std::find_if(pages.begin(), pages.end(), [&pageId](const PageState& page) {
            return page.id == pageId;
        });
        if (pageIt != pages.end()) {
            sourceDocument = documentIndex;
            sourcePage = static_cast<int>(std::distance(pages.begin(), pageIt));
            break;
        }
    }
    if (sourceDocument < 0) return false;

    int currentGlobalIndex = 0;
    for (int documentIndex = 0; documentIndex < sourceDocument; ++documentIndex) {
        currentGlobalIndex += static_cast<int>(m_documents[documentIndex].pages.size());
    }
    currentGlobalIndex += sourcePage;
    if (currentGlobalIndex == targetGlobalIndex) return false;

    PageState page = std::move(m_documents[sourceDocument].pages[sourcePage]);
    m_documents[sourceDocument].pages.erase(m_documents[sourceDocument].pages.begin() + sourcePage);

    // Resolve against the post-removal workspace so target indices can cross
    // document boundaries. The final slot is valid in the last document.
    int remainingIndex = targetGlobalIndex;
    for (auto& document : m_documents) {
        const int pageCount = static_cast<int>(document.pages.size());
        if (remainingIndex <= pageCount) {
            document.pages.insert(document.pages.begin() + remainingIndex, std::move(page));
            emit pagesReordered();
            return true;
        }
        remainingIndex -= pageCount;
    }
    if (!m_documents.empty()) {
        m_documents.back().pages.push_back(std::move(page));
        emit pagesReordered();
        return true;
    }
    return false;
}

std::vector<PageState> WorkspaceState::getSelectedPages() const {
    std::vector<PageState> selected;
    for (const auto& doc : m_documents) {
        for (const auto& page : doc.pages) {
            if (page.isSelected) {
                selected.push_back(page);
            }
        }
    }
    return selected;
}

std::vector<PageState> WorkspaceState::getAllPages() const {
    std::vector<PageState> all;
    for (const auto& doc : m_documents) {
        for (const auto& page : doc.pages) {
            all.push_back(page);
        }
    }
    return all;
}

} // namespace LocalTools::Core::State
