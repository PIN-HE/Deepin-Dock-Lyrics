#include "infrastructure/lrclibprovider.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <QTest>
#include <QUrlQuery>

using namespace deepin::lyrics;

class FakeHttpTransport final : public HttpTransport
{
    Q_OBJECT

public:
    using HttpTransport::HttpTransport;

    void get(const HttpRequest &request, Callback callback) override
    {
        requests.append(request);
        dispatchTimes.append(QDateTime::currentMSecsSinceEpoch());
        callbacks.enqueue(std::move(callback));
        emit requested();
    }

    void respond(HttpResponse response)
    {
        QVERIFY(!callbacks.isEmpty());
        callbacks.dequeue()(std::move(response));
    }

signals:
    void requested();

public:
    QList<HttpRequest> requests;
    QList<qint64> dispatchTimes;
    QQueue<Callback> callbacks;
};

class LRCLIBProviderTest final : public QObject
{
    Q_OBJECT

private slots:
    void buildsExactRequestAndParsesRecord();
    void serializesRequestsWithMinimumInterval();
    void retriesServerFailureThenReportsNetworkError();
    void retryBackoffPreservesQueueOrder();
    void honorsRetryAfterCooldown();
    void acceptsRetryAfterHttpDate();
    void rejectsUnknownDurationAndInvalidId();
};

TrackIdentity testTrack()
{
    TrackIdentity track;
    track.title = QStringLiteral("Example Track");
    track.artists = {QStringLiteral("Example Artist")};
    track.album = QStringLiteral("Example Album");
    track.durationMs = 213400;
    track.searchable = true;
    return track;
}

QByteArray recordJson(const QString &id = QStringLiteral("1000000"))
{
    return QStringLiteral(
        R"({"id":%1,"trackName":"Example Track","artistName":"Example Artist","albumName":"Example Album","duration":213.4,"instrumental":false,"plainLyrics":"First line","syncedLyrics":"[00:00.00]First line"})")
        .arg(id).toUtf8();
}

void LRCLIBProviderTest::buildsExactRequestAndParsesRecord()
{
    FakeHttpTransport transport;
    LRCLIBProvider provider(transport);
    ProviderResult result;
    bool completed = false;
    provider.getExact(testTrack(), [&](ProviderResult value) {
        result = std::move(value);
        completed = true;
    });

    QCOMPARE(transport.requests.size(), 1);
    const HttpRequest &request = transport.requests.constFirst();
    QCOMPARE(request.url.path(), QStringLiteral("/api/get"));
    const QUrlQuery query(request.url);
    QCOMPARE(query.queryItemValue(QStringLiteral("track_name")), QStringLiteral("Example Track"));
    QCOMPARE(query.queryItemValue(QStringLiteral("artist_name")), QStringLiteral("Example Artist"));
    QCOMPARE(query.queryItemValue(QStringLiteral("duration")), QStringLiteral("213"));
    QVERIFY(request.headers.value(QByteArrayLiteral("User-Agent")).contains("DeepinDockLyrics"));
    QVERIFY(request.headers.value(QByteArrayLiteral("User-Agent")).contains("https://"));

    transport.respond({200, recordJson(), {}, {}});
    QVERIFY(completed);
    QCOMPARE(result.kind, ProviderResultKind::Success);
    QCOMPARE(result.record.id, QStringLiteral("1000000"));
    QCOMPARE(result.record.payload.timing, TimingCapability::Line);
}

void LRCLIBProviderTest::serializesRequestsWithMinimumInterval()
{
    FakeHttpTransport transport;
    LRCLIBProvider provider(transport);
    QSignalSpy requestSpy(&transport, &FakeHttpTransport::requested);
    provider.getById(QStringLiteral("1"), [](ProviderResult) { });
    provider.getById(QStringLiteral("2"), [](ProviderResult) { });
    QCOMPARE(transport.requests.size(), 1);

    transport.respond({200, recordJson(QStringLiteral("1")), {}, {}});
    QVERIFY(requestSpy.wait(1000));
    QCOMPARE(transport.requests.size(), 2);
    QVERIFY(transport.dispatchTimes.at(1) - transport.dispatchTimes.at(0) >= 280);
}

void LRCLIBProviderTest::retriesServerFailureThenReportsNetworkError()
{
    FakeHttpTransport transport;
    LRCLIBProvider provider(transport);
    QSignalSpy requestSpy(&transport, &FakeHttpTransport::requested);
    ProviderResult result;
    bool completed = false;
    provider.getById(QStringLiteral("1"), [&](ProviderResult value) {
        result = std::move(value);
        completed = true;
    });

    transport.respond({500, {}, {}, {}});
    QVERIFY(requestSpy.wait(1000));
    transport.respond({500, {}, {}, {}});
    QVERIFY(requestSpy.wait(1500));
    transport.respond({500, {}, {}, {}});
    QVERIFY(completed);
    QCOMPARE(result.kind, ProviderResultKind::NetworkError);
    QCOMPARE(result.errorCode, QStringLiteral("network-unavailable"));
}

void LRCLIBProviderTest::retryBackoffPreservesQueueOrder()
{
    FakeHttpTransport transport;
    LRCLIBProvider provider(transport);
    QSignalSpy requestSpy(&transport, &FakeHttpTransport::requested);
    provider.getById(QStringLiteral("1"), [](ProviderResult) { });
    transport.respond({500, {}, {}, {}});
    provider.getById(QStringLiteral("2"), [](ProviderResult) { });
    QTest::qWait(150);
    QCOMPARE(transport.requests.size(), 1);

    QVERIFY(requestSpy.wait(1000));
    QCOMPARE(transport.requests.at(1).url.path(), QStringLiteral("/api/get/1"));
    transport.respond({200, recordJson(QStringLiteral("1")), {}, {}});
    QVERIFY(requestSpy.wait(1000));
    QCOMPARE(transport.requests.at(2).url.path(), QStringLiteral("/api/get/2"));
}

void LRCLIBProviderTest::honorsRetryAfterCooldown()
{
    FakeHttpTransport transport;
    LRCLIBProvider provider(transport);
    QSignalSpy requestSpy(&transport, &FakeHttpTransport::requested);
    ProviderResult limited;
    provider.getById(QStringLiteral("1"), [&](ProviderResult value) { limited = std::move(value); });
    transport.respond({429, {}, {{QByteArrayLiteral("retry-after"), QByteArrayLiteral("1")}}, {}});
    QCOMPARE(limited.kind, ProviderResultKind::RateLimited);
    QVERIFY(limited.retryAt > QDateTime::currentDateTimeUtc());

    provider.getById(QStringLiteral("2"), [](ProviderResult) { });
    QTest::qWait(500);
    QCOMPARE(transport.requests.size(), 1);
    QVERIFY(requestSpy.wait(1200));
    QCOMPARE(transport.requests.size(), 2);
}

void LRCLIBProviderTest::acceptsRetryAfterHttpDate()
{
    FakeHttpTransport transport;
    LRCLIBProvider provider(transport);
    ProviderResult limited;
    provider.getById(QStringLiteral("1"), [&](ProviderResult value) { limited = std::move(value); });
    const QDateTime expected = QDateTime::currentDateTimeUtc().addSecs(30);
    transport.respond({429, {},
                       {{QByteArrayLiteral("retry-after"),
                         expected.toString(Qt::RFC2822Date).toLatin1()}}, {}});
    QCOMPARE(limited.kind, ProviderResultKind::RateLimited);
    QVERIFY(limited.retryAt >= expected.addSecs(-1));
}

void LRCLIBProviderTest::rejectsUnknownDurationAndInvalidId()
{
    FakeHttpTransport transport;
    LRCLIBProvider provider(transport);
    TrackIdentity track = testTrack();
    track.durationMs = -1;
    ProviderResult exactResult;
    provider.getExact(track, [&](ProviderResult value) { exactResult = std::move(value); });
    QCOMPARE(exactResult.errorCode, QStringLiteral("track-not-searchable"));
    provider.getById(QStringLiteral("../../private"), [&](ProviderResult value) {
        exactResult = std::move(value);
    });
    QCOMPARE(exactResult.errorCode, QStringLiteral("invalid-candidate"));
    QVERIFY(transport.requests.isEmpty());
}

QTEST_MAIN(LRCLIBProviderTest)
#include "test_lrclibprovider.moc"
