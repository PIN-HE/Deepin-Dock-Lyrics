#include "application/lyricsservicecontroller.h"
#include "infrastructure/dconfigsettingsadapter.h"
#include "infrastructure/lyricsdbusadapter.h"
#include "infrastructure/lrcliblyricsadapter.h"
#include "infrastructure/lrclibprovider.h"
#include "infrastructure/mprisplayeradapter.h"
#include "infrastructure/qtnetworktransport.h"
#include "infrastructure/sqlitelyricscache.h"

#include <lyricslogging/logengine.h>
#include <deepinlyrics/version.h>

#include <DLog>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>

using namespace deepin::lyrics;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("lyrics-dockd"));
    app.setApplicationVersion(QStringLiteral(DEEPIN_DOCK_LYRICS_VERSION));

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

    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    Dtk::Core::DLogManager::registerConsoleAppender();
    Dtk::Core::DLogManager::registerFileAppender();

    QtLogSink logSink;
    LogEngine logger(logSink);
    DConfigSettingsAdapter settings;
    MprisPlayerAdapter player(QDBusConnection::sessionBus());
    QtNetworkTransport transport;
    LRCLIBProvider provider(transport);
    SqliteLyricsCache cache;
    LrclibLyricsAdapter lyrics(provider, cache, logger);
    if (parser.isSet(QStringLiteral("player")))
        settings.setPlayerBusName(parser.value(QStringLiteral("player")));

    LyricsServiceController controller(player, settings, logger, &lyrics);
    LyricsDbusAdapter dbus(controller, QDBusConnection::sessionBus());
    QString errorCode;
    if (!dbus.registerService(&errorCode)) {
        logger.write(LogLevel::Critical, QStringLiteral("dbus"),
                     QStringLiteral("service_registration_failed"),
                     {{QStringLiteral("error_code"), errorCode}});
        return 2;
    }

    QObject::connect(&dbus, &LyricsDbusAdapter::serviceOwnershipLost,
                     &app, &QCoreApplication::quit);
    logger.write(LogLevel::Info, QStringLiteral("daemon"), QStringLiteral("service_started"));
    controller.start();
    return app.exec();
}
