#include "infrastructure/termusicframesource.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(terMusicFrameLog, "deepin.lyrics.dock.termusic")

namespace deepin::lyrics {

namespace {

constexpr auto serviceName = "org.mpris.MediaPlayer2.ter_music";
constexpr auto objectPath = "/org/mpris/MediaPlayer2";
constexpr auto interfaceName = "org.yxzl.ter_music.Lyrics";

// JSON 快照字段（见 Ter-Music docs/API_LYRICS_en_US.md）。
// Snapshot JSON fields.
constexpr auto keyActiveLine = "active_line";
constexpr auto keyLineA = "line_a";
constexpr auto keyLineB = "line_b";
constexpr auto keyIndex = "index";
constexpr auto keyText = "text";
constexpr auto keyTrackId = "track_id";
constexpr auto keyHasLyrics = "has_lyrics";
constexpr auto keyHasTimestamps = "has_timestamps";
constexpr auto keyRevision = "revision";

QString slotText(const QJsonObject &slot)
{
    return slot.value(QLatin1String(keyText)).toString();
}

int slotIndex(const QJsonObject &slot)
{
    return slot.value(QLatin1String(keyIndex)).toInt(-1);
}

} // namespace

TerMusicFrameSource::TerMusicFrameSource(const QDBusConnection &connection, QObject *parent)
    : ExternalFramePort(parent)
    , m_connection(connection)
    , m_watcher(QString::fromLatin1(serviceName), connection,
                QDBusServiceWatcher::WatchForOwnerChange, this)
{
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &TerMusicFrameSource::onServiceOwnerChanged);
}

void TerMusicFrameSource::setSelectedPlayer(const QString &busName)
{
    // 选中播放器变化：只有 Ter-Music 时启用本帧源。
    // Enable the source only while the selected player is Ter-Music.
    const bool shouldListen = busName == QLatin1String(serviceName);
    if (shouldListen == m_listening && busName == m_selectedBusName)
        return;
    m_selectedBusName = busName;
    if (shouldListen)
        startListening();
    else
        stopListening();
}

bool TerMusicFrameSource::active() const
{
    return m_active;
}

void TerMusicFrameSource::startListening()
{
    if (m_listening)
        return;
    m_listening = true;
    // 订阅 LyricsChanged：仅内容变化时由对方发出。
    // Subscribe to LyricsChanged, which the peer emits only on content change.
    m_connection.connect(QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
                         QString::fromLatin1(interfaceName), QStringLiteral("LyricsChanged"),
                         this, SLOT(onLyricsChanged(QString)));
    m_lastRevision = 0;
    m_lastTrackId.clear();
    // 拉一次当前快照（拉取本身也是订阅前丢失帧的补偿）。
    // Fetch the current snapshot once to recover frames missed before subscribing.
    requestSnapshot();
}

void TerMusicFrameSource::stopListening()
{
    if (!m_listening)
        return;
    m_listening = false;
    m_connection.disconnect(QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
                            QString::fromLatin1(interfaceName), QStringLiteral("LyricsChanged"),
                            this, SLOT(onLyricsChanged(QString)));
    emitStopped();
}

void TerMusicFrameSource::requestSnapshot()
{
    const QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
        QString::fromLatin1(interfaceName), QStringLiteral("GetLyrics"));
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher] {
        const QDBusPendingReply<QString> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            // 服务存在但快照拉取失败：不广播错误，等待后续 LyricsChanged。
            // Snapshot fetch failed while the service exists; wait for signals.
            qCWarning(terMusicFrameLog) << "GetLyrics failed:" << reply.error().name();
            return;
        }
        handleSnapshot(reply.value());
    });
}

void TerMusicFrameSource::onLyricsChanged(const QString &json)
{
    handleSnapshot(json);
}

void TerMusicFrameSource::handleSnapshot(const QString &json)
{
    // 空载荷按无歌词处理。
    // Treat an empty payload as "no lyrics".
    if (json.trimmed().isEmpty() || json.trimmed() == QLatin1String("{}")) {
        emitStopped();
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isObject()) {
        qCWarning(terMusicFrameLog) << "Invalid lyrics snapshot JSON";
        return;
    }
    parseSnapshot(document.object());
}

void TerMusicFrameSource::parseSnapshot(const QJsonObject &object)
{
    if (!object.value(QLatin1String(keyHasLyrics)).toBool(false)) {
        emitStopped();
        return;
    }

    // revision 单调递增：重复或乱序（<= 当前值）的更新直接丢弃。
    // The revision is monotonic; drop duplicate or out-of-order (<= current) updates.
    const quint64 revision = object.value(QLatin1String(keyRevision)).toVariant().toULongLong();
    if (revision > 0 && revision <= m_lastRevision)
        return;
    m_lastRevision = revision;

    // 曲目归属：track_id 与 MPRIS mpris:trackid 同源（FNV-1a hash）。
    // Track ownership: track_id is the same FNV-1a hash as MPRIS mpris:trackid.
    const QString trackId = object.value(QLatin1String(keyTrackId)).toString();
    if (!trackId.isEmpty() && trackId != m_lastTrackId) {
        m_lastTrackId = trackId;
        m_pending = ExternalLyricFrame{};
    }

    const bool hasTimestamps = object.value(QLatin1String(keyHasTimestamps)).toBool(false);
    const QString activeSlot = object.value(QLatin1String(keyActiveLine)).toString();
    const QJsonObject lineA = object.value(QLatin1String(keyLineA)).toObject();
    const QJsonObject lineB = object.value(QLatin1String(keyLineB)).toObject();

    // A/B 双缓冲：活动槽为当前行，另一槽为下一行。
    // A/B double buffer: the active slot is the current line, the other is the next.
    const bool activeIsA = activeSlot == QLatin1String("A");
    const QJsonObject &active = activeIsA ? lineA : lineB;
    const QJsonObject &next = activeIsA ? lineB : lineA;

    ExternalLyricFrame frame;
    frame.currentText = slotText(active);
    frame.secondaryText = slotText(next);
    frame.timing = hasTimestamps ? TimingCapability::Line : TimingCapability::Plain;
    frame.lineIndex = hasTimestamps ? slotIndex(active) : -1;
    frame.revision = revision;
    frame.trackId = trackId;
    m_pending = frame;

    // 无歌词时帧文本为空，不发布（由 stopped 语义覆盖）。
    // Publish only when the active slot carries text.
    if (!frame.currentText.isEmpty()) {
        m_active = true;
        emit frameAvailable(frame);
    }
}

void TerMusicFrameSource::onServiceOwnerChanged(const QString &, const QString &, const QString &newOwner)
{
    // Ter-Music 退出：停止帧源；重新出现且仍被选中时恢复。
    // Ter-Music exited: stop the source; recover when it returns while selected.
    if (newOwner.isEmpty()) {
        emitStopped();
    } else if (m_listening) {
        requestSnapshot();
    }
}

void TerMusicFrameSource::emitStopped()
{
    const bool wasActive = m_active;
    m_active = false;
    if (m_listening) {
        m_lastRevision = 0;
        m_lastTrackId.clear();
        m_pending = ExternalLyricFrame{};
    }
    if (wasActive)
        emit stopped();
}

} // namespace deepin::lyrics
