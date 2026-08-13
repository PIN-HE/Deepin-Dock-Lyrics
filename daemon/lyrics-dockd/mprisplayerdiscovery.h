#pragma once

#include <lyricscore/types.h>

#include <QDBusConnection>
#include <QElapsedTimer>
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
    void updateSnapshotPosition();
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
    // 位置外推基准：播放器 Position 上报粒度可能很粗（ter-music 约 1s 步进），
    // 播放中按 Rate 从最近已知位置插值，保证行内进度平滑。
    // Extrapolation base: coarse Position updates (ter-music ~1s steps) are
    // interpolated by Rate while playing for smooth intra-line progress.
    QElapsedTimer m_positionClock;
    qint64 m_lastPositionUs = 0;
    qint64 m_lastPositionClockMs = 0;
};

} // namespace deepin::lyrics
