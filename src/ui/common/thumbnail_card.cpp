#include "thumbnail_card.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QEvent>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QTimer>
#include "icon_utils.h"
#include "../../core/state/workspace_state.h"

namespace LocalTools::UI {

static QPixmap drawCheckmarkPixmap(bool checked, int size = 20, QPalette palette = QPalette()) {
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    if (checked) {
        // Filled Blue Box
        p.setBrush(QColor("#2563eb"));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(0, 0, size, size, 5, 5);

        // White Checkmark Icon
        QPen pen(Qt::white, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawLine(size * 0.25, size * 0.52, size * 0.44, size * 0.72);
        p.drawLine(size * 0.44, size * 0.72, size * 0.76, size * 0.30);
    } else {
        // Outline Box — use palette
        p.setBrush(palette.color(QPalette::Base));
        p.setPen(QPen(palette.color(QPalette::Mid), 1.8));
        p.drawRoundedRect(1, 1, size - 2, size - 2, 5, 5);
    }

    return pix;
}

ThumbnailCardWidget::ThumbnailCardWidget(const QString& pageId, int displayIndex, QWidget *parent)
    : QFrame(parent), m_pageId(pageId), m_displayIndex(displayIndex) {
    setObjectName("CardFrame");
    setCursor(Qt::PointingHandCursor);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(R"(
        QFrame#CardFrame {
            border: 1px solid palette(mid);
            border-radius: 10px;
            background-color: palette(base);
        }
        QFrame#CardFrame:hover {
            border: 1px solid palette(highlight);
        }
        QFrame#CardFrame[selected="true"] {
            border: 2px solid #2563eb;
        }
        QCheckBox::indicator {
            width: 0px;
            height: 0px;
        }
    )");
    setupUi();
}

void ThumbnailCardWidget::setupUi() {
    setFixedSize(160, 240);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Top Header: Selection Checkbox & Page Number
    auto *topLayout = new QHBoxLayout();
    topLayout->setContentsMargins(0, 0, 0, 0);

    m_selectCheckBox = new QCheckBox(this);
    m_selectCheckBox->setCursor(Qt::PointingHandCursor);
    m_selectCheckBox->setIcon(drawCheckmarkPixmap(false, 20, palette()));
    m_selectCheckBox->setIconSize(QSize(20, 20));
    topLayout->addWidget(m_selectCheckBox);

    m_pageNumberLabel = new QLabel(tr("Page %1").arg(m_displayIndex + 1), this);
    QFont pageFont = m_pageNumberLabel->font();
    pageFont.setBold(true);
    m_pageNumberLabel->setFont(pageFont);
    topLayout->addWidget(m_pageNumberLabel, 1, Qt::AlignLeft | Qt::AlignVCenter);

    m_rotationBadge = new QLabel("0°", this);
    m_rotationBadge->setStyleSheet("font-size: 11px; opacity: 0.7; border: 1px solid palette(mid); border-radius: 4px; padding: 1px 4px;");
    topLayout->addWidget(m_rotationBadge);

    layout->addLayout(topLayout);

    // Thumbnail Preview Label
    m_thumbnailLabel = new QLabel(this);
    m_thumbnailLabel->setFixedSize(144, 155);
    m_thumbnailLabel->setStyleSheet("border: 1px solid palette(mid); border-radius: 6px;");
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setText(tr("Loading..."));
    layout->addWidget(m_thumbnailLabel);

    // Bottom Action Bar: compact move, rotate and delete actions.
    auto *actionsLayout = new QHBoxLayout();
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(4);

    m_rotateLeftBtn = new QPushButton(this);
    m_rotateLeftBtn->setObjectName("ToolTabButtonCompact");
    m_rotateLeftBtn->setIcon(IconUtils::getIcon(IconType::RotateLeft, QColor(), 17));
    m_rotateLeftBtn->setIconSize(QSize(17, 17));
    m_rotateLeftBtn->setToolTip(tr("Rotate Left (-90°)"));
    m_rotateLeftBtn->setAccessibleName(m_rotateLeftBtn->toolTip());
    m_rotateLeftBtn->setFixedSize(26, 26);
    m_rotateLeftBtn->setCursor(Qt::PointingHandCursor);

    m_rotateRightBtn = new QPushButton(this);
    m_rotateRightBtn->setObjectName("ToolTabButtonCompact");
    m_rotateRightBtn->setIcon(IconUtils::getIcon(IconType::RotateRight, QColor(), 17));
    m_rotateRightBtn->setIconSize(QSize(17, 17));
    m_rotateRightBtn->setToolTip(tr("Rotate Right (+90°)"));
    m_rotateRightBtn->setAccessibleName(m_rotateRightBtn->toolTip());
    m_rotateRightBtn->setFixedSize(26, 26);
    m_rotateRightBtn->setCursor(Qt::PointingHandCursor);

    m_moveLeftBtn = new QPushButton(this);
    m_moveLeftBtn->setObjectName("ToolTabButtonCompact");
    m_moveLeftBtn->setIcon(IconUtils::getIcon(IconType::ArrowLeft, QColor(), 17));
    m_moveLeftBtn->setIconSize(QSize(17, 17));
    m_moveLeftBtn->setToolTip(tr("Move Page Left"));
    m_moveLeftBtn->setAccessibleName(m_moveLeftBtn->toolTip());
    m_moveLeftBtn->setFixedSize(26, 26);
    m_moveLeftBtn->setCursor(Qt::PointingHandCursor);

    m_moveRightBtn = new QPushButton(this);
    m_moveRightBtn->setObjectName("ToolTabButtonCompact");
    m_moveRightBtn->setIcon(IconUtils::getIcon(IconType::ArrowRight, QColor(), 17));
    m_moveRightBtn->setIconSize(QSize(17, 17));
    m_moveRightBtn->setToolTip(tr("Move Page Right"));
    m_moveRightBtn->setAccessibleName(m_moveRightBtn->toolTip());
    m_moveRightBtn->setFixedSize(26, 26);
    m_moveRightBtn->setCursor(Qt::PointingHandCursor);

    m_deleteBtn = new QPushButton(this);
    m_deleteBtn->setIcon(IconUtils::getIcon(IconType::Delete, QColor("#ef4444"), 18));
    m_deleteBtn->setToolTip(tr("Delete Page"));
    m_deleteBtn->setAccessibleName(m_deleteBtn->toolTip());
    m_deleteBtn->setFixedSize(26, 26);
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setStyleSheet("QPushButton { border: none; background: transparent; } QPushButton:hover { background: #fee2e2; border-radius: 4px; }");

    actionsLayout->addWidget(m_moveLeftBtn);
    actionsLayout->addWidget(m_moveRightBtn);
    actionsLayout->addWidget(m_rotateLeftBtn);
    actionsLayout->addWidget(m_rotateRightBtn);
    actionsLayout->addStretch();
    actionsLayout->addWidget(m_deleteBtn);

    layout->addLayout(actionsLayout);

    connect(m_selectCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_selectCheckBox->setIcon(drawCheckmarkPixmap(checked));
        setProperty("selected", checked);
        style()->unpolish(this);
        style()->polish(this);
        emit selectionChanged(m_pageId, checked);
    });

    connect(m_rotateLeftBtn, &QPushButton::clicked, this, [this]() {
        emit rotateRequested(m_pageId, -90);
    });

    connect(m_rotateRightBtn, &QPushButton::clicked, this, [this]() {
        emit rotateRequested(m_pageId, 90);
    });

    connect(m_moveLeftBtn, &QPushButton::clicked, this, [this]() {
        Core::State::WorkspaceState::instance().movePage(m_pageId, -1);
    });
    connect(m_moveRightBtn, &QPushButton::clicked, this, [this]() {
        Core::State::WorkspaceState::instance().movePage(m_pageId, 1);
    });

    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        emit deleteRequested(m_pageId);
    });

    setActionMode(m_actionMode);
}

void ThumbnailCardWidget::mousePressEvent(QMouseEvent *event) {
    m_pressPosition = event->pos();
    m_dragStarted = false;
    setFocus(Qt::MouseFocusReason);
    QFrame::mousePressEvent(event);
}

void ThumbnailCardWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !m_dragStarted && !isActionWidget(childAt(event->pos()))) {
        setSelected(!isSelected());
    }
    QFrame::mouseReleaseEvent(event);
}

void ThumbnailCardWidget::mouseMoveEvent(QMouseEvent *event) {
    const bool canMove = m_actionMode == ThumbnailActionMode::Merge ||
                         m_actionMode == ThumbnailActionMode::Reorder;
    if (!canMove || !(event->buttons() & Qt::LeftButton) || m_dragStarted ||
        (event->pos() - m_pressPosition).manhattanLength() < QApplication::startDragDistance()) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    m_dragStarted = true;
    auto* drag = new QDrag(this);
    auto* mime = new QMimeData();
    mime->setData("application/x-localtools-page-id", m_pageId.toUtf8());
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
}

void ThumbnailCardWidget::dragEnterEvent(QDragEnterEvent *event) {
    const bool canMove = m_actionMode == ThumbnailActionMode::Merge ||
                         m_actionMode == ThumbnailActionMode::Reorder;
    if (canMove && event->mimeData()->hasFormat("application/x-localtools-page-id")) event->acceptProposedAction();
}

void ThumbnailCardWidget::dragMoveEvent(QDragMoveEvent *event) {
    const bool canMove = m_actionMode == ThumbnailActionMode::Merge ||
                         m_actionMode == ThumbnailActionMode::Reorder;
    if (canMove && event->mimeData()->hasFormat("application/x-localtools-page-id")) event->acceptProposedAction();
}

void ThumbnailCardWidget::dropEvent(QDropEvent *event) {
    const bool canMove = m_actionMode == ThumbnailActionMode::Merge ||
                         m_actionMode == ThumbnailActionMode::Reorder;
    if (!canMove || !event->mimeData()->hasFormat("application/x-localtools-page-id")) return;
    const QString sourceId = QString::fromUtf8(event->mimeData()->data("application/x-localtools-page-id"));
    if (sourceId == m_pageId) return;
    const int targetIndex = m_displayIndex + (event->position().x() > width() / 2 ? 1 : 0);
    QTimer::singleShot(0, this, [sourceId, targetIndex]() {
        Core::State::WorkspaceState::instance().movePageTo(sourceId, targetIndex);
    });
    event->acceptProposedAction();
}

void ThumbnailCardWidget::keyPressEvent(QKeyEvent *event) {
    const bool canMove = m_actionMode == ThumbnailActionMode::Merge ||
                         m_actionMode == ThumbnailActionMode::Reorder;
    if (canMove && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
        const int offset = event->key() == Qt::Key_Left ? -1 : 1;
        QTimer::singleShot(0, this, [pageId = m_pageId, offset]() {
            Core::State::WorkspaceState::instance().movePage(pageId, offset);
        });
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

bool ThumbnailCardWidget::isActionWidget(const QWidget* widget) const {
    for (const QWidget* action : {static_cast<const QWidget*>(m_selectCheckBox), static_cast<const QWidget*>(m_moveLeftBtn),
                                  static_cast<const QWidget*>(m_moveRightBtn), static_cast<const QWidget*>(m_rotateLeftBtn),
                                  static_cast<const QWidget*>(m_rotateRightBtn), static_cast<const QWidget*>(m_deleteBtn)}) {
        if (widget == action || (widget && action && action->isAncestorOf(widget))) return true;
    }
    return false;
}

void ThumbnailCardWidget::setPageData(const QPixmap& thumbnail, int currentRotation) {
    m_currentRotation = currentRotation % 360;
    updateRotationBadge();

    if (!thumbnail.isNull()) {
        m_thumbnailLabel->setPixmap(thumbnail.scaled(m_thumbnailLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void ThumbnailCardWidget::setRotation(int rotation) {
    m_currentRotation = rotation % 360;
    if (m_currentRotation < 0) m_currentRotation += 360;
    updateRotationBadge();
}

void ThumbnailCardWidget::setActionMode(ThumbnailActionMode mode) {
    m_actionMode = mode;

    const bool showMove = mode == ThumbnailActionMode::Merge || mode == ThumbnailActionMode::Reorder;
    const bool showRotate = mode == ThumbnailActionMode::Rotate;
    const bool showDelete = mode == ThumbnailActionMode::Delete;

    m_moveLeftBtn->setVisible(showMove);
    m_moveRightBtn->setVisible(showMove);
    m_rotateLeftBtn->setVisible(showRotate);
    m_rotateRightBtn->setVisible(showRotate);
    m_deleteBtn->setVisible(showDelete);
    m_rotationBadge->setVisible(showRotate);
    setAcceptDrops(showMove);
}

void ThumbnailCardWidget::setMoveAvailability(bool canMoveLeft, bool canMoveRight) {
    m_moveLeftBtn->setEnabled(canMoveLeft);
    m_moveRightBtn->setEnabled(canMoveRight);
}

void ThumbnailCardWidget::updateRotationBadge() {
    m_rotationBadge->setText(tr("%1°").arg(m_currentRotation));
}

bool ThumbnailCardWidget::isSelected() const {
    return m_selectCheckBox->isChecked();
}

void ThumbnailCardWidget::setSelected(bool selected) {
    if (m_selectCheckBox->isChecked() != selected) {
        m_selectCheckBox->setChecked(selected);
    } else {
        m_selectCheckBox->setIcon(drawCheckmarkPixmap(selected));
    }
    setProperty("selected", selected);
    style()->unpolish(this);
    style()->polish(this);
}

void ThumbnailCardWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QFrame::changeEvent(event);
}

void ThumbnailCardWidget::retranslateUi() {
    m_pageNumberLabel->setText(tr("Page %1").arg(m_displayIndex + 1));
    const QString selectPageLabel = tr("Select Page %1").arg(m_displayIndex + 1);
    m_selectCheckBox->setAccessibleName(selectPageLabel);
    m_selectCheckBox->setToolTip(selectPageLabel);
    if (m_thumbnailLabel->pixmap(Qt::ReturnByValue).isNull()) {
        m_thumbnailLabel->setText(tr("Loading..."));
    }
    m_rotateLeftBtn->setToolTip(tr("Rotate Left (-90°)"));
    m_rotateLeftBtn->setAccessibleName(m_rotateLeftBtn->toolTip());
    m_rotateRightBtn->setToolTip(tr("Rotate Right (+90°)"));
    m_rotateRightBtn->setAccessibleName(m_rotateRightBtn->toolTip());
    m_moveLeftBtn->setToolTip(tr("Move Page Left"));
    m_moveLeftBtn->setAccessibleName(m_moveLeftBtn->toolTip());
    m_moveRightBtn->setToolTip(tr("Move Page Right"));
    m_moveRightBtn->setAccessibleName(m_moveRightBtn->toolTip());
    m_deleteBtn->setToolTip(tr("Delete Page"));
    m_deleteBtn->setAccessibleName(m_deleteBtn->toolTip());
    updateRotationBadge();
}

} // namespace LocalTools::UI
