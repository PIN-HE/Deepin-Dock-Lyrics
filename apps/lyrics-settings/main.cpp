#include "settingsviewmodel.h"
#include "settingswindow.h"

#include <deepinlyrics/version.h>

#include <DAboutDialog>
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
    app.setApplicationVersion(QStringLiteral(DEEPIN_DOCK_LYRICS_VERSION));
    app.setOrganizationName(QStringLiteral("deepin"));
    app.loadTranslator();

    QTranslator productTranslator;
    // 使用系统 locale 的完整语言和地区候选，未提供对应翻译时回退英文源码。
    // Use the system locale's language/territory candidates and fall back to English source text.
    if (productTranslator.load(QLocale::system(), QStringLiteral("deepin-lyrics-settings"),
                               QStringLiteral("_"), QStringLiteral(":/translations")))
        app.installTranslator(&productTranslator);
    app.setProductName(QObject::tr("Deepin Dock Lyrics"));
    app.setProductIcon(QIcon(QStringLiteral(":/icons/deepin-lyrics-dock.png")));
    app.setApplicationDescription(QObject::tr("Display synchronized lyrics in the Deepin Dock."));
    app.setApplicationLicense(QStringLiteral("MIT"));

    // 关于对话框（标题栏汉堡菜单 → 关于）：元数据 + 第三方致谢。
    // About dialog (title-bar hamburger menu): metadata plus third-party
    // acknowledgements.
    auto *aboutDialog = new Dtk::Widget::DAboutDialog;
    aboutDialog->setProductName(QObject::tr("Deepin Dock Lyrics"));
    aboutDialog->setVersion(QStringLiteral(DEEPIN_DOCK_LYRICS_VERSION));
    aboutDialog->setDescription(QObject::tr(
        "Display synchronized lyrics in the Deepin Dock.\n"
        "\n"
        "致谢 Acknowledgements:\n"
        "端闼乐部（Ter-Music）· 终端音乐播放器，提供歌词 D-Bus 接口，感谢开发者 燕戏竹林："
        "https://github.com/HuanSoft-Open-Source-Community/ter-music\n"
        "歌词数据与接口参考：LRCLIB（lrclib.net）、TaskbarLyrics。"));
    aboutDialog->setWebsiteName(QStringLiteral("GitHub"));
    aboutDialog->setWebsiteLink(QStringLiteral("https://github.com/PIN-HE/Deepin-Dock-Lyrics"));
    app.setAboutDialog(aboutDialog);
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
