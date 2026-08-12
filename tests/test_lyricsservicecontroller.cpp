#include "application/lyricsservicecontroller.h"

#include <QSignalSpy>
#include <QTest>

using namespace deepin::lyrics;

class FakePlayerPort final : public PlayerPort
{
    Q_OBJECT

public:
    using PlayerPort::PlayerPort;

    void start() override { started = true; }
    QList<PlayerDescriptor> availablePlayers() const override { return players; }
    QString selectedPlayer() const override { return selected; }
    PlayerSnapshot snapshot() const override { return currentSnapshot; }
    bool selectedPlayerAvailable() const override { return selectedAvailable; }
    void setSelectedPlayer(const QString &busName) override { selected = busName; }

    void publishPlayers(const QList<PlayerDescriptor> &value)
    {
        players = value;
        selectedAvailable = std::any_of(players.cbegin(), players.cend(), [this](const auto &player) {
            return player.busName == selected;
        });
        emit availablePlayersChanged(players);
        emit selectedPlayerAvailableChanged(selectedAvailable);
    }

    void publishTrack(const TrackIdentity &track)
    {
        currentSnapshot.busName = selected;
        currentSnapshot.track = track;
        emit snapshotChanged(currentSnapshot);
        emit trackChanged(track);
    }

    void publishRawTrack(const TrackIdentity &track)
    {
        currentSnapshot.busName = selected;
        currentSnapshot.track = track;
        emit snapshotChanged(currentSnapshot);
    }

    QList<PlayerDescriptor> players;
    QString selected;
    PlayerSnapshot currentSnapshot;
    bool selectedAvailable = false;
    bool started = false;
};

class MemorySettingsPort final : public SettingsPort
{
public:
    bool enabled() const override { return enabledValue; }
    QString playerBusName() const override { return playerValue; }
    int offsetMs() const override { return offsetValue; }
    void setEnabled(bool value) override { enabledValue = value; }
    void setPlayerBusName(const QString &value) override { playerValue = value; }
    void setOffsetMs(int value) override { offsetValue = value; }

    bool enabledValue = false;
    QString playerValue;
    int offsetValue = 0;
};

class NullLogSink final : public LogSink
{
public:
    void write(LogLevel, const LogEvent &) override { }
};

class LyricsServiceControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void followsCoreStateTransitions();
    void validatesPublicInputs();
    void sessionHiddenIsNotPersisted();
    void suppressesPositionOnlyStateUpdates();
    void waitsForStableTrackBeforeLookup();
    void clearsFrameWhenRawTrackChanges();
};

void LyricsServiceControllerTest::followsCoreStateTransitions()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    NullLogSink sink;
    LogEngine logger(sink);
    LyricsServiceController controller(player, settings, logger);

    controller.start();
    QVERIFY(player.started);
    QCOMPARE(controller.status(), ServiceStatus::Disabled);

    QVERIFY(controller.setEnabled(true));
    QCOMPARE(controller.status(), ServiceStatus::WaitingForPlayer);

    player.publishPlayers({{QStringLiteral("org.mpris.MediaPlayer2.demo"),
                            QStringLiteral("Demo"), {}, true}});
    QVERIFY(controller.setPlayer(QStringLiteral("org.mpris.MediaPlayer2.demo")));
    player.selectedAvailable = true;
    emit player.selectedPlayerAvailableChanged(true);
    QCOMPARE(controller.status(), ServiceStatus::WaitingForTrack);

    TrackIdentity track;
    track.title = QStringLiteral("Never logged title");
    track.artists = {QStringLiteral("Never logged artist")};
    track.durationMs = 123000;
    track.playerBusName = player.selected;
    track.searchable = true;
    player.publishTrack(track);
    QCOMPARE(controller.status(), ServiceStatus::LookingUpLyrics);
    QVERIFY(controller.state().value(QStringLiteral("canSearchCandidates")).toBool());

    QVERIFY(controller.setEnabled(false));
    QCOMPARE(controller.status(), ServiceStatus::Disabled);
}

void LyricsServiceControllerTest::validatesPublicInputs()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    NullLogSink sink;
    LogEngine logger(sink);
    LyricsServiceController controller(player, settings, logger);
    controller.start();

    QString errorCode;
    QVERIFY(!controller.setPlayer(QStringLiteral("org.mpris.MediaPlayer2.unknown"), &errorCode));
    QCOMPARE(errorCode, QStringLiteral("invalid-player"));

    errorCode.clear();
    QVERIFY(!controller.setOffsetMs(10001, &errorCode));
    QCOMPARE(errorCode, QStringLiteral("offset-out-of-range"));
    QCOMPARE(settings.offsetValue, 0);

    errorCode.clear();
    QVERIFY(!controller.selectCandidate(QStringLiteral("other"), QStringLiteral("123"), &errorCode));
    QCOMPARE(errorCode, QStringLiteral("invalid-candidate"));
}

void LyricsServiceControllerTest::sessionHiddenIsNotPersisted()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    NullLogSink sink;
    LogEngine logger(sink);
    LyricsServiceController first(player, settings, logger);
    first.start();
    first.setSessionHidden(true);
    QVERIFY(first.state().value(QStringLiteral("sessionHidden")).toBool());

    FakePlayerPort restartedPlayer;
    LyricsServiceController restarted(restartedPlayer, settings, logger);
    restarted.start();
    QVERIFY(!restarted.state().value(QStringLiteral("sessionHidden")).toBool());
}

void LyricsServiceControllerTest::suppressesPositionOnlyStateUpdates()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    NullLogSink sink;
    LogEngine logger(sink);
    LyricsServiceController controller(player, settings, logger);
    QSignalSpy stateSpy(&controller, &LyricsServiceController::stateChanged);
    controller.start();
    stateSpy.clear();

    player.currentSnapshot.positionMs = 500;
    emit player.snapshotChanged(player.currentSnapshot);
    QCOMPARE(stateSpy.count(), 0);
}

void LyricsServiceControllerTest::waitsForStableTrackBeforeLookup()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    NullLogSink sink;
    LogEngine logger(sink);
    LyricsServiceController controller(player, settings, logger);
    controller.start();
    controller.setEnabled(true);
    player.publishPlayers({{QStringLiteral("org.mpris.MediaPlayer2.demo"),
                            QStringLiteral("Demo"), {}, true}});
    controller.setPlayer(QStringLiteral("org.mpris.MediaPlayer2.demo"));
    player.selectedAvailable = true;
    emit player.selectedPlayerAvailableChanged(true);

    TrackIdentity track;
    track.title = QStringLiteral("Private title");
    track.artists = {QStringLiteral("Private artist")};
    track.durationMs = 90000;
    track.playerBusName = player.selected;
    track.searchable = true;
    player.publishRawTrack(track);
    QCOMPARE(controller.status(), ServiceStatus::WaitingForTrack);

    emit player.trackChanged(track);
    QCOMPARE(controller.status(), ServiceStatus::LookingUpLyrics);
}

void LyricsServiceControllerTest::clearsFrameWhenRawTrackChanges()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    NullLogSink sink;
    LogEngine logger(sink);
    LyricsServiceController controller(player, settings, logger);
    QSignalSpy frameSpy(&controller, &LyricsServiceController::frameChanged);
    controller.start();
    controller.setEnabled(true);
    player.publishPlayers({{QStringLiteral("org.mpris.MediaPlayer2.demo"),
                            QStringLiteral("Demo"), {}, true}});
    controller.setPlayer(QStringLiteral("org.mpris.MediaPlayer2.demo"));
    player.selectedAvailable = true;
    emit player.selectedPlayerAvailableChanged(true);
    frameSpy.clear();

    TrackIdentity track;
    track.title = QStringLiteral("Private title");
    track.artists = {QStringLiteral("Private artist")};
    track.durationMs = 90000;
    track.playerBusName = player.selected;
    track.searchable = true;
    player.publishRawTrack(track);

    QCOMPARE(frameSpy.count(), 1);
    const QVariantMap frame = frameSpy.constFirst().constFirst().toMap();
    QCOMPARE(frame.value(QStringLiteral("currentText")).toString(), QString());
    QCOMPARE(frame.value(QStringLiteral("lineIndex")).toInt(), -1);
    QCOMPARE(frame.value(QStringLiteral("timingCapability")).toString(), QStringLiteral("none"));
}

QTEST_MAIN(LyricsServiceControllerTest)
#include "test_lyricsservicecontroller.moc"
