#ifndef LOCALTOOLS_UI_MAIN_WINDOW_H
#define LOCALTOOLS_UI_MAIN_WINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <memory>

#include "common/sidebar.h"
#include "pages/home_page.h"
#include "pages/organize_page.h"
#include "pages/convert_page.h"
#include "pages/optimize_page.h"
#include "pages/metadata_page.h"
#include "pages/images_page.h"
#include "pages/documents_page.h"
#include "pages/files_page.h"
#include "pages/quick_tools_page.h"
#include "pages/video_page.h"
#include "pages/audio_page.h"
#include "pages/settings_page.h"
#include "../services/settings_service.h"

namespace LocalTools::UI {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void applyTheme(Services::AppTheme theme);

    SidebarWidget* m_sidebar{nullptr};
    QStackedWidget* m_pageStack{nullptr};

    HomePage* m_homePage{nullptr};
    OrganizePage* m_organizePage{nullptr};
    ConvertPage* m_convertPage{nullptr};
    OptimizePage* m_optimizePage{nullptr};
    MetadataPage* m_metadataPage{nullptr};
    ImagesPage* m_imagesPage{nullptr};
    DocumentsPage* m_documentsPage{nullptr};
    FilesPage* m_filesPage{nullptr};
    QuickToolsPage* m_quickToolsPage{nullptr};
    VideoPage* m_videoPage{nullptr};
    AudioPage* m_audioPage{nullptr};
    SettingsPage* m_settingsPage{nullptr};

};

} // namespace LocalTools::UI

#endif // LOCALTOOLS_UI_MAIN_WINDOW_H
