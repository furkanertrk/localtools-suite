#include "file_card.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QEvent>
#include "../../services/file_service.h"

#include "icon_utils.h"

namespace LocalTools::UI {

FileCardWidget::FileCardWidget(const QString& filePath, QWidget *parent)
    : QFrame(parent), m_filePath(filePath) {
    setObjectName("CardFrame");
    setupUi();
}

void FileCardWidget::setupUi() {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(12);

    m_thumbnailLabel = new QLabel(this);
    m_thumbnailLabel->setFixedSize(36, 44);
    m_thumbnailLabel->setStyleSheet("border-radius: 4px; border: 1px solid #334155;");
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setPixmap(IconUtils::getPixmap(IconType::PdfToImages, QColor("#2563eb"), 20));
    layout->addWidget(m_thumbnailLabel);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(4);

    QFileInfo fileInfo(m_filePath);
    m_nameLabel = new QLabel(fileInfo.fileName(), this);
    QFont nameFont = m_nameLabel->font();
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setStyleSheet("font-size: 14px;");
    textLayout->addWidget(m_nameLabel);

    std::string formattedSize = Services::FileService::instance().formatFileSize(fileInfo.size());
    m_detailsLabel = new QLabel(tr("%1").arg(QString::fromStdString(formattedSize)), this);
    m_detailsLabel->setStyleSheet("font-size: 12px; opacity: 0.75;");
    textLayout->addWidget(m_detailsLabel);

    layout->addLayout(textLayout, 1);

    m_removeButton = new QPushButton("✕", this);
    m_removeButton->setFixedSize(28, 28);
    m_removeButton->setCursor(Qt::PointingHandCursor);
    m_removeButton->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            color: #94a3b8;
            border: none;
            border-radius: 14px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #fee2e2;
            color: #ef4444;
        }
    )");
    layout->addWidget(m_removeButton);

    connect(m_removeButton, &QPushButton::clicked, this, [this]() {
        emit removeRequested(m_filePath);
    });
}

void FileCardWidget::setDocumentDetails(int pageCount, uint64_t fileSizeBytes, const QPixmap& thumbnail) {
    std::string formattedSize = Services::FileService::instance().formatFileSize(fileSizeBytes);
    m_detailsLabel->setText(tr("%1 Pages · %2").arg(pageCount).arg(QString::fromStdString(formattedSize)));

    if (!thumbnail.isNull()) {
        m_thumbnailLabel->setPixmap(thumbnail.scaled(m_thumbnailLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void FileCardWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QFrame::changeEvent(event);
}

void FileCardWidget::retranslateUi() {
    // If the card already has a size stored we would need to re-format it,
    // but the card's details label text contains the formatted size and pages.
    // Instead of storing them, we can try to re-apply if we know them, but FileCardWidget
    // is usually recreated or we can leave it to the parent to update details.
}

} // namespace LocalTools::UI
