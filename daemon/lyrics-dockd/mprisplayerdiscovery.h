#pragma once

#include <lyricscore/types.h>

#include <QDBusConnection>
#include <QHash>
#include <QObject>
#include <QTimer>

namespace deepin::lyrics {

class MprisPlayerDiscovery final : public QObject
{
    Q_OBJECT

public:
    explicit MprisPlayerDiscovery(const QDBusConnection &connection, QObject *parent = nullptr);

    void start();
    QList<PlayerDescriptor> availablePlayers() const;
    QString selectedPlayer() const;
    PlayerSnapshot snapshot() const;
    bool selectedPlayerAvailable() const;
    qint64 selectedPlayerProcessId() const;

public slots:
    void setSelectedPlayer(const QString &busName);

signals:
    void availablePlayersChanged(const QList<PlayerDescriptor> &players);
    void snapshotChanged(const PlayerSnapshot &snapshot);
    void selectedPlayerAvailableChanged(bool available);
    void selectedPlayerProcessIdChanged(qint64 processId);
    void trackChanged(const TrackIdentity &track);

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onPropertiesChanged(const QString &interfaceName,
                             const QVariantMap &changedProperties,
                             const QStringList &invalidatedProperties);
    void onSeeked(qlonglong positionUs);
    void pollPosition();
    void publishStableTrack();

private:
    void enumeratePlayers();
    void addPlayer(const QString &busName);
    void removePlayer(const QString &busName);
    void requestRootProperties(const QString &busName);
    void requestSelectedProperties();
    void requestSelectedPlayerProcessId();
    void applyPlayerProperties(const QVariantMap &properties);
    void updateSelectedAvailability();
    void updatePositionPolling();
    void publishSnapshot();
    void clearSnapshot();
    void clearSelectedPlayerProcessId();

    QDBusConnection m_connection;
    QHash<QString, PlayerDescriptor> m_players;
    QString m_selectedPlayer;
    PlayerSnapshot m_snapshot;
    TrackIdentity m_pendingTrack;
    QTimer m_positionTimer;
    QTimer m_trackDebounceTimer;
    int m_selectionGeneration = 0;
    bool m_started = false;
    bool m_selectedPlayerAvailable = false;
    qint64 m_selectedPlayerProcessId = 0;
    bool m_positionRequestPending = false;
};

} // namespace deepin::lyrics
