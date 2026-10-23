#include "search_field.h"

#include <QKeyEvent>

#include "icon_utils.h"
#include "../theme.h"

namespace LocalTools::UI {

SearchField::SearchField(QWidget* parent)
    : QLineEdit(parent) {
    setObjectName("SearchField");
    setClearButtonEnabled(true);

    m_searchIcon = addAction(QIcon(), QLineEdit::LeadingPosition);
    refreshIcon();

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(Theme::DurationNormal);

    connect(this, &QLineEdit::textChanged, this, [this]() {
        m_debounce->start();
    });
    connect(m_debounce, &QTimer::timeout, this, [this]() {
        emit searchChanged(text());
    });
}

void SearchField::setDebounceInterval(int ms) {
    m_debounce->setInterval(ms);
}

void SearchField::refreshIcon() {
    if (m_searchIcon) {
        m_searchIcon->setIcon(
            IconUtils::getIcon(IconType::Search, Theme::color(ColorToken::TextTertiary), 16));
    }
}

void SearchField::keyPressEvent(QKeyEvent* event) {
    // Escape clears the filter instead of leaving the user stuck with a stale
    // query and no visible way back to the full list.
    if (event->key() == Qt::Key_Escape && !text().isEmpty()) {
        clear();
        emit searchChanged(QString());
        event->accept();
        return;
    }
    QLineEdit::keyPressEvent(event);
}

void SearchField::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange) {
        refreshIcon();
    }
    QLineEdit::changeEvent(event);
}

} // namespace LocalTools::UI
