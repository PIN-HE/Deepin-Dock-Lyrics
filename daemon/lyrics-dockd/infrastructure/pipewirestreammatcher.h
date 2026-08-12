#pragma once

#include <QList>
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

} // namespace deepin::lyrics
