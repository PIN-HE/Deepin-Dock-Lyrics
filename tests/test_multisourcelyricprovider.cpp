#include "infrastructure/multisourcelyricprovider.h"

#include <QTest>

using namespace deepin::lyrics;

namespace {

class FakeProvider final : public LyricProvider
{
public:
    explicit FakeProvider(QString providerId)
        : m_id(std::move(providerId))
    {
    }

    QString id() const override { return m_id; }
    void getExact(const TrackIdentity &, ResultCallback callback) override
    {
        ++exactCalls;
        callback(exactResult);
    }
    void search(const TrackIdentity &, ResultCallback callback) override
    {
        ++searchCalls;
        callback(searchResult);
    }
    void getById(const QString &, ResultCallback callback) override
    {
        ++byIdCalls;
        callback(byIdResult);
    }

    QString m_id;
    ProviderResult exactResult;
    ProviderResult searchResult;
    ProviderResult byIdResult;
    int exactCalls = 0;
    int searchCalls = 0;
    int byIdCalls = 0;
};

ProviderRecord record(const QString &id, const QString &providerId)
{
    ProviderRecord value;
    value.id = id;
    value.trackName = QStringLiteral("Track");
    value.artistName = QStringLiteral("Artist");
    value.payload.providerId = providerId;
    value.payload.recordId = id;
    value.payload.timing = TimingCapability::Line;
    value.payload.syncedLyrics = QStringLiteral("[00:00.00]line");
    return value;
}

} // namespace

class MultiSourceLyricProviderTest final : public QObject
{
    Q_OBJECT

private slots:
    void ordersExactByTrustAndFallsBack();
    void aggregatesCandidatesAndAnnotatesTrust();
    void reportsNotFoundWhenAllSourcesMiss();
    void disablesSourceAndTriesByIdFallback();
    void keepsCandidateOwnershipAcrossSources();
};

void MultiSourceLyricProviderTest::ordersExactByTrustAndFallsBack()
{
    FakeProvider low(QStringLiteral("low"));
    FakeProvider high(QStringLiteral("high"));
    low.exactResult.kind = ProviderResultKind::Success;
    low.exactResult.record = record(QStringLiteral("1"), low.id());
    high.exactResult.kind = ProviderResultKind::NotFound;

    MultiSourceLyricProvider provider;
    provider.addSource(low, 0.4);
    provider.addSource(high, 0.9);
    ProviderResult result;
    provider.getExact({}, [&](ProviderResult value) { result = std::move(value); });

    QCOMPARE(high.exactCalls, 1);
    QCOMPARE(low.exactCalls, 1);
    QCOMPARE(result.kind, ProviderResultKind::Success);
    QCOMPARE(result.record.payload.providerId, QStringLiteral("low"));
    QCOMPARE(result.record.sourceTrust, 0.4);
}

void MultiSourceLyricProviderTest::aggregatesCandidatesAndAnnotatesTrust()
{
    FakeProvider first(QStringLiteral("first"));
    FakeProvider second(QStringLiteral("second"));
    first.searchResult.kind = ProviderResultKind::Success;
    first.searchResult.records = {record(QStringLiteral("1"), first.id())};
    second.searchResult.kind = ProviderResultKind::Success;
    second.searchResult.records = {record(QStringLiteral("2"), second.id())};

    MultiSourceLyricProvider provider;
    provider.addSource(first, 0.6);
    provider.addSource(second, 0.8);
    ProviderResult result;
    provider.search({}, [&](ProviderResult value) { result = std::move(value); });

    QCOMPARE(result.kind, ProviderResultKind::Success);
    QCOMPARE(result.records.size(), 2);
    QCOMPARE(result.records.at(0).sourceTrust, 0.8);
    QCOMPARE(result.records.at(1).sourceTrust, 0.6);
}

void MultiSourceLyricProviderTest::reportsNotFoundWhenAllSourcesMiss()
{
    FakeProvider first(QStringLiteral("first"));
    FakeProvider second(QStringLiteral("second"));
    first.searchResult.kind = ProviderResultKind::NotFound;
    second.searchResult.kind = ProviderResultKind::NotFound;

    MultiSourceLyricProvider provider;
    provider.addSource(first, 0.8);
    provider.addSource(second, 0.6);
    ProviderResult result;
    provider.search({}, [&](ProviderResult value) { result = std::move(value); });

    QCOMPARE(result.kind, ProviderResultKind::NotFound);
}

void MultiSourceLyricProviderTest::disablesSourceAndTriesByIdFallback()
{
    FakeProvider disabled(QStringLiteral("disabled"));
    FakeProvider enabled(QStringLiteral("enabled"));
    disabled.byIdResult.kind = ProviderResultKind::Success;
    enabled.byIdResult.kind = ProviderResultKind::Success;
    enabled.byIdResult.record = record(QStringLiteral("2"), enabled.id());

    MultiSourceLyricProvider provider;
    provider.addSource(disabled, 0.9);
    provider.addSource(enabled, 0.5);
    QVERIFY(provider.setSourceEnabled(disabled.id(), false));
    QVERIFY(!provider.setSourceEnabled(QStringLiteral("missing"), false));

    ProviderResult result;
    provider.getById(QStringLiteral("2"), [&](ProviderResult value) { result = std::move(value); });
    QCOMPARE(disabled.byIdCalls, 0);
    QCOMPARE(enabled.byIdCalls, 1);
    QCOMPARE(result.record.sourceTrust, 0.5);
}

void MultiSourceLyricProviderTest::keepsCandidateOwnershipAcrossSources()
{
    FakeProvider first(QStringLiteral("first"));
    FakeProvider second(QStringLiteral("second"));
    first.searchResult.kind = ProviderResultKind::Success;
    first.searchResult.records = {record(QStringLiteral("same"), first.id())};
    second.searchResult.kind = ProviderResultKind::Success;
    second.searchResult.records = {record(QStringLiteral("same"), second.id())};
    first.byIdResult.kind = ProviderResultKind::Success;
    first.byIdResult.record = record(QStringLiteral("same"), first.id());
    second.byIdResult.kind = ProviderResultKind::Success;
    second.byIdResult.record = record(QStringLiteral("same"), second.id());

    MultiSourceLyricProvider provider;
    provider.addSource(first, 0.6);
    provider.addSource(second, 0.8);
    provider.search({}, [](ProviderResult) { });
    ProviderResult result;
    provider.getById(QStringLiteral("same"), [&](ProviderResult value) { result = std::move(value); });

    QCOMPARE(first.byIdCalls, 0);
    QCOMPARE(second.byIdCalls, 1);
    QCOMPARE(result.record.payload.providerId, QStringLiteral("second"));
}

QTEST_MAIN(MultiSourceLyricProviderTest)
#include "test_multisourcelyricprovider.moc"
