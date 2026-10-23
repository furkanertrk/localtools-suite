#ifndef LOCALTOOLS_CORE_STATE_WORKSPACE_STATE_H
#define LOCALTOOLS_CORE_STATE_WORKSPACE_STATE_H

#include <QObject>
#include <QString>
#include <QPixmap>
#include <vector>
#include <memory>
#include <optional>

namespace LocalTools::Core::State {

struct PageState {
    QString id; // Format: "filePath:pageIndex"
    QString filePath;
    int pageIndex;
    int rotation{0}; // 0, 90, 180, 270
    bool isSelected{false};
    QPixmap thumbnail;
};

struct DocumentState {
    QString id; // file path
    QString filePath;
    int totalPages{0};
    bool isEncrypted{false};
    bool isCorrupted{false};
    std::vector<PageState> pages;
};

class WorkspaceState : public QObject {
    Q_OBJECT

public:
    static WorkspaceState& instance() {
        static WorkspaceState instance;
        return instance;
    }

    // Accessors
    const std::vector<DocumentState>& documents() const { return m_documents; }
    
    // Modifiers
    void addDocument(const DocumentState& doc);
    void removeDocument(const QString& docId);
    void clear();

    void togglePageSelection(const QString& pageId);
    void setPageSelection(const QString& pageId, bool selected);
    void selectAllPages(bool selected);

    void rotatePage(const QString& pageId, int delta);
    void rotateSelectedPages(int delta);
    void deleteSelectedPages();
    void removePage(const QString& pageId);
    bool movePage(const QString& pageId, int offset);
    bool movePageTo(const QString& pageId, int targetGlobalIndex);

    // Queries
    std::vector<PageState> getSelectedPages() const;
    std::vector<PageState> getAllPages() const;
    std::optional<PageState> getPage(const QString& pageId) const;

signals:
    void documentAdded(const QString& docId);
    void documentRemoved(const QString& docId);
    void workspaceCleared();
    void pageSelectionChanged(const QString& pageId, bool isSelected);
    void allPagesSelectionChanged(bool isSelected);
    void pageRotationChanged(const QString& pageId, int newRotation);
    void pagesDeleted();
    void pagesReordered();

private:
    WorkspaceState() = default;
    ~WorkspaceState() override = default;

    std::vector<DocumentState> m_documents;

    // Helper to find a page
    PageState* findPagePtr(const QString& pageId);
};

} // namespace LocalTools::Core::State

#endif // LOCALTOOLS_CORE_STATE_WORKSPACE_STATE_H
