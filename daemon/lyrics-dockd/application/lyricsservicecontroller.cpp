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

QVariantMap frameMap(const LyricFrame &frame, const QString &source)
{
    return {
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

void LyricsServiceController::start()
{
    if (m_started)
        return;
    m_started = true;
    m_enabled = m_settings.enabled();
    m_offsetMs = std::clamp(m_settings.offsetMs(), -10000, 10000);
    m_player.setSelectedPlayer(m_settings.playerBusName());
    m_player.start();
    m_players = m_player.availablePlayers();
    m_snapshot = m_player.snapshot();
    refreshStatus();
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
        {QStringLiteral("durationMs"), track.durationMs},
        {QStringLiteral("offsetMs"), m_offsetMs},
        {QStringLiteral("errorCode"), m_errorCode},
        {QStringLiteral("canSearchCandidates"), m_enabled && currentTrackSearchable()},
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
    if (providerId != QStringLiteral("lrclib") || candidateId.trimmed().isEmpty()) {
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
    publishState();
}

void LyricsServiceController::onSelectedPlayerAvailableChanged(bool)
{
    if (!m_player.selectedPlayerAvailable()) {
        m_trackStable = false;
        clearFrame();
    }
    refreshStatus();
    publishState();
}

void LyricsServiceController::onTrackChanged(const TrackIdentity &track)
{
    clearFrame();
    m_snapshot.track = track;
    m_trackStable = true;
    if (m_enabled && m_player.selectedPlayerAvailable() && track.searchable) {
        setStatus(ServiceStatus::LookingUpLyrics);
        if (m_lyrics)
            m_lyrics->search(track);
    } else {
        refreshStatus();
    }
    publishState();
}

void LyricsServiceController::onLyricsReady(const LyricPayload &payload)
{
    m_parsedLyrics = parseLyrics(payload);
    m_lyricsSource = payload.providerId;
    if (m_parsedLyrics.timing == TimingCapability::None) {
        clearFrame();
        setStatus(ServiceStatus::NoLyrics);
        publishState();
        return;
    }
    setStatus(ServiceStatus::LyricsReady);
    publishState();
    publishFrame();
}

void LyricsServiceController::onNoLyrics()
{
    clearFrame();
    setStatus(ServiceStatus::NoLyrics);
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
        });
    }
    setStatus(ServiceStatus::NeedsCandidateSelection);
    publishState();
    emit candidatesChanged(maps);
}

void LyricsServiceController::onLyricsFailed(const QString &errorCode)
{
    setStatus(ServiceStatus::Error, stableErrorCode(errorCode));
    publishState();
}

void LyricsServiceController::refreshStatus()
{
    if (!m_enabled)
        setStatus(ServiceStatus::Disabled);
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
    m_parsedLyrics = {};
    m_lyricsSource.clear();
    const QVariantMap emptyFrame{
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

} // namespace deepin::lyrics
