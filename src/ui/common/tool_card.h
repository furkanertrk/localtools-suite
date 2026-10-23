#ifndef LOCALTOOLS_UI_COMMON_TOOL_CARD_H
#define LOCALTOOLS_UI_COMMON_TOOL_CARD_H

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QString>

namespace LocalTools::Core {
struct ToolDescriptor;
}

namespace LocalTools::UI {

/**
 * A single tool tile in the Home launcher.
 *
 * Built from a Core::ToolDescriptor so the launcher stays in step with
 * ToolRegistry instead of maintaining its own parallel list of tools.
 *
 * The whole card is one activation target (click, Enter or Space), which keeps
 * "open a tool" at exactly one interaction.
 */
class ToolCard : public QFrame {
    Q_OBJECT

public:
    explicit ToolCard(const Core::ToolDescriptor* tool, QWidget* parent = nullptr);
    ~ToolCard() override = default;

    QString toolId() const;
    const Core::ToolDescriptor* tool() const { return m_tool; }

    void setFavorite(bool favorite);
    bool isFavorite() const { return m_favorite; }

    /** Re-reads name/description from the descriptor and re-tints the icons. */
    void retranslateUi();

signals:
    void activated(const QString& toolId);
    void favoriteToggled(const QString& toolId, bool favorite);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void refreshIcons();

    const Core::ToolDescriptor* m_tool{nullptr};
    QLabel* m_icon{nullptr};
    QLabel* m_name{nullptr};
    QLabel* m_description{nullptr};
    QPushButton* m_favoriteBtn{nullptr};
    bool m_favorite{false};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_TOOL_CARD_H
