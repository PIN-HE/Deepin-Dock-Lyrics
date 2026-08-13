#include "application/lyricsservicecontroller.h"

#include <lyricscore/lrcparser.h>
#include <lyricscore/lyricsync.h>
#include <lyricscore/normalization.h>

#include <QSet>

#include <algorithm>

namespace deepin::lyrics {

namespace {

QVariantList playerMaps(const QList<PlayerDescriptor> &players)
{
    QVariantList result;
    for (const auto &player : players) {
        result.append(QVariantMap{
            {QStringLiteral("busName"), player.busName},
            {QStringLiteral("identity"), player.identity},
            {QStringLiteral("desktopEntry"), player.desktopEntry},
            {QStringLiteral("available"), player.available},
        });
    }
    return result;
}

QString timingName(TimingCapability timing)
{
    switch (timing) {
    case TimingCapability::Plain:
        return QStringLiteral("plain");
    case TimingCapability::Line:
        return QStringLiteral("line");
    case TimingCapability::None:
        return QStringLiteral("none");
    }
    return QStringLiteral("none");
}

QString visualizerStateName(VisualizerState state)
{
    switch (state) {
    case VisualizerState::Disabled:
        return QStringLiteral("disabled");
    case VisualizerState::WaitingForPlayer:
        return QStringLiteral("waiting-for-player");
    case VisualizerState::ResolvingAudioStream:
        return QStringLiteral("resolving-audio-stream");
    case VisualizerState::Active:
        return QStringLiteral("active");
    case VisualizerState::Unavailable:
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

QVariantList visualizerLevels(const VisualizerFrame &frame)
{
    QVariantList levels;
    levels.reserve(visualizerBandCount);
    for (const float level : frame.levels)
        levels.append(qBound(0.0, double(level), 1.0));
    return levels;
}

QVariantMap frameMap(const LyricFrame &frame, const QString &source)
{
    return {
        {QStringLiteral("previousText"), frame.previousText},
        {QStringLiteral("currentText"), frame.currentText},
        {QStringLiteral("secondaryText"), frame.secondaryText},
        {QStringLiteral("translationText"), frame.translationText},
        {QStringLiteral("lineIndex"), frame.lineIndex},
        {QStringLiteral("lineProgress"), frame.lineProgress},
        {QStringLiteral("timingCapability"), timingName(frame.timing)},
        {QStringLiteral("source"), source},
        {QStringLiteral("trackKey"), source.isEmpty() ? QString() : makeTrackKey(frame.track)},
    };
}

QString stableErrorCode(const QString &errorCode)
{
    static const QSet<QString> allowed{
        QStringLiteral("network-unavailable"),
        QStringLiteral("rate-limited"),
        QStringLiteral("database-failed"),
        QStringLiteral("provider-failed"),
    };
    return allowed.contains(errorCode) ? errorCode : QStringLiteral("provider-failed");
}

} // namespace

LyricsServiceController::LyricsServiceController(PlayerPort &player,
                                                 SettingsPort &settings,
                                                 LogEngine &logger,
                                                 LyricsPort *lyrics,
                                                 QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_settings(settings)
    , m_logger(logger)
    , m_lyrics(lyrics)
{
    connect(&m_player, &PlayerPort::availablePlayersChanged,
            this, &LyricsServiceController::onAvailablePlayersChanged);
    connect(&m_player, &PlayerPort::snapshotChanged,
            this, &LyricsServiceController::onSnapshotChanged);
    connect(&m_player, &PlayerPort::selectedPlayerAvailableChanged,
            this, &LyricsServiceController::onSelectedPlayerAvailableChanged);
    connect(&m_player, &PlayerPort::selectedPlayerProcessIdChanged,
            this, &LyricsServiceController::onSelectedPlayerProcessIdChanged);
    connect(&m_player, &PlayerPort::trackChanged,
            this, &LyricsServiceController::onTrackChanged);

    if (m_lyrics) {
        connect(m_lyrics, &LyricsPort::lyricsReady, this, &LyricsServiceController::onLyricsReady);
        connect(m_lyrics, &LyricsPort::noLyrics, this, &LyricsServiceController::onNoLyrics);
        connect(m_lyrics, &LyricsPort::candidatesChanged,
                this, &LyricsServiceController::onCandidatesChanged);
        connect(m_lyrics, &LyricsPort::failed, this, &LyricsServiceController::onLyricsFailed);
    }
}

void LyricsServiceController::setExternalFramePort(ExternalFramePort *externalFrame)
{
    m_externalFrame = externalFrame;
    if (!m_externalFrame)
        return;
    connect(m_externalFrame, &ExternalFramePort::frameAvailable,
            this, &LyricsServiceController::onExternalFrameAvailable);
    connect(m_externalFrame, &ExternalFramePort::stopped,
            this, &LyricsServiceController::onExternalFrameStopped);
}

LyricsServiceController::LyricsServiceController(PlayerPort &player,
                                                 SettingsPort &settings,
                                                 LogEngine &logger,
                                                 ChineseScriptConverter &scriptConverter,
                                                 LyricsPort *lyrics,
                                                 QObject *parent)
    : LyricsServiceController(player, settings, logger, lyrics, parent)
{
    m_scriptConverter = &scriptConverter;
}

LyricsServiceController::LyricsServiceController(PlayerPort &player,
                                                 SettingsPort &settings,
                                                 LogEngine &logger,
                                                 ChineseScriptConverter &scriptConverter,
                                                 LyricsPort *lyrics,
                                                 AudioVisualizerPort *visualizer,
                                                 QObject *parent)
    : LyricsServiceController(player, settings, logger, scriptConverter, lyrics, parent)
{
    m_visualizer = visualizer;
    if (m_visualizer) {
        connect(m_visualizer, &AudioVisualizerPort::stateChanged,
                this, &LyricsServiceController::onVisualizerStateChanged);
        connect(m_visualizer, &AudioVisualizerPort::frameChanged,
                this, &LyricsServiceController::onVisualizerFrameChanged);
    }
}

void LyricsServiceController::start()
{
    if (m_started)
        return;
    m_started = true;
    m_enabled = m_settings.enabled();
    m_offsetMs = std::clamp(m_settings.offsetMs(), -10000, 10000);
    m_audioVisualizerEnabled = m_settings.audioVisualizerEnabled();
    m_lyricLayout = m_settings.lyricLayout();
    m_player.setSelectedPlayer(m_settings.playerBusName());
    m_player.start();
    m_players = m_player.availablePlayers();
    m_snapshot = m_player.snapshot();
    syncExternalFramePlayer();
    refreshStatus();
    refreshVisualizer();
    publishState();
}

QVariantMap LyricsServiceController::state() const
{
    const TrackIdentity &track = m_snapshot.track;
    return {
        {QStringLiteral("status"), statusName(m_status)},
        {QStringLiteral("enabled"), m_enabled},
        {QStringLiteral("sessionHidden"), m_sessionHidden},
        {QStringLiteral("playerBusName"), m_player.selectedPlayer()},
        {QStringLiteral("availablePlayers"), playerMaps(m_players)},
        {QStringLiteral("trackTitle"), track.title},
        {QStringLiteral("trackArtists"), track.artists},
        {QStringLiteral("trackAlbum"), track.album},
        {QStringLiteral("trackArtUrl"), track.artUrl},
        {QStringLiteral("durationMs"), track.durationMs},
        {QStringLiteral("positionMs"), m_snapshot.positionMs},
        {QStringLiteral("offsetMs"), m_offsetMs},
        {QStringLiteral("audioVisualizerEnabled"), m_audioVisualizerEnabled},
        {QStringLiteral("lyricLayout"), m_lyricLayout},
        {QStringLiteral("visualizerState"), visualizerStateName(m_visualizerState)},
        {QStringLiteral("visualizerAvailable"), m_visualizerState == VisualizerState::Active},
        {QStringLiteral("visualizerLevels"), visualizerLevels(m_visualizerFrame)},
        {QStringLiteral("errorCode"), m_errorCode},
        {QStringLiteral("canSearchCandidates"), m_enabled && currentTrackSearchable()},
        {QStringLiteral("lyricsSource"), m_lyricsSource},
        {QStringLiteral("timingCapability"), timingName(m_parsedLyrics.timing)},
        {QStringLiteral("candidates"), m_candidateMaps},
    };
}

ServiceStatus LyricsServiceController::status() const
{
    return m_status;
}

bool LyricsServiceController::setEnabled(bool enabled, QString *)
{
    if (m_enabled == enabled)
        return true;
    m_enabled = enabled;
    m_settings.setEnabled(enabled);
    if (!enabled) {
        clearFrame();
    } else if (m_player.selectedPlayerAvailable() && m_trackStable && currentTrackSearchable()) {
        setStatus(ServiceStatus::LookingUpLyrics);
        if (m_lyrics)
            m_lyrics->search(m_snapshot.track);
    }
    refreshStatus();
    refreshVisualizer();
    publishState();
    return true;
}

bool LyricsServiceController::setPlayer(const QString &busName, QString *errorCode)
{
    const bool known = busName.isEmpty()
        || std::any_of(m_players.cbegin(), m_players.cend(), [&busName](const auto &player) {
               return player.busName == busName && player.available;
           });
    if (!known) {
        if (errorCode)
            *errorCode = QStringLiteral("invalid-player");
        return false;
    }

    m_settings.setPlayerBusName(busName);
    m_trackStable = false;
    clearFrame();
    m_player.setSelectedPlayer(busName);
    m_snapshot = m_player.snapshot();
    refreshStatus();
    refreshVisualizer();
    publishState();
    return true;
}

bool LyricsServiceController::setOffsetMs(int offsetMs, QString *errorCode)
{
    if (offsetMs < -10000 || offsetMs > 10000) {
        if (errorCode)
            *errorCode = QStringLiteral("offset-out-of-range");
        return false;
    }
    if (m_offsetMs == offsetMs)
        return true;
    m_offsetMs = offsetMs;
    m_settings.setOffsetMs(offsetMs);
    publishFrame();
    publishState();
    return true;
}

bool LyricsServiceController::setAudioVisualizerEnabled(bool enabled, QString *)
{
    if (m_audioVisualizerEnabled == enabled)
        return true;
    m_audioVisualizerEnabled = enabled;
    m_settings.setAudioVisualizerEnabled(enabled);
    refreshVisualizer();
    publishState();
    return true;
}

bool LyricsServiceController::setLyricLayout(const QString &layout, QString *errorCode)
{
    // 仅接受受支持的布局值，防止无效值进入 DConfig 与 QML 绑定。
    // Accept only supported layout values so invalid input never reaches
    // DConfig or the QML binding.
    if (layout != QStringLiteral("classic") && layout != QStringLiteral("karaoke")) {
        if (errorCode)
            *errorCode = QStringLiteral("invalid-lyric-layout");
        return false;
    }
    if (m_lyricLayout == layout)
        return true;
    m_lyricLayout = layout;
    m_settings.setLyricLayout(layout);
    publishState();
    return true;
}

bool LyricsServiceController::searchCandidates(QString *errorCode)
{
    if (!m_enabled || !currentTrackSearchable()) {
        if (errorCode)
            *errorCode = QStringLiteral("track-not-searchable");
        return false;
    }
    setStatus(ServiceStatus::LookingUpLyrics);
    publishState();
    if (m_lyrics)
        m_lyrics->searchCandidates(m_snapshot.track);
    return true;
}

bool LyricsServiceController::selectCandidate(const QString &providerId,
                                              const QString &candidateId,
                                              QString *errorCode)
{
    const bool knownCandidate = std::any_of(
        m_candidateMaps.cbegin(), m_candidateMaps.cend(), [&](const QVariant &value) {
            const QVariantMap map = value.toMap();
            return map.value(QStringLiteral("providerId")).toString() == providerId
                && map.value(QStringLiteral("candidateId")).toString() == candidateId;
        });
    if (providerId.trimmed().isEmpty() || candidateId.trimmed().isEmpty()
        || !knownCandidate) {
        if (errorCode)
            *errorCode = QStringLiteral("invalid-candidate");
        return false;
    }
    if (!m_enabled || !currentTrackSearchable()) {
        if (errorCode)
            *errorCode = QStringLiteral("track-not-searchable");
        return false;
    }
    setStatus(ServiceStatus::LookingUpLyrics);
    publishState();
    if (m_lyrics)
        m_lyrics->selectCandidate(providerId, candidateId);
    return true;
}

void LyricsServiceController::setSessionHidden(bool hidden)
{
    if (m_sessionHidden == hidden)
        return;
    m_sessionHidden = hidden;
    publishState();
}

void LyricsServiceController::clearCache()
{
    if (m_lyrics)
        m_lyrics->clearCache();
    m_logger.write(LogLevel::Info, QStringLiteral("cache"), QStringLiteral("cache_cleared"));
}

void LyricsServiceController::onAvailablePlayersChanged(const QList<PlayerDescriptor> &players)
{
    m_players = players;
    refreshStatus();
    m_logger.write(LogLevel::Info, QStringLiteral("player"), QStringLiteral("players_changed"),
                   {{QStringLiteral("player_available_count"), players.size()}});
    publishState();
}

void LyricsServiceController::onSnapshotChanged(const PlayerSnapshot &snapshot)
{
    const bool trackChanged = snapshot.track.title != m_snapshot.track.title
        || snapshot.track.artists != m_snapshot.track.artists
        || snapshot.track.album != m_snapshot.track.album
        || snapshot.track.durationMs != m_snapshot.track.durationMs
        || snapshot.track.playerBusName != m_snapshot.track.playerBusName;
    m_snapshot = snapshot;
    if (trackChanged && m_enabled && m_player.selectedPlayerAvailable()) {
        // 原始元数据变化时立即丢弃旧歌词状态，稳定曲目事件到达后才允许检索。
        // Drop stale lyric state on raw metadata changes; lookup starts only after the stable-track event.
        setStatus(ServiceStatus::WaitingForTrack);
        m_trackStable = false;
        clearFrame();
    } else {
        publishFrame();
    }
    refreshStatus();
    refreshVisualizer();
    publishState();
}

void LyricsServiceController::onSelectedPlayerAvailableChanged(bool)
{
    if (!m_player.selectedPlayerAvailable()) {
        m_trackStable = false;
        clearFrame();
    }
    syncExternalFramePlayer();
    refreshStatus();
    refreshVisualizer();
    publishState();
}

void LyricsServiceController::onTrackChanged(const TrackIdentity &track)
{
    clearFrame();
    m_snapshot.track = track;
    m_trackStable = true;
    // 选中 Ter-Music 等自带歌词的播放器时，由外部帧源直接供帧，跳过查询链。
    // Players with built-in lyrics (e.g. Ter-Music) feed frames directly;
    // skip the lookup chain while the external source is active.
    const bool externalFeedsLyrics = m_externalFrame && m_externalFrame->active();
    if (m_enabled && m_player.selectedPlayerAvailable() && track.searchable
        && !externalFeedsLyrics) {
        setStatus(ServiceStatus::LookingUpLyrics);
        if (m_lyrics)
            m_lyrics->search(track);
    } else {
        refreshStatus();
    }
    refreshVisualizer();
    publishState();
}

void LyricsServiceController::onLyricsReady(const LyricPayload &payload)
{
    if (!m_candidateMaps.isEmpty()) {
        m_candidateMaps.clear();
        emit candidatesChanged({});
    }
    LyricPayload displayPayload = payload;
    if (m_scriptConverter) {
        // 缓存保留来源原文，仅在解析显示副本前统一繁转简。
        // Preserve source text in cache and convert only the display copy before parsing.
        if (const auto converted = m_scriptConverter->toSimplified(displayPayload.syncedLyrics))
            displayPayload.syncedLyrics = *converted;
        if (const auto converted = m_scriptConverter->toSimplified(displayPayload.plainLyrics))
            displayPayload.plainLyrics = *converted;
        if (const auto converted = m_scriptConverter->toSimplified(displayPayload.translationLyrics))
            displayPayload.translationLyrics = *converted;
    }
    m_parsedLyrics = parseLyrics(displayPayload);
    m_lyricsSource = payload.providerId;
    if (m_parsedLyrics.timing == TimingCapability::None) {
        clearFrame();
        setStatus(ServiceStatus::NoLyrics);
        refreshVisualizer();
        publishState();
        return;
    }
    setStatus(ServiceStatus::LyricsReady);
    refreshVisualizer();
    publishState();
    publishFrame();
}

void LyricsServiceController::onNoLyrics()
{
    clearFrame();
    setStatus(ServiceStatus::NoLyrics);
    refreshVisualizer();
    publishState();
}

void LyricsServiceController::onCandidatesChanged(const QList<LyricCandidate> &candidates)
{
    QList<QVariantMap> maps;
    for (const auto &candidate : candidates) {
        maps.append({
            {QStringLiteral("providerId"), candidate.providerId},
            {QStringLiteral("candidateId"), candidate.candidateId},
            {QStringLiteral("title"), candidate.title},
            {QStringLiteral("artist"), candidate.artist},
            {QStringLiteral("album"), candidate.album},
            {QStringLiteral("durationMs"), candidate.durationMs},
            {QStringLiteral("score"), candidate.score},
            {QStringLiteral("sourceTrust"), candidate.sourceTrust},
        });
    }
    m_candidateMaps.clear();
    for (const auto &map : maps)
        m_candidateMaps.append(map);
    setStatus(ServiceStatus::NeedsCandidateSelection);
    publishState();
    emit candidatesChanged(maps);
}

void LyricsServiceController::onLyricsFailed(const QString &errorCode)
{
    setStatus(ServiceStatus::Error, stableErrorCode(errorCode));
    refreshVisualizer();
    publishState();
}

void LyricsServiceController::onSelectedPlayerProcessIdChanged(qint64)
{
    refreshVisualizer();
}

void LyricsServiceController::onVisualizerStateChanged(VisualizerState state,
                                                        StreamMatchConfidence confidence)
{
    if (m_visualizerState == state && m_visualizerConfidence == confidence)
        return;
    m_visualizerState = state;
    m_visualizerConfidence = confidence;
    if (state != VisualizerState::Active)
        clearVisualizerFrame();
    publishState();
}

void LyricsServiceController::onVisualizerFrameChanged(const VisualizerFrame &frame)
{
    if (!shouldVisualize() || m_visualizerState != VisualizerState::Active)
        return;
    // PipeWire callbacks can exceed the Dock's useful refresh rate; publish at most 20 FPS.
    // PipeWire 回调可能快于 Dock 的有效刷新率，因此最多以 20 FPS 发布。
    if (m_visualizerFrameTimer.isValid() && m_visualizerFrameTimer.elapsed() < 50)
        return;
    m_visualizerFrameTimer.restart();
    m_visualizerFrame = frame;
    publishState();
}

void LyricsServiceController::refreshStatus()
{
    if (!m_enabled)
        setStatus(ServiceStatus::Disabled);
    else if (m_externalFrameActive && !m_externalLyricFrame.currentText.isEmpty())
        // 外部帧源（Ter-Music 等）推送中：锁定歌词就绪状态。
        // External frames are flowing: pin the ready state.
        setStatus(ServiceStatus::LyricsReady);
    else if (m_player.selectedPlayer().isEmpty() || !m_player.selectedPlayerAvailable())
        setStatus(ServiceStatus::WaitingForPlayer);
    else if (!currentTrackSearchable())
        setStatus(ServiceStatus::WaitingForTrack);
    else if (!m_trackStable)
        setStatus(ServiceStatus::WaitingForTrack);
    else if (m_status != ServiceStatus::LookingUpLyrics
             && m_status != ServiceStatus::LyricsReady
             && m_status != ServiceStatus::NoLyrics
             && m_status != ServiceStatus::NeedsCandidateSelection
             && m_status != ServiceStatus::Error) {
        setStatus(ServiceStatus::LookingUpLyrics);
    }
}

void LyricsServiceController::setStatus(ServiceStatus status, const QString &errorCode)
{
    const ServiceStatus previous = m_status;
    const QString previousError = m_errorCode;
    m_status = status;
    m_errorCode = status == ServiceStatus::Error ? errorCode : QString();
    if (previous == status && previousError == m_errorCode)
        return;
    m_logger.write(LogLevel::Info, QStringLiteral("state"), QStringLiteral("state_changed"),
                   {{QStringLiteral("state_from"), statusName(previous)},
                    {QStringLiteral("state_to"), statusName(status)},
                    {QStringLiteral("error_code"), m_errorCode}});
}

void LyricsServiceController::publishState()
{
    const QVariantMap nextState = state();
    if (nextState == m_lastPublishedState)
        return;
    m_lastPublishedState = nextState;
    emit stateChanged(nextState);
}

void LyricsServiceController::publishFrame()
{
    // 外部帧源激活期间，帧完全由外部事件驱动，轮询路径不参与。
    // While the external frame source is active, frames are event-driven;
    // the polling path stays out of the way.
    if (m_externalFrameActive)
        return;
    if (m_parsedLyrics.timing == TimingCapability::None)
        return;
    const QVariantMap nextFrame = frameMap(
        frameAt(m_snapshot.track, m_parsedLyrics, m_snapshot.positionMs, m_offsetMs),
        m_lyricsSource);
    if (nextFrame == m_lastPublishedFrame)
        return;
    m_lastPublishedFrame = nextFrame;
    emit frameChanged(nextFrame);
}

void LyricsServiceController::clearFrame()
{
    const bool hadCandidates = !m_candidateMaps.isEmpty();
    m_candidateMaps.clear();
    if (hadCandidates)
        emit candidatesChanged({});
    m_parsedLyrics = {};
    m_lyricsSource.clear();
    m_externalFrameActive = false;
    m_externalLyricFrame = {};
    const QVariantMap emptyFrame{
        {QStringLiteral("previousText"), QString()},
        {QStringLiteral("currentText"), QString()},
        {QStringLiteral("secondaryText"), QString()},
        {QStringLiteral("translationText"), QString()},
        {QStringLiteral("lineIndex"), -1},
        {QStringLiteral("lineProgress"), 0.0},
        {QStringLiteral("timingCapability"), QStringLiteral("none")},
        {QStringLiteral("source"), QString()},
        {QStringLiteral("trackKey"), QString()},
    };
    if (emptyFrame == m_lastPublishedFrame)
        return;
    m_lastPublishedFrame = emptyFrame;
    emit frameChanged(emptyFrame);
}

void LyricsServiceController::refreshVisualizer()
{
    if (!m_visualizer)
        return;
    if (!m_enabled || !shouldVisualize()) {
        m_visualizer->stop();
        clearVisualizerFrame();
        return;
    }
    m_visualizer->setEnabled(true);
    m_visualizer->setPlayerProcessId(m_player.selectedPlayerProcessId());
}

void LyricsServiceController::clearVisualizerFrame()
{
    m_visualizerFrame = {};
}

bool LyricsServiceController::shouldVisualize() const
{
    return m_player.selectedPlayerAvailable()
        && m_snapshot.playbackStatus == PlaybackStatus::Playing
        && (m_audioVisualizerEnabled || m_status != ServiceStatus::LyricsReady);
}

bool LyricsServiceController::currentTrackSearchable() const
{
    return m_snapshot.track.searchable;
}

QString LyricsServiceController::statusName(ServiceStatus status)
{
    switch (status) {
    case ServiceStatus::Disabled:
        return QStringLiteral("Disabled");
    case ServiceStatus::WaitingForPlayer:
        return QStringLiteral("WaitingForPlayer");
    case ServiceStatus::WaitingForTrack:
        return QStringLiteral("WaitingForTrack");
    case ServiceStatus::LookingUpLyrics:
        return QStringLiteral("LookingUpLyrics");
    case ServiceStatus::LyricsReady:
        return QStringLiteral("LyricsReady");
    case ServiceStatus::NoLyrics:
        return QStringLiteral("NoLyrics");
    case ServiceStatus::NeedsCandidateSelection:
        return QStringLiteral("NeedsCandidateSelection");
    case ServiceStatus::Error:
        return QStringLiteral("Error");
    }
    return QStringLiteral("Error");
}

void LyricsServiceController::syncExternalFramePlayer()
{
    // 把当前选中播放器同步给外部帧源，由源决定启停（仅 Ter-Music 启用）。
    // Forward the selected player to the external source, which enables
    // itself only while Ter-Music is selected.
    if (m_externalFrame)
        m_externalFrame->setSelectedPlayer(m_player.selectedPlayer());
}

void LyricsServiceController::onExternalFrameAvailable(const ExternalLyricFrame &frame)
{
    if (!m_enabled || !m_player.selectedPlayerAvailable())
        return;
    // 新曲目的第一帧：外部源已按 track_id 重置内部状态，这里直接采用。
    // The source resets its state on track_id changes; adopt the frame as-is.
    m_externalLyricFrame = frame;
    m_externalFrameActive = true;
    setStatus(ServiceStatus::LyricsReady);
    publishExternalFrame();
    publishState();
}

void LyricsServiceController::onExternalFrameStopped()
{
    const bool wasActive = m_externalFrameActive;
    m_externalFrameActive = false;
    m_externalLyricFrame = {};
    if (!wasActive)
        return;
    // 外部帧停止后回到常规状态机：若已有 LRCLIB 歌词则继续显示。
    // Fall back to the regular state machine; keep showing LRCLIB lyrics if any.
    refreshStatus();
    publishState();
    publishFrame();
}

void LyricsServiceController::publishExternalFrame()
{
    LyricFrame frame;
    frame.track = m_snapshot.track;
    frame.currentText = m_externalLyricFrame.currentText;
    frame.secondaryText = m_externalLyricFrame.secondaryText;
    frame.lineIndex = m_externalLyricFrame.lineIndex;
    frame.timing = m_externalLyricFrame.timing;
    // 外部帧不携带行内进度：保持 0，UI 对 Plain/Line 都按整行显示。
    // External frames carry no intra-line progress; keep 0.
    frame.lineProgress = 0.0;

    const QVariantMap nextFrame = frameMap(frame, QStringLiteral("ter-music"));
    if (nextFrame == m_lastPublishedFrame)
        return;
    m_lastPublishedFrame = nextFrame;
    emit frameChanged(nextFrame);
}

} // namespace deepin::lyrics
