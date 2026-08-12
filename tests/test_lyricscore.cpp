#include "lyricscore/normalization.h"

#include <QtTest>

using namespace deepin::lyrics;

class LyricsCoreTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizesWhitespaceUnicodeAndCase();
    void makesStableTrackKeys();
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

QTEST_APPLESS_MAIN(LyricsCoreTest)

#include "test_lyricscore.moc"
