#pragma once

#include "infrastructure/pipewirestreammatcher.h"
#include "ports/audiovisualizerport.h"

#include <QList>
#include <QMutex>

#include <memory>

struct pw_thread_loop;
struct pw_context;
struct pw_core;
struct pw_registry;
struct pw_stream;
struct pw_client;
struct pw_client_info;
struct spa_hook;
struct spa_dict;
struct pw_buffer;

namespace deepin::lyrics {

class PipeWireAudioVisualizerAdapter final : public AudioVisualizerPort
{
    Q_OBJECT

public:
    explicit PipeWireAudioVisualizerAdapter(QObject *parent = nullptr);
    ~PipeWireAudioVisualizerAdapter() override;

    void setEnabled(bool enabled) override;
    void setPlayerProcessId(qint64 processId) override;
    VisualizerState state() const override;
    StreamMatchConfidence matchConfidence() const override;
    void stop() override;

private:
#ifdef DEEPIN_DOCK_LYRICS_HAS_PIPEWIRE
private slots:
    void reevaluateTarget();
    void consumeSamples(const QVector<float> &samples);

private:
    static void onGlobal(void *data, quint32 id, quint32 permissions, const char *type,
                         quint32 version, const spa_dict *properties);
    static void onGlobalRemove(void *data, quint32 id);
    static void onClientInfo(void *data, const struct pw_client_info *info);
    static void onProcess(void *data);
    void ensurePipeWireConnection();
    void destroyCapture();
    QSet<qint64> mprisOwnedProcessIds(const QList<PipeWireClientInfo> &clients) const;
    struct ClientBinding {
        PipeWireAudioVisualizerAdapter *owner = nullptr;
        quint32 id = 0;
        pw_client *client = nullptr;
        spa_hook *listener = nullptr;
    };
    void removeClientBinding(quint32 id);
#endif
    void setState(VisualizerState state, StreamMatchConfidence confidence);

    mutable QMutex m_mutex;
    QList<PipeWireClientInfo> m_clients;
    QList<PipeWireNodeInfo> m_nodes;
#ifdef DEEPIN_DOCK_LYRICS_HAS_PIPEWIRE
    QList<std::shared_ptr<ClientBinding>> m_clientBindings;
#endif
    pw_thread_loop *m_loop = nullptr;
    pw_context *m_context = nullptr;
    pw_core *m_core = nullptr;
    pw_registry *m_registry = nullptr;
    pw_stream *m_stream = nullptr;
    spa_hook *m_registryListener = nullptr;
    qint64 m_processId = 0;
    quint32 m_captureNodeId = 0;
    bool m_enabled = false;
    VisualizerState m_state = VisualizerState::Disabled;
    StreamMatchConfidence m_confidence = StreamMatchConfidence::None;
};

} // namespace deepin::lyrics
