#include "lyricscore/lyricsync.h"

#include <QtTest>

using namespace deepin::lyrics;

class LyricSyncTest final : public QObject
{
    Q_OBJECT

private slots:
    void computesLineBoundaries();
    void recomputesAfterSeek();
    void appliesUserOffsetDirection();
    void handlesPlainAndEmptyLyrics();
};

ParsedLyrics timedLyrics()
{
    return {{{1000, QStringLiteral("First")},
             {3000, QStringLiteral("Second")},
             {5000, QStringLiteral("Last")}},
            {}, 0, TimingCapability::Line};
}

void LyricSyncTest::computesLineBoundaries()
{
    const TrackIdentity track{QStringLiteral("Track")};
    const ParsedLyrics lyrics = timedLyrics();

    LyricFrame frame = frameAt(track, lyrics, 500, 0);
    QCOMPARE(frame.currentText, QString());
    QCOMPARE(frame.secondaryText, QStringLiteral("First"));
    QCOMPARE(frame.lineIndex, -1);
    QCOMPARE(frame.lineProgress, 0.0);
    QVERIFY(frame.translationText.isEmpty());

    frame = frameAt(track, lyrics, 2000, 0);
    QCOMPARE(frame.currentText, QStringLiteral("First"));
    QCOMPARE(frame.secondaryText, QStringLiteral("Second"));
    QCOMPARE(frame.lineIndex, 0);
    QCOMPARE(frame.lineProgress, 0.5);

    frame = frameAt(track, lyrics, 6000, 0);
    QCOMPARE(frame.currentText, QStringLiteral("Last"));
    QCOMPARE(frame.secondaryText, QString());
    QCOMPARE(frame.lineIndex, 2);
    QCOMPARE(frame.lineProgress, 1.0);
}

void LyricSyncTest::recomputesAfterSeek()
{
    const ParsedLyrics lyrics = timedLyrics();
    QCOMPARE(frameAt({}, lyrics, 5200, 0).lineIndex, 2);

    const LyricFrame rewound = frameAt({}, lyrics, 1500, 0);
    QCOMPARE(rewound.lineIndex, 0);
    QCOMPARE(rewound.currentText, QStringLiteral("First"));
    QCOMPARE(rewound.lineProgress, 0.25);
}

void LyricSyncTest::appliesUserOffsetDirection()
{
    ParsedLyrics lyrics = timedLyrics();
    lyrics.sourceOffsetMs = 9000;

    QCOMPARE(frameAt({}, lyrics, 2500, 0).lineIndex, 0);
    const LyricFrame advanced = frameAt({}, lyrics, 2500, 500);
    QCOMPARE(advanced.lineIndex, 1);
    QCOMPARE(advanced.lineProgress, 0.0);

    const LyricFrame delayed = frameAt({}, lyrics, 3500, -1000);
    QCOMPARE(delayed.lineIndex, 0);
    QCOMPARE(delayed.lineProgress, 0.75);
}

void LyricSyncTest::handlesPlainAndEmptyLyrics()
{
    ParsedLyrics plain;
    plain.plainText = QStringLiteral("Plain lyric");
    plain.timing = TimingCapability::Plain;
    const LyricFrame plainFrame = frameAt({}, plain, 1000, 0);
    QCOMPARE(plainFrame.currentText, QStringLiteral("Plain lyric"));
    QCOMPARE(plainFrame.lineIndex, -1);
    QCOMPARE(plainFrame.timing, TimingCapability::Plain);

    const LyricFrame emptyFrame = frameAt({}, {}, 1000, 0);
    QVERIFY(emptyFrame.currentText.isEmpty());
    QCOMPARE(emptyFrame.timing, TimingCapability::None);
}

QTEST_APPLESS_MAIN(LyricSyncTest)
#include "test_lyricsync.moc"
