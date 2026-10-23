#ifndef LOCALTOOLS_UI_COMMON_SIDEBAR_H
#define LOCALTOOLS_UI_COMMON_SIDEBAR_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QLabel>
#include <vector>

#include "icon_utils.h"

namespace LocalTools::UI {

enum class NavItem {
    Home = 0,
    Organize = 1,
    Convert = 2,
    Optimize = 3,
    Metadata = 4,
    Settings = 5,
    AllTools = 6,
    Images = 7,
    Documents = 8,
    Files = 9,
    QuickTools = 10,
    Video = 11,
    Audio = 12
};

class SidebarWidget : public QWidget {
    Q_OBJECT

public:
    explicit SidebarWidget(QWidget *parent = nullptr);
    ~SidebarWidget() override = default;

    void setCurrentItem(NavItem item);
    void updateTheme(bool isDark);

signals:
    void navItemSelected(NavItem item);

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();
    QPushButton* createNavButton(const QString& text, NavItem item, IconType iconType);

    struct NavItemData {
        QPushButton* btn;
        NavItem item;
        IconType iconType;
    };

    QVBoxLayout* m_layout{nullptr};
    QButtonGroup* m_buttonGroup{nullptr};
    std::vector<NavItemData> m_navButtons;

    QLabel* m_brandLabel{nullptr};
    QLabel* m_categoryLabel{nullptr};
    QLabel* m_privacyIcon{nullptr};
    QLabel* m_privacyText{nullptr};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_COMMON_SIDEBAR_H
