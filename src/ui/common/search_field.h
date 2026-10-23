#ifndef LOCALTOOLS_UI_COMMON_SEARCH_FIELD_H
#define LOCALTOOLS_UI_COMMON_SEARCH_FIELD_H

#include <QLineEdit>
#include <QTimer>
#include <QAction>

namespace LocalTools::UI {

/**
 * Search input with a leading magnifier, a clear affordance, and debounced
 * output.
 *
 * Debouncing matters here because each keystroke re-queries ToolRegistry and
 * rebuilds the launcher grid; firing on every character makes typing stutter.
 */
class SearchField : public QLineEdit {
    Q_OBJECT

public:
    explicit SearchField(QWidget* parent = nullptr);
    ~SearchField() override = default;

    void setDebounceInterval(int ms);
    /** Re-applies the themed leading icon. Call after a theme change. */
    void refreshIcon();

signals:
    /** Emitted once the user stops typing for the debounce interval. */
    void searchChanged(const QString& text);

protected:
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QTimer* m_debounce{nullptr};
    QAction* m_searchIcon{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_SEARCH_FIELD_H
