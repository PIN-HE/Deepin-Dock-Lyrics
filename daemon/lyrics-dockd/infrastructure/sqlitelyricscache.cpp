#include "infrastructure/sqlitelyricscache.h"

#include <DStandardPaths>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QUuid>

namespace deepin::lyrics {

namespace {

constexpr qint64 recordTtlSeconds = 30LL * 24 * 60 * 60;
constexpr qint64 negativeTtlSeconds = 7LL * 24 * 60 * 60;

bool execute(QSqlQuery &query, const QString &sql)
{
    return query.exec(sql);
}

} // namespace

SqliteLyricsCache::SqliteLyricsCache(QString databasePath)
    : m_databasePath(std::move(databasePath))
    , m_connectionName(QStringLiteral("lyrics-cache-%1").arg(QUuid::createUuid().toString(QUuid::Id128)))
{
    if (m_databasePath.isEmpty()) {
        const QString cacheDirectory = Dtk::Core::DStandardPaths::writableLocation(
            QStandardPaths::CacheLocation);
        m_databasePath = QDir(cacheDirectory).filePath(QStringLiteral("lyrics.sqlite"));
    }
}

SqliteLyricsCache::~SqliteLyricsCache()
{
    close();
}

bool SqliteLyricsCache::open(QString *errorCode)
{
    if (m_database.isValid() && m_database.isOpen())
        return true;
    if (!QDir().mkpath(QFileInfo(m_databasePath).absolutePath())) {
        if (errorCode)
            *errorCode = QStringLiteral("database-failed");
        return false;
    }

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    if (m_database.open() && createSchema(errorCode))
        return true;
    return recoverCorruptDatabase(errorCode);
}

std::optional<CachedLyrics> SqliteLyricsCache::find(const QString &trackKey,
                                                    const QDateTime &now)
{
    if (!open())
        return std::nullopt;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT r.provider_id, r.record_id, r.synced_lyrics, r.plain_lyrics, "
        "m.confidence, m.user_confirmed, r.fetched_at "
        "FROM track_mappings m JOIN lyrics_records r ON r.record_id=m.record_id "
        "WHERE m.track_key=? AND m.created_at>=? "
        "ORDER BY m.user_confirmed DESC, m.created_at DESC LIMIT 1"));
    query.addBindValue(trackKey);
    query.addBindValue(now.toSecsSinceEpoch() - recordTtlSeconds);
    if (!query.exec() || !query.next())
        return std::nullopt;

    CachedLyrics result;
    result.payload.providerId = query.value(0).toString();
    result.payload.recordId = query.value(1).toString();
    result.payload.syncedLyrics = query.value(2).toString();
    result.payload.plainLyrics = query.value(3).toString();
    result.payload.timing = !result.payload.syncedLyrics.isEmpty()
        ? TimingCapability::Line
        : (!result.payload.plainLyrics.isEmpty() ? TimingCapability::Plain : TimingCapability::None);
    result.confidence = query.value(4).toDouble();
    result.userConfirmed = query.value(5).toBool();
    return result;
}

bool SqliteLyricsCache::store(const QString &trackKey,
                              const ProviderRecord &record,
                              double confidence,
                              bool userConfirmed,
                              const QDateTime &now)
{
    if (!open() || !m_database.transaction())
        return false;
    const QByteArray content = record.payload.syncedLyrics.toUtf8() + '\0'
        + record.payload.plainLyrics.toUtf8();
    const QByteArray hash = QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex();

    QSqlQuery recordQuery(m_database);
    recordQuery.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO lyrics_records "
        "(record_id, provider_id, synced_lyrics, plain_lyrics, content_hash, fetched_at, parse_version) "
        "VALUES (?, ?, ?, ?, ?, ?, 1)"));
    recordQuery.addBindValue(record.id);
    recordQuery.addBindValue(QStringLiteral("lrclib"));
    recordQuery.addBindValue(record.payload.syncedLyrics);
    recordQuery.addBindValue(record.payload.plainLyrics);
    recordQuery.addBindValue(QString::fromLatin1(hash));
    recordQuery.addBindValue(now.toSecsSinceEpoch());

    QSqlQuery mappingQuery(m_database);
    mappingQuery.prepare(QStringLiteral(
        "INSERT INTO track_mappings "
        "(track_key, record_id, confidence, user_confirmed, created_at) VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(track_key) DO UPDATE SET "
        "record_id=CASE WHEN track_mappings.user_confirmed=1 AND excluded.user_confirmed=0 "
        "THEN track_mappings.record_id ELSE excluded.record_id END, "
        "confidence=CASE WHEN track_mappings.user_confirmed=1 AND excluded.user_confirmed=0 "
        "THEN track_mappings.confidence ELSE excluded.confidence END, "
        "user_confirmed=MAX(track_mappings.user_confirmed, excluded.user_confirmed), "
        "created_at=CASE WHEN track_mappings.user_confirmed=1 AND excluded.user_confirmed=0 "
        "THEN track_mappings.created_at ELSE excluded.created_at END"));
    mappingQuery.addBindValue(trackKey);
    mappingQuery.addBindValue(record.id);
    mappingQuery.addBindValue(confidence);
    mappingQuery.addBindValue(userConfirmed);
    mappingQuery.addBindValue(now.toSecsSinceEpoch());

    QSqlQuery negativeQuery(m_database);
    negativeQuery.prepare(QStringLiteral("DELETE FROM negative_results WHERE track_key=?"));
    negativeQuery.addBindValue(trackKey);
    const bool success = recordQuery.exec() && mappingQuery.exec() && negativeQuery.exec();
    return success ? m_database.commit() : (m_database.rollback(), false);
}

bool SqliteLyricsCache::isNegative(const QString &trackKey, const QDateTime &now)
{
    if (!open())
        return false;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM negative_results WHERE track_key=? AND created_at>=? LIMIT 1"));
    query.addBindValue(trackKey);
    query.addBindValue(now.toSecsSinceEpoch() - negativeTtlSeconds);
    return query.exec() && query.next();
}

bool SqliteLyricsCache::storeNegative(const QString &trackKey,
                                      const QString &reason,
                                      const QDateTime &now)
{
    if (!open())
        return false;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO negative_results (track_key, reason, created_at) VALUES (?, ?, ?)"));
    query.addBindValue(trackKey);
    query.addBindValue(reason);
    query.addBindValue(now.toSecsSinceEpoch());
    return query.exec();
}

QDateTime SqliteLyricsCache::cooldownUntil()
{
    if (!open())
        return {};
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT until_time FROM request_cooldown WHERE provider_id=?"));
    query.addBindValue(QStringLiteral("lrclib"));
    if (!query.exec() || !query.next())
        return {};
    return QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong(), QTimeZone::UTC);
}

bool SqliteLyricsCache::setCooldownUntil(const QDateTime &until)
{
    if (!open())
        return false;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO request_cooldown (provider_id, until_time) VALUES (?, ?)"));
    query.addBindValue(QStringLiteral("lrclib"));
    query.addBindValue(until.toSecsSinceEpoch());
    return query.exec();
}

bool SqliteLyricsCache::clear(QString *errorCode)
{
    close();
    const bool removedMain = !QFile::exists(m_databasePath) || QFile::remove(m_databasePath);
    const bool removedWal = !QFile::exists(m_databasePath + QStringLiteral("-wal"))
        || QFile::remove(m_databasePath + QStringLiteral("-wal"));
    const bool removedShm = !QFile::exists(m_databasePath + QStringLiteral("-shm"))
        || QFile::remove(m_databasePath + QStringLiteral("-shm"));
    if (!removedMain || !removedWal || !removedShm) {
        if (errorCode)
            *errorCode = QStringLiteral("database-failed");
        return false;
    }
    return open(errorCode);
}

QString SqliteLyricsCache::databasePath() const
{
    return m_databasePath;
}

bool SqliteLyricsCache::createSchema(QString *errorCode)
{
    QSqlQuery query(m_database);
    const bool success = execute(query, QStringLiteral("PRAGMA journal_mode=WAL"))
        && execute(query, QStringLiteral("PRAGMA user_version=1"))
        && execute(query, QStringLiteral(
               "CREATE TABLE IF NOT EXISTS lyrics_records ("
               "record_id TEXT PRIMARY KEY, provider_id TEXT NOT NULL, synced_lyrics TEXT NOT NULL, "
               "plain_lyrics TEXT NOT NULL, content_hash TEXT NOT NULL, fetched_at INTEGER NOT NULL, "
               "parse_version INTEGER NOT NULL)"))
        && execute(query, QStringLiteral(
               "CREATE TABLE IF NOT EXISTS track_mappings ("
               "track_key TEXT PRIMARY KEY, record_id TEXT NOT NULL, confidence REAL NOT NULL, "
               "user_confirmed INTEGER NOT NULL, created_at INTEGER NOT NULL, "
               "FOREIGN KEY(record_id) REFERENCES lyrics_records(record_id))"))
        && execute(query, QStringLiteral(
               "CREATE TABLE IF NOT EXISTS negative_results ("
               "track_key TEXT PRIMARY KEY, reason TEXT NOT NULL, created_at INTEGER NOT NULL)"))
        && execute(query, QStringLiteral(
               "CREATE TABLE IF NOT EXISTS request_cooldown ("
               "provider_id TEXT PRIMARY KEY, until_time INTEGER NOT NULL)"));
    if (!success && errorCode)
        *errorCode = QStringLiteral("database-failed");
    return success;
}

bool SqliteLyricsCache::recoverCorruptDatabase(QString *errorCode)
{
    close();
    if (QFile::exists(m_databasePath)) {
        const QString suffix = QStringLiteral(".corrupt-")
            + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmsszzz"));
        const QString corruptPath = m_databasePath + suffix;
        if (!QFile::rename(m_databasePath, corruptPath)) {
            if (errorCode)
                *errorCode = QStringLiteral("database-failed");
            return false;
        }
        if (QFile::exists(m_databasePath + QStringLiteral("-wal")))
            QFile::rename(m_databasePath + QStringLiteral("-wal"), corruptPath + QStringLiteral("-wal"));
        if (QFile::exists(m_databasePath + QStringLiteral("-shm")))
            QFile::rename(m_databasePath + QStringLiteral("-shm"), corruptPath + QStringLiteral("-shm"));
    }
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    if (!m_database.open()) {
        if (errorCode)
            *errorCode = QStringLiteral("database-failed");
        return false;
    }
    return createSchema(errorCode);
}

void SqliteLyricsCache::close()
{
    if (!m_database.isValid())
        return;
    m_database.close();
    m_database = {};
    QSqlDatabase::removeDatabase(m_connectionName);
}

} // namespace deepin::lyrics
