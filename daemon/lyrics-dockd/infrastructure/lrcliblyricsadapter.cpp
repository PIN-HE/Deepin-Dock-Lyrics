#include "infrastructure/lrcliblyricsadapter.h"

#include <lyricscore/normalization.h>

#include <cmath>

namespace deepin::lyrics {

LrclibLyricsAdapter::LrclibLyricsAdapter(LyricProvider &provider,
                                         LyricsCache &cache,
                                         LogEngine &logger,
                                         QObject *parent)
    : LyricsPort(parent)
    , m_provider(provider)
    , m_cache(cache)
    , m_logger(logger)
{
}

void LrclibLyricsAdapter::search(const TrackIdentity &track)
{
    const int generation = ++m_generation;
    m_candidateIds.clear();
    m_track = track;
    m_trackKey = makeTrackKey(track);
    QString errorCode;
    if (!m_cache.open(&errorCode)) {
        emit failed(QStringLiteral("database-failed"));
        return;
    }
    if (const auto cached = m_cache.find(m_trackKey, QDateTime::currentDateTimeUtc())) {
        m_logger.write(LogLevel::Info, QStringLiteral("lrclib"), QStringLiteral("cache_hit"),
                       {{QStringLiteral("result_code"), QStringLiteral("lyrics")},
                        {QStringLiteral("record_id"), cached->payload.recordId}});
        emit lyricsReady(cached->payload);
        return;
    }
    if (m_cache.isNegative(m_trackKey, QDateTime::currentDateTimeUtc())) {
        m_logger.write(LogLevel::Info, QStringLiteral("lrclib"), QStringLiteral("cache_hit"),
                       {{QStringLiteral("result_code"), QStringLiteral("negative")}});
        emit noLyrics();
        return;
    }
    if (m_cache.cooldownUntil() > QDateTime::currentDateTimeUtc()) {
        emit failed(QStringLiteral("rate-limited"));
        return;
    }
    if (!track.searchable || track.durationMs <= 0) {
        emit noLyrics();
        return;
    }
    beginExact(track, generation);
}

void LrclibLyricsAdapter::searchCandidates(const TrackIdentity &track)
{
    const int generation = ++m_generation;
    m_candidateIds.clear();
    m_track = track;
    m_trackKey = makeTrackKey(track);
    QString errorCode;
    if (!m_cache.open(&errorCode)) {
        emit failed(QStringLiteral("database-failed"));
        return;
    }
    if (m_cache.cooldownUntil() > QDateTime::currentDateTimeUtc()) {
        emit failed(QStringLiteral("rate-limited"));
        return;
    }
    beginCandidateSearch(track, generation);
}

void LrclibLyricsAdapter::selectCandidate(const QString &providerId,
                                          const QString &candidateId)
{
    if (providerId != m_provider.id() || !m_candidateIds.contains(candidateId)) {
        emit failed(QStringLiteral("provider-failed"));
        return;
    }
    const int generation = ++m_generation;
    m_candidateIds.clear();
    fetchRecord(m_track, candidateId, 1.0, true, generation);
}

void LrclibLyricsAdapter::clearCache()
{
    ++m_generation;
    m_candidateIds.clear();
    QString errorCode;
    if (!m_cache.clear(&errorCode))
        emit failed(QStringLiteral("database-failed"));
}

void LrclibLyricsAdapter::beginExact(const TrackIdentity &track, int generation)
{
    m_provider.getExact(track, [this, track, generation](ProviderResult result) {
        if (!isCurrent(generation))
            return;
        if (result.kind == ProviderResultKind::Success) {
            if (result.record.instrumental || result.record.payload.timing == TimingCapability::None) {
                m_cache.storeNegative(m_trackKey, QStringLiteral("no-lyrics"),
                                      QDateTime::currentDateTimeUtc());
                emit noLyrics();
                return;
            }
            const double score = scoreLyricCandidate(track, result.record);
            const bool durationEligible = std::abs(track.durationMs - result.record.durationMs) <= 2000;
            if (score >= 0.85 && durationEligible) {
                if (!m_cache.store(m_trackKey, result.record, score, false,
                                   QDateTime::currentDateTimeUtc())) {
                    emit failed(QStringLiteral("database-failed"));
                    return;
                }
                emit lyricsReady(result.record.payload);
                return;
            }
            emit failed(QStringLiteral("provider-failed"));
            return;
        }
        if (result.kind == ProviderResultKind::NotFound) {
            beginCandidateSearch(track, generation);
            return;
        }
        handleFailure(result);
    });
}

void LrclibLyricsAdapter::beginCandidateSearch(const TrackIdentity &track, int generation)
{
    m_provider.search(track, [this, track, generation](ProviderResult result) {
        if (isCurrent(generation))
            handleCandidates(track, generation, std::move(result));
    });
}

void LrclibLyricsAdapter::handleCandidates(const TrackIdentity &track,
                                           int generation,
                                           ProviderResult result)
{
    if (result.kind != ProviderResultKind::Success) {
        handleFailure(result);
        return;
    }
    const QList<LyricCandidate> candidates = rankLyricCandidates(track, result.records, 20);
    if (candidates.isEmpty()) {
        m_cache.storeNegative(m_trackKey, QStringLiteral("not-found"),
                              QDateTime::currentDateTimeUtc());
        emit noLyrics();
        return;
    }
    const LyricCandidate &best = candidates.constFirst();
    const bool durationEligible = std::abs(track.durationMs - best.durationMs) <= 2000;
    if (best.score >= 0.85 && durationEligible) {
        fetchRecord(track, best.candidateId, best.score, false, generation);
        return;
    }
    for (const auto &candidate : candidates)
        m_candidateIds.insert(candidate.candidateId);
    emit candidatesChanged(candidates);
}

void LrclibLyricsAdapter::fetchRecord(const TrackIdentity &track,
                                      const QString &recordId,
                                      double confidence,
                                      bool userConfirmed,
                                      int generation)
{
    m_provider.getById(recordId,
                       [this, track, confidence, userConfirmed, generation](ProviderResult result) {
        if (!isCurrent(generation))
            return;
        if (result.kind != ProviderResultKind::Success) {
            handleFailure(result);
            return;
        }
        if (result.record.instrumental || result.record.payload.timing == TimingCapability::None) {
            m_cache.storeNegative(makeTrackKey(track), QStringLiteral("no-lyrics"),
                                  QDateTime::currentDateTimeUtc());
            emit noLyrics();
            return;
        }
        if (!m_cache.store(makeTrackKey(track), result.record, confidence, userConfirmed,
                           QDateTime::currentDateTimeUtc())) {
            emit failed(QStringLiteral("database-failed"));
            return;
        }
        m_logger.write(LogLevel::Info, QStringLiteral("lrclib"), QStringLiteral("record_confirmed"),
                       {{QStringLiteral("record_id"), result.record.id},
                        {QStringLiteral("result_code"), userConfirmed
                             ? QStringLiteral("user-confirmed") : QStringLiteral("automatic")}});
        emit lyricsReady(result.record.payload);
    });
}

void LrclibLyricsAdapter::handleFailure(const ProviderResult &result)
{
    if (result.kind == ProviderResultKind::RateLimited && result.retryAt.isValid())
        m_cache.setCooldownUntil(result.retryAt);
    emit failed(result.errorCode.isEmpty() ? QStringLiteral("provider-failed") : result.errorCode);
}

bool LrclibLyricsAdapter::isCurrent(int generation) const
{
    return generation == m_generation;
}

} // namespace deepin::lyrics
