#include "page_header.h"

#include "../theme.h"

namespace LocalTools::UI {

PageHeader::PageHeader(QWidget* parent)
    : QWidget(parent) {
    m_row = new QHBoxLayout(this);
    m_row->setContentsMargins(0, 0, 0, 0);
    m_row->setSpacing(Theme::space(Space::S));

    auto* textColumn = new QVBoxLayout();
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(Theme::space(Space::XXS));

    m_title = new QLabel(this);
    m_title->setObjectName("PageTitle");
    m_title->setWordWrap(true);
    textColumn->addWidget(m_title);

    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName("PageSubtitle");
    m_subtitle->setWordWrap(true);
    m_subtitle->hide();
    textColumn->addWidget(m_subtitle);

    m_row->addLayout(textColumn, 1);
}

void PageHeader::setText(const QString& title, const QString& subtitle) {
    m_title->setText(title);
    m_subtitle->setText(subtitle);
    m_subtitle->setVisible(!subtitle.isEmpty());
}

void PageHeader::addAction(QWidget* widget) {
    if (!widget) return;
    widget->setParent(this);
    m_row->addWidget(widget, 0, Qt::AlignTop | Qt::AlignRight);
}

} // namespace LocalTools::UI
