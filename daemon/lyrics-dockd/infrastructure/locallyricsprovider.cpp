#include "infrastructure/locallyricsprovider.h"

#include <lyricscore/lrcparser.h>

#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <algorithm>

namespace {

QString decodeId3Text(const QByteArray &data, int encoding)
{
    if (encoding == 1 || encoding == 2)
        return QString::fromUtf16(reinterpret_cast<const char16_t *>(data.constData()),
                                  data.size() / 2);
    if (encoding == 3)
        return QString::fromUtf8(data);
    return QString::fromLatin1(data);
}

QByteArray id3FrameText(const QByteArray &body, const QByteArray &frameId)
{
    if (body.size() < 10 || body.left(3) != QByteArrayLiteral("ID3"))
        return {};
    const int version = static_cast<unsigned char>(body.at(3));
    const auto syncSafe = [](const char *bytes) {
        return (static_cast<unsigned char>(bytes[0]) << 21)
            | (static_cast<unsigned char>(bytes[1]) << 14)
            | (static_cast<unsigned char>(bytes[2]) << 7)
            | static_cast<unsigned char>(bytes[3]);
    };
    const int tagSize = syncSafe(body.constData() + 6);
    int offset = 10;
    const int end = std::min<int>(body.size(), offset + tagSize);
    while (offset + 10 <= end) {
        const QByteArray id = body.mid(offset, 4);
        if (id.trimmed().isEmpty())
            break;
        const char *sizeBytes = body.constData() + offset + 4;
        const int size = version >= 4 ? syncSafe(sizeBytes)
                                      : ((static_cast<unsigned char>(sizeBytes[0]) << 24)
                                         | (static_cast<unsigned char>(sizeBytes[1]) << 16)
                                         | (static_cast<unsigned char>(sizeBytes[2]) << 8)
                                         | static_cast<unsigned char>(sizeBytes[3]));
        offset += 10;
        if (size <= 0 || offset + size > end)
            break;
        if (id == frameId)
            return body.mid(offset, size);
        offset += size;
    }
    return {};
}

QString embeddedLyrics(const QByteArray &body)
{
    const QByteArray uslt = id3FrameText(body, QByteArrayLiteral("USLT"));
    if (uslt.size() > 4) {
        const int encoding = static_cast<unsigned char>(uslt.at(0));
        const int start = uslt.indexOf('\0', 4);
        if (start >= 0)
            return decodeId3Text(uslt.mid(start + 1), encoding).trimmed();
    }
    const QByteArray sylt = id3FrameText(body, QByteArrayLiteral("SYLT"));
    if (sylt.size() > 6) {
        const int encoding = static_cast<unsigned char>(sylt.at(0));
        int cursor = 6;
        const int descriptionEnd = sylt.indexOf('\0', cursor);
        if (descriptionEnd >= 0)
            cursor = descriptionEnd + 1;
        QStringList lines;
        while (cursor < sylt.size()) {
            const int textEnd = sylt.indexOf('\0', cursor);
            if (textEnd < 0 || textEnd + 5 > sylt.size())
                break;
            const QString text = decodeId3Text(sylt.mid(cursor, textEnd - cursor), encoding);
            const quint32 timestamp = (static_cast<unsigned char>(sylt.at(textEnd + 1)) << 24)
                | (static_cast<unsigned char>(sylt.at(textEnd + 2)) << 16)
                | (static_cast<unsigned char>(sylt.at(textEnd + 3)) << 8)
                | static_cast<unsigned char>(sylt.at(textEnd + 4));
            if (!text.trimmed().isEmpty())
                lines.append(QStringLiteral("[%1:%2.%3]%4")
                                 .arg(timestamp / 60000, 2, 10, QLatin1Char('0'))
                                 .arg((timestamp / 1000) % 60, 2, 10, QLatin1Char('0'))
                                 .arg(timestamp % 1000, 3, 10, QLatin1Char('0'))
                                 .arg(text));
            cursor = textEnd + 5;
        }
        return lines.join(QLatin1Char('\n'));
    }
    return {};
}

} // namespace

namespace deepin::lyrics {

namespace {

ProviderResult notFound()
{
    ProviderResult result;
    result.kind = ProviderResultKind::NotFound;
    return result;
}

ProviderResult recordResult(const QString &id, const QString &text, const TrackIdentity &track)
{
    ProviderRecord record;
    record.id = id;
    record.trackName = track.title;
    record.artistName = track.artists.join(QStringLiteral(", "));
    record.albumName = track.album;
    record.durationMs = track.durationMs;
    record.payload.providerId = QStringLiteral("local");
    record.payload.recordId = id;
    record.payload.syncedLyrics = text;
    record.payload.plainLyrics = text;
    record.payload.timing = parseLyrics(record.payload).timing;
    ProviderResult result;
    result.kind = ProviderResultKind::Success;
    result.record = std::move(record);
    return result;
}

QString mediaPath(const TrackIdentity &track)
{
    const QUrl url(track.mediaUrl);
    return url.isLocalFile() ? url.toLocalFile() : QString();
}

} // namespace

QString LocalLyricsProvider::id() const
{
    return QStringLiteral("local");
}

void LocalLyricsProvider::getExact(const TrackIdentity &track, ResultCallback callback)
{
    callback(readTrack(track));
}

void LocalLyricsProvider::search(const TrackIdentity &track, ResultCallback callback)
{
    callback(readTrack(track));
}

void LocalLyricsProvider::getById(const QString &recordId, ResultCallback callback)
{
    callback(readFile(recordId, {}));
}

ProviderResult LocalLyricsProvider::readTrack(const TrackIdentity &track)
{
    const QString audioPath = mediaPath(track);
    if (audioPath.isEmpty())
        return notFound();
    const QFileInfo audioInfo(audioPath);
    const QStringList candidates = {
        audioInfo.path() + QLatin1Char('/') + audioInfo.completeBaseName() + QStringLiteral(".lrc"),
        audioInfo.path() + QLatin1Char('/') + track.title + QStringLiteral(".lrc"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return readFile(candidate, track);
    }

    QFile audio(audioPath);
    if (audio.open(QIODevice::ReadOnly)) {
        const QString lyrics = embeddedLyrics(audio.read(8 * 1024 * 1024));
        if (!lyrics.isEmpty())
            return recordResult(audioPath + QStringLiteral("#embedded"), lyrics, track);
    }
    return notFound();
}

ProviderResult LocalLyricsProvider::readFile(const QString &path, const TrackIdentity &track)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return notFound();
    const QByteArray body = file.readAll();
    if (body.isEmpty())
        return notFound();
    return recordResult(path, QString::fromUtf8(body), track);
}

} // namespace deepin::lyrics
