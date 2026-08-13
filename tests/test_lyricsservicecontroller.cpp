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
    bool audioVisualizerEnabled() const override { return audioVisualizerEnabledValue; }
    QString lyricLayout() const override { return lyricLayoutValue; }
    void setEnabled(bool value) override { enabledValue = value; }
    void setPlayerBusName(const QString &value) override { playerValue = value; }
    void setOffsetMs(int value) override { offsetValue = value; }
    void setAudioVisualizerEnabled(bool value) override { audioVisualizerEnabledValue = value; }
    void setLyricLayout(const QString &value) override { lyricLayoutValue = value; }

    bool enabledValue = false;
    QString playerValue;
    int offsetValue = 0;
    bool audioVisualizerEnabledValue = false;
    QString lyricLayoutValue = QStringLiteral("classic");
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

class FakeAudioVisualizerPort final : public AudioVisualizerPort
{
    Q_OBJECT

public:
    using AudioVisualizerPort::AudioVisualizerPort;

    void setEnabled(bool enabled) override { enabledValue = enabled; }
    void setPlayerProcessId(qint64 processId) override { lastProcessId = processId; }
    VisualizerState state() const override { return currentState; }
    StreamMatchConfidence matchConfidence() const override { return confidence; }
    void stop() override
    {
        ++stopCount;
        enabledValue = false;
        currentState = VisualizerState::Disabled;
    }

    bool enabledValue = false;
    qint64 lastProcessId = 0;
    int stopCount = 0;
    VisualizerState currentState = VisualizerState::Disabled;
    StreamMatchConfidence confidence = StreamMatchConfidence::None;
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

class FakeExternalFramePort final : public ExternalFramePort
{
    Q_OBJECT

public:
    using ExternalFramePort::ExternalFramePort;

    void setSelectedPlayer(const QString &busName) override
    {
        lastSelected = busName;
        activeValue = busName == QStringLiteral("org.mpris.MediaPlayer2.ter_music");
    }
    bool active() const override { return activeValue; }

    void publishFrame(const ExternalLyricFrame &frame) { emit frameAvailable(frame); }
    void publishStopped()
    {
        activeValue = false;
        emit stopped();
    }

    QString lastSelected;
    bool activeValue = false;
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
    void keepsVisualizerActiveWhenExplicitlyEnabled();
    void keepsVisualizerDisabledByDefault();
    void publishesExternalFramesAndSkipsLookup();
    void restoresLookupAfterExternalFrameStops();
    void derivesExternalLineProgressFromPosition();
    void validatesLyricLayoutValue();
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
    // 播放位置现在需要供详情弹窗进度条使用，因此位置变化会发布状态。
    // Position changes are published because the details popup uses them for playback progress.
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(controller.state().value(QStringLiteral("positionMs")).toLongLong(), 500);
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

void LyricsServiceControllerTest::keepsVisualizerActiveWhenExplicitlyEnabled()
{
    FakePlayerPort player;
    player.selected = QStringLiteral("org.mpris.MediaPlayer2.demo");
    player.selectedAvailable = true;
    player.selectedProcessId = 4321;
    player.currentSnapshot.playbackStatus = PlaybackStatus::Playing;
    player.currentSnapshot.track = {
        QStringLiteral("Song"), {QStringLiteral("Artist")}, {}, 180000,
        player.selected, true};
    MemorySettingsPort settings;
    settings.enabledValue = true;
    settings.audioVisualizerEnabledValue = true;
    FakeLyricsPort lyrics;
    FakeAudioVisualizerPort visualizer;
    NullLogSink sink;
    LogEngine logger(sink);
    FakeChineseScriptConverter converter;
    LyricsServiceController controller(player, settings, logger, converter, &lyrics, &visualizer);
    controller.start();

    player.publishTrack(player.currentSnapshot.track);
    QTRY_VERIFY(visualizer.enabledValue);
    QCOMPARE(visualizer.lastProcessId, 4321);

    emit lyrics.noLyrics();
    QTRY_VERIFY(visualizer.enabledValue);

    lyrics.publishLyrics(QStringLiteral("[00:01.00]Line"));
    QTRY_VERIFY(visualizer.enabledValue);

    emit lyrics.failed(QStringLiteral("provider-failed"));
    QTRY_VERIFY(visualizer.enabledValue);
    QCOMPARE(visualizer.lastProcessId, 4321);

    QVERIFY(controller.setAudioVisualizerEnabled(false));
    // With the option off, fallback visualization remains active while lyrics are unavailable.
    // 关闭选项后，在歌词不可用状态仍保留可视化作为降级显示。
    QVERIFY(visualizer.enabledValue);
}

void LyricsServiceControllerTest::keepsVisualizerDisabledByDefault()
{
    FakePlayerPort player;
    player.selected = QStringLiteral("org.mpris.MediaPlayer2.demo");
    player.selectedAvailable = true;
    player.selectedProcessId = 4321;
    player.currentSnapshot.playbackStatus = PlaybackStatus::Playing;
    player.currentSnapshot.track.searchable = true;
    MemorySettingsPort settings;
    settings.enabledValue = true;
    settings.audioVisualizerEnabledValue = false;
    FakeLyricsPort lyrics;
    FakeAudioVisualizerPort visualizer;
    NullLogSink sink;
    LogEngine logger(sink);
    FakeChineseScriptConverter converter;
    LyricsServiceController controller(player, settings, logger, converter, &lyrics, &visualizer);

    controller.start();
    player.publishTrack(player.currentSnapshot.track);
    emit lyrics.noLyrics();

    QVERIFY(visualizer.enabledValue);
    QCOMPARE(controller.state().value(QStringLiteral("audioVisualizerEnabled")).toBool(), false);
}

void LyricsServiceControllerTest::publishesExternalFramesAndSkipsLookup()
{
    FakePlayerPort player;
    player.selected = QStringLiteral("org.mpris.MediaPlayer2.ter_music");
    player.selectedAvailable = true;
    player.currentSnapshot.playbackStatus = PlaybackStatus::Playing;
    player.currentSnapshot.track.searchable = true;
    MemorySettingsPort settings;
    settings.enabledValue = true;
    settings.playerValue = QStringLiteral("org.mpris.MediaPlayer2.ter_music");
    FakeLyricsPort lyrics;
    FakeExternalFramePort external;
    NullLogSink sink;
    LogEngine logger(sink);
    LyricsServiceController controller(player, settings, logger, nullptr, nullptr);
    controller.setExternalFramePort(&external);
    QSignalSpy frameSpy(&controller, &LyricsServiceController::frameChanged);

    controller.start();
    QCOMPARE(external.lastSelected, QStringLiteral("org.mpris.MediaPlayer2.ter_music"));

    // 外部帧到达：直接发布，来源 ter-music，状态 LyricsReady。
    // An external frame is published as-is with source ter-music.
    ExternalLyricFrame frame;
    frame.currentText = QStringLiteral("Line A");
    frame.secondaryText = QStringLiteral("Line B");
    frame.lineIndex = 0;
    frame.timing = TimingCapability::Line;
    frame.revision = 1;
    external.publishFrame(frame);
    QCOMPARE(frameSpy.count(), 1);
    const QVariantMap published = frameSpy.constFirst().constFirst().toMap();
    QCOMPARE(published.value(QStringLiteral("currentText")).toString(), QStringLiteral("Line A"));
    QCOMPARE(published.value(QStringLiteral("secondaryText")).toString(), QStringLiteral("Line B"));
    QCOMPARE(published.value(QStringLiteral("source")).toString(), QStringLiteral("ter-music"));
    QCOMPARE(controller.status(), ServiceStatus::LyricsReady);

    // 外部源活跃时切歌：跳过 LRCLIB 查询链。
    // While the external source is active, track changes skip the lookup chain.
    TrackIdentity track;
    track.title = QStringLiteral("New Song");
    track.artists = {QStringLiteral("Artist")};
    track.searchable = true;
    player.publishTrack(track);
    QCOMPARE(lyrics.lastTrack.title, QString());
}

void LyricsServiceControllerTest::restoresLookupAfterExternalFrameStops()
{
    FakePlayerPort player;
    player.selected = QStringLiteral("org.mpris.MediaPlayer2.ter_music");
    player.selectedAvailable = true;
    player.currentSnapshot.playbackStatus = PlaybackStatus::Playing;
    player.currentSnapshot.track.searchable = true;
    MemorySettingsPort settings;
    settings.enabledValue = true;
    FakeLyricsPort lyrics;
    FakeExternalFramePort external;
    NullLogSink sink;
    LogEngine logger(sink);
    LyricsServiceController controller(player, settings, logger, &lyrics, nullptr);
    controller.setExternalFramePort(&external);
    controller.start();

    ExternalLyricFrame frame;
    frame.currentText = QStringLiteral("Line A");
    frame.secondaryText = QStringLiteral("Line B");
    frame.timing = TimingCapability::Line;
    external.publishFrame(frame);
    QCOMPARE(controller.status(), ServiceStatus::LyricsReady);

    // 外部源停止：回到常规状态机，后续切歌恢复 LRCLIB 查询。
    // After the source stops, the regular state machine resumes and track
    // changes trigger the lookup chain again.
    external.publishStopped();
    TrackIdentity track;
    track.title = QStringLiteral("New Song");
    track.artists = {QStringLiteral("Artist")};
    track.searchable = true;
    player.publishTrack(track);
    QCOMPARE(lyrics.lastTrack.title, QStringLiteral("New Song"));
}

void LyricsServiceControllerTest::validatesLyricLayoutValue()
{
    FakePlayerPort player;
    MemorySettingsPort settings;
    settings.enabledValue = true;
    FakeLyricsPort lyrics;
    NullLogSink sink;
    LogEngine logger(sink);
    LyricsServiceController controller(player, settings, logger, &lyrics, nullptr);
    controller.start();

    // 默认经典布局；无效值被拒绝且不写入设置。
    // Classic by default; invalid values are rejected and never persisted.
    QCOMPARE(controller.state().value(QStringLiteral("lyricLayout")).toString(),
             QStringLiteral("classic"));

    QString errorCode;
    QVERIFY(!controller.setLyricLayout(QStringLiteral("sideways"), &errorCode));
    QCOMPARE(errorCode, QStringLiteral("invalid-lyric-layout"));
    QCOMPARE(controller.state().value(QStringLiteral("lyricLayout")).toString(),
             QStringLiteral("classic"));
    QCOMPARE(settings.lyricLayoutValue, QStringLiteral("classic"));

    QVERIFY(controller.setLyricLayout(QStringLiteral("karaoke")));
    QCOMPARE(controller.state().value(QStringLiteral("lyricLayout")).toString(),
             QStringLiteral("karaoke"));
    QCOMPARE(settings.lyricLayoutValue, QStringLiteral("karaoke"));
}

void LyricsServiceControllerTest::derivesExternalLineProgressFromPosition()
{
    FakePlayerPort player;
    player.selected = QStringLiteral("org.mpris.MediaPlayer2.ter_music");
    player.selectedAvailable = true;
    player.currentSnapshot.playbackStatus = PlaybackStatus::Playing;
    player.currentSnapshot.positionMs = 4000;
    player.currentSnapshot.track.searchable = true;
    MemorySettingsPort settings;
    settings.enabledValue = true;
    settings.playerValue = QStringLiteral("org.mpris.MediaPlayer2.ter_music");
    FakeLyricsPort lyrics;
    FakeExternalFramePort external;
    NullLogSink sink;
    LogEngine logger(sink);
    LyricsServiceController controller(player, settings, logger, &lyrics, nullptr);
    controller.setExternalFramePort(&external);
    QSignalSpy frameSpy(&controller, &LyricsServiceController::frameChanged);
    controller.start();

    // 外部帧只带行时间轴；染色进度由播放位置推算。
    // External frames carry only the line timeline; highlight progress is
    // derived from the playback position.
    ExternalLyricFrame frame;
    frame.currentText = QStringLiteral("Line A");
    frame.secondaryText = QStringLiteral("Line B");
    frame.lineIndex = 0;
    frame.timing = TimingCapability::Line;
    frame.currentLineStartMs = 2000;
    frame.nextLineStartMs = 6000;
    frame.revision = 1;
    external.publishFrame(frame);
    QCOMPARE(frameSpy.count(), 1);
    QCOMPARE(frameSpy.constFirst().constFirst().toMap()
                 .value(QStringLiteral("lineProgress")).toDouble(), 0.5);

    // 位置推进后轮询路径刷新染色。
    // Position advances refresh the highlight through the polling path.
    player.currentSnapshot.positionMs = 5000;
    player.publishRawTrack(player.currentSnapshot.track);
    QTRY_VERIFY(frameSpy.count() >= 2);
    QCOMPARE(frameSpy.constLast().constFirst().toMap()
                 .value(QStringLiteral("lineProgress")).toDouble(), 0.75);
}

QTEST_MAIN(LyricsServiceControllerTest)
#include "test_lyricsservicecontroller.moc"
