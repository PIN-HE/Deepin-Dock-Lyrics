#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include <array>

namespace deepin::lyrics {

enum class PlaybackStatus {
    Stopped,
    Playing,
    Paused,
};

enum class TimingCapability {
    None,
    Plain,
    Line,
};

enum class VisualizerState {
    Disabled,
    WaitingForPlayer,
    ResolvingAudioStream,
    Active,
    Unavailable,
};

enum class StreamMatchConfidence {
    None,
    ExactPid,
    ExactMprisProcessTree,
};

constexpr int visualizerBandCount = 16;

struct VisualizerFrame {
    std::array<float, visualizerBandCount> levels {};
};

struct TrackIdentity {
    QString title;
    QStringList artists;
    QString album;
    qint64 durationMs = -1;
    QString playerBusName;
    bool searchable = false;
};

struct PlayerDescriptor {
    QString busName;
    QString identity;
    QString desktopEntry;
    bool available = false;
};

struct PlayerSnapshot {
    QString busName;
    QString identity;
    PlaybackStatus playbackStatus = PlaybackStatus::Stopped;
    qint64 positionMs = -1;
    double playbackRate = 1.0;
    QDateTime capturedAt;
    TrackIdentity track;
};

struct LyricLine {
    qint64 startMs = 0;
    QString text;
};

struct ParsedLyrics {
    QVector<LyricLine> lines;
    QString plainText;
    qint64 sourceOffsetMs = 0;
    TimingCapability timing = TimingCapability::None;
};

struct LyricCandidate {
    QString providerId;
    QString candidateId;
    QString title;
    QString artist;
    QString album;
    qint64 durationMs = -1;
    double score = 0.0;
};

struct LyricFrame {
    TrackIdentity track;
    QString currentText;
    QString secondaryText;
    QString translationText;
    int lineIndex = -1;
    double lineProgress = 0.0;
    TimingCapability timing = TimingCapability::None;
};

} // namespace deepin::lyrics

Q_DECLARE_METATYPE(deepin::lyrics::TrackIdentity)
Q_DECLARE_METATYPE(deepin::lyrics::PlayerDescriptor)
Q_DECLARE_METATYPE(QList<deepin::lyrics::PlayerDescriptor>)
Q_DECLARE_METATYPE(deepin::lyrics::PlayerSnapshot)
Q_DECLARE_METATYPE(deepin::lyrics::VisualizerState)
Q_DECLARE_METATYPE(deepin::lyrics::StreamMatchConfidence)
Q_DECLARE_METATYPE(deepin::lyrics::VisualizerFrame)
Q_DECLARE_METATYPE(deepin::lyrics::LyricCandidate)
Q_DECLARE_METATYPE(QList<deepin::lyrics::LyricCandidate>)
