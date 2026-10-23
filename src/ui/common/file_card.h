#ifndef LOCALTOOLS_UI_COMMON_FILE_CARD_H
#define LOCALTOOLS_UI_COMMON_FILE_CARD_H

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QString>

namespace LocalTools::UI {

class FileCardWidget : public QFrame {
    Q_OBJECT

public:
    explicit FileCardWidget(const QString& filePath, QWidget *parent = nullptr);
    ~FileCardWidget() override = default;

    void setDocumentDetails(int pageCount, uint64_t fileSizeBytes, const QPixmap& thumbnail = QPixmap());
    QString getFilePath() const { return m_filePath; }

signals:
    void removeRequested(const QString& filePath);

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();

    QString m_filePath;
    QLabel* m_thumbnailLabel{nullptr};
    QLabel* m_nameLabel{nullptr};
    QLabel* m_detailsLabel{nullptr};
    QPushButton* m_removeButton{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_FILE_CARD_H
