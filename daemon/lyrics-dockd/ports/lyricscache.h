#pragma once

#include <lyricscore/lyricprovider.h>

#include <QDateTime>
#include <optional>

namespace deepin::lyrics {

struct CachedLyrics {
    LyricPayload payload;
    double confidence = 0.0;
    bool userConfirmed = false;
};

class LyricsCache
{
public:
    virtual ~LyricsCache() = default;
    virtual bool open(QString *errorCode = nullptr) = 0;
    virtual std::optional<CachedLyrics> find(const QString &trackKey,
                                             const QDateTime &now) = 0;
    virtual bool store(const QString &trackKey,
                       const ProviderRecord &record,
                       double confidence,
                       bool userConfirmed,
                       const QDateTime &now) = 0;
    virtual bool isNegative(const QString &trackKey, const QDateTime &now) = 0;
    virtual bool storeNegative(const QString &trackKey,
                               const QString &reason,
                               const QDateTime &now) = 0;
    virtual QDateTime cooldownUntil() = 0;
    virtual bool setCooldownUntil(const QDateTime &until) = 0;
    virtual bool clear(QString *errorCode = nullptr) = 0;
};

} // namespace deepin::lyrics
