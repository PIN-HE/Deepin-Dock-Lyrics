#include "infrastructure/lrclibprovider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include <algorithm>
#include <limits>

namespace deepin::lyrics {

namespace {

constexpr int minimumRequestIntervalMs = 300;
constexpr int maximumAttempts = 2;

ProviderRecord recordFromJson(const QJsonObject &object, bool *valid)
{
    ProviderRecord record;
    const QJsonValue idValue = object.value(QStringLiteral("id"));
    if (idValue.isDouble())
        record.id = QString::number(static_cast<qint64>(idValue.toDouble()));
    else if (idValue.isString())
        record.id = idValue.toString();
    record.trackName = object.value(QStringLiteral("trackName")).toString().trimmed();
    record.artistName = object.value(QStringLiteral("artistName")).toString().trimmed();
    record.albumName = object.value(QStringLiteral("albumName")).toString().trimmed();
    record.durationMs = qRound64(object.value(QStringLiteral("duration")).toDouble(-1.0) * 1000.0);
    record.instrumental = object.value(QStringLiteral("instrumental")).toBool(false);
    record.payload.providerId = QStringLiteral("lrclib");
    record.payload.recordId = record.id;
    record.payload.syncedLyrics = object.value(QStringLiteral("syncedLyrics")).toString();
    record.payload.plainLyrics = object.value(QStringLiteral("plainLyrics")).toString();
    record.payload.translationLyrics = object.value(QStringLiteral("translationLyrics")).toString();
    record.payload.timing = !record.payload.syncedLyrics.isEmpty()
        ? TimingCapability::Line
        : (!record.payload.plainLyrics.isEmpty() ? TimingCapability::Plain : TimingCapability::None);
    *valid = !record.id.isEmpty() && record.id.size() <= 32
        && std::all_of(record.id.cbegin(), record.id.cend(), [](QChar character) {
               return character.isDigit();
           });
    return record;
}

ProviderResult errorResult(ProviderResultKind kind, const QString &errorCode)
{
    ProviderResult result;
    result.kind = kind;
    result.errorCode = errorCode;
    return result;
}

QDateTime retryAfter(const QByteArray &header, const QDateTime &now)
{
    bool validSeconds = false;
    const int seconds = header.toInt(&validSeconds);
    if (validSeconds)
        return now.addSecs(std::clamp(seconds, 1, 3600));
    const QDateTime date = QDateTime::fromString(QString::fromLatin1(header), Qt::RFC2822Date);
    if (date.isValid() && date > now)
        return std::min(date.toUTC(), now.addSecs(3600));
    return now.addSecs(60);
}

} // namespace

LRCLIBProvider::LRCLIBProvider(HttpTransport &transport, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
{
    m_queueTimer.setSingleShot(true);
    connect(&m_queueTimer, &QTimer::timeout, this, &LRCLIBProvider::processQueue);
}

QString LRCLIBProvider::id() const
{
    return QStringLiteral("lrclib");
}

void LRCLIBProvider::getExact(const TrackIdentity &track, ResultCallback callback)
{
    if (track.durationMs <= 0) {
        callback(errorResult(ProviderResultKind::ProviderError, QStringLiteral("track-not-searchable")));
        return;
    }
    QUrl url = m_baseUrl.resolved(QUrl(QStringLiteral("/api/get")));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("track_name"), track.title);
    query.addQueryItem(QStringLiteral("artist_name"), track.artists.join(QStringLiteral(", ")));
    if (!track.album.trimmed().isEmpty())
        query.addQueryItem(QStringLiteral("album_name"), track.album.trimmed());
    query.addQueryItem(QStringLiteral("duration"),
                       QString::number(qRound64(track.durationMs / 1000.0)));
    url.setQuery(query);
    enqueue({url, false, false, 0, std::move(callback)});
}

void LRCLIBProvider::search(const TrackIdentity &track, ResultCallback callback)
{
    QUrl url = m_baseUrl.resolved(QUrl(QStringLiteral("/api/search")));
    QUrlQuery query;
    const QString title = track.title.trimmed();
    const QString artist = track.artists.join(QStringLiteral(", ")).trimmed();
    query.addQueryItem(QStringLiteral("track_name"), title);
    query.addQueryItem(QStringLiteral("artist_name"), artist);
    if (!track.album.trimmed().isEmpty())
        query.addQueryItem(QStringLiteral("album_name"), track.album.trimmed());
    url.setQuery(query);
    enqueue({url, true, true, 0, std::move(callback)});
}

void LRCLIBProvider::getById(const QString &recordId, ResultCallback callback)
{
    const bool valid = !recordId.isEmpty() && recordId.size() <= 32
        && std::all_of(recordId.cbegin(), recordId.cend(), [](QChar character) {
               return character.isDigit();
           });
    if (!valid) {
        callback(errorResult(ProviderResultKind::ProviderError, QStringLiteral("invalid-candidate")));
        return;
    }
    enqueue({m_baseUrl.resolved(QUrl(QStringLiteral("/api/get/") + recordId)),
             false, false, 0, std::move(callback)});
}

void LRCLIBProvider::setBaseUrl(const QUrl &baseUrl)
{
    m_baseUrl = baseUrl;
}

void LRCLIBProvider::setUserAgent(const QByteArray &userAgent)
{
    if (!userAgent.trimmed().isEmpty())
        m_userAgent = userAgent;
}

void LRCLIBProvider::enqueue(PendingRequest request)
{
    m_queue.enqueue(std::move(request));
    processQueue();
}

void LRCLIBProvider::processQueue()
{
    if (m_inFlight || m_queue.isEmpty())
        return;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (m_cooldownUntil > now) {
        m_queueTimer.start(static_cast<int>(std::min<qint64>(
            now.msecsTo(m_cooldownUntil), std::numeric_limits<int>::max())));
        return;
    }
    if (m_lastDispatch.isValid() && m_lastDispatch.elapsed() < minimumRequestIntervalMs) {
        m_queueTimer.start(minimumRequestIntervalMs - static_cast<int>(m_lastDispatch.elapsed()));
        return;
    }
    dispatch(m_queue.dequeue());
}

void LRCLIBProvider::dispatch(PendingRequest request)
{
    m_inFlight = true;
    m_lastDispatch.restart();
    HttpRequest httpRequest;
    httpRequest.url = request.url;
    httpRequest.headers.insert(QByteArrayLiteral("User-Agent"), m_userAgent);
    httpRequest.headers.insert(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    httpRequest.timeoutMs = 10000;
    m_transport.get(httpRequest,
                    [this, request = std::move(request)](HttpResponse response) mutable {
                        handleResponse(std::move(request), std::move(response));
                    });
}

void LRCLIBProvider::handleResponse(PendingRequest request, HttpResponse response)
{
    m_inFlight = false;
    if (response.statusCode == 429) {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        m_cooldownUntil = retryAfter(response.headers.value(QByteArrayLiteral("retry-after")), now);
        ProviderResult result = errorResult(ProviderResultKind::RateLimited,
                                            QStringLiteral("rate-limited"));
        result.retryAt = m_cooldownUntil;
        complete(std::move(request), std::move(result));
        return;
    }
    if ((!response.networkError.isEmpty() || response.statusCode >= 500)
        && request.attempt < maximumAttempts) {
        ++request.attempt;
        const int delayMs = 300 * (1 << (request.attempt - 1));
        m_inFlight = true;
        QTimer::singleShot(delayMs, this, [this, request = std::move(request)]() mutable {
            m_inFlight = false;
            m_queue.prepend(std::move(request));
            processQueue();
        });
        return;
    }
    if (!response.networkError.isEmpty() || response.statusCode >= 500) {
        complete(std::move(request),
                 errorResult(ProviderResultKind::NetworkError, QStringLiteral("network-unavailable")));
        return;
    }
    if (response.statusCode == 404) {
        if (request.allowQueryFallback) {
            QUrl url = m_baseUrl.resolved(QUrl(QStringLiteral("/api/search")));
            const QUrlQuery structured(request.url);
            QUrlQuery query;
            // The structured request is replaced with a compact keyword query.
            // 结构化请求失败后改用紧凑关键词查询，避免重复发送 album_name。
            query.removeAllQueryItems(QStringLiteral("q"));
            query.addQueryItem(QStringLiteral("q"),
                               structured.queryItemValue(QStringLiteral("track_name"))
                                   + QStringLiteral(" ")
                                   + structured.queryItemValue(QStringLiteral("artist_name")));
            url.setQuery(query);
            enqueue({url, true, false, 0, std::move(request.callback)});
            processQueue();
            return;
        }
        complete(std::move(request), errorResult(ProviderResultKind::NotFound, {}));
        return;
    }
    if (response.statusCode != 200) {
        complete(std::move(request),
                 errorResult(ProviderResultKind::ProviderError, QStringLiteral("provider-failed")));
        return;
    }
    ProviderResult result = request.listResponse ? parseRecords(response.body)
                                                 : parseRecord(response.body);
    if (request.allowQueryFallback
        && (result.kind != ProviderResultKind::Success || result.records.isEmpty())) {
        const QUrlQuery structured(request.url);
        QUrl url = m_baseUrl.resolved(QUrl(QStringLiteral("/api/search")));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("q"),
                           structured.queryItemValue(QStringLiteral("track_name"))
                               + QStringLiteral(" ")
                               + structured.queryItemValue(QStringLiteral("artist_name")));
        url.setQuery(query);
        enqueue({url, true, false, 0, std::move(request.callback)});
        processQueue();
        return;
    }
    complete(std::move(request), std::move(result));
}

void LRCLIBProvider::complete(PendingRequest request, ProviderResult result)
{
    request.callback(std::move(result));
    processQueue();
}

ProviderResult LRCLIBProvider::parseRecord(const QByteArray &body)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return errorResult(ProviderResultKind::ProviderError, QStringLiteral("provider-failed"));
    bool valid = false;
    ProviderRecord record = recordFromJson(document.object(), &valid);
    if (!valid)
        return errorResult(ProviderResultKind::ProviderError, QStringLiteral("provider-failed"));
    ProviderResult result;
    result.kind = ProviderResultKind::Success;
    result.record = std::move(record);
    return result;
}

ProviderResult LRCLIBProvider::parseRecords(const QByteArray &body)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return errorResult(ProviderResultKind::ProviderError, QStringLiteral("provider-failed"));
    ProviderResult result;
    result.kind = ProviderResultKind::Success;
    const QJsonArray array = document.array();
    const qsizetype count = std::min<qsizetype>(array.size(), 20);
    for (int index = 0; index < count; ++index) {
        if (!array.at(index).isObject())
            continue;
        bool valid = false;
        ProviderRecord record = recordFromJson(array.at(index).toObject(), &valid);
        if (valid)
            result.records.append(std::move(record));
    }
    return result;
}

} // namespace deepin::lyrics
