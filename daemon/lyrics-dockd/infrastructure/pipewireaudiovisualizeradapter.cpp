#include "infrastructure/pipewireaudiovisualizeradapter.h"

#ifdef DEEPIN_DOCK_LYRICS_HAS_PIPEWIRE
#include "infrastructure/audiospectrumanalyzer.h"
#include "infrastructure/pipewirestreammatcher.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw-utils.h>

#include <QMetaObject>

#include <algorithm>
#include <memory>

namespace deepin::lyrics {

namespace {

QString pipeWireProperty(const spa_dict *properties, const char *key)
{
    const char *value = properties ? spa_dict_lookup(properties, key) : nullptr;
    return value ? QString::fromUtf8(value) : QString();
}

bool isInterface(const char *type, const char *expected)
{
    return type && qstrcmp(type, expected) == 0;
}

} // namespace

PipeWireAudioVisualizerAdapter::PipeWireAudioVisualizerAdapter(QObject *parent)
    : AudioVisualizerPort(parent)
{
}

PipeWireAudioVisualizerAdapter::~PipeWireAudioVisualizerAdapter()
{
    stop();
    if (m_registryListener) {
        spa_hook_remove(m_registryListener);
        delete m_registryListener;
    }
    if (m_core)
        pw_core_disconnect(m_core);
    if (m_context)
        pw_context_destroy(m_context);
    if (m_loop) {
        pw_thread_loop_stop(m_loop);
        pw_thread_loop_destroy(m_loop);
    }
}

void PipeWireAudioVisualizerAdapter::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (!enabled) {
        stop();
        return;
    }
    ensurePipeWireConnection();
    reevaluateTarget();
}

void PipeWireAudioVisualizerAdapter::setPlayerProcessId(qint64 processId)
{
    if (m_processId == processId)
        return;
    m_processId = processId;
    reevaluateTarget();
}

VisualizerState PipeWireAudioVisualizerAdapter::state() const
{
    return m_state;
}

StreamMatchConfidence PipeWireAudioVisualizerAdapter::matchConfidence() const
{
    return m_confidence;
}

void PipeWireAudioVisualizerAdapter::stop()
{
    destroyCapture();
    m_enabled = false;
    m_processId = 0;
    setState(VisualizerState::Disabled, StreamMatchConfidence::None);
    emit frameChanged({});
}

void PipeWireAudioVisualizerAdapter::ensurePipeWireConnection()
{
    if (m_loop)
        return;

    pw_init(nullptr, nullptr);
    m_loop = pw_thread_loop_new("deepin-lyrics-visualizer", nullptr);
    if (!m_loop) {
        setState(VisualizerState::Unavailable, StreamMatchConfidence::None);
        return;
    }
    m_context = pw_context_new(pw_thread_loop_get_loop(m_loop), nullptr, 0);
    m_core = m_context ? pw_context_connect(m_context, nullptr, 0) : nullptr;
    if (!m_core || pw_thread_loop_start(m_loop) < 0) {
        setState(VisualizerState::Unavailable, StreamMatchConfidence::None);
        return;
    }

    m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0);
    m_registryListener = new spa_hook;
    static const pw_registry_events events = [] {
        pw_registry_events value {};
        value.version = PW_VERSION_REGISTRY_EVENTS;
        value.global = &PipeWireAudioVisualizerAdapter::onGlobal;
        value.global_remove = &PipeWireAudioVisualizerAdapter::onGlobalRemove;
        return value;
    }();
    pw_registry_add_listener(m_registry, m_registryListener, &events, this);
}

void PipeWireAudioVisualizerAdapter::onGlobal(void *data, quint32 id, quint32,
                                               const char *type, quint32,
                                               const spa_dict *properties)
{
    auto *self = static_cast<PipeWireAudioVisualizerAdapter *>(data);
    QMutexLocker locker(&self->m_mutex);
    if (isInterface(type, PW_TYPE_INTERFACE_Client)) {
        bool valid = false;
        const qint64 processId = pipeWireProperty(properties, PW_KEY_APP_PROCESS_ID)
                                     .toLongLong(&valid);
        if (valid && processId > 0)
            self->m_clients.append({id, processId});
    } else if (isInterface(type, PW_TYPE_INTERFACE_Node)) {
        bool valid = false;
        const quint32 clientId = pipeWireProperty(properties, PW_KEY_CLIENT_ID).toUInt(&valid);
        const bool isOutput = pipeWireProperty(properties, PW_KEY_MEDIA_CLASS)
            == QStringLiteral("Stream/Output/Audio");
        const QString serial = pipeWireProperty(properties, PW_KEY_OBJECT_SERIAL);
        if (valid && isOutput && !serial.isEmpty())
            self->m_nodes.append({id, clientId, serial, true});
    }
    QMetaObject::invokeMethod(self, &PipeWireAudioVisualizerAdapter::reevaluateTarget,
                              Qt::QueuedConnection);
}

void PipeWireAudioVisualizerAdapter::onGlobalRemove(void *data, quint32 id)
{
    auto *self = static_cast<PipeWireAudioVisualizerAdapter *>(data);
    {
        QMutexLocker locker(&self->m_mutex);
        self->m_clients.erase(std::remove_if(self->m_clients.begin(), self->m_clients.end(),
                                             [id](const auto &client) { return client.id == id; }),
                              self->m_clients.end());
        self->m_nodes.erase(std::remove_if(self->m_nodes.begin(), self->m_nodes.end(),
                                           [id](const auto &node) { return node.id == id; }),
                            self->m_nodes.end());
    }
    QMetaObject::invokeMethod(self, &PipeWireAudioVisualizerAdapter::reevaluateTarget,
                              Qt::QueuedConnection);
}

void PipeWireAudioVisualizerAdapter::reevaluateTarget()
{
    if (!m_enabled || m_processId <= 0) {
        destroyCapture();
        setState(m_enabled ? VisualizerState::WaitingForPlayer : VisualizerState::Disabled,
                 StreamMatchConfidence::None);
        return;
    }
    if (!m_core || !m_registry) {
        setState(VisualizerState::Unavailable, StreamMatchConfidence::None);
        return;
    }

    QList<PipeWireClientInfo> clients;
    QList<PipeWireNodeInfo> nodes;
    {
        QMutexLocker locker(&m_mutex);
        clients = m_clients;
        nodes = m_nodes;
    }
    const auto target = selectExactPipeWireAudioStream(clients, nodes, m_processId);
    if (!target) {
        destroyCapture();
        setState(VisualizerState::Unavailable, StreamMatchConfidence::None);
        emit frameChanged({});
        return;
    }
    if (m_stream && m_captureNodeId == target->id)
        return;

    destroyCapture();
    setState(VisualizerState::ResolvingAudioStream, StreamMatchConfidence::ExactPid);

    uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    spa_audio_info_raw audioInfo {};
    audioInfo.format = SPA_AUDIO_FORMAT_F32;
    audioInfo.rate = 48000;
    audioInfo.channels = 1;
    const spa_pod *params[] = {
        spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audioInfo),
    };
    const QByteArray targetSerial = target->objectSerial.toUtf8();
    static const pw_stream_events events = [] {
        pw_stream_events value {};
        value.version = PW_VERSION_STREAM_EVENTS;
        value.process = &PipeWireAudioVisualizerAdapter::onProcess;
        return value;
    }();
    pw_thread_loop_lock(m_loop);
    m_stream = pw_stream_new_simple(
        pw_thread_loop_get_loop(m_loop), "deepin-lyrics-visualizer",
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture",
                          PW_KEY_MEDIA_ROLE, "Music", PW_KEY_TARGET_OBJECT,
                          targetSerial.constData(), nullptr),
        &events, this);
    const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT
                                                     | PW_STREAM_FLAG_MAP_BUFFERS);
    const bool connected = m_stream
        && pw_stream_connect(m_stream, PW_DIRECTION_INPUT, PW_ID_ANY, flags, params, 1) >= 0;
    pw_thread_loop_unlock(m_loop);
    if (!connected) {
        destroyCapture();
        setState(VisualizerState::Unavailable, StreamMatchConfidence::None);
        return;
    }
    m_captureNodeId = target->id;
    setState(VisualizerState::Active, StreamMatchConfidence::ExactPid);
}

void PipeWireAudioVisualizerAdapter::destroyCapture()
{
    if (!m_stream)
        return;
    if (m_loop)
        pw_thread_loop_lock(m_loop);
    pw_stream_destroy(m_stream);
    m_stream = nullptr;
    m_captureNodeId = 0;
    if (m_loop)
        pw_thread_loop_unlock(m_loop);
}

void PipeWireAudioVisualizerAdapter::onProcess(void *data)
{
    auto *self = static_cast<PipeWireAudioVisualizerAdapter *>(data);
    pw_buffer *buffer = pw_stream_dequeue_buffer(self->m_stream);
    if (!buffer)
        return;
    const spa_buffer *spaBuffer = buffer->buffer;
    const spa_data &audio = spaBuffer->datas[0];
    const spa_chunk *chunk = audio.chunk;
    QVector<float> samples;
    if (audio.data && chunk && chunk->size >= sizeof(float)) {
        const auto *source = static_cast<const float *>(audio.data) + chunk->offset / sizeof(float);
        const int count = std::min<int>(chunk->size / sizeof(float), 512);
        samples = QVector<float>(source, source + count);
    }
    pw_stream_queue_buffer(self->m_stream, buffer);
    if (!samples.isEmpty())
        QMetaObject::invokeMethod(self, [self, samples] { self->consumeSamples(samples); },
                                  Qt::QueuedConnection);
}

void PipeWireAudioVisualizerAdapter::consumeSamples(const QVector<float> &samples)
{
    if (m_state != VisualizerState::Active)
        return;
    static const AudioSpectrumAnalyzer analyzer;
    emit frameChanged(analyzer.analyze(samples));
}

void PipeWireAudioVisualizerAdapter::setState(VisualizerState state,
                                              StreamMatchConfidence confidence)
{
    if (m_state == state && m_confidence == confidence)
        return;
    m_state = state;
    m_confidence = confidence;
    emit stateChanged(state, confidence);
}

} // namespace deepin::lyrics
#else

namespace deepin::lyrics {

PipeWireAudioVisualizerAdapter::PipeWireAudioVisualizerAdapter(QObject *parent)
    : AudioVisualizerPort(parent)
{
}

PipeWireAudioVisualizerAdapter::~PipeWireAudioVisualizerAdapter() = default;

void PipeWireAudioVisualizerAdapter::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (enabled)
        setState(VisualizerState::Unavailable, StreamMatchConfidence::None);
    else
        stop();
}

void PipeWireAudioVisualizerAdapter::setPlayerProcessId(qint64 processId)
{
    m_processId = processId;
}

VisualizerState PipeWireAudioVisualizerAdapter::state() const
{
    return m_state;
}

StreamMatchConfidence PipeWireAudioVisualizerAdapter::matchConfidence() const
{
    return m_confidence;
}

void PipeWireAudioVisualizerAdapter::stop()
{
    m_enabled = false;
    m_processId = 0;
    setState(VisualizerState::Disabled, StreamMatchConfidence::None);
    emit frameChanged({});
}

void PipeWireAudioVisualizerAdapter::setState(VisualizerState state,
                                              StreamMatchConfidence confidence)
{
    if (m_state == state && m_confidence == confidence)
        return;
    m_state = state;
    m_confidence = confidence;
    emit stateChanged(state, confidence);
}

} // namespace deepin::lyrics
#endif
