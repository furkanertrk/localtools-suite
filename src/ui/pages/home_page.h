#ifndef LOCALTOOLS_UI_PAGES_HOME_PAGE_H
#define LOCALTOOLS_UI_PAGES_HOME_PAGE_H

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <vector>

#include "../common/drop_zone.h"
#include "../common/page_header.h"
#include "../common/search_field.h"
#include "../common/tool_card.h"
#include "../common/icon_utils.h"

namespace LocalTools::UI {

/**
 * Tool launcher.
 *
 * Every tile is built from Core::ToolRegistry rather than a list maintained
 * here, so Home shows all registered tools and cannot drift out of sync with
 * the registry the way the previous hardcoded 15-tool array did.
 */
class HomePage : public QWidget {
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);
    ~HomePage() override = default;

    void updateThemeIcon(bool isDark);

signals:
    void toolSelected(int toolId);
    void filesSelected(const QStringList& files);
    void toolRequestedWithFiles(int toolId, const QStringList& files);
    void themeToggleRequested();

public slots:
    void handleFilesDropped(const QStringList& files);
    void hideActionSheet();
    void refreshRecentFiles();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    /** A heading plus a responsive grid of tool cards. */
    struct CardSection {
        QWidget* container{nullptr};
        QLabel* heading{nullptr};
        QLabel* count{nullptr};
        QGridLayout* grid{nullptr};
        std::vector<ToolCard*> cards;
    };

    void setupUi();
    void retranslateUi();

    void buildToolSections();
    CardSection createSection(QVBoxLayout* parent, const QString& headingText);
    void populateSection(CardSection& section, const std::vector<const Core::ToolDescriptor*>& tools);
    void layoutSection(CardSection& section, int columns);
    void relayoutAllSections();
    int columnsForWidth() const;

    void applySearch(const QString& query);
    void setBrowseMode(bool browsing);

    void connectCard(ToolCard* card);
    void activateTool(const QString& toolId);

    QWidget* createQuickActionSheet();
    QWidget* createRecentFilesSection();

    // Shell
    PageHeader* m_header{nullptr};
    SearchField* m_search{nullptr};
    QPushButton* m_themeToggleBtn{nullptr};
    QVBoxLayout* m_contentLayout{nullptr};

    // Sections
    CardSection m_favorites;
    CardSection m_results;
    std::vector<CardSection> m_categories;

    // Search empty state
    QWidget* m_noResults{nullptr};
    QLabel* m_noResultsTitle{nullptr};
    QLabel* m_noResultsBody{nullptr};

    // Browse-mode-only widgets
    QWidget* m_recentSection{nullptr};
    DropZoneWidget* m_dropZone{nullptr};

    // Quick action sheet (shown after a drop)
    QWidget* m_actionSheetContainer{nullptr};
    QLabel* m_actionSheetFileLabel{nullptr};
    QLabel* m_promptLabel{nullptr};
    QWidget* m_pdfActionsWidget{nullptr};
    QWidget* m_imageActionsWidget{nullptr};
    QStringList m_droppedFiles;

    QVBoxLayout* m_recentFilesLayout{nullptr};
    QLabel* m_recentSectionTitle{nullptr};

    int m_currentColumns{4};
};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_PAGES_HOME_PAGE_H
