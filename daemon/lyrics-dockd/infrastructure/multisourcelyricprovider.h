#pragma once

#include <lyricscore/lyricprovider.h>

#include <QList>
#include <QHash>

namespace deepin::lyrics {

// Coordinates registered lyric sources without exposing source-specific DTOs.
// 协调已注册歌词源，不向上层暴露来源专属 DTO。
class MultiSourceLyricProvider final : public LyricProvider
{
public:
    struct Source {
        LyricProvider *provider = nullptr;
        double trust = 0.0;
        bool enabled = true;
    };

    void addSource(LyricProvider &provider, double trust);
    bool setSourceEnabled(const QString &providerId, bool enabled);
    QList<QString> sourceIds() const;

    QString id() const override;
    void getExact(const TrackIdentity &track, ResultCallback callback) override;
    void search(const TrackIdentity &track, ResultCallback callback) override;
    void getById(const QString &recordId, ResultCallback callback) override;

private:
    QList<Source> enabledSources() const;
    static void applyTrust(ProviderResult &result, const Source &source);

    QList<Source> m_sources;
    QHash<QString, LyricProvider *> m_candidateOwners;
};

} // namespace deepin::lyrics
