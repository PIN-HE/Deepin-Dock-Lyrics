#pragma once

#include "ports/externalframeport.h"
#include "ports/lyricsport.h"
#include "ports/playerport.h"
#include "ports/settingsport.h"
#include "ports/chinesescriptconverter.h"
#include "ports/audiovisualizerport.h"

#include <lyricslogging/logengine.h>
#include <lyricscore/types.h>

#include <QObject>
#include <QElapsedTimer>
#include <QVariantMap>

namespace deepin::lyrics {

enum class ServiceStatus {
    Disabled,
    WaitingForPlayer,
    WaitingForTrack,
    LookingUpLyrics,
    LyricsReady,
    NoLyrics,
    NeedsCandidateSelection,
    Error,
};

class LyricsServiceController final : public QObject
{
    Q_OBJECT

public:
    LyricsServiceController(PlayerPort &player,
                            SettingsPort &settings,
                            LogEngine &logger,
                            LyricsPort *lyrics = nullptr,
                            QObject *parent = nullptr);
    LyricsServiceController(PlayerPort &player,
                            SettingsPort &settings,
                            LogEngine &logger,
                            ChineseScriptConverter &scriptConverter,
                            LyricsPort *lyrics = nullptr,
                            QObject *parent = nullptr);
    LyricsServiceController(PlayerPort &player,
                            SettingsPort &settings,
                            LogEngine &logger,
                            ChineseScriptConverter &scriptConverter,
                            LyricsPort *lyrics,
                            AudioVisualizerPort *visualizer,
                            QObject *parent = nullptr);

    void start();
    QVariantMap state() const;
    ServiceStatus status() const;

    bool setEnabled(bool enabled, QString *errorCode = nullptr);
    bool setPlayer(const QString &busName, QString *errorCode = nullptr);
    bool setOffsetMs(int offsetMs, QString *errorCode = nullptr);
    bool setAudioVisualizerEnabled(bool enabled, QString *errorCode = nullptr);
    bool searchCandidates(QString *errorCode = nullptr);
    bool selectCandidate(const QString &providerId,
                         const QString &candidateId,
                         QString *errorCode = nullptr);
    void setSessionHidden(bool hidden);
    void clearCache();
    // 注入外部帧源（Ter-Music 等播放器内置歌词）。在 start() 前调用。
    // Inject an external frame source (player built-in lyrics). Call before start().
    void setExternalFramePort(ExternalFramePort *externalFrame);

signals:
    void stateChanged(const QVariantMap &state);
    void frameChanged(const QVariantMap &frame);
    void candidatesChanged(const QList<QVariantMap> &candidates);

private slots:
    void onAvailablePlayersChanged(const QList<PlayerDescriptor> &players);
    void onSnapshotChanged(const PlayerSnapshot &snapshot);
    void onSelectedPlayerAvailableChanged(bool available);
    void onTrackChanged(const TrackIdentity &track);
    void onSelectedPlayerProcessIdChanged(qint64 processId);
    void onVisualizerStateChanged(VisualizerState state, StreamMatchConfidence confidence);
    void onVisualizerFrameChanged(const VisualizerFrame &frame);
    void onLyricsReady(const LyricPayload &payload);
    void onNoLyrics();
    void onCandidatesChanged(const QList<LyricCandidate> &candidates);
    void onLyricsFailed(const QString &errorCode);
    void onExternalFrameAvailable(const ExternalLyricFrame &frame);
    void onExternalFrameStopped();

private:
    void refreshStatus();
    void setStatus(ServiceStatus status, const QString &errorCode = {});
    void publishState();
    void publishFrame();
    void clearFrame();
    void refreshVisualizer();
    void clearVisualizerFrame();
    void syncExternalFramePlayer();
    void publishExternalFrame();
    bool shouldVisualize() const;
    bool currentTrackSearchable() const;
    static QString statusName(ServiceStatus status);

    PlayerPort &m_player;
    SettingsPort &m_settings;
    LogEngine &m_logger;
    LyricsPort *m_lyrics = nullptr;
    AudioVisualizerPort *m_visualizer = nullptr;
    ChineseScriptConverter *m_scriptConverter = nullptr;
    ExternalFramePort *m_externalFrame = nullptr;
    QList<PlayerDescriptor> m_players;
    QVariantList m_candidateMaps;
    PlayerSnapshot m_snapshot;
    ParsedLyrics m_parsedLyrics;
    ExternalLyricFrame m_externalLyricFrame;
    bool m_externalFrameActive = false;
    QVariantMap m_lastPublishedState;
    QVariantMap m_lastPublishedFrame;
    VisualizerFrame m_visualizerFrame;
    VisualizerState m_visualizerState = VisualizerState::Disabled;
    StreamMatchConfidence m_visualizerConfidence = StreamMatchConfidence::None;
    QString m_lyricsSource;
    ServiceStatus m_status = ServiceStatus::Disabled;
    QString m_errorCode;
    int m_offsetMs = 0;
    bool m_enabled = false;
    bool m_sessionHidden = false;
    bool m_audioVisualizerEnabled = false;
    QElapsedTimer m_visualizerFrameTimer;
    bool m_trackStable = false;
    bool m_started = false;
};

} // namespace deepin::lyrics
