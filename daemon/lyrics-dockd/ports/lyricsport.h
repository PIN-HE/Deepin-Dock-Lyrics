#pragma once

#include <lyricscore/types.h>

#include <QObject>

namespace deepin::lyrics {

class LyricsPort : public QObject
{
    Q_OBJECT

public:
    explicit LyricsPort(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    virtual void search(const TrackIdentity &track) = 0;
    virtual void selectCandidate(const QString &providerId, const QString &candidateId) = 0;
    virtual void clearCache() = 0;

signals:
    void lyricsReady(const ParsedLyrics &lyrics);
    void noLyrics();
    void candidatesChanged(const QList<LyricCandidate> &candidates);
    void failed(const QString &errorCode);
};

} // namespace deepin::lyrics
