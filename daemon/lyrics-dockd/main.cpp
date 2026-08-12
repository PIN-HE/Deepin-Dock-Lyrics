#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("lyrics-dockd"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Deepin Dock Lyrics background service"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("check"), QStringLiteral("Verify the S00 daemon skeleton and exit.")});
    parser.process(app);

    if (parser.isSet(QStringLiteral("check"))) {
        QTextStream(stdout) << "lyrics-dockd S00 skeleton is available\n";
        return 0;
    }

    QTextStream(stderr) << "lyrics-dockd is not active until the daemon specification is implemented\n";
    return 0;
}
