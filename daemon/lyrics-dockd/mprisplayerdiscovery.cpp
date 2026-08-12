#include "mprisplayerdiscovery.h"

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QLoggingCategory>

#include <algorithm>

namespace deepin::lyrics {

namespace {

Q_LOGGING_CATEGORY(mprisLog, "org.deepin.lyricsdock.mpris")

constexpr auto mprisPrefix = "org.mpris.MediaPlayer2.";
constexpr auto mprisPath = "/org/mpris/MediaPlayer2";
constexpr auto rootInterface = "org.mpris.MediaPlayer2";
constexpr auto playerInterface = "org.mpris.MediaPlayer2.Player";
constexpr auto propertiesInterface = "org.freedesktop.DBus.Properties";

QVariant unwrapped(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QDBusVariant>())
        return value.value<QDBusVariant>().variant();
    return value;
}

QVariantMap variantMap(const QVariant &value)
{
    const QVariant plain = unwrapped(value);
    if (plain.canConvert<QVariantMap>())
        return plain.toMap();
    if (plain.metaType() == QMetaType::fromType<QDBusArgument>())
        return qdbus_cast<QVariantMap>(plain.value<QDBusArgument>());
    return {};
}

QStringList stringList(const QVariant &value)
{
    const QVariant plain = unwrapped(value);
    if (plain.canConvert<QStringList>())
        return plain.toStringList();
    if (plain.metaType() == QMetaType::fromType<QDBusArgument>())
        return qdbus_cast<QStringList>(plain.value<QDBusArgument>());
    return {};
}

PlaybackStatus playbackStatus(const QString &value)
{
    if (value == QStringLiteral("Playing"))
        return PlaybackStatus::Playing;
    if (value == QStringLiteral("Paused"))
        return PlaybackStatus::Paused;
    if (value != QStringLiteral("Stopped"))
        qCWarning(mprisLog) << "Unknown MPRIS playback status; treating it as stopped";
    return PlaybackStatus::Stopped;
}

bool sameTrack(const TrackIdentity &left, const TrackIdentity &right)
{
    return left.title == right.title
        && left.artists == right.artists
        && left.album == right.album
        && left.durationMs == right.durationMs
        && left.playerBusName == right.playerBusName;
}

TrackIdentity trackFromMetadata(const QVariantMap &metadata, const QString &busName)
{
    TrackIdentity track;
    track.playerBusName = busName;
    track.title = unwrapped(metadata.value(QStringLiteral("xesam:title"))).toString().trimmed();
    track.artists = stringList(metadata.value(QStringLiteral("xesam:artist")));
    track.album = unwrapped(metadata.value(QStringLiteral("xesam:album"))).toString().trimmed();

    bool durationValid = false;
    const qlonglong durationUs = unwrapped(metadata.value(QStringLiteral("mpris:length")))
                                     .toLongLong(&durationValid);
    if (durationValid && durationUs > 0)
        track.durationMs = durationUs / 1000;

    // LRCLIB 精确匹配依赖标题、艺人和时长，缺少任一项都不应联网。
    // LRCLIB exact matching needs title, artist, and duration; never query without all three.
    track.searchable = !track.title.isEmpty()
        && !track.artists.isEmpty()
        && std::any_of(track.artists.cbegin(), track.artists.cend(), [](const QString &artist) {
               return !artist.trimmed().isEmpty();
           })
        && track.durationMs > 0;
    return track;
}

QDBusMessage getAllMessage(const QString &service, const QString &interfaceName)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        service, QString::fromLatin1(mprisPath), QString::fromLatin1(propertiesInterface),
        QStringLiteral("GetAll"));
    message << interfaceName;
    return message;
}

} // namespace

MprisPlayerDiscovery::MprisPlayerDiscovery(const QDBusConnection &connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
{
    qRegisterMetaType<TrackIdentity>();
    qRegisterMetaType<PlayerDescriptor>();
    qRegisterMetaType<QList<PlayerDescriptor>>();
    qRegisterMetaType<PlayerSnapshot>();

    m_positionTimer.setInterval(200);
    m_trackDebounceTimer.setSingleShot(true);
    m_trackDebounceTimer.setInterval(400);

    connect(&m_positionTimer, &QTimer::timeout, this, &MprisPlayerDiscovery::pollPosition);
    connect(&m_trackDebounceTimer, &QTimer::timeout, this, &MprisPlayerDiscovery::publishStableTrack);
}

void MprisPlayerDiscovery::start()
{
    if (m_started)
        return;
    m_started = true;

    m_connection.connect(QStringLiteral("org.freedesktop.DBus"),
                         QStringLiteral("/org/freedesktop/DBus"),
                         QStringLiteral("org.freedesktop.DBus"),
                         QStringLiteral("NameOwnerChanged"),
                         this,
                         SLOT(onNameOwnerChanged(QString,QString,QString)));
    enumeratePlayers();
}

QList<PlayerDescriptor> MprisPlayerDiscovery::availablePlayers() const
{
    QList<PlayerDescriptor> players = m_players.values();
    std::sort(players.begin(), players.end(), [](const auto &left, const auto &right) {
        return left.busName < right.busName;
    });
    return players;
}

QString MprisPlayerDiscovery::selectedPlayer() const
{
    return m_selectedPlayer;
}

PlayerSnapshot MprisPlayerDiscovery::snapshot() const
{
    return m_snapshot;
}

bool MprisPlayerDiscovery::selectedPlayerAvailable() const
{
    return m_selectedPlayerAvailable;
}

void MprisPlayerDiscovery::setSelectedPlayer(const QString &busName)
{
    if (m_selectedPlayer == busName)
        return;

    if (!m_selectedPlayer.isEmpty()) {
        m_connection.disconnect(m_selectedPlayer,
                                QString::fromLatin1(mprisPath),
                                QString::fromLatin1(propertiesInterface),
                                QStringLiteral("PropertiesChanged"),
                                this,
                                SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
        m_connection.disconnect(m_selectedPlayer,
                                QString::fromLatin1(mprisPath),
                                QString::fromLatin1(playerInterface),
                                QStringLiteral("Seeked"),
                                this,
                                SLOT(onSeeked(qlonglong)));
    }

    ++m_selectionGeneration;
    m_selectedPlayer = busName;
    m_trackDebounceTimer.stop();
    m_positionTimer.stop();
    m_positionRequestPending = false;
    clearSnapshot();

    if (!m_selectedPlayer.isEmpty()) {
        m_connection.connect(m_selectedPlayer,
                             QString::fromLatin1(mprisPath),
                             QString::fromLatin1(propertiesInterface),
                             QStringLiteral("PropertiesChanged"),
                             this,
                             SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
        m_connection.connect(m_selectedPlayer,
                             QString::fromLatin1(mprisPath),
                             QString::fromLatin1(playerInterface),
                             QStringLiteral("Seeked"),
                             this,
                             SLOT(onSeeked(qlonglong)));
    }

    updateSelectedAvailability();
    if (m_selectedPlayerAvailable)
        requestSelectedProperties();
}

void MprisPlayerDiscovery::onNameOwnerChanged(const QString &name,
                                               const QString &oldOwner,
                                               const QString &newOwner)
{
    if (!name.startsWith(QString::fromLatin1(mprisPrefix)))
        return;

    if (oldOwner.isEmpty() && !newOwner.isEmpty())
        addPlayer(name);
    else if (!oldOwner.isEmpty() && newOwner.isEmpty())
        removePlayer(name);
    else if (!oldOwner.isEmpty() && !newOwner.isEmpty()) {
        removePlayer(name);
        addPlayer(name);
    }
}

void MprisPlayerDiscovery::onPropertiesChanged(const QString &interfaceName,
                                                const QVariantMap &changedProperties,
                                                const QStringList &invalidatedProperties)
{
    if (interfaceName != QString::fromLatin1(playerInterface) || !m_selectedPlayerAvailable)
        return;

    if (!invalidatedProperties.isEmpty()) {
        requestSelectedProperties();
        return;
    }

    applyPlayerProperties(changedProperties);
}

void MprisPlayerDiscovery::onSeeked(qlonglong positionUs)
{
    if (!m_selectedPlayerAvailable)
        return;

    // MPRIS 用 Seeked 通知主动跳转，暂停时也必须立即刷新位置。
    // MPRIS reports explicit seeks through Seeked, which must update even while paused.
    m_snapshot.positionMs = std::max<qlonglong>(0, positionUs / 1000);
    publishSnapshot();
}

void MprisPlayerDiscovery::pollPosition()
{
    if (!m_selectedPlayerAvailable || m_snapshot.playbackStatus != PlaybackStatus::Playing
        || m_positionRequestPending) {
        return;
    }

    m_positionRequestPending = true;
    const int generation = m_selectionGeneration;
    const QString service = m_selectedPlayer;
    QDBusMessage message = QDBusMessage::createMethodCall(
        service, QString::fromLatin1(mprisPath), QString::fromLatin1(propertiesInterface),
        QStringLiteral("Get"));
    message << QString::fromLatin1(playerInterface) << QStringLiteral("Position");

    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, service] {
                m_positionRequestPending = false;
                QDBusPendingReply<QDBusVariant> reply = *watcher;
                watcher->deleteLater();
                if (reply.isError() || generation != m_selectionGeneration
                    || service != m_selectedPlayer || !m_selectedPlayerAvailable) {
                    return;
                }

                bool valid = false;
                const qlonglong positionUs = reply.value().variant().toLongLong(&valid);
                if (!valid)
                    return;
                m_snapshot.positionMs = std::max<qlonglong>(0, positionUs / 1000);
                publishSnapshot();
            });
}

void MprisPlayerDiscovery::publishStableTrack()
{
    if (!m_selectedPlayerAvailable || !sameTrack(m_pendingTrack, m_snapshot.track))
        return;
    emit trackChanged(m_pendingTrack);
}

void MprisPlayerDiscovery::enumeratePlayers()
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("ListNames"));
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        QDBusPendingReply<QStringList> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            qCWarning(mprisLog) << "Failed to enumerate MPRIS services:" << reply.error().name();
            return;
        }
        for (const QString &name : reply.value()) {
            if (name.startsWith(QString::fromLatin1(mprisPrefix)))
                addPlayer(name);
        }
    });
}

void MprisPlayerDiscovery::addPlayer(const QString &busName)
{
    if (m_players.contains(busName))
        return;

    PlayerDescriptor descriptor;
    descriptor.busName = busName;
    descriptor.identity = busName.mid(QString::fromLatin1(mprisPrefix).size());
    descriptor.available = true;
    m_players.insert(busName, descriptor);
    emit availablePlayersChanged(availablePlayers());

    requestRootProperties(busName);
    updateSelectedAvailability();
    if (busName == m_selectedPlayer)
        requestSelectedProperties();
}

void MprisPlayerDiscovery::removePlayer(const QString &busName)
{
    if (!m_players.remove(busName))
        return;

    emit availablePlayersChanged(availablePlayers());
    if (busName == m_selectedPlayer) {
        ++m_selectionGeneration;
        m_trackDebounceTimer.stop();
        m_positionTimer.stop();
        m_positionRequestPending = false;
        clearSnapshot();
    }
    updateSelectedAvailability();
}

void MprisPlayerDiscovery::requestRootProperties(const QString &busName)
{
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(getAllMessage(busName, QString::fromLatin1(rootInterface))), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, busName] {
        QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError() || !m_players.contains(busName))
            return;

        auto descriptor = m_players.value(busName);
        const QString identity = unwrapped(reply.value().value(QStringLiteral("Identity"))).toString();
        const QString desktopEntry = unwrapped(reply.value().value(QStringLiteral("DesktopEntry"))).toString();
        if (!identity.isEmpty())
            descriptor.identity = identity;
        descriptor.desktopEntry = desktopEntry;
        m_players.insert(busName, descriptor);
        emit availablePlayersChanged(availablePlayers());

        if (busName == m_selectedPlayer && m_selectedPlayerAvailable
            && m_snapshot.busName == busName && m_snapshot.identity != descriptor.identity) {
            m_snapshot.identity = descriptor.identity;
            publishSnapshot();
        }
    });
}

void MprisPlayerDiscovery::requestSelectedProperties()
{
    if (!m_selectedPlayerAvailable)
        return;

    const int generation = m_selectionGeneration;
    const QString service = m_selectedPlayer;
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(getAllMessage(service, QString::fromLatin1(playerInterface))), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, service] {
                QDBusPendingReply<QVariantMap> reply = *watcher;
                watcher->deleteLater();
                if (reply.isError() || generation != m_selectionGeneration
                    || service != m_selectedPlayer || !m_selectedPlayerAvailable) {
                    return;
                }
                applyPlayerProperties(reply.value());
            });
}

void MprisPlayerDiscovery::applyPlayerProperties(const QVariantMap &properties)
{
    bool metadataChanged = false;
    if (properties.contains(QStringLiteral("Metadata"))) {
        const TrackIdentity track = trackFromMetadata(
            variantMap(properties.value(QStringLiteral("Metadata"))), m_selectedPlayer);
        metadataChanged = !sameTrack(track, m_snapshot.track);
        m_snapshot.track = track;
    }

    if (properties.contains(QStringLiteral("PlaybackStatus"))) {
        m_snapshot.playbackStatus = playbackStatus(
            unwrapped(properties.value(QStringLiteral("PlaybackStatus"))).toString());
    }
    if (properties.contains(QStringLiteral("Rate"))) {
        bool valid = false;
        const double rate = unwrapped(properties.value(QStringLiteral("Rate"))).toDouble(&valid);
        m_snapshot.playbackRate = valid && rate > 0.0 ? rate : 1.0;
    }
    if (properties.contains(QStringLiteral("Position"))) {
        bool valid = false;
        const qlonglong positionUs = unwrapped(properties.value(QStringLiteral("Position")))
                                         .toLongLong(&valid);
        m_snapshot.positionMs = valid ? std::max<qlonglong>(0, positionUs / 1000) : -1;
    }

    const auto descriptor = m_players.value(m_selectedPlayer);
    m_snapshot.busName = m_selectedPlayer;
    m_snapshot.identity = descriptor.identity;
    publishSnapshot();
    updatePositionPolling();

    if (metadataChanged) {
        m_pendingTrack = m_snapshot.track;
        m_trackDebounceTimer.start();
    }
}

void MprisPlayerDiscovery::updateSelectedAvailability()
{
    const bool available = !m_selectedPlayer.isEmpty() && m_players.contains(m_selectedPlayer);
    if (m_selectedPlayerAvailable == available)
        return;
    m_selectedPlayerAvailable = available;
    emit selectedPlayerAvailableChanged(available);
}

void MprisPlayerDiscovery::updatePositionPolling()
{
    if (m_selectedPlayerAvailable && m_snapshot.playbackStatus == PlaybackStatus::Playing) {
        if (!m_positionTimer.isActive())
            m_positionTimer.start();
        pollPosition();
    } else {
        m_positionTimer.stop();
    }
}

void MprisPlayerDiscovery::publishSnapshot()
{
    m_snapshot.capturedAt = QDateTime::currentDateTimeUtc();
    emit snapshotChanged(m_snapshot);
}

void MprisPlayerDiscovery::clearSnapshot()
{
    m_snapshot = {};
    m_pendingTrack = {};
    emit snapshotChanged(m_snapshot);
}

} // namespace deepin::lyrics
