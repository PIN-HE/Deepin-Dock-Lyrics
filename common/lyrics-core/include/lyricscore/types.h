#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

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

struct TrackIdentity {
    QString title;
    QStringList artists;
    QString album;
    qint64 durationMs = -1;
    QString playerBusName;
};

struct PlayerSnapshot {
    QString busName;
    QString identity;
    PlaybackStatus playbackStatus = PlaybackStatus::Stopped;
    qint64 positionMs = -1;
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
