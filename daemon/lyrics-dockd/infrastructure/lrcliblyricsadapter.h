#pragma once

#include "ports/lyricscache.h"
#include "ports/lyricsport.h"

#include <lyricscore/lyricprovider.h>
#include <lyricslogging/logengine.h>

#include <QSet>

namespace deepin::lyrics {

class LrclibLyricsAdapter final : public LyricsPort
{
    Q_OBJECT

public:
    LrclibLyricsAdapter(LyricProvider &provider,
                        LyricsCache &cache,
                        LogEngine &logger,
                        QObject *parent = nullptr);

    void search(const TrackIdentity &track) override;
    void searchCandidates(const TrackIdentity &track) override;
    void selectCandidate(const QString &providerId, const QString &candidateId) override;
    void clearCache() override;

private:
    void beginExact(const TrackIdentity &track, int generation);
    void beginCandidateSearch(const TrackIdentity &track, int generation);
    void handleCandidates(const TrackIdentity &track,
                          int generation,
                          ProviderResult result);
    void fetchRecord(const TrackIdentity &track,
                     const QString &recordId,
                     double confidence,
                     bool userConfirmed,
                     int generation);
    void handleFailure(const ProviderResult &result);
    bool isCurrent(int generation) const;

    LyricProvider &m_provider;
    LyricsCache &m_cache;
    LogEngine &m_logger;
    TrackIdentity m_track;
    QString m_trackKey;
    QSet<QString> m_candidateIds;
    int m_generation = 0;
};

} // namespace deepin::lyrics
