#include "infrastructure/playercachelyricsprovider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QRegularExpression>
#include <QLoggingCategory>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>

#include <lyricscore/lrcparser.h>

namespace deepin::lyrics {

Q_LOGGING_CATEGORY(playerCacheLog, "org.deepin.lyricsdock.playercache")

namespace {

ProviderResult miss()
{
    ProviderResult result;
    result.kind = ProviderResultKind::NotFound;
    return result;
}

QString safeName(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._ -]")), QStringLiteral("_"));
    return value.trimmed();
}

} // namespace

PlayerCacheLyricsProvider::PlayerCacheLyricsProvider()
{
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    m_roots = {cache + QStringLiteral("/lyrics"), base + QStringLiteral("/netease"),
               base + QStringLiteral("/qqmusic"), base + QStringLiteral("/kugou")};

    // Open Orpheus stores Netease lyrics by song ID and metadata in webdb.dat.
    // Open Orpheus 按歌曲 ID 保存歌词，曲目信息位于 webdb.dat；两者均只读访问。
    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString orpheus = QDir(config).filePath(QStringLiteral("open-orpheus"));
    m_openOrpheusCacheRoot = QDir(orpheus).filePath(QStringLiteral("cache/lyrics"));
    m_openOrpheusDatabase = QDir(orpheus).filePath(QStringLiteral("webdb.dat"));
}

QString PlayerCacheLyricsProvider::id() const
{
    return QStringLiteral("player-cache");
}

void PlayerCacheLyricsProvider::getExact(const TrackIdentity &track, ResultCallback callback)
{
    callback(readTrack(track));
}

void PlayerCacheLyricsProvider::search(const TrackIdentity &track, ResultCallback callback)
{
    callback(readTrack(track));
}

void PlayerCacheLyricsProvider::getById(const QString &recordId, ResultCallback callback)
{
    callback(readTrack({recordId, {}, {}, -1, {}, false, {}, {}}));
}

QStringList PlayerCacheLyricsProvider::cacheRoots() const
{
    return m_roots;
}

void PlayerCacheLyricsProvider::setCacheRoots(const QStringList &roots)
{
    m_roots.clear();
    for (const QString &root : roots) {
        const QFileInfo info(root);
        if (info.isAbsolute())
            m_roots.append(QDir::cleanPath(root));
    }
}

ProviderResult PlayerCacheLyricsProvider::readTrack(const TrackIdentity &track) const
{
    const QString title = safeName(track.title);
    if (title.isEmpty())
        return miss();
    const QStringList names = {title + QStringLiteral(".lrc"),
                               title + QStringLiteral(".txt")};
    for (const QString &root : m_roots) {
        for (const QString &name : names) {
            const QString path = QDir(root).filePath(name);
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                continue;
            const QByteArray body = file.readAll();
            if (body.isEmpty())
                continue;
            ProviderRecord record;
            record.id = path;
            record.trackName = track.title;
            record.artistName = track.artists.join(QStringLiteral(", "));
            record.albumName = track.album;
            record.durationMs = track.durationMs;
            LyricPayload payload;
            payload.providerId = id();
            payload.recordId = path;
            payload.syncedLyrics = QString::fromUtf8(body);
            payload.plainLyrics = payload.syncedLyrics;
            payload.timing = parseLyrics(payload).timing;
            record.payload = std::move(payload);
            ProviderResult result;
            result.kind = ProviderResultKind::Success;
            result.record = std::move(record);
            return result;
        }
    }

    // Electron-based Open Orpheus may advertise an org.mpris.MediaPlayer2.chromium.*
    // bus, so the cache lookup cannot rely on the bus-name prefix.
    // Open Orpheus 可能使用 org.mpris.MediaPlayer2.chromium.* 总线名，不能仅靠总线前缀判断。
    const ProviderResult openOrpheusResult = readOpenOrpheusTrack(track);
    return openOrpheusResult.kind == ProviderResultKind::Success ? openOrpheusResult : miss();
}

ProviderResult PlayerCacheLyricsProvider::readOpenOrpheusTrack(const TrackIdentity &track) const
{
    if (m_openOrpheusCacheRoot.isEmpty() || m_openOrpheusDatabase.isEmpty()
        || !QFileInfo::exists(m_openOrpheusDatabase))
        return miss();

    const QString connectionName = QStringLiteral("lyrics-cache-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(m_openOrpheusDatabase);
    database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    ProviderResult result = miss();
    if (database.open()) {
        QSqlQuery query(database);
        if (query.exec(QStringLiteral("SELECT id, jsonStr FROM historyTracks WHERE jsonStr IS NOT NULL"))) {
            while (query.next()) {
                const QString songId = query.value(0).toString();
                const QJsonDocument document = QJsonDocument::fromJson(query.value(1).toString().toUtf8());
                const QJsonObject metadata = document.object();
                if (metadata.value(QStringLiteral("name")).toString().trimmed() != track.title.trimmed())
                    continue;
                const QJsonArray artists = metadata.value(QStringLiteral("artists")).toArray();
                bool artistMatch = track.artists.isEmpty();
                for (const QJsonValue &artist : artists) {
                    if (track.artists.contains(artist.toObject().value(QStringLiteral("name")).toString(),
                                               Qt::CaseInsensitive)) {
                        artistMatch = true;
                        break;
                    }
                }
                if (!artistMatch)
                    continue;

                QFile file(QDir(m_openOrpheusCacheRoot).filePath(songId));
                if (!file.open(QIODevice::ReadOnly))
                    continue;
                const QJsonDocument lyricsDocument = QJsonDocument::fromJson(file.readAll());
                const QString lyrics = lyricsDocument.object().value(QStringLiteral("lrc"))
                                           .toObject().value(QStringLiteral("lyric")).toString();
                if (lyrics.trimmed().isEmpty())
                    continue;

                ProviderRecord record;
                record.id = file.fileName();
                record.trackName = track.title;
                record.artistName = track.artists.join(QStringLiteral(", "));
                record.albumName = track.album;
                record.durationMs = track.durationMs;
                LyricPayload payload;
                payload.providerId = this->id();
                payload.recordId = file.fileName();
                payload.syncedLyrics = lyrics;
                payload.plainLyrics = lyrics;
                payload.timing = parseLyrics(payload).timing;
                record.payload = std::move(payload);
                result.kind = ProviderResultKind::Success;
                result.record = std::move(record);
                break;
            }
        } else {
            qCWarning(playerCacheLog) << "Open Orpheus metadata query failed" << query.lastError();
        }
        query.clear();
    } else {
        qCWarning(playerCacheLog) << "Open Orpheus database open failed" << database.lastError();
    }
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
    return result;
}

} // namespace deepin::lyrics
