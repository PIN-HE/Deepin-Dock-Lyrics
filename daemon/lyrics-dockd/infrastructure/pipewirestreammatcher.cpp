#include "infrastructure/pipewirestreammatcher.h"

#include <QSet>

namespace deepin::lyrics {

std::optional<PipeWireNodeInfo> selectExactPipeWireAudioStream(
    const QList<PipeWireClientInfo> &clients,
    const QList<PipeWireNodeInfo> &nodes,
    qint64 processId)
{
    if (processId <= 0)
        return std::nullopt;

    QSet<quint32> matchingClientIds;
    for (const PipeWireClientInfo &client : clients) {
        if (client.processId == processId)
            matchingClientIds.insert(client.id);
    }

    std::optional<PipeWireNodeInfo> candidate;
    for (const PipeWireNodeInfo &node : nodes) {
        if (!node.isAudioOutputStream || node.objectSerial.isEmpty()
            || !matchingClientIds.contains(node.clientId)) {
            continue;
        }
        // 多个输出流时无法证明哪一条是 MPRIS 当前曲目，宁可不可用也不采错流。
        // Multiple output streams cannot prove which belongs to the active MPRIS track; refuse them.
        if (candidate)
            return std::nullopt;
        candidate = node;
    }
    return candidate;
}

} // namespace deepin::lyrics
