#ifndef LOCALTOOLS_APP_APPLICATION_H
#define LOCALTOOLS_APP_APPLICATION_H

#include <QApplication>
#include <memory>
#include "../ui/main_window.h"

namespace LocalTools::App {

class Application {
public:
    Application(int& argc, char** argv);
    ~Application();

    int run();

private:
    std::unique_ptr<QApplication> m_qapp;
    std::unique_ptr<LocalTools::UI::MainWindow> m_mainWindow;
};

} // namespace LocalTools::App

#endif // LOCALTOOLS_APP_APPLICATION_H
