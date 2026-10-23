#include "drop_zone.h"

#include <QMimeData>
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>
#include <QStyle>

#include "icon_utils.h"
#include "../theme.h"

namespace LocalTools::UI {

DropZoneWidget::DropZoneWidget(QWidget *parent)
    : QFrame(parent) {
    setObjectName("DropZoneFrame");
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);
    setupUi();
}

void DropZoneWidget::setupUi() {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(240);
    setMaximumHeight(260);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(0);

    layout->addStretch(1);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(36, 36);
    m_iconLabel->setContentsMargins(0, 0, 0, 0);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_iconLabel, 0, Qt::AlignCenter);

    layout->addSpacing(Theme::space(Space::XS));

    m_titleLabel = new QLabel(m_defaultTitle, this);
    m_titleLabel->setObjectName("DropZoneTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_titleLabel);

    layout->addSpacing(Theme::space(Space::XXS));

    m_subtitleLabel = new QLabel(m_defaultSubtitle, this);
    m_subtitleLabel->setObjectName("DropZoneSubtitle");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_subtitleLabel);

    layout->addSpacing(Theme::space(Space::S));

    m_browseButton = new QPushButton(tr("Select File"), this);
    m_browseButton->setObjectName("PrimaryButton");
    m_browseButton->setCursor(Qt::PointingHandCursor);
    // Minimum, not fixed: the Turkish label is markedly longer.
    m_browseButton->setMinimumSize(120, 32);
    layout->addWidget(m_browseButton, 0, Qt::AlignCenter);

    layout->addStretch(1);

    resetNormalState();

    connect(m_browseButton, &QPushButton::clicked, this, [this]() {
        QString nameFilter;
        if (m_acceptedExtensions.isEmpty()) {
            nameFilter = tr("All Files (*.*)");
        } else {
            QStringList patterns;
            for (const QString& extension : m_acceptedExtensions) {
                patterns.append("*." + extension);
            }
            nameFilter = tr("Supported Files (%1)").arg(patterns.join(' '));
        }
        QStringList paths = QFileDialog::getOpenFileNames(
            this,
            m_dialogTitle,
            QString(),
            nameFilter
        );
        if (!paths.isEmpty()) {
            emit filesDropped(paths);
        }
    });
}

void DropZoneWidget::setInstruction(const QString& title, const QString& subtitle) {
    m_rawTitleKey = title;
    m_rawSubtitleKey = subtitle;
    m_defaultTitle = tr(title.toUtf8().constData());
    m_defaultSubtitle = tr(subtitle.toUtf8().constData());
    m_titleLabel->setText(m_defaultTitle);
    m_subtitleLabel->setText(m_defaultSubtitle);
}

void DropZoneWidget::setAcceptedFileExtensions(const QStringList& extensions, const QString& dialogTitle, const QString& invalidMessage) {
    m_acceptedExtensions.clear();
    for (QString extension : extensions) {
        extension = extension.trimmed().toLower();
        if (extension.startsWith('.')) {
            extension.remove(0, 1);
        }
        if (!extension.isEmpty() && !m_acceptedExtensions.contains(extension)) {
            m_acceptedExtensions.append(extension);
        }
    }
    m_rawDialogTitleKey = dialogTitle;
    m_dialogTitle = tr(dialogTitle.toUtf8().constData());
    if (!invalidMessage.isEmpty()) {
        m_rawInvalidMsgKey = invalidMessage;
        m_invalidFileMessage = tr(invalidMessage.toUtf8().constData());
    }
}

void DropZoneWidget::setInvalidFileMessage(const QString& message) {
    m_rawInvalidMsgKey = message;
    m_invalidFileMessage = tr(message.toUtf8().constData());
}

bool DropZoneWidget::acceptsFile(const QString& filePath) const {
    if (m_acceptedExtensions.isEmpty()) {
        return true;
    }
    return m_acceptedExtensions.contains(QFileInfo(filePath).suffix(), Qt::CaseInsensitive);
}

void DropZoneWidget::applyDropState(const char* state) {
    // The visual states live in the application stylesheet, keyed off this
    // property, so the drop zone stays themed. Qt only re-evaluates property
    // selectors after an explicit unpolish/polish cycle.
    setProperty("dropState", state);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void DropZoneWidget::setDragOverState(bool isDragOver, bool isValidFile) {
    if (!isDragOver) {
        resetNormalState();
        return;
    }

    m_subtitleLabel->hide();
    m_browseButton->hide();

    if (isValidFile) {
        m_iconLabel->setPixmap(IconUtils::getPixmap(
            IconType::Folder, Theme::color(ColorToken::Accent), 26));
        m_titleLabel->setText(tr("Drop File Here"));
        applyDropState("valid");
    } else {
        m_iconLabel->setPixmap(IconUtils::getPixmap(
            IconType::Warning, Theme::color(ColorToken::ErrorText), 26));
        m_titleLabel->setText(m_invalidFileMessage);
        applyDropState("invalid");
    }
}

void DropZoneWidget::resetNormalState() {
    m_iconLabel->setPixmap(IconUtils::getPixmap(
        IconType::Folder, Theme::color(ColorToken::Accent), 26));
    m_titleLabel->setText(m_defaultTitle);
    m_subtitleLabel->setText(m_defaultSubtitle);
    m_subtitleLabel->show();
    m_browseButton->show();
    applyDropState("idle");
}

void DropZoneWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        bool hasValid = false;
        for (const QUrl &url : urls) {
            if (acceptsFile(url.toLocalFile())) {
                hasValid = true;
                break;
            }
        }
        event->acceptProposedAction();
        setDragOverState(true, hasValid);
    }
}

void DropZoneWidget::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void DropZoneWidget::dragLeaveEvent(QDragLeaveEvent *event) {
    Q_UNUSED(event);
    resetNormalState();
}

void DropZoneWidget::dropEvent(QDropEvent *event) {
    resetNormalState();
    QStringList acceptedPaths;
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl &url : urls) {
            QString localPath = url.toLocalFile();
            if (acceptsFile(localPath)) {
                acceptedPaths.append(localPath);
            }
        }
    }
    if (!acceptedPaths.isEmpty()) {
        event->acceptProposedAction();
        emit filesDropped(acceptedPaths);
    }
}

void DropZoneWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        m_defaultTitle = tr(m_rawTitleKey.toUtf8().constData());
        m_defaultSubtitle = tr(m_rawSubtitleKey.toUtf8().constData());
        m_dialogTitle = tr(m_rawDialogTitleKey.toUtf8().constData());
        m_invalidFileMessage = tr(m_rawInvalidMsgKey.toUtf8().constData());

        if (m_browseButton) m_browseButton->setText(tr("Select File"));
        if (m_titleLabel) m_titleLabel->setText(m_defaultTitle);
        if (m_subtitleLabel) m_subtitleLabel->setText(m_defaultSubtitle);
    }
    QFrame::changeEvent(event);
}

} // namespace LocalTools::UI
