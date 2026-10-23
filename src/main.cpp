#include <QGuiApplication>

#include "app/application.h"

int main(int argc, char* argv[]) {
    // Use Windows Media Foundation for in-app playback. File conversion remains
    // handled by the separately audited standalone FFmpeg executables; avoiding
    // Qt's older FFmpeg decoder backend reduces untrusted-media attack surface.
#ifdef Q_OS_WIN
    qputenv("QT_MEDIA_BACKEND", "windows");
#endif

    // Must be set before any QApplication instance exists.
    // PassThrough keeps fractional Windows scaling (125% / 150%) intact instead
    // of rounding it away; IconUtils renders at the resulting devicePixelRatio so
    // icons stay crisp.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    LocalTools::App::Application app(argc, argv);
    return app.run();
}
