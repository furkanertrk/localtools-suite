#ifndef LOCALTOOLS_UI_COMMON_PAGE_HEADER_H
#define LOCALTOOLS_UI_COMMON_PAGE_HEADER_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace LocalTools::UI {

/**
 * Standard page header: title, optional subtitle, optional trailing actions.
 *
 * Replaces the subtitle stylesheet that was copy-pasted across ten pages under
 * three different member names (m_descLabel / m_tabDescriptionLabel /
 * m_subtitleLabel), each having drifted to slightly different opacity values.
 *
 * Text is set through setText() rather than the constructor so callers can call
 * it again from their retranslateUi().
 */
class PageHeader : public QWidget {
    Q_OBJECT

public:
    explicit PageHeader(QWidget* parent = nullptr);
    ~PageHeader() override = default;

    /** An empty @p subtitle hides the subtitle line entirely. */
    void setText(const QString& title, const QString& subtitle = QString());

    /** Adds a control to the trailing (right) edge of the header row. */
    void addAction(QWidget* widget);

private:
    QLabel* m_title{nullptr};
    QLabel* m_subtitle{nullptr};
    QHBoxLayout* m_row{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_PAGE_HEADER_H
