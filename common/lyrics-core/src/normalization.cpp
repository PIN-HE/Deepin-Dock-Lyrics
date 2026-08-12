#include "lyricscore/normalization.h"

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

} // namespace deepin::lyrics
