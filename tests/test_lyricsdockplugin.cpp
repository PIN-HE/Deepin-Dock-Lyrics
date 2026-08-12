#include <applet.h>
#include <pluginfactory.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPluginLoader>
#include <QTranslator>
#include <QtTest>

DS_USE_NAMESPACE

class LyricsDockPluginTest final : public QObject
{
    Q_OBJECT

private slots:
    void instantiatesRegisteredApplet();
    void packageMatchesDockContract();
    void providesChineseStatusTranslations();
};

void LyricsDockPluginTest::instantiatesRegisteredApplet()
{
    QPluginLoader loader(QStringLiteral(LYRICS_DOCK_PLUGIN_PATH));
    QObject *plugin = loader.instance();
    QVERIFY2(plugin, qPrintable(loader.errorString()));

    auto *factory = qobject_cast<DAppletFactory *>(plugin);
    QVERIFY(factory);
    DApplet *applet = factory->create();
    QVERIFY(applet);
    QCOMPARE(applet->metaObject()->className(), "LyricsDockApplet");
    QVERIFY(applet->property("viewModel").value<QObject *>());
    delete applet;
    QVERIFY(loader.unload());
}

void LyricsDockPluginTest::packageMatchesDockContract()
{
    QFile metadata(QStringLiteral(LYRICS_DOCK_PACKAGE_PATH "/metadata.json"));
    QVERIFY(metadata.open(QIODevice::ReadOnly));
    const QJsonObject plugin = QJsonDocument::fromJson(metadata.readAll())
                                   .object().value(QStringLiteral("Plugin")).toObject();
    QCOMPARE(plugin.value(QStringLiteral("Id")).toString(),
             QStringLiteral("org.deepin.ds.lyrics-dock"));
    QCOMPARE(plugin.value(QStringLiteral("Parent")).toString(),
             QStringLiteral("org.deepin.ds.dock"));
    QCOMPARE(plugin.value(QStringLiteral("Url")).toString(), QStringLiteral("main.qml"));
    QCOMPARE(plugin.value(QStringLiteral("Name[zh_CN]")).toString(),
             QStringLiteral("Deepin任务栏歌词"));
    QCOMPARE(plugin.value(QStringLiteral("Description[zh_CN]")).toString(),
             QStringLiteral("在Deepin系统任务栏中显示逐行同步歌词"));

    QFile mainQml(QStringLiteral(LYRICS_DOCK_PACKAGE_PATH "/main.qml"));
    QVERIFY(mainQml.open(QIODevice::ReadOnly));
    const QByteArray source = mainQml.readAll();
    QVERIFY(source.contains("property int dockOrder: 24"));
    QVERIFY(source.contains("property bool shouldVisible"));
    QVERIFY(source.contains("viewModel.setSessionHidden(true)"));
    QVERIFY(!source.contains("QDBusInterface"));

    QVERIFY(QFile::exists(QStringLiteral(LYRICS_DOCK_PACKAGE_PATH "/qml/LyricBar.qml")));
    QVERIFY(QFile::exists(QStringLiteral(LYRICS_DOCK_PACKAGE_PATH "/qml/MarqueeText.qml")));
    QVERIFY(QFile::exists(QStringLiteral(LYRICS_DOCK_PACKAGE_PATH "/qml/LyricsPopup.qml")));
    QVERIFY(QFile::exists(QStringLiteral(
        LYRICS_DOCK_PACKAGE_PATH "/translations/org.deepin.ds.lyrics-dock_zh_CN.qm")));
}

void LyricsDockPluginTest::providesChineseStatusTranslations()
{
    const QString source = QStringLiteral("Waiting for a music player");
    QCOMPARE(source, QStringLiteral("Waiting for a music player"));

    QTranslator translator;
    QVERIFY(translator.load(QStringLiteral(
        LYRICS_DOCK_PACKAGE_PATH "/translations/org.deepin.ds.lyrics-dock_zh_CN.qm")));
    QCOMPARE(translator.translate("main", source.toUtf8().constData()),
             QStringLiteral("正在等待音乐播放器"));
    QCOMPARE(translator.translate("main", "No lyrics found"),
             QStringLiteral("未找到歌词"));
    QCOMPARE(translator.translate("LyricBar", "Hide Dock lyrics"),
             QStringLiteral("隐藏任务栏歌词"));
}

QTEST_APPLESS_MAIN(LyricsDockPluginTest)
#include "test_lyricsdockplugin.moc"
