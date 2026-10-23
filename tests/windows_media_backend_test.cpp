#include <QAudioOutput>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QVideoSink>

#include <cassert>
#include <iostream>

int main(int argc, char *argv[]) {
    qputenv("QT_MEDIA_BACKEND", "windows");
    QCoreApplication app(argc, argv);
    assert(argc >= 2);

    const QString ffmpeg = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    assert(QFileInfo::exists(ffmpeg));
    const QString fixture = QDir::current().absoluteFilePath("windows_media_fixture.mp4");

    QProcess generator;
    generator.start(ffmpeg,
                    {"-nostdin", "-hide_banner", "-loglevel", "error", "-y",
                     "-f", "lavfi", "-i", "testsrc=size=320x240:rate=10",
                     "-f", "lavfi", "-i", "sine=frequency=880:sample_rate=44100",
                     "-t", "1", "-c:v", "libopenh264", "-c:a", "aac", fixture});
    assert(generator.waitForStarted(5000));
    assert(generator.waitForFinished(30000));
    assert(generator.exitStatus() == QProcess::NormalExit && generator.exitCode() == 0);
    assert(QFileInfo(fixture).size() > 0);

    QMediaPlayer player;
    QAudioOutput audioOutput;
    QVideoSink videoSink;
    player.setAudioOutput(&audioOutput);
    player.setVideoOutput(&videoSink);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool loaded = false;
    QString errorText;
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &loop,
                     [&](QMediaPlayer::Error, const QString& text) {
                         errorText = text;
                         loop.quit();
                     });
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, &loop,
                     [&](QMediaPlayer::MediaStatus status) {
                         if (status == QMediaPlayer::LoadedMedia ||
                             status == QMediaPlayer::BufferedMedia) {
                             loaded = true;
                             loop.quit();
                         }
                     });

    player.setSource(QUrl::fromLocalFile(fixture));
    timeout.start(15000);
    loop.exec();

    if (!loaded) {
        std::cerr << "Windows media backend failed to load the MP4 fixture: "
                  << errorText.toStdString() << std::endl;
    }
    assert(loaded);
    assert(player.duration() > 0);
    QFile::remove(fixture);
    std::cout << "Windows media backend loaded the H.264/AAC MP4 fixture." << std::endl;
    return 0;
}
