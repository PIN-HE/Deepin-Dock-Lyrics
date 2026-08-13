#pragma once

#include <lyricscore/lyricprovider.h>

#include <QStringList>

namespace deepin::lyrics {

class PlayerCacheLyricsProvider final : public LyricProvider
{
public:
    PlayerCacheLyricsProvider();

    QString id() const override;
    void getExact(const TrackIdentity &track, ResultCallback callback) override;
    void search(const TrackIdentity &track, ResultCallback callback) override;
    void getById(const QString &recordId, ResultCallback callback) override;

    QStringList cacheRoots() const;
    void setCacheRoots(const QStringList &roots);

private:
    ProviderResult readTrack(const TrackIdentity &track) const;
    ProviderResult readOpenOrpheusTrack(const TrackIdentity &track) const;
    QStringList m_roots;
    QString m_openOrpheusCacheRoot;
    QString m_openOrpheusDatabase;
};

} // namespace deepin::lyrics
