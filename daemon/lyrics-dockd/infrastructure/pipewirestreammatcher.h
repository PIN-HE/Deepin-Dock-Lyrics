#pragma once

#include <QList>
#include <QSet>
#include <QString>

#include <optional>

namespace deepin::lyrics {

struct PipeWireClientInfo {
    quint32 id = 0;
    qint64 processId = 0;
};

struct PipeWireNodeInfo {
    quint32 id = 0;
    quint32 clientId = 0;
    QString objectSerial;
    bool isAudioOutputStream = false;
};

std::optional<PipeWireNodeInfo> selectExactPipeWireAudioStream(
    const QList<PipeWireClientInfo> &clients,
    const QList<PipeWireNodeInfo> &nodes,
    qint64 processId);

std::optional<PipeWireNodeInfo> selectMprisOwnedPipeWireAudioStream(
    const QList<PipeWireClientInfo> &clients,
    const QList<PipeWireNodeInfo> &nodes,
    const QSet<qint64> &processIds);

} // namespace deepin::lyrics
