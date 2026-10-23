#ifndef LOCALTOOLS_UI_COMMON_DROP_ZONE_H
#define LOCALTOOLS_UI_COMMON_DROP_ZONE_H

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStringList>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>

namespace LocalTools::UI {

class DropZoneWidget : public QFrame {
    Q_OBJECT

public:
    explicit DropZoneWidget(QWidget *parent = nullptr);
    ~DropZoneWidget() override = default;

    void setInstruction(const QString& title, const QString& subtitle);
    void setAcceptedFileExtensions(const QStringList& extensions, const QString& dialogTitle, const QString& invalidMessage = QString());
    void setInvalidFileMessage(const QString& message);

    QSize sizeHint() const override { return QSize(360, 240); }
    QSize minimumSizeHint() const override { return QSize(240, 220); }

signals:
    void filesDropped(const QStringList& filePaths);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void setDragOverState(bool isDragOver, bool isValidFile);
    void resetNormalState();
    /** Sets the "dropState" property the application stylesheet keys off, and repolishes. */
    void applyDropState(const char* state);
    bool acceptsFile(const QString& filePath) const;

    QLabel* m_iconLabel{nullptr};
    QLabel* m_titleLabel{nullptr};
    QLabel* m_subtitleLabel{nullptr};
    QPushButton* m_browseButton{nullptr};

    QString m_rawTitleKey{"Drag and Drop PDF File Here"};
    QString m_rawSubtitleKey{"or click to select file"};
    QString m_rawDialogTitleKey{"Select PDF Files"};
    QString m_rawInvalidMsgKey{"Only PDF files are accepted"};

    QString m_defaultTitle{tr("Drag and Drop PDF File Here")};
    QString m_defaultSubtitle{tr("or click to select file")};
    QStringList m_acceptedExtensions{QStringLiteral("pdf")};
    QString m_dialogTitle{tr("Select PDF Files")};
    QString m_invalidFileMessage{tr("Only PDF files are accepted")};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_DROP_ZONE_H
