#include "lyricscore/normalization.h"

#include <QtTest>

#include <algorithm>

using namespace deepin::lyrics;

class LyricsCoreTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizesWhitespaceUnicodeAndCase();
    void makesStableTrackKeys();
    void scoresAndRanksCandidates();
};

void LyricsCoreTest::normalizesWhitespaceUnicodeAndCase()
{
    QCOMPARE(normalizeText(QString::fromUtf8("  \xef\xbc\xa1\xe3\x80\x80\xef\xbc\xa2  ")), QStringLiteral("a b"));
    QCOMPARE(normalizeText(QStringLiteral("  M\u00dcSIC\tPlayer  ")), QStringLiteral("m\u00fcsic player"));
}

void LyricsCoreTest::makesStableTrackKeys()
{
    TrackIdentity first;
    first.title = QStringLiteral("  \uff2d\uff59 Song ");
    first.artists = {QStringLiteral("An Artist"), QStringLiteral("Second Artist")};
    first.album = QStringLiteral(" The Album ");
    first.durationMs = 213000;
    first.playerBusName = QStringLiteral("org.mpris.MediaPlayer2.First");

    TrackIdentity second = first;
    second.title = QStringLiteral("my song");
    second.album = QStringLiteral("the album");
    second.playerBusName = QStringLiteral("org.mpris.MediaPlayer2.Second");

    QCOMPARE(makeTrackKey(first), makeTrackKey(second));
    QVERIFY(!makeTrackKey(first).contains(first.playerBusName));
}

void LyricsCoreTest::scoresAndRanksCandidates()
{
    TrackIdentity track;
    track.title = QStringLiteral("Example Track");
    track.artists = {QStringLiteral("Example Artist")};
    track.album = QStringLiteral("Example Album");
    track.durationMs = 213000;

    ProviderRecord exact;
    exact.id = QStringLiteral("1");
    exact.trackName = QStringLiteral(" example   track ");
    exact.artistName = QStringLiteral("EXAMPLE ARTIST");
    exact.albumName = QStringLiteral("Example Album");
    exact.durationMs = 214000;
    QVERIFY(scoreLyricCandidate(track, exact) >= 0.85);

    ProviderRecord wrongDuration = exact;
    wrongDuration.id = QStringLiteral("2");
    wrongDuration.durationMs = 220000;
    QVERIFY(scoreLyricCandidate(track, wrongDuration) < scoreLyricCandidate(track, exact));

    ProviderRecord unrelated = exact;
    unrelated.id = QStringLiteral("3");
    unrelated.trackName = QStringLiteral("Different");
    unrelated.artistName = QStringLiteral("Unknown");
    unrelated.albumName = QStringLiteral("Elsewhere");
    unrelated.durationMs = 300000;

    const QList<LyricCandidate> ranked = rankLyricCandidates(
        track, {unrelated, wrongDuration, exact});
    QCOMPARE(ranked.constFirst().candidateId, QStringLiteral("1"));
    QVERIFY(std::none_of(ranked.cbegin(), ranked.cend(), [](const auto &candidate) {
        return candidate.candidateId == QStringLiteral("3");
    }));
}

QTEST_APPLESS_MAIN(LyricsCoreTest)

#include "test_lyricscore.moc"
