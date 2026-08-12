#pragma once

#include "ports/playerport.h"

#include "mprisplayerdiscovery.h"

namespace deepin::lyrics {

class MprisPlayerAdapter final : public PlayerPort
{
    Q_OBJECT

public:
    explicit MprisPlayerAdapter(const QDBusConnection &connection, QObject *parent = nullptr);

    void start() override;
    QList<PlayerDescriptor> availablePlayers() const override;
    QString selectedPlayer() const override;
    PlayerSnapshot snapshot() const override;
    bool selectedPlayerAvailable() const override;
    qint64 selectedPlayerProcessId() const override;

public slots:
    void setSelectedPlayer(const QString &busName) override;

private:
    MprisPlayerDiscovery m_discovery;
};

} // namespace deepin::lyrics
