#ifndef LOCALTOOLS_UI_COMMON_THUMBNAIL_GRID_H
#define LOCALTOOLS_UI_COMMON_THUMBNAIL_GRID_H

#include <QWidget>
#include <QGridLayout>
#include <QResizeEvent>
#include <QScrollArea>
#include <vector>
#include <memory>
#include "thumbnail_card.h"
#include "../../core/backend/pdfium_backend.h"

namespace LocalTools::UI {

class ThumbnailGridWidget : public QWidget {
    Q_OBJECT

public:
    explicit ThumbnailGridWidget(QWidget *parent = nullptr);
    ~ThumbnailGridWidget() override = default;

    void loadFromState();
    void clear();

    std::vector<int> getSelectedPageIndices() const; // We might not even need this if we read from state, but keep for compatibility
    std::vector<std::pair<int, int>> getPageRotations() const;
    std::vector<int> getPageOrder() const;
    int getPageCount() const { return static_cast<int>(m_cards.size()); }

    bool moveSelectedPage(int offset);
    void rotateSelectedPages(int degreesDelta);

    void selectAll();
    void deselectAll();

    /** Applies the active organize tool's capabilities to existing and future cards. */
    void setActionMode(ThumbnailActionMode mode);
    ThumbnailActionMode actionMode() const { return m_actionMode; }

public slots:
    void onPageSelectionChanged(const QString& pageId, bool isSelected);
    void onAllPagesSelectionChanged(bool isSelected);
    void onPageRotationChanged(const QString& pageId, int newRotation);

signals:
    void selectionCountChanged(int selectedCount, int totalPages);
    void pageDeleteRequested(int pageIndex);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateGridColumns();

    QGridLayout* m_gridLayout{nullptr};
    QScrollArea* m_scrollArea{nullptr};
    std::vector<ThumbnailCardWidget*> m_cards;
    int m_currentColumns{4};
    ThumbnailActionMode m_actionMode{ThumbnailActionMode::Merge};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_THUMBNAIL_GRID_H
