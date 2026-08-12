#include "lyricscore/lrcparser.h"

#include <QFile>
#include <QtTest>

using namespace deepin::lyrics;

class LrcParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesTimedFixture();
    void removesControlCharacters();
    void fallsBackToPlainLyrics();
    void returnsNoneForEmptyLyrics();
};

void LrcParserTest::parsesTimedFixture()
{
    QFile fixture(QStringLiteral(TEST_FIXTURE_DIR "/complex.lrc"));
    QVERIFY(fixture.open(QIODevice::ReadOnly));

    LyricPayload payload;
    payload.syncedLyrics = QString::fromUtf8(fixture.readAll());
    payload.plainLyrics = QStringLiteral("unused plain lyric");
    const ParsedLyrics parsed = parseLyrics(payload);

    QCOMPARE(parsed.timing, TimingCapability::Line);
    QCOMPARE(parsed.sourceOffsetMs, 120);
    QCOMPARE(parsed.lines.size(), 5);
    QCOMPARE(parsed.lines.at(0).startMs, 1250);
    QCOMPARE(parsed.lines.at(0).text, QStringLiteral("中文歌词"));
    QCOMPARE(parsed.lines.at(1).startMs, 2345);
    QCOMPARE(parsed.lines.at(1).text, QStringLiteral("English lyric"));
    QCOMPARE(parsed.lines.at(2).startMs, 3000);
    QCOMPARE(parsed.lines.at(2).text, QStringLiteral("日本語 한국어"));
    QCOMPARE(parsed.lines.at(3).startMs, 4000);
    QCOMPARE(parsed.lines.at(3).text, QStringLiteral("last duplicate"));
    QCOMPARE(parsed.lines.at(4).startMs, 5500);
    QCOMPARE(parsed.lines.at(4).text, QStringLiteral("中文歌词"));
}

void LrcParserTest::removesControlCharacters()
{
    LyricPayload payload;
    payload.syncedLyrics = QStringLiteral("[00:01.00]保留")
        + QChar(0x0001) + QStringLiteral("Unicode 日本語");

    const ParsedLyrics parsed = parseLyrics(payload);

    QCOMPARE(parsed.lines.size(), 1);
    QCOMPARE(parsed.lines.constFirst().text, QStringLiteral("保留Unicode 日本語"));
}

void LrcParserTest::fallsBackToPlainLyrics()
{
    LyricPayload payload;
    payload.syncedLyrics = QStringLiteral("[ti:metadata only]\ninvalid");
    payload.plainLyrics = QStringLiteral("\n  Plain first line  \nsecond line");

    const ParsedLyrics parsed = parseLyrics(payload);

    QCOMPARE(parsed.timing, TimingCapability::Plain);
    QCOMPARE(parsed.plainText, QStringLiteral("Plain first line"));
    QVERIFY(parsed.lines.isEmpty());
}

void LrcParserTest::returnsNoneForEmptyLyrics()
{
    const ParsedLyrics parsed = parseLyrics({});

    QCOMPARE(parsed.timing, TimingCapability::None);
    QVERIFY(parsed.lines.isEmpty());
    QVERIFY(parsed.plainText.isEmpty());
}

QTEST_APPLESS_MAIN(LrcParserTest)
#include "test_lrcparser.moc"
