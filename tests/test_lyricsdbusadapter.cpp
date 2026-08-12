#include "application/lyricsservicecontroller.h"
#include "infrastructure/lyricsdbusadapter.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

using namespace deepin::lyrics;

class DbusFakePlayer final : public PlayerPort
{
    Q_OBJECT

public:
    using PlayerPort::PlayerPort;

    void start() override { }
    QList<PlayerDescriptor> availablePlayers() const override { return {}; }
    QString selectedPlayer() const override { return selected; }
    PlayerSnapshot snapshot() const override { return {}; }
    bool selectedPlayerAvailable() const override { return false; }
    qint64 selectedPlayerProcessId() const override { return 0; }
    void setSelectedPlayer(const QString &value) override { selected = value; }

    QString selected;
};

class DbusMemorySettings final : public SettingsPort
{
public:
    bool enabled() const override { return enabledValue; }
    QString playerBusName() const override { return {}; }
    int offsetMs() const override { return offsetValue; }
    bool audioVisualizerEnabled() const override { return false; }
    void setEnabled(bool value) override { enabledValue = value; }
    void setPlayerBusName(const QString &) override { }
    void setOffsetMs(int value) override { offsetValue = value; }
    void setAudioVisualizerEnabled(bool) override { }

    bool enabledValue = false;
    int offsetValue = 0;
};

class DbusNullLogSink final : public LogSink
{
public:
    void write(LogLevel, const LogEvent &) override { }
};

class LyricsDbusAdapterTest final : public QObject
{
    Q_OBJECT

private slots:
    void exportsContractAndRejectsSecondOwner();
};

class DbusServiceWorker final : public QObject
{
    Q_OBJECT

public slots:
    void initialize()
    {
        m_connection = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                      QStringLiteral("lyrics-dbus-test-service"));
        m_player = new DbusFakePlayer(this);
        m_settings = new DbusMemorySettings;
        m_sink = new DbusNullLogSink;
        m_logger = new LogEngine(*m_sink);
        m_controller = new LyricsServiceController(*m_player, *m_settings, *m_logger, nullptr, this);
        m_adapter = new LyricsDbusAdapter(*m_controller, m_connection, this);
        QString errorCode;
        const bool registered = m_adapter->registerService(&errorCode);
        if (registered)
            m_controller->start();
        emit ready(registered, errorCode);
    }

    void shutdown()
    {
        delete m_adapter;
        m_adapter = nullptr;
        delete m_controller;
        m_controller = nullptr;
        delete m_logger;
        m_logger = nullptr;
        delete m_sink;
        m_sink = nullptr;
        delete m_settings;
        m_settings = nullptr;
        QDBusConnection::disconnectFromBus(QStringLiteral("lyrics-dbus-test-service"));
    }

signals:
    void ready(bool registered, const QString &errorCode);

private:
    QDBusConnection m_connection = QDBusConnection::sessionBus();
    DbusFakePlayer *m_player = nullptr;
    DbusMemorySettings *m_settings = nullptr;
    DbusNullLogSink *m_sink = nullptr;
    LogEngine *m_logger = nullptr;
    LyricsServiceController *m_controller = nullptr;
    LyricsDbusAdapter *m_adapter = nullptr;
};

void LyricsDbusAdapterTest::exportsContractAndRejectsSecondOwner()
{
    const QString secondName = QStringLiteral("lyrics-dbus-test-client");
    QDBusConnection secondConnection = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                                      secondName);
    QVERIFY(secondConnection.isConnected());

    QThread serviceThread;
    DbusServiceWorker worker;
    worker.moveToThread(&serviceThread);
    connect(&serviceThread, &QThread::started, &worker, &DbusServiceWorker::initialize);
    QSignalSpy readySpy(&worker, &DbusServiceWorker::ready);
    serviceThread.start();
    QVERIFY(readySpy.wait(2000));
    QVERIFY(readySpy.at(0).at(0).toBool());

    QDBusInterface interface(QStringLiteral("org.deepin.LyricsDock1"),
                             QStringLiteral("/org/deepin/LyricsDock1"),
                             QStringLiteral("org.deepin.LyricsDock1"), secondConnection);
    QVERIFY(interface.isValid());

    QDBusMessage introspection = secondConnection.call(QDBusMessage::createMethodCall(
        QStringLiteral("org.deepin.LyricsDock1"), QStringLiteral("/org/deepin/LyricsDock1"),
        QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect")));
    QCOMPARE(introspection.type(), QDBusMessage::ReplyMessage);
    const QString xml = introspection.arguments().constFirst().toString();
    QVERIFY(xml.contains(QStringLiteral("name=\"GetState\"")));
    QVERIFY(xml.contains(QStringLiteral("name=\"SetEnabled\"")));
    QVERIFY(xml.contains(QStringLiteral("name=\"SetPlayer\"")));
    QVERIFY(xml.contains(QStringLiteral("name=\"SetOffsetMs\"")));
    QVERIFY(xml.contains(QStringLiteral("name=\"SearchCandidates\"")));
    QVERIFY(xml.contains(QStringLiteral("name=\"SelectCandidate\"")));
    QVERIFY(xml.contains(QStringLiteral("name=\"SetSessionHidden\"")));
    QVERIFY(xml.contains(QStringLiteral("name=\"ClearCache\"")));
    QVERIFY(xml.contains(QStringLiteral("name=\"StateChanged\"")));
    QVERIFY(xml.contains(QStringLiteral("type=\"a{sv}\"")));
    QVERIFY(xml.contains(QStringLiteral("name=\"CandidatesChanged\"")));
    QVERIFY(xml.contains(QStringLiteral("type=\"aa{sv}\"")));

    QDBusReply<QVariantMap> initial = interface.call(QStringLiteral("GetState"));
    QVERIFY2(initial.isValid(), qPrintable(initial.error().message()));
    QCOMPARE(initial.value().value(QStringLiteral("status")).toString(), QStringLiteral("Disabled"));

    QDBusMessage enableReply = interface.call(QStringLiteral("SetEnabled"), true);
    QCOMPARE(enableReply.type(), QDBusMessage::ReplyMessage);
    QDBusReply<QVariantMap> enabled = interface.call(QStringLiteral("GetState"));
    QVERIFY(enabled.isValid());
    QVERIFY(enabled.value().value(QStringLiteral("enabled")).toBool());

    QDBusMessage invalidOffset = interface.call(QStringLiteral("SetOffsetMs"), 10001);
    QCOMPARE(invalidOffset.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(invalidOffset.errorName(),
             QStringLiteral("org.deepin.LyricsDock1.Error.OffsetOutOfRange"));

    DbusFakePlayer secondPlayer;
    DbusMemorySettings secondSettings;
    DbusNullLogSink secondSink;
    LogEngine secondLogger(secondSink);
    LyricsServiceController secondController(secondPlayer, secondSettings, secondLogger);
    LyricsDbusAdapter secondAdapter(secondController, secondConnection);
    QString errorCode;
    QVERIFY(!secondAdapter.registerService(&errorCode));
    QCOMPARE(errorCode, QStringLiteral("service-name-unavailable"));

    QMetaObject::invokeMethod(&worker, &DbusServiceWorker::shutdown,
                              Qt::BlockingQueuedConnection);
    serviceThread.quit();
    QVERIFY(serviceThread.wait(2000));
    QDBusConnection::disconnectFromBus(secondName);
}

QTEST_MAIN(LyricsDbusAdapterTest)
#include "test_lyricsdbusadapter.moc"
