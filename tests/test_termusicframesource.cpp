#include "infrastructure/termusicframesource.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QThread>
#include <QtTest>

using namespace deepin::lyrics;

namespace {

constexpr auto serviceName = "org.mpris.MediaPlayer2.ter_music";
constexpr auto objectPath = "/org/mpris/MediaPlayer2";
constexpr auto interfaceName = "org.yxzl.ter_music.Lyrics";

// 构造与 Ter-Music session.c 一致的 JSON 快照。
// Builds a snapshot JSON matching Ter-Music's session.c.
QString snapshotJson(const QString &activeLine,
                     const QString &lineA,
                     const QString &lineB,
                     const QString &trackId,
                     bool hasTimestamps,
                     bool hasLyrics,
                     quint64 revision)
{
    const auto line = [](const QString &text, int index, double timestamp) {
        return QStringLiteral("{\"index\":%1,\"timestamp\":%2,\"text\":\"%3\"}")
            .arg(index).arg(timestamp).arg(text);
    };
    return QStringLiteral("{\"active_line\":\"%1\",\"line_a\":%2,\"line_b\":%3,"
                          "\"track_id\":\"%4\",\"has_lyrics\":%5,\"has_timestamps\":%6,"
                          "\"revision\":%7}")
        .arg(activeLine,
             line(lineA, 0, 0.0),
             line(lineB, 1, 5.0),
             trackId,
             hasLyrics ? QStringLiteral("true") : QStringLiteral("false"),
             hasTimestamps ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(revision);
}

// 在独立线程注册 Ter-Music 同名服务的假实现。
// Fake Ter-Music service registered on its own thread and connection.
class FakeTerMusicService final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.yxzl.ter_music.Lyrics")

public:
    explicit FakeTerMusicService(QString connectionName)
        : m_connectionName(std::move(connectionName))
    {
    }

    QString snapshot;

public slots:
    QString GetLyrics() const { return snapshot; }

    void publish(const QString &json)
    {
        snapshot = json;
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(objectPath), QString::fromLatin1(interfaceName),
            QStringLiteral("LyricsChanged"));
        signal << json;
        m_connection.send(signal);
    }

    void initialize()
    {
        m_connection = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                     m_connectionName);
        const bool registered = m_connection.registerObject(
            QString::fromLatin1(objectPath), this,
            QDBusConnection::ExportAllSlots)
            && m_connection.registerService(QString::fromLatin1(serviceName));
        emit ready(registered);
    }

    void shutdown()
    {
        m_connection.unregisterService(QString::fromLatin1(serviceName));
        m_connection.unregisterObject(QString::fromLatin1(objectPath));
        QDBusConnection::disconnectFromBus(m_connectionName);
    }

signals:
    void ready(bool registered);

private:
    QString m_connectionName;
    QDBusConnection m_connection = QDBusConnection::sessionBus();
};

bool startService(FakeTerMusicService &service, QThread &thread)
{
    QSignalSpy readySpy(&service, &FakeTerMusicService::ready);
    QObject::connect(&thread, &QThread::started, &service, &FakeTerMusicService::initialize);
    service.moveToThread(&thread);
    thread.start();
    return readySpy.wait(2000) && readySpy.constFirst().constFirst().toBool();
}

void stopService(FakeTerMusicService &service, QThread &thread)
{
    QMetaObject::invokeMethod(&service, &FakeTerMusicService::shutdown,
                              Qt::BlockingQueuedConnection);
    thread.quit();
    QVERIFY(thread.wait(2000));
}

} // namespace

class TerMusicFrameSourceTest final : public QObject
{
    Q_OBJECT

private slots:
    void followsDoubleBufferAndRejectsStaleRevisions();
    void resetsOnTrackChangeAndHandlesPlainLyrics();
    void emitsStoppedWhenLyricsAbsentOrPlayerUnselected();
};

void TerMusicFrameSourceTest::followsDoubleBufferAndRejectsStaleRevisions()
{
    const QString serviceConnection = QStringLiteral("ter-music-service-1");
    const QString clientConnection = QStringLiteral("ter-music-client-1");
    QThread serviceThread;
    FakeTerMusicService service(serviceConnection);
    QVERIFY(startService(service, serviceThread));
    QDBusConnection client = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnection);

    TerMusicFrameSource source(client);
    QSignalSpy frameSpy(&source, &ExternalFramePort::frameAvailable);
    QSignalSpy stoppedSpy(&source, &ExternalFramePort::stopped);

    // 未选中 Ter-Music 时保持非活跃。
    // Inactive until Ter-Music is selected.
    source.setSelectedPlayer(QStringLiteral("org.mpris.MediaPlayer2.vlc"));
    QVERIFY(!source.active());
    QCOMPARE(frameSpy.count(), 0);

    // 选中后拉取快照：A 槽当前行、B 槽下一行。
    // On selection a snapshot is pulled: slot A current, slot B next.
    service.snapshot = snapshotJson(QStringLiteral("A"), QStringLiteral("First"),
                                    QStringLiteral("Second"), QStringLiteral("track-1"),
                                    true, true, 5);
    source.setSelectedPlayer(QLatin1String(serviceName));
    QTRY_COMPARE(frameSpy.count(), 1);
    QVERIFY(source.active());
    ExternalLyricFrame frame = frameSpy.constFirst().constFirst().value<ExternalLyricFrame>();
    QCOMPARE(frame.currentText, QStringLiteral("First"));
    QCOMPARE(frame.secondaryText, QStringLiteral("Second"));
    QCOMPARE(frame.timing, TimingCapability::Line);
    QCOMPARE(frame.lineIndex, 0);
    QCOMPARE(frame.revision, quint64(5));

    // 双缓冲推进：活动槽切到 B，A 刷新为下下行。
    // Double buffer advances: active slot becomes B, A refreshes.
    QMetaObject::invokeMethod(&service,
                              [&service] { service.publish(snapshotJson(
                                  QStringLiteral("B"), QStringLiteral("Third"),
                                  QStringLiteral("Second"), QStringLiteral("track-1"),
                                  true, true, 6)); },
                              Qt::QueuedConnection);
    QTRY_COMPARE(frameSpy.count(), 2);
    frame = frameSpy.constLast().constFirst().value<ExternalLyricFrame>();
    QCOMPARE(frame.currentText, QStringLiteral("Second"));
    QCOMPARE(frame.secondaryText, QStringLiteral("Third"));
    QCOMPARE(frame.lineIndex, 1);

    // 相同 revision 与乱序（更小）的更新被丢弃。
    // Duplicate and out-of-order (smaller) revisions are dropped.
    QMetaObject::invokeMethod(&service,
                              [&service] { service.publish(snapshotJson(
                                  QStringLiteral("B"), QStringLiteral("Third"),
                                  QStringLiteral("Second"), QStringLiteral("track-1"),
                                  true, true, 6)); },
                              Qt::QueuedConnection);
    QMetaObject::invokeMethod(&service,
                              [&service] { service.publish(snapshotJson(
                                  QStringLiteral("A"), QStringLiteral("First"),
                                  QStringLiteral("Second"), QStringLiteral("track-1"),
                                  true, true, 4)); },
                              Qt::QueuedConnection);
    QTest::qWait(200);
    QCOMPARE(frameSpy.count(), 2);

    stopService(service, serviceThread);
    QDBusConnection::disconnectFromBus(clientConnection);
}

void TerMusicFrameSourceTest::resetsOnTrackChangeAndHandlesPlainLyrics()
{
    const QString serviceConnection = QStringLiteral("ter-music-service-2");
    const QString clientConnection = QStringLiteral("ter-music-client-2");
    QThread serviceThread;
    FakeTerMusicService service(serviceConnection);
    QVERIFY(startService(service, serviceThread));
    QDBusConnection client = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnection);

    TerMusicFrameSource source(client);
    QSignalSpy frameSpy(&source, &ExternalFramePort::frameAvailable);

    // 无时间戳纯文本：A/B 两行固定、timing 为 Plain、lineIndex 无效。
    // Plain lyrics without timestamps: fixed A/B lines, Plain timing, no index.
    service.snapshot = snapshotJson(QStringLiteral("A"), QStringLiteral("Only Line"),
                                    QStringLiteral("Next Line"), QStringLiteral("track-1"),
                                    false, true, 1);
    source.setSelectedPlayer(QLatin1String(serviceName));
    QTRY_COMPARE(frameSpy.count(), 1);
    ExternalLyricFrame frame = frameSpy.constFirst().constFirst().value<ExternalLyricFrame>();
    QCOMPARE(frame.currentText, QStringLiteral("Only Line"));
    QCOMPARE(frame.timing, TimingCapability::Plain);
    QCOMPARE(frame.lineIndex, -1);

    // 切歌（track_id 变化）后新帧被采纳，内容来自新曲目。
    // After a track change (track_id differs) new frames are adopted.
    QMetaObject::invokeMethod(&service,
                              [&service] { service.publish(snapshotJson(
                                  QStringLiteral("A"), QStringLiteral("New Song"),
                                  QStringLiteral("New Next"), QStringLiteral("track-2"),
                                  true, true, 2)); },
                              Qt::QueuedConnection);
    QTRY_COMPARE(frameSpy.count(), 2);
    frame = frameSpy.constLast().constFirst().value<ExternalLyricFrame>();
    QCOMPARE(frame.currentText, QStringLiteral("New Song"));
    QCOMPARE(frame.trackId, QStringLiteral("track-2"));

    stopService(service, serviceThread);
    QDBusConnection::disconnectFromBus(clientConnection);
}

void TerMusicFrameSourceTest::emitsStoppedWhenLyricsAbsentOrPlayerUnselected()
{
    const QString serviceConnection = QStringLiteral("ter-music-service-3");
    const QString clientConnection = QStringLiteral("ter-music-client-3");
    QThread serviceThread;
    FakeTerMusicService service(serviceConnection);
    QVERIFY(startService(service, serviceThread));
    QDBusConnection client = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnection);

    TerMusicFrameSource source(client);
    QSignalSpy stoppedSpy(&source, &ExternalFramePort::stopped);
    service.snapshot = snapshotJson(QStringLiteral("A"), QStringLiteral("First"),
                                    QStringLiteral("Second"), QStringLiteral("track-1"),
                                    true, true, 1);
    source.setSelectedPlayer(QLatin1String(serviceName));
    QTRY_VERIFY(source.active());

    // has_lyrics=false 时发出 stopped。
    // A snapshot without lyrics stops the source.
    QMetaObject::invokeMethod(&service,
                              [&service] { service.publish(snapshotJson(
                                  QStringLiteral("A"), QString(), QString(),
                                  QStringLiteral("track-1"), true, false, 2)); },
                              Qt::QueuedConnection);
    QTRY_COMPARE(stoppedSpy.count(), 1);
    QVERIFY(!source.active());

    // 切走播放器：已停止的源不重复发 stopped（controller 幂等处理）。
    // Selecting another player: an already-stopped source does not re-emit
    // stopped (the controller handles it idempotently).
    source.setSelectedPlayer(QStringLiteral("org.mpris.MediaPlayer2.vlc"));
    QCOMPARE(stoppedSpy.count(), 1);
    QVERIFY(!source.active());

    stopService(service, serviceThread);
    QDBusConnection::disconnectFromBus(clientConnection);
}

QTEST_GUILESS_MAIN(TerMusicFrameSourceTest)
#include "test_termusicframesource.moc"
