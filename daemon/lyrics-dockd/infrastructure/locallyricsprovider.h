#pragma once

#include <lyricscore/lyricprovider.h>

#include <QStringList>

namespace deepin::lyrics {

class LocalLyricsProvider final : public LyricProvider
{
public:
    QString id() const override;
    void getExact(const TrackIdentity &track, ResultCallback callback) override;
    void search(const TrackIdentity &track, ResultCallback callback) override;
    void getById(const QString &recordId, ResultCallback callback) override;

private:
    static ProviderResult readTrack(const TrackIdentity &track);
    static ProviderResult readFile(const QString &path, const TrackIdentity &track);
};

} // namespace deepin::lyrics
