#pragma once

#include "lyricscore/types.h"

#include <functional>
#include <QDateTime>

namespace deepin::lyrics {

struct LyricPayload {
    QString providerId;
    QString recordId;
    QString syncedLyrics;
    QString plainLyrics;
    TimingCapability timing = TimingCapability::None;
};

struct ProviderRecord {
    QString id;
    QString trackName;
    QString artistName;
    QString albumName;
    qint64 durationMs = -1;
    bool instrumental = false;
    LyricPayload payload;
};

enum class ProviderResultKind {
    Success,
    NotFound,
    RateLimited,
    NetworkError,
    ProviderError,
};

struct ProviderResult {
    ProviderResultKind kind = ProviderResultKind::ProviderError;
    ProviderRecord record;
    QList<ProviderRecord> records;
    QString errorCode;
    QDateTime retryAt;
};

class LyricProvider
{
public:
    using ResultCallback = std::function<void(ProviderResult)>;

    virtual ~LyricProvider() = default;

    virtual QString id() const = 0;
    virtual void getExact(const TrackIdentity &track, ResultCallback callback) = 0;
    virtual void search(const TrackIdentity &track, ResultCallback callback) = 0;
    virtual void getById(const QString &recordId, ResultCallback callback) = 0;
};

} // namespace deepin::lyrics

Q_DECLARE_METATYPE(deepin::lyrics::LyricPayload)
Q_DECLARE_METATYPE(deepin::lyrics::ProviderRecord)
Q_DECLARE_METATYPE(deepin::lyrics::ProviderResult)
