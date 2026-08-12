#pragma once

#include "ports/lyricscache.h"

#include <QSqlDatabase>

namespace deepin::lyrics {

class SqliteLyricsCache final : public LyricsCache
{
public:
    explicit SqliteLyricsCache(QString databasePath = {});
    ~SqliteLyricsCache() override;

    bool open(QString *errorCode = nullptr) override;
    std::optional<CachedLyrics> find(const QString &trackKey,
                                     const QDateTime &now) override;
    bool store(const QString &trackKey,
               const ProviderRecord &record,
               double confidence,
               bool userConfirmed,
               const QDateTime &now) override;
    bool isNegative(const QString &trackKey, const QDateTime &now) override;
    bool storeNegative(const QString &trackKey,
                       const QString &reason,
                       const QDateTime &now) override;
    QDateTime cooldownUntil() override;
    bool setCooldownUntil(const QDateTime &until) override;
    bool clear(QString *errorCode = nullptr) override;
    QString databasePath() const;

private:
    bool createSchema(QString *errorCode);
    bool recoverCorruptDatabase(QString *errorCode);
    void close();

    QString m_databasePath;
    QString m_connectionName;
    QSqlDatabase m_database;
};

} // namespace deepin::lyrics
