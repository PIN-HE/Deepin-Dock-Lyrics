#include "settingsviewmodel.h"
#include "settingswindow.h"

#include <DApplication>
#include <DIconTheme>
#include <DLog>
#include <QCommandLineParser>
#include <QLocale>
#include <QTextStream>
#include <QTranslator>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("deepin-lyrics-settings"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("deepin"));
    app.loadTranslator();

    QTranslator productTranslator;
    if (QLocale::system().language() == QLocale::Chinese) {
        const bool translationLoaded = productTranslator.load(
            QStringLiteral(":/translations/deepin-lyrics-settings_zh_CN.qm"));
        if (translationLoaded)
            app.installTranslator(&productTranslator);
    }
    app.setProductName(QObject::tr("Dock Lyrics"));
    app.setProductIcon(DIconTheme::findQIcon(QStringLiteral("music")));
    app.setApplicationDescription(QObject::tr("Display synchronized lyrics in the Deepin Dock."));
    app.setApplicationLicense(QStringLiteral("MIT"));
    Dtk::Core::DLogManager::registerConsoleAppender();
    Dtk::Core::DLogManager::registerFileAppender();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Deepin Dock Lyrics settings"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("check"), QStringLiteral("Verify the settings application and exit.")});
    parser.process(app);

    if (parser.isSet(QStringLiteral("check"))) {
        QTextStream(stdout) << "deepin-lyrics-settings is available\n";
        return 0;
    }

    if (!app.setSingleInstance(QStringLiteral("org.deepin.LyricsDock.Settings"),
                               DApplication::UserScope))
        return 0;

    SettingsViewModel viewModel;
    SettingsWindow window(viewModel);
    QObject::connect(&app, &DApplication::newInstanceStarted, &window, [&window] {
        window.showNormal();
        window.raise();
        window.activateWindow();
    });
    window.show();
    return app.exec();
}
