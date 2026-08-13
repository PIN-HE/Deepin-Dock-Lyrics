#include "lyricscore/normalization.h"

#include <QRegularExpression>
#include <QSet>
#include <QMutex>

#include <opencc/opencc.h>

#include <algorithm>
#include <cmath>

namespace deepin::lyrics {

QString normalizeText(const QString &text)
{
    static QMutex mutex;
    static const opencc_t converter = opencc_open(OPENCC_DEFAULT_CONFIG_TRAD_TO_SIMP);
    const QString normalized = text.normalized(QString::NormalizationForm_KC)
                                   .simplified().toCaseFolded();
    if (normalized.isEmpty() || converter == reinterpret_cast<opencc_t>(-1))
        return normalized;

    const QByteArray source = normalized.toUtf8();
    QMutexLocker locker(&mutex);
    char *converted = opencc_convert_utf8(converter, source.constData(), source.size());
    if (!converted)
        return normalized;
    const QString result = QString::fromUtf8(converted);
    opencc_convert_utf8_free(converted);
    return result;
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
        QString::number(track.durationMs > 0 ? qRound64(track.durationMs / 1000.0) * 1000 : -1),
    }.join(fieldSeparator);
}

namespace {

bool containsCjk(const QString &text)
{
    return std::any_of(text.cbegin(), text.cend(), [](const QChar character) {
        const ushort code = character.unicode();
        return (code >= 0x3400 && code <= 0x9fff) || (code >= 0xf900 && code <= 0xfaff);
    });
}

QString matchingText(const QString &text)
{
    QString result = normalizeText(text);
    if (!containsCjk(result))
        return result;
    result.remove(QRegularExpression(QStringLiteral("\\s*(\\([^)]*\\)|\\[[^]]*\\])\\s*$")));
    result.remove(QRegularExpression(QStringLiteral("\\s+(feat\\.?|ft\\.?)\\s+.*$"),
                                      QRegularExpression::CaseInsensitiveOption));
    return result.trimmed();
}

QSet<QString> words(const QString &text)
{
    const QStringList parts = normalizeText(text).split(
        QRegularExpression(QStringLiteral("[\\s,;/&]+")), Qt::SkipEmptyParts);
    return QSet<QString>(parts.cbegin(), parts.cend());
}

} // namespace

double textScore(const QString &left, const QString &right)
{
    const QString normalizedLeft = matchingText(left);
    const QString normalizedRight = matchingText(right);
    if (normalizedLeft.isEmpty() || normalizedRight.isEmpty())
        return 0.0;
    if (normalizedLeft == normalizedRight)
        return 1.0;

    if (containsCjk(normalizedLeft) || containsCjk(normalizedRight)) {
        const auto bigrams = [](const QString &value) {
            QSet<QString> result;
            const QString compact = value.simplified().remove(QRegularExpression(QStringLiteral("\\s+")));
            if (compact.size() == 1) {
                result.insert(compact);
                return result;
            }
            for (int index = 0; index + 1 < compact.size(); ++index)
                result.insert(compact.mid(index, 2));
            return result;
        };
        const QSet<QString> leftBigrams = bigrams(normalizedLeft);
        const QSet<QString> rightBigrams = bigrams(normalizedRight);
        const int intersection = (leftBigrams & rightBigrams).size();
        return (2.0 * intersection) / (leftBigrams.size() + rightBigrams.size());
    }

    const QSet<QString> leftWords = words(normalizedLeft);
    const QSet<QString> rightWords = words(normalizedRight);
    if (leftWords.isEmpty() || rightWords.isEmpty())
        return 0.0;
    const int intersection = (leftWords & rightWords).size();
    return (2.0 * intersection) / (leftWords.size() + rightWords.size());
}

namespace {

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
    const bool hasDuration = track.durationMs > 0 && record.durationMs > 0;
    const double durationDifference = hasDuration
        ? std::abs(track.durationMs - record.durationMs) : 0.0;
    const double durationScore = hasDuration
        ? std::max(0.0, 1.0 - (durationDifference / 10000.0)) : 0.0;
    const double titleWeight = track.album.isEmpty() || record.albumName.isEmpty() ? 0.55 : 0.45;
    const double artistWeight = track.album.isEmpty() || record.albumName.isEmpty() ? 0.35 : 0.30;
    const double albumWeight = track.album.isEmpty() || record.albumName.isEmpty() ? 0.0 : 0.10;
    const double durationWeight = hasDuration ? 0.15 : 0.0;
    const double base = (titleWeight * textScore(track.title, record.trackName))
        + (artistWeight * artistScore(track.artists, record.artistName))
        + (albumWeight * textScore(track.album, record.albumName))
        + (durationWeight * durationScore);
    return hasDuration ? base : base / (titleWeight + artistWeight);
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
        const QString providerId = record.payload.providerId.isEmpty()
            ? QStringLiteral("lrclib") : record.payload.providerId;
        candidates.append({providerId, record.id, record.trackName,
                           record.artistName, record.albumName, record.durationMs, score,
                           record.sourceTrust});
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto &left, const auto &right) {
        // Confidence is primary; trust breaks near-ties between providers.
        // 匹配置信度优先，来源信任度用于打破接近分数的跨源并列。
        if (std::abs(left.score - right.score) > 0.05)
            return left.score > right.score;
        if (!qFuzzyCompare(left.sourceTrust, right.sourceTrust))
            return left.sourceTrust > right.sourceTrust;
        if (!qFuzzyCompare(left.score, right.score))
            return left.score > right.score;
        return left.candidateId < right.candidateId;
    });
    if (limit >= 0 && candidates.size() > limit)
        candidates.resize(limit);
    return candidates;
}

} // namespace deepin::lyrics
