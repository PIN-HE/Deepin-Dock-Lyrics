#pragma once

#include <lyricscore/types.h>

#include <QObject>

namespace deepin::lyrics {

class PlayerPort : public QObject
{
    Q_OBJECT

public:
    explicit PlayerPort(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    virtual void start() = 0;
    virtual QList<PlayerDescriptor> availablePlayers() const = 0;
    virtual QString selectedPlayer() const = 0;
    virtual PlayerSnapshot snapshot() const = 0;
    virtual bool selectedPlayerAvailable() const = 0;
    virtual qint64 selectedPlayerProcessId() const = 0;

public slots:
    virtual void setSelectedPlayer(const QString &busName) = 0;

signals:
    void availablePlayersChanged(const QList<PlayerDescriptor> &players);
    void snapshotChanged(const PlayerSnapshot &snapshot);
    void selectedPlayerAvailableChanged(bool available);
    void selectedPlayerProcessIdChanged(qint64 processId);
    void trackChanged(const TrackIdentity &track);
};

} // namespace deepin::lyrics
