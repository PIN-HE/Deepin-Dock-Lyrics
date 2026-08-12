#include "infrastructure/mprisplayeradapter.h"

namespace deepin::lyrics {

MprisPlayerAdapter::MprisPlayerAdapter(const QDBusConnection &connection, QObject *parent)
    : PlayerPort(parent)
    , m_discovery(connection, this)
{
    connect(&m_discovery, &MprisPlayerDiscovery::availablePlayersChanged,
            this, &PlayerPort::availablePlayersChanged);
    connect(&m_discovery, &MprisPlayerDiscovery::snapshotChanged,
            this, &PlayerPort::snapshotChanged);
    connect(&m_discovery, &MprisPlayerDiscovery::selectedPlayerAvailableChanged,
            this, &PlayerPort::selectedPlayerAvailableChanged);
    connect(&m_discovery, &MprisPlayerDiscovery::selectedPlayerProcessIdChanged,
            this, &PlayerPort::selectedPlayerProcessIdChanged);
    connect(&m_discovery, &MprisPlayerDiscovery::trackChanged,
            this, &PlayerPort::trackChanged);
}

void MprisPlayerAdapter::start()
{
    m_discovery.start();
}

QList<PlayerDescriptor> MprisPlayerAdapter::availablePlayers() const
{
    return m_discovery.availablePlayers();
}

QString MprisPlayerAdapter::selectedPlayer() const
{
    return m_discovery.selectedPlayer();
}

PlayerSnapshot MprisPlayerAdapter::snapshot() const
{
    return m_discovery.snapshot();
}

bool MprisPlayerAdapter::selectedPlayerAvailable() const
{
    return m_discovery.selectedPlayerAvailable();
}

qint64 MprisPlayerAdapter::selectedPlayerProcessId() const
{
    return m_discovery.selectedPlayerProcessId();
}

void MprisPlayerAdapter::setSelectedPlayer(const QString &busName)
{
    m_discovery.setSelectedPlayer(busName);
}

} // namespace deepin::lyrics
