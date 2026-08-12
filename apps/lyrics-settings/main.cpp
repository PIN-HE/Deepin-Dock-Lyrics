#include <DApplication>
#include <QCommandLineParser>
#include <QTextStream>

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("deepin-lyrics-settings"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Deepin Dock Lyrics settings"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("check"), QStringLiteral("Verify the S00 settings skeleton and exit.")});
    parser.process(app);

    if (parser.isSet(QStringLiteral("check"))) {
        QTextStream(stdout) << "deepin-lyrics-settings S00 skeleton is available\n";
        return 0;
    }

    QTextStream(stderr) << "The settings interface is scheduled for S07\n";
    return 0;
}
