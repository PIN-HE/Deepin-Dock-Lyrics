#include "infrastructure/lrcliblyricsadapter.h"
#include "infrastructure/sqlitelyricscache.h"

#include <lyricscore/normalization.h>

#include <QSignalSpy>
#include <QQueue>
#include <QTemporaryDir>
#include <QTest>
#include <QDir>
#include <QFile>

using namespace deepin::lyrics;

class FakeLyricProvider final : public LyricProvider
{
public:
    QString id() const override { return QStringLiteral("lrclib"); }
    void getExact(const TrackIdentity &, ResultCallback callback) override
    {
        ++exactCalls;
        exactCallbacks.enqueue(std::move(callback));
    }
    void search(const TrackIdentity &, ResultCallback callback) override
    {
        ++searchCalls;
        searchCallbacks.enqueue(std::move(callback));
    }
    void getById(const QString &recordId, ResultCallback callback) override
    {
        ++byIdCalls;
        requestedIds.append(recordId);
        byIdCallbacks.enqueue(std::move(callback));
    }

    void respondExact(ProviderResult result) { exactCallbacks.dequeue()(std::move(result)); }
    void respondSearch(ProviderResult result) { searchCallbacks.dequeue()(std::move(result)); }
    void respondById(ProviderResult result) { byIdCallbacks.dequeue()(std::move(result)); }

    int exactCalls = 0;
    int searchCalls = 0;
    int byIdCalls = 0;
    QStringList requestedIds;
    QQueue<ResultCallback> exactCallbacks;
    QQueue<ResultCallback> searchCallbacks;
    QQueue<ResultCallback> byIdCallbacks;
};

class TestNullLogSink final : public LogSink
{
public:
    void write(LogLevel, const LogEvent &) override { }
};

class LRCLIBLyricsAdapterTest final : public QObject
{
    Q_OBJECT

private slots:
    void exactHitCachesAndReplaysOffline();
    void exact404SearchesAndFetchesHighConfidenceRecord();
    void exactProviderErrorDoesNotSearch();
    void publishesAmbiguousCandidatesForUserSelection();
    void instrumentalResultBecomesNegativeCache();
    void negativeCacheBlocksAutomaticButManualBypasses();
    void persistsRetryAfterCooldown();
    void clearsOnlyLyricsDatabase();
    void recoversCorruptDatabase();
    void userConfirmedMappingOutranksAutomaticMapping();
};

TrackIdentity adapterTrack()
{
    TrackIdentity track;
    track.title = QStringLiteral("Example Track");
    track.artists = {QStringLiteral("Example Artist")};
    track.album = QStringLiteral("Example Album");
    track.durationMs = 213000;
    track.searchable = true;
    return track;
}

ProviderRecord adapterRecord(const QString &id = QStringLiteral("1000000"),
                             const QString &title = QStringLiteral("Example Track"))
{
    ProviderRecord record;
    record.id = id;
    record.trackName = title;
    record.artistName = QStringLiteral("Example Artist");
    record.albumName = QStringLiteral("Example Album");
    record.durationMs = 213000;
    record.payload.providerId = QStringLiteral("lrclib");
    record.payload.recordId = id;
    record.payload.plainLyrics = QStringLiteral("First line");
    record.payload.syncedLyrics = QStringLiteral("[00:00.00]First line");
    record.payload.timing = TimingCapability::Line;
    return record;
}

ProviderResult successfulRecord(const ProviderRecord &record)
{
    ProviderResult result;
    result.kind = ProviderResultKind::Success;
    result.record = record;
    return result;
}

void LRCLIBLyricsAdapterTest::exactHitCachesAndReplaysOffline()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteLyricsCache cache(directory.filePath(QStringLiteral("lyrics.sqlite")));
    FakeLyricProvider provider;
    TestNullLogSink sink;
    LogEngine logger(sink);
    LrclibLyricsAdapter adapter(provider, cache, logger);
    QSignalSpy readySpy(&adapter, &LyricsPort::lyricsReady);

    adapter.search(adapterTrack());
    QCOMPARE(provider.exactCalls, 1);
    provider.respondExact(successfulRecord(adapterRecord()));
    QCOMPARE(readySpy.count(), 1);

    FakeLyricProvider offlineProvider;
    LrclibLyricsAdapter offline(offlineProvider, cache, logger);
    QSignalSpy offlineReady(&offline, &LyricsPort::lyricsReady);
    offline.search(adapterTrack());
    QCOMPARE(offlineReady.count(), 1);
    QCOMPARE(offlineProvider.exactCalls, 0);
    QCOMPARE(offlineReady.constFirst().constFirst().value<LyricPayload>().recordId,
             QStringLiteral("1000000"));
}

void LRCLIBLyricsAdapterTest::exact404SearchesAndFetchesHighConfidenceRecord()
{
    QTemporaryDir directory;
    SqliteLyricsCache cache(directory.filePath(QStringLiteral("lyrics.sqlite")));
    FakeLyricProvider provider;
    TestNullLogSink sink;
    LogEngine logger(sink);
    LrclibLyricsAdapter adapter(provider, cache, logger);
    QSignalSpy readySpy(&adapter, &LyricsPort::lyricsReady);

    adapter.search(adapterTrack());
    ProviderResult notFound;
    notFound.kind = ProviderResultKind::NotFound;
    provider.respondExact(notFound);
    QCOMPARE(provider.searchCalls, 1);
    ProviderResult searchResult;
    searchResult.kind = ProviderResultKind::Success;
    searchResult.records = {adapterRecord()};
    provider.respondSearch(searchResult);
    QCOMPARE(provider.byIdCalls, 1);
    QCOMPARE(provider.requestedIds.constFirst(), QStringLiteral("1000000"));
    provider.respondById(successfulRecord(adapterRecord()));
    QCOMPARE(readySpy.count(), 1);
}

void LRCLIBLyricsAdapterTest::exactProviderErrorDoesNotSearch()
{
    QTemporaryDir directory;
    SqliteLyricsCache cache(directory.filePath(QStringLiteral("lyrics.sqlite")));
    FakeLyricProvider provider;
    TestNullLogSink sink;
    LogEngine logger(sink);
    LrclibLyricsAdapter adapter(provider, cache, logger);
    QSignalSpy failedSpy(&adapter, &LyricsPort::failed);

    adapter.search(adapterTrack());
    ProviderResult invalid;
    invalid.kind = ProviderResultKind::ProviderError;
    invalid.errorCode = QStringLiteral("provider-failed");
    provider.respondExact(invalid);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(provider.searchCalls, 0);
}

void LRCLIBLyricsAdapterTest::publishesAmbiguousCandidatesForUserSelection()
{
    QTemporaryDir directory;
    SqliteLyricsCache cache(directory.filePath(QStringLiteral("lyrics.sqlite")));
    FakeLyricProvider provider;
    TestNullLogSink sink;
    LogEngine logger(sink);
    LrclibLyricsAdapter adapter(provider, cache, logger);
    QSignalSpy candidateSpy(&adapter, &LyricsPort::candidatesChanged);
    QSignalSpy readySpy(&adapter, &LyricsPort::lyricsReady);
    QSignalSpy failedSpy(&adapter, &LyricsPort::failed);

    adapter.search(adapterTrack());
    ProviderResult notFound;
    notFound.kind = ProviderResultKind::NotFound;
    provider.respondExact(notFound);
    ProviderRecord ambiguous = adapterRecord(QStringLiteral("2000000"));
    ambiguous.artistName = QStringLiteral("Other Performer");
    const double ambiguousScore = scoreLyricCandidate(adapterTrack(), ambiguous);
    QVERIFY(ambiguousScore >= 0.60);
    QVERIFY(ambiguousScore < 0.85);
    ProviderResult searchResult;
    searchResult.kind = ProviderResultKind::Success;
    searchResult.records = {ambiguous};
    provider.respondSearch(searchResult);
    QCOMPARE(candidateSpy.count(), 1);
    QCOMPARE(provider.byIdCalls, 0);

    adapter.selectCandidate(QStringLiteral("lrclib"), QStringLiteral("9999999"));
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(provider.byIdCalls, 0);

    adapter.selectCandidate(QStringLiteral("lrclib"), QStringLiteral("2000000"));
    QCOMPARE(provider.byIdCalls, 1);
    provider.respondById(successfulRecord(ambiguous));
    QCOMPARE(readySpy.count(), 1);
}

void LRCLIBLyricsAdapterTest::instrumentalResultBecomesNegativeCache()
{
    QTemporaryDir directory;
    SqliteLyricsCache cache(directory.filePath(QStringLiteral("lyrics.sqlite")));
    FakeLyricProvider provider;
    TestNullLogSink sink;
    LogEngine logger(sink);
    LrclibLyricsAdapter adapter(provider, cache, logger);
    QSignalSpy noLyricsSpy(&adapter, &LyricsPort::noLyrics);

    adapter.search(adapterTrack());
    ProviderRecord instrumental = adapterRecord();
    instrumental.instrumental = true;
    instrumental.payload = {QStringLiteral("lrclib"), instrumental.id, {}, {},
                            TimingCapability::None};
    provider.respondExact(successfulRecord(instrumental));
    QCOMPARE(noLyricsSpy.count(), 1);

    adapter.search(adapterTrack());
    QCOMPARE(noLyricsSpy.count(), 2);
    QCOMPARE(provider.exactCalls, 1);
}

void LRCLIBLyricsAdapterTest::negativeCacheBlocksAutomaticButManualBypasses()
{
    QTemporaryDir directory;
    SqliteLyricsCache cache(directory.filePath(QStringLiteral("lyrics.sqlite")));
    FakeLyricProvider provider;
    TestNullLogSink sink;
    LogEngine logger(sink);
    LrclibLyricsAdapter adapter(provider, cache, logger);
    QSignalSpy noLyricsSpy(&adapter, &LyricsPort::noLyrics);

    adapter.search(adapterTrack());
    ProviderResult notFound;
    notFound.kind = ProviderResultKind::NotFound;
    provider.respondExact(notFound);
    ProviderResult empty;
    empty.kind = ProviderResultKind::Success;
    provider.respondSearch(empty);
    QCOMPARE(noLyricsSpy.count(), 1);

    adapter.search(adapterTrack());
    QCOMPARE(noLyricsSpy.count(), 2);
    QCOMPARE(provider.exactCalls, 1);

    adapter.searchCandidates(adapterTrack());
    QCOMPARE(provider.searchCalls, 2);
}

void LRCLIBLyricsAdapterTest::persistsRetryAfterCooldown()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("lyrics.sqlite"));
    SqliteLyricsCache cache(path);
    FakeLyricProvider provider;
    TestNullLogSink sink;
    LogEngine logger(sink);
    LrclibLyricsAdapter adapter(provider, cache, logger);
    QSignalSpy failedSpy(&adapter, &LyricsPort::failed);

    adapter.search(adapterTrack());
    ProviderResult limited;
    limited.kind = ProviderResultKind::RateLimited;
    limited.errorCode = QStringLiteral("rate-limited");
    limited.retryAt = QDateTime::currentDateTimeUtc().addSecs(30);
    provider.respondExact(limited);
    QCOMPARE(failedSpy.count(), 1);

    FakeLyricProvider secondProvider;
    SqliteLyricsCache reopened(path);
    LrclibLyricsAdapter restarted(secondProvider, reopened, logger);
    QSignalSpy restartedFailed(&restarted, &LyricsPort::failed);
    restarted.search(adapterTrack());
    QCOMPARE(restartedFailed.count(), 1);
    QCOMPARE(secondProvider.exactCalls, 0);
}

void LRCLIBLyricsAdapterTest::clearsOnlyLyricsDatabase()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("lyrics.sqlite"));
    SqliteLyricsCache cache(path);
    QVERIFY(cache.open());
    QVERIFY(cache.store(QStringLiteral("key"), adapterRecord(), 1.0, true,
                        QDateTime::currentDateTimeUtc()));
    QVERIFY(cache.find(QStringLiteral("key"), QDateTime::currentDateTimeUtc()).has_value());
    QVERIFY(cache.clear());
    QVERIFY(!cache.find(QStringLiteral("key"), QDateTime::currentDateTimeUtc()).has_value());
    QVERIFY(QFile::exists(path));
}

void LRCLIBLyricsAdapterTest::recoversCorruptDatabase()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("lyrics.sqlite"));
    QFile corrupt(path);
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    QCOMPARE(corrupt.write("not a sqlite database"), qint64(21));
    corrupt.close();

    SqliteLyricsCache cache(path);
    QVERIFY(cache.open());
    QVERIFY(QFile::exists(path));
    const QStringList corruptFiles = QDir(directory.path()).entryList(
        {QStringLiteral("lyrics.sqlite.corrupt-*")}, QDir::Files);
    QCOMPARE(corruptFiles.size(), 1);
}

void LRCLIBLyricsAdapterTest::userConfirmedMappingOutranksAutomaticMapping()
{
    QTemporaryDir directory;
    SqliteLyricsCache cache(directory.filePath(QStringLiteral("lyrics.sqlite")));
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(cache.store(QStringLiteral("key"), adapterRecord(QStringLiteral("2000000")),
                        1.0, true, now));
    QVERIFY(cache.store(QStringLiteral("key"), adapterRecord(QStringLiteral("1000000")),
                        0.95, false, now.addSecs(1)));

    const auto cached = cache.find(QStringLiteral("key"), now.addSecs(2));
    QVERIFY(cached.has_value());
    QCOMPARE(cached->payload.recordId, QStringLiteral("2000000"));
    QVERIFY(cached->userConfirmed);
}

QTEST_MAIN(LRCLIBLyricsAdapterTest)
#include "test_lrcliblyricsadapter.moc"
