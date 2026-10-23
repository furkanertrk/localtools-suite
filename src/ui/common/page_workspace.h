#ifndef LOCALTOOLS_UI_COMMON_PAGE_WORKSPACE_H
#define LOCALTOOLS_UI_COMMON_PAGE_WORKSPACE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <vector>

#include "thumbnail_grid.h"

namespace LocalTools::UI {

class PageWorkspace : public QWidget {
    Q_OBJECT

public:
    explicit PageWorkspace(QWidget *parent = nullptr);
    ~PageWorkspace() override = default;

    ThumbnailGridWidget* grid() const { return m_thumbnailGrid; }
    
    void reloadFromState();
    void clear();

    void selectAllPages(bool select);
    std::vector<int> getSelectedPageIndices() const;

signals:
    void selectionCountChanged(int selectedCount, int totalPages);

private:
    void setupUi();

    QVBoxLayout* m_mainLayout{nullptr};
    ThumbnailGridWidget* m_thumbnailGrid{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_PAGE_WORKSPACE_H
