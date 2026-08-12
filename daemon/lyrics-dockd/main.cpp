#include "mprisplayerdiscovery.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QTextStream>

using deepin::lyrics::MprisPlayerDiscovery;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("lyrics-dockd"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Deepin Dock Lyrics background service"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("check"), QStringLiteral("Verify the daemon executable and exit.")});
    parser.addOption({QStringLiteral("player"),
                      QStringLiteral("Track one MPRIS bus name."),
                      QStringLiteral("bus-name")});
    parser.process(app);

    if (parser.isSet(QStringLiteral("check"))) {
        QTextStream(stdout) << "lyrics-dockd is available\n";
        return 0;
    }

    MprisPlayerDiscovery discovery(QDBusConnection::sessionBus());
    discovery.setSelectedPlayer(parser.value(QStringLiteral("player")));
    discovery.start();

    // S02 仅保持 MPRIS 发现循环；S03 将在同一进程中注册产品 D-Bus 服务。
    // S02 only keeps MPRIS discovery alive; S03 will export the product D-Bus service here.
    return app.exec();
}
