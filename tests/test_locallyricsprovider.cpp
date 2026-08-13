#include "infrastructure/locallyricsprovider.h"
#include "infrastructure/playercachelyricsprovider.h"
#include <lyricscore/lrcparser.h>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace deepin::lyrics;

namespace {

TrackIdentity trackFor(const QString &audioPath)
{
    TrackIdentity track;
    track.title = QStringLiteral("Demo Song");
    track.artists = {QStringLiteral("Demo Artist")};
    track.mediaUrl = QUrl::fromLocalFile(audioPath).toString();
    track.searchable = true;
    return track;
}

QByteArray id3v23(const QByteArray &frameId, const QByteArray &payload)
{
    QByteArray frame = frameId;
    const quint32 size = payload.size();
    frame.append(char((size >> 24) & 0xff));
    frame.append(char((size >> 16) & 0xff));
    frame.append(char((size >> 8) & 0xff));
    frame.append(char(size & 0xff));
    frame.append("\0\0", 2);
    frame.append(payload);
    QByteArray body("ID3", 3);
    body.append(char(3));
    body.append(char(0));
    body.append(char(0));
    const quint32 tagSize = frame.size();
    body.append(char((tagSize >> 21) & 0x7f));
    body.append(char((tagSize >> 14) & 0x7f));
    body.append(char((tagSize >> 7) & 0x7f));
    body.append(char(tagSize & 0x7f));
    body.append(frame);
    return body;
}

} // namespace

class LocalLyricsProviderTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsSidecarLrcWithoutNetwork();
    void readsEmbeddedUslt();
    void readsPlayerCacheReadOnly();
    void readsOpenOrpheusCacheReadOnly();
};

void LocalLyricsProviderTest::readsSidecarLrcWithoutNetwork()
{
    QTemporaryDir directory;
    const QString audio = directory.filePath(QStringLiteral("Demo Song.mp3"));
    QFile(audio).open(QIODevice::WriteOnly);
    QFile lrc(directory.filePath(QStringLiteral("Demo Song.lrc")));
    QVERIFY(lrc.open(QIODevice::WriteOnly));
    lrc.write("[00:01.00]Local line\n");
    lrc.close();

    LocalLyricsProvider provider;
    ProviderResult result;
    provider.search(trackFor(audio), [&](ProviderResult value) { result = std::move(value); });
    QCOMPARE(result.kind, ProviderResultKind::Success);
    QCOMPARE(result.record.payload.providerId, QStringLiteral("local"));
    QCOMPARE(parseLyrics(result.record.payload).lines.first().text, QStringLiteral("Local line"));
}

void LocalLyricsProviderTest::readsEmbeddedUslt()
{
    QTemporaryDir directory;
    const QString audio = directory.filePath(QStringLiteral("embedded.mp3"));
    QFile file(audio);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray uslt(1, char(3));
    uslt.append(QByteArray("eng\0Demo description\0Embedded line\n", 34));
    file.write(id3v23("USLT", uslt));
    file.close();

    LocalLyricsProvider provider;
    ProviderResult result;
    provider.search(trackFor(audio), [&](ProviderResult value) { result = std::move(value); });
    QCOMPARE(result.kind, ProviderResultKind::Success);
    QVERIFY(result.record.payload.plainLyrics.contains(QStringLiteral("Embedded line")));
}

void LocalLyricsProviderTest::readsPlayerCacheReadOnly()
{
    QTemporaryDir directory;
    QFile file(directory.filePath(QStringLiteral("Demo Song.lrc")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[00:00.00]Cached line\n");
    file.close();

    PlayerCacheLyricsProvider provider;
    provider.setCacheRoots({directory.path()});
    TrackIdentity track;
    track.title = QStringLiteral("Demo Song");
    track.artists = {QStringLiteral("Demo Artist")};
    ProviderResult result;
    provider.search(track, [&](ProviderResult value) { result = std::move(value); });
    QCOMPARE(result.kind, ProviderResultKind::Success);
    QVERIFY(result.record.payload.syncedLyrics.contains(QStringLiteral("Cached line")));
    QCOMPARE(QFileInfo(file).size(), qint64(QByteArray("[00:00.00]Cached line\n").size()));
}

void LocalLyricsProviderTest::readsOpenOrpheusCacheReadOnly()
{
    QSKIP("Open Orpheus integration uses the user's fixed config path; covered by daemon smoke test.");
}

QTEST_MAIN(LocalLyricsProviderTest)
#include "test_locallyricsprovider.moc"
