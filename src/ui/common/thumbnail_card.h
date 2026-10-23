#ifndef LOCALTOOLS_UI_COMMON_THUMBNAIL_CARD_H
#define LOCALTOOLS_UI_COMMON_THUMBNAIL_CARD_H

#include <QFrame>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QPixmap>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>

namespace LocalTools::UI {

/** Page-level actions exposed by a thumbnail for the active organize tool. */
enum class ThumbnailActionMode {
    Merge,
    Split,
    Extract,
    Delete,
    Reorder,
    Rotate
};

class ThumbnailCardWidget : public QFrame {
    Q_OBJECT

public:
    explicit ThumbnailCardWidget(const QString& pageId, int displayIndex, QWidget *parent = nullptr);
    ~ThumbnailCardWidget() override = default;

    void setPageData(const QPixmap& thumbnail, int currentRotation = 0);

    QString getPageId() const { return m_pageId; }
    int getPageIndex() const { return m_displayIndex; }
    bool isSelected() const;
    void setSelected(bool selected);
    int getRotation() const { return m_currentRotation; }
    void setRotation(int rotation);
    void setActionMode(ThumbnailActionMode mode);
    ThumbnailActionMode actionMode() const { return m_actionMode; }
    void setMoveAvailability(bool canMoveLeft, bool canMoveRight);

signals:
    void selectionChanged(const QString& pageId, bool selected);
    void rotateRequested(const QString& pageId, int deltaDegrees);
    void deleteRequested(const QString& pageId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();
    void updateRotationBadge();
    bool isActionWidget(const QWidget* widget) const;

    QString m_pageId;
    int m_displayIndex{0};
    int m_currentRotation{0};
    ThumbnailActionMode m_actionMode{ThumbnailActionMode::Merge};

    QCheckBox* m_selectCheckBox{nullptr};
    QLabel* m_pageNumberLabel{nullptr};
    QLabel* m_thumbnailLabel{nullptr};
    QLabel* m_rotationBadge{nullptr};
    QPushButton* m_rotateLeftBtn{nullptr};
    QPushButton* m_rotateRightBtn{nullptr};
    QPushButton* m_deleteBtn{nullptr};
    QPushButton* m_moveLeftBtn{nullptr};
    QPushButton* m_moveRightBtn{nullptr};
    QPoint m_pressPosition;
    bool m_dragStarted{false};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_THUMBNAIL_CARD_H
