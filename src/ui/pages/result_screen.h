#ifndef LOCALTOOLS_UI_PAGES_RESULT_SCREEN_H
#define LOCALTOOLS_UI_PAGES_RESULT_SCREEN_H

#include <QWidget>
#include <QString>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace LocalTools::UI {

class ResultScreenWidget : public QWidget {
    Q_OBJECT

public:
    explicit ResultScreenWidget(QWidget *parent = nullptr);
    ~ResultScreenWidget() override = default;

    void showResult(const QString& filePath, const QString& message);

signals:
    void backToWorkspaceRequested();

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();

    QString m_currentFilePath;
    QString m_rawMessageKey{"Operation Completed Successfully!"};

    QLabel* m_iconLabel{nullptr};
    QLabel* m_messageLabel{nullptr};
    QLabel* m_filePathLabel{nullptr};
    QPushButton* m_downloadBtn{nullptr};
    QPushButton* m_backBtn{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_RESULT_SCREEN_H
