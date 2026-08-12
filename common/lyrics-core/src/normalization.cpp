#include "lyricscore/normalization.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace deepin::lyrics {

QString normalizeText(const QString &text)
{
    return text.normalized(QString::NormalizationForm_KC).simplified().toCaseFolded();
}

QString makeTrackKey(const TrackIdentity &track)
{
    constexpr QChar fieldSeparator(0x1f);
    constexpr QChar artistSeparator(0x1e);

    QStringList artists;
    artists.reserve(track.artists.size());
    for (const QString &artist : track.artists)
        artists.append(normalizeText(artist));

    return QStringList {
        normalizeText(track.title),
        artists.join(artistSeparator),
        normalizeText(track.album),
        QString::number(track.durationMs),
    }.join(fieldSeparator);
}

namespace {

QSet<QString> words(const QString &text)
{
    const QStringList parts = normalizeText(text).split(
        QRegularExpression(QStringLiteral("[\\s,;/&]+")), Qt::SkipEmptyParts);
    return QSet<QString>(parts.cbegin(), parts.cend());
}

double textScore(const QString &left, const QString &right)
{
    const QString normalizedLeft = normalizeText(left);
    const QString normalizedRight = normalizeText(right);
    if (normalizedLeft.isEmpty() || normalizedRight.isEmpty())
        return 0.0;
    if (normalizedLeft == normalizedRight)
        return 1.0;

    const QSet<QString> leftWords = words(left);
    const QSet<QString> rightWords = words(right);
    if (leftWords.isEmpty() || rightWords.isEmpty())
        return 0.0;
    const int intersection = (leftWords & rightWords).size();
    return (2.0 * intersection) / (leftWords.size() + rightWords.size());
}

double artistScore(const QStringList &artists, const QString &candidateArtist)
{
    double best = 0.0;
    for (const QString &artist : artists)
        best = std::max(best, textScore(artist, candidateArtist));
    return best;
}

} // namespace

double scoreLyricCandidate(const TrackIdentity &track, const ProviderRecord &record)
{
    const double durationDifference = track.durationMs > 0 && record.durationMs > 0
        ? std::abs(track.durationMs - record.durationMs)
        : 10000.0;
    const double durationScore = durationDifference <= 2000.0
        ? 1.0
        : std::max(0.0, 1.0 - ((durationDifference - 2000.0) / 8000.0));

    return (0.45 * textScore(track.title, record.trackName))
        + (0.30 * artistScore(track.artists, record.artistName))
        + (0.10 * textScore(track.album, record.albumName))
        + (0.15 * durationScore);
}

QList<LyricCandidate> rankLyricCandidates(const TrackIdentity &track,
                                          const QList<ProviderRecord> &records,
                                          int limit)
{
    QList<LyricCandidate> candidates;
    for (const auto &record : records) {
        const double score = scoreLyricCandidate(track, record);
        if (score < 0.60)
            continue;
        candidates.append({QStringLiteral("lrclib"), record.id, record.trackName,
                           record.artistName, record.albumName, record.durationMs, score});
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto &left, const auto &right) {
        if (!qFuzzyCompare(left.score, right.score))
            return left.score > right.score;
        return left.candidateId < right.candidateId;
    });
    if (limit >= 0 && candidates.size() > limit)
        candidates.resize(limit);
    return candidates;
}

} // namespace deepin::lyrics
