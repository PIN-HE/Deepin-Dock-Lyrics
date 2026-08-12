#include "mprisplayerdiscovery.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QtTest>

using namespace deepin::lyrics;

namespace {

constexpr auto mprisPath = "/org/mpris/MediaPlayer2";

class FakeMprisObject : public QObject
{
    Q_OBJECT

public:
    explicit FakeMprisObject(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    QString identity = QStringLiteral("Fake Player");
    QString desktopEntry = QStringLiteral("fake-player");
    QString playbackStatus = QStringLiteral("Stopped");
    double rate = 1.0;
    QVariantMap metadata;
    mutable qlonglong positionUs = 0;
    mutable int positionReadCount = 0;
};

class FakeRootAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)

public:
    explicit FakeRootAdaptor(FakeMprisObject *object)
        : QDBusAbstractAdaptor(object)
        , m_object(object)
    {
    }

    QString identity() const { return m_object->identity; }
    QString desktopEntry() const { return m_object->desktopEntry; }

private:
    FakeMprisObject *m_object;
};

class FakePlayerAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(double Rate READ rate)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(qlonglong Position READ position)

public:
    explicit FakePlayerAdaptor(FakeMprisObject *object)
        : QDBusAbstractAdaptor(object)
        , m_object(object)
    {
    }

    QString playbackStatus() const { return m_object->playbackStatus; }
    double rate() const { return m_object->rate; }
    QVariantMap metadata() const { return m_object->metadata; }
    qlonglong position() const
    {
        ++m_object->positionReadCount;
        return m_object->positionUs;
    }

private:
    FakeMprisObject *m_object;
};

QVariantMap metadata(const QString &title,
                     const QStringList &artists = {QStringLiteral("Example Artist")},
                     qlonglong durationUs = 213000000)
{
    return {
        {QStringLiteral("xesam:title"), title},
        {QStringLiteral("xesam:artist"), artists},
        {QStringLiteral("xesam:album"), QStringLiteral("Example Album")},
        {QStringLiteral("mpris:length"), durationUs},
    };
}

class FakeMprisService final
{
public:
    FakeMprisService(const QString &busName, const QString &identity)
        : m_busName(busName)
        , m_connectionName(QStringLiteral("fake-mpris-%1").arg(++s_connectionCounter))
        , m_connection(QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                     m_connectionName))
    {
        object.identity = identity;
        object.metadata = metadata(QStringLiteral("Initial Track"));
        new FakeRootAdaptor(&object);
        new FakePlayerAdaptor(&object);
    }

    ~FakeMprisService()
    {
        stop();
        QDBusConnection::disconnectFromBus(m_connectionName);
    }

    bool start()
    {
        // 真实播放器是独立进程，因此测试替身也必须拥有各自的 D-Bus 连接。
        // Real players are separate processes, so each fake needs its own D-Bus connection too.
        if (!m_connection.isConnected() || !m_connection.registerService(m_busName))
            return false;
        m_started = m_connection.registerObject(QString::fromLatin1(mprisPath), &object,
                                                QDBusConnection::ExportAdaptors);
        if (!m_started)
            m_connection.unregisterService(m_busName);
        return m_started;
    }

    void stop()
    {
        if (!m_started)
            return;
        m_connection.unregisterObject(QString::fromLatin1(mprisPath));
        m_connection.unregisterService(m_busName);
        m_started = false;
    }

    void changePlayerProperties(const QVariantMap &properties)
    {
        if (properties.contains(QStringLiteral("PlaybackStatus")))
            object.playbackStatus = properties.value(QStringLiteral("PlaybackStatus")).toString();
        if (properties.contains(QStringLiteral("Rate")))
            object.rate = properties.value(QStringLiteral("Rate")).toDouble();
        if (properties.contains(QStringLiteral("Metadata")))
            object.metadata = properties.value(QStringLiteral("Metadata")).toMap();
        if (properties.contains(QStringLiteral("Position")))
            object.positionUs = properties.value(QStringLiteral("Position")).toLongLong();

        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(mprisPath), QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));
        signal << QStringLiteral("org.mpris.MediaPlayer2.Player") << properties << QStringList {};
        m_connection.send(signal);
    }

    void seek(qlonglong positionUs)
    {
        object.positionUs = positionUs;
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(mprisPath), QStringLiteral("org.mpris.MediaPlayer2.Player"),
            QStringLiteral("Seeked"));
        signal << positionUs;
        m_connection.send(signal);
    }

    FakeMprisObject object;

private:
    inline static int s_connectionCounter = 0;
    QString m_busName;
    QString m_connectionName;
    QDBusConnection m_connection;
    bool m_started = false;
};

} // namespace

class MprisPlayerDiscoveryTest : public QObject
{
    Q_OBJECT

private slots:
    void discoversAndSwitchesPlayers();
    void rediscoversRestartedPlayer();
    void pollsOnlyWhilePlaying();
    void debouncesTrackChanges();
    void handlesIncompleteMetadataAndExit();
    void resolvesAndClearsSelectedPlayerProcessId();
};

void MprisPlayerDiscoveryTest::discoversAndSwitchesPlayers()
{
    FakeMprisService first(QStringLiteral("org.mpris.MediaPlayer2.fakeone"),
                           QStringLiteral("Fake One"));
    FakeMprisService second(QStringLiteral("org.mpris.MediaPlayer2.faketwo"),
                            QStringLiteral("Fake Two"));
    QVERIFY(first.start());
    QVERIFY(second.start());

    MprisPlayerDiscovery discovery(QDBusConnection::sessionBus());
    QSignalSpy snapshotSpy(&discovery, &MprisPlayerDiscovery::snapshotChanged);
    discovery.start();
    QTRY_COMPARE(discovery.availablePlayers().size(), 2);
    QTRY_COMPARE(discovery.availablePlayers().at(0).identity, QStringLiteral("Fake One"));

    discovery.setSelectedPlayer(QStringLiteral("org.mpris.MediaPlayer2.fakeone"));
    QTRY_VERIFY(discovery.selectedPlayerAvailable());
    QTRY_COMPARE(discovery.snapshot().track.title, QStringLiteral("Initial Track"));
    QCOMPARE(discovery.snapshot().busName, QStringLiteral("org.mpris.MediaPlayer2.fakeone"));

    const int snapshotsBeforeOtherPlayerChange = snapshotSpy.count();
    second.changePlayerProperties({
        {QStringLiteral("Metadata"), metadata(QStringLiteral("Wrong Player Track"))},
    });
    QTest::qWait(100);
    QCOMPARE(snapshotSpy.count(), snapshotsBeforeOtherPlayerChange);

    discovery.setSelectedPlayer(QStringLiteral("org.mpris.MediaPlayer2.faketwo"));
    QTRY_COMPARE(discovery.snapshot().busName, QStringLiteral("org.mpris.MediaPlayer2.faketwo"));
    QTRY_COMPARE(discovery.snapshot().track.title, QStringLiteral("Wrong Player Track"));

    first.stop();
    second.stop();
}

void MprisPlayerDiscoveryTest::rediscoversRestartedPlayer()
{
    const QString busName = QStringLiteral("org.mpris.MediaPlayer2.restart");
    FakeMprisService firstInstance(busName, QStringLiteral("Before Restart"));
    QVERIFY(firstInstance.start());

    MprisPlayerDiscovery discovery(QDBusConnection::sessionBus());
    discovery.setSelectedPlayer(busName);
    discovery.start();
    QTRY_VERIFY(discovery.selectedPlayerAvailable());
    QTRY_COMPARE(discovery.snapshot().identity, QStringLiteral("Before Restart"));

    firstInstance.stop();
    QTRY_VERIFY(!discovery.selectedPlayerAvailable());

    FakeMprisService secondInstance(busName, QStringLiteral("After Restart"));
    secondInstance.object.metadata = metadata(QStringLiteral("Restarted Track"));
    QVERIFY(secondInstance.start());
    QTRY_VERIFY(discovery.selectedPlayerAvailable());
    QTRY_COMPARE(discovery.snapshot().track.title, QStringLiteral("Restarted Track"));
    QTRY_COMPARE(discovery.availablePlayers().first().identity,
                 QStringLiteral("After Restart"));
}

void MprisPlayerDiscoveryTest::pollsOnlyWhilePlaying()
{
    FakeMprisService player(QStringLiteral("org.mpris.MediaPlayer2.polling"),
                            QStringLiteral("Polling Player"));
    QVERIFY(player.start());

    MprisPlayerDiscovery discovery(QDBusConnection::sessionBus());
    discovery.start();
    QTRY_COMPARE(discovery.availablePlayers().size(), 1);
    discovery.setSelectedPlayer(QStringLiteral("org.mpris.MediaPlayer2.polling"));
    QTRY_COMPARE(discovery.snapshot().track.title, QStringLiteral("Initial Track"));

    player.object.positionUs = 5000000;
    player.changePlayerProperties({{QStringLiteral("PlaybackStatus"), QStringLiteral("Playing")}});
    QTRY_VERIFY_WITH_TIMEOUT(player.object.positionReadCount >= 2, 500);
    QTRY_COMPARE_WITH_TIMEOUT(discovery.snapshot().positionMs, 5000, 250);

    player.changePlayerProperties({{QStringLiteral("PlaybackStatus"), QStringLiteral("Paused")}});
    QTest::qWait(250);
    const int readsWhilePaused = player.object.positionReadCount;
    QTest::qWait(500);
    QCOMPARE(player.object.positionReadCount, readsWhilePaused);

    player.seek(42000000);
    QTRY_COMPARE(discovery.snapshot().positionMs, 42000);
    player.stop();
}

void MprisPlayerDiscoveryTest::debouncesTrackChanges()
{
    FakeMprisService player(QStringLiteral("org.mpris.MediaPlayer2.debounce"),
                            QStringLiteral("Debounce Player"));
    QVERIFY(player.start());

    MprisPlayerDiscovery discovery(QDBusConnection::sessionBus());
    discovery.start();
    QTRY_COMPARE(discovery.availablePlayers().size(), 1);
    discovery.setSelectedPlayer(QStringLiteral("org.mpris.MediaPlayer2.debounce"));
    QTRY_COMPARE(discovery.snapshot().track.title, QStringLiteral("Initial Track"));
    QTest::qWait(450);

    QSignalSpy trackSpy(&discovery, &MprisPlayerDiscovery::trackChanged);
    player.changePlayerProperties({
        {QStringLiteral("Metadata"), metadata(QStringLiteral("Transient Track"))},
    });
    QTest::qWait(200);
    player.changePlayerProperties({
        {QStringLiteral("Metadata"), metadata(QStringLiteral("Stable Track"))},
    });
    QTest::qWait(300);
    QCOMPARE(trackSpy.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(trackSpy.count(), 1, 250);
    QCOMPARE(qvariant_cast<TrackIdentity>(trackSpy.at(0).at(0)).title,
             QStringLiteral("Stable Track"));
    player.stop();
}

void MprisPlayerDiscoveryTest::handlesIncompleteMetadataAndExit()
{
    FakeMprisService player(QStringLiteral("org.mpris.MediaPlayer2.incomplete"),
                            QStringLiteral("Incomplete Player"));
    player.object.metadata = metadata(QString(), {}, 0);
    QVERIFY(player.start());

    MprisPlayerDiscovery discovery(QDBusConnection::sessionBus());
    QSignalSpy availabilitySpy(&discovery,
                               &MprisPlayerDiscovery::selectedPlayerAvailableChanged);
    discovery.setSelectedPlayer(QStringLiteral("org.mpris.MediaPlayer2.incomplete"));
    discovery.start();
    QTRY_VERIFY(discovery.selectedPlayerAvailable());
    QTRY_COMPARE(discovery.snapshot().track.durationMs, -1);
    QVERIFY(!discovery.snapshot().track.searchable);

    player.stop();
    QTRY_VERIFY(!discovery.selectedPlayerAvailable());
    QTRY_VERIFY(discovery.snapshot().busName.isEmpty());
    QVERIFY(availabilitySpy.count() >= 2);
}

void MprisPlayerDiscoveryTest::resolvesAndClearsSelectedPlayerProcessId()
{
    FakeMprisService first(QStringLiteral("org.mpris.MediaPlayer2.pid-one"),
                           QStringLiteral("PID One"));
    FakeMprisService second(QStringLiteral("org.mpris.MediaPlayer2.pid-two"),
                            QStringLiteral("PID Two"));
    QVERIFY(first.start());
    QVERIFY(second.start());

    MprisPlayerDiscovery discovery(QDBusConnection::sessionBus());
    QSignalSpy processIdSpy(&discovery, &MprisPlayerDiscovery::selectedPlayerProcessIdChanged);
    discovery.start();
    QTRY_COMPARE(discovery.availablePlayers().size(), 2);

    discovery.setSelectedPlayer(QStringLiteral("org.mpris.MediaPlayer2.pid-one"));
    QTRY_VERIFY(discovery.selectedPlayerProcessId() > 0);
    QCOMPARE(discovery.selectedPlayerProcessId(), qint64(QCoreApplication::applicationPid()));

    discovery.setSelectedPlayer(QStringLiteral("org.mpris.MediaPlayer2.pid-two"));
    QTRY_VERIFY(discovery.selectedPlayerProcessId() > 0);
    QCOMPARE(discovery.selectedPlayerProcessId(), qint64(QCoreApplication::applicationPid()));

    second.stop();
    QTRY_COMPARE(discovery.selectedPlayerProcessId(), 0);
    QVERIFY(processIdSpy.count() >= 3);
}

QTEST_GUILESS_MAIN(MprisPlayerDiscoveryTest)

#include "test_mprisplayerdiscovery.moc"
