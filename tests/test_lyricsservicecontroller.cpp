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
    qint64 selectedPlayerProcessId() const override { return selectedProcessId; }
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
    qint64 selectedProcessId = 0;
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

class FakeLyricsPort final : public LyricsPort
{
    Q_OBJECT

public:
    using LyricsPort::LyricsPort;

    void search(const TrackIdentity &track) override { lastTrack = track; }
    void searchCandidates(const TrackIdentity &) override { }
    void selectCandidate(const QString &, const QString &) override { }
    void clearCache() override { }

    void publishLyrics(const QString &syncedLyrics, const QString &plainLyrics = {})
    {
        LyricPayload payload;
        payload.providerId = QStringLiteral("lrclib");
        payload.syncedLyrics = syncedLyrics;
        payload.plainLyrics = plainLyrics;
        payload.timing = syncedLyrics.isEmpty() ? TimingCapability::Plain : TimingCapability::Line;
        emit lyricsReady(payload);
    }

    TrackIdentity lastTrack;
};

class NullLogSink final : public LogSink
{
public:
    void write(LogLevel, const LogEvent &) override { }
};

class FakeChineseScriptConverter final : public ChineseScriptConverter
{
public:
    std::optional<QString> toSimplified(const QString &text) const override
    {
        if (failConversion)
            return std::nullopt;
        QString result = text;
        result.replace(QStringLiteral("後來"), QStringLiteral("后来"));
        result.replace(QStringLiteral("學會"), QStringLiteral("学会"));
        return result;
    }

    bool failConversion = false;
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
    void publishesFramesForPositionPauseSeekAndOffset();
    void restartsLookupAfterReenable();
    void convertsTraditionalLyricsBeforeParsing();
    void keepsSourceLyricsWhenConversionFails();
};

void LyricsServiceControllerTest::followsCoreStateTransitions()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    NullLogSink sink;
    LogEngine logger(sink);
    FakeLyricsPort lyrics;
    LyricsServiceController controller(player, settings, logger, &lyrics);

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
    QCOMPARE(controller.state().value(QStringLiteral("timingCapability")).toString(),
             QStringLiteral("none"));
    QVERIFY(controller.state().value(QStringLiteral("lyricsSource")).toString().isEmpty());
    QVERIFY(controller.state().value(QStringLiteral("candidates")).toList().isEmpty());

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
    FakeLyricsPort lyrics;
    LyricsServiceController controller(player, settings, logger, &lyrics);
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
    player.publishTrack(track);
    player.currentSnapshot.positionMs = 1500;
    emit player.snapshotChanged(player.currentSnapshot);
    lyrics.publishLyrics(QStringLiteral("[00:01.00]Old lyric\n[00:03.00]Next lyric"));
    frameSpy.clear();

    TrackIdentity replacement = track;
    replacement.title = QStringLiteral("Replacement title");
    player.publishRawTrack(replacement);

    QCOMPARE(frameSpy.count(), 1);
    const QVariantMap frame = frameSpy.constFirst().constFirst().toMap();
    QCOMPARE(frame.value(QStringLiteral("currentText")).toString(), QString());
    QCOMPARE(frame.value(QStringLiteral("lineIndex")).toInt(), -1);
    QCOMPARE(frame.value(QStringLiteral("timingCapability")).toString(), QStringLiteral("none"));
}

void LyricsServiceControllerTest::publishesFramesForPositionPauseSeekAndOffset()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    NullLogSink sink;
    LogEngine logger(sink);
    FakeLyricsPort lyrics;
    LyricsServiceController controller(player, settings, logger, &lyrics);
    QSignalSpy frameSpy(&controller, &LyricsServiceController::frameChanged);

    settings.enabledValue = true;
    settings.playerValue = QStringLiteral("org.mpris.MediaPlayer2.demo");
    player.selected = settings.playerValue;
    player.players = {{player.selected, QStringLiteral("Demo"), {}, true}};
    player.selectedAvailable = true;
    controller.start();

    TrackIdentity track;
    track.title = QStringLiteral("Private title");
    track.artists = {QStringLiteral("Private artist")};
    track.durationMs = 90000;
    track.playerBusName = player.selected;
    track.searchable = true;
    player.currentSnapshot.positionMs = 1500;
    player.currentSnapshot.playbackStatus = PlaybackStatus::Playing;
    player.publishTrack(track);
    lyrics.publishLyrics(QStringLiteral("[00:01.00]First\n[00:03.00]Second\n[00:05.00]Last"));

    QVariantMap frame = frameSpy.constLast().constFirst().toMap();
    QCOMPARE(frame.value(QStringLiteral("currentText")).toString(), QStringLiteral("First"));
    QCOMPARE(frame.value(QStringLiteral("lineProgress")).toDouble(), 0.25);
    QCOMPARE(frame.value(QStringLiteral("timingCapability")).toString(), QStringLiteral("line"));
    QCOMPARE(frame.value(QStringLiteral("source")).toString(), QStringLiteral("lrclib"));
    QVERIFY(!frame.value(QStringLiteral("trackKey")).toString().isEmpty());

    const int playingFrameCount = frameSpy.count();
    player.currentSnapshot.playbackStatus = PlaybackStatus::Paused;
    emit player.snapshotChanged(player.currentSnapshot);
    QCOMPARE(frameSpy.count(), playingFrameCount);

    player.currentSnapshot.positionMs = 5200;
    emit player.snapshotChanged(player.currentSnapshot);
    frame = frameSpy.constLast().constFirst().toMap();
    QCOMPARE(frame.value(QStringLiteral("currentText")).toString(), QStringLiteral("Last"));
    QCOMPARE(frame.value(QStringLiteral("lineProgress")).toDouble(), 1.0);

    player.currentSnapshot.positionMs = 1500;
    emit player.snapshotChanged(player.currentSnapshot);
    frame = frameSpy.constLast().constFirst().toMap();
    QCOMPARE(frame.value(QStringLiteral("currentText")).toString(), QStringLiteral("First"));

    QVERIFY(controller.setOffsetMs(1500));
    frame = frameSpy.constLast().constFirst().toMap();
    QCOMPARE(frame.value(QStringLiteral("currentText")).toString(), QStringLiteral("Second"));
    QCOMPARE(frame.value(QStringLiteral("lineProgress")).toDouble(), 0.0);
}

void LyricsServiceControllerTest::restartsLookupAfterReenable()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    NullLogSink sink;
    LogEngine logger(sink);
    FakeLyricsPort lyrics;
    LyricsServiceController controller(player, settings, logger, &lyrics);

    settings.enabledValue = true;
    settings.playerValue = QStringLiteral("org.mpris.MediaPlayer2.demo");
    player.selected = settings.playerValue;
    player.players = {{player.selected, QStringLiteral("Demo"), {}, true}};
    player.selectedAvailable = true;
    controller.start();

    TrackIdentity track;
    track.title = QStringLiteral("Private title");
    track.playerBusName = player.selected;
    track.searchable = true;
    player.publishTrack(track);
    QCOMPARE(lyrics.lastTrack.title, track.title);

    lyrics.lastTrack = {};
    QVERIFY(controller.setEnabled(false));
    QVERIFY(controller.setEnabled(true));
    QCOMPARE(controller.status(), ServiceStatus::LookingUpLyrics);
    QCOMPARE(lyrics.lastTrack.title, track.title);
}

void LyricsServiceControllerTest::convertsTraditionalLyricsBeforeParsing()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    NullLogSink sink;
    LogEngine logger(sink);
    FakeLyricsPort lyrics;
    FakeChineseScriptConverter converter;
    LyricsServiceController controller(player, settings, logger, converter, &lyrics);
    QSignalSpy frameSpy(&controller, &LyricsServiceController::frameChanged);

    settings.enabledValue = true;
    settings.playerValue = QStringLiteral("org.mpris.MediaPlayer2.demo");
    player.selected = settings.playerValue;
    player.players = {{player.selected, QStringLiteral("Demo"), {}, true}};
    player.selectedAvailable = true;
    controller.start();

    TrackIdentity track;
    track.title = QStringLiteral("Private title");
    track.artists = {QStringLiteral("Private artist")};
    track.durationMs = 90000;
    track.playerBusName = player.selected;
    track.searchable = true;
    player.currentSnapshot.positionMs = 1500;
    player.publishTrack(track);
    lyrics.publishLyrics(QStringLiteral("[00:01.00]後來我總算學會\n[00:03.00]Next"));

    const QVariantMap frame = frameSpy.constLast().constFirst().toMap();
    QCOMPARE(frame.value(QStringLiteral("currentText")).toString(),
             QStringLiteral("后来我總算学会"));
}

void LyricsServiceControllerTest::keepsSourceLyricsWhenConversionFails()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    NullLogSink sink;
    LogEngine logger(sink);
    FakeLyricsPort lyrics;
    FakeChineseScriptConverter converter;
    converter.failConversion = true;
    LyricsServiceController controller(player, settings, logger, converter, &lyrics);
    QSignalSpy frameSpy(&controller, &LyricsServiceController::frameChanged);

    settings.enabledValue = true;
    settings.playerValue = QStringLiteral("org.mpris.MediaPlayer2.demo");
    player.selected = settings.playerValue;
    player.players = {{player.selected, QStringLiteral("Demo"), {}, true}};
    player.selectedAvailable = true;
    controller.start();

    TrackIdentity track;
    track.title = QStringLiteral("Private title");
    track.artists = {QStringLiteral("Private artist")};
    track.durationMs = 90000;
    track.playerBusName = player.selected;
    track.searchable = true;
    player.currentSnapshot.positionMs = 1500;
    player.publishTrack(track);
    lyrics.publishLyrics(QStringLiteral("[00:01.00]後來仍是原文\n[00:03.00]Next"));

    const QVariantMap frame = frameSpy.constLast().constFirst().toMap();
    QCOMPARE(frame.value(QStringLiteral("currentText")).toString(),
             QStringLiteral("後來仍是原文"));
}

QTEST_MAIN(LyricsServiceControllerTest)
#include "test_lyricsservicecontroller.moc"
