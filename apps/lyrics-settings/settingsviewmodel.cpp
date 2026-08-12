#include "settingsviewmodel.h"

#include <QDBusConnectionInterface>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QLoggingCategory>
#include <QSet>

Q_LOGGING_CATEGORY(settingsViewModelLog, "deepin.lyrics.settings.viewmodel")

namespace {

constexpr auto serviceName = "org.deepin.LyricsDock1";
constexpr auto objectPath = "/org/deepin/LyricsDock1";
constexpr auto interfaceName = "org.deepin.LyricsDock1";
constexpr int stateRetryIntervalMs = 200;
constexpr int maximumStateRetryCount = 5;

QDBusMessage methodCall(const QString &method)
{
    return QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
        QString::fromLatin1(interfaceName), method);
}

QVariantList variantList(const QVariant &value)
{
    QVariantList values;
    if (value.metaType() == QMetaType::fromType<QDBusArgument>())
        values = qdbus_cast<QVariantList>(value.value<QDBusArgument>());
    else
        values = value.toList();

    // a{sv} 嵌套在 av 中时 Qt 会延迟解码每个元素，统一在 D-Bus 边界展开。
    // Qt lazily decodes each a{sv} nested in av; normalize them at the D-Bus boundary.
    for (QVariant &item : values) {
        if (item.metaType() == QMetaType::fromType<QDBusArgument>())
            item = qdbus_cast<QVariantMap>(item.value<QDBusArgument>());
    }
    return values;
}

} // namespace

SettingsViewModel::SettingsViewModel(const QDBusConnection &connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
    , m_serviceWatcher(QString::fromLatin1(serviceName), connection,
                       QDBusServiceWatcher::WatchForOwnerChange, this)
{
    qDBusRegisterMetaType<QList<QVariantMap>>();
    connect(&m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &SettingsViewModel::onServiceOwnerChanged);
    m_stateRetryTimer.setInterval(stateRetryIntervalMs);
    m_stateRetryTimer.setSingleShot(true);
    connect(&m_stateRetryTimer, &QTimer::timeout, this, [this] {
        if (m_serviceOwned)
            requestState();
    });

    if (!m_connection.isConnected() || !m_connection.interface())
        return;
    const QDBusReply<bool> registered = m_connection.interface()->isServiceRegistered(
        QString::fromLatin1(serviceName));
    if (registered.isValid() && registered.value()) {
        m_serviceOwned = true;
        ++m_serviceGeneration;
        connectServiceSignals();
        requestState();
    }
}

bool SettingsViewModel::serviceAvailable() const { return m_serviceAvailable; }
bool SettingsViewModel::busy() const { return m_pendingOperations > 0; }
QVariantMap SettingsViewModel::state() const { return m_state; }
QVariantMap SettingsViewModel::frame() const { return m_frame; }
QVariantList SettingsViewModel::candidates() const { return m_candidates; }

void SettingsViewModel::startLyrics()
{
    callVoid(QStringLiteral("SetEnabled"), {true}, QStringLiteral("start"),
             [this](bool success) {
        if (success)
            callVoid(QStringLiteral("SetSessionHidden"), {false}, QStringLiteral("start"));
    });
}

void SettingsViewModel::setEnabled(bool enabled)
{
    callVoid(QStringLiteral("SetEnabled"), {enabled}, QStringLiteral("enabled"));
}

void SettingsViewModel::setPlayer(const QString &busName)
{
    callVoid(QStringLiteral("SetPlayer"), {busName}, QStringLiteral("player"));
}

void SettingsViewModel::setOffsetMs(int offsetMs)
{
    callVoid(QStringLiteral("SetOffsetMs"), {offsetMs}, QStringLiteral("offset"));
}

void SettingsViewModel::setAudioVisualizerEnabled(bool enabled)
{
    callVoid(QStringLiteral("SetAudioVisualizerEnabled"), {enabled},
             QStringLiteral("audio-visualizer"));
}

void SettingsViewModel::searchCandidates()
{
    callVoid(QStringLiteral("SearchCandidates"), {}, QStringLiteral("search"));
}

void SettingsViewModel::selectCandidate(const QString &providerId, const QString &candidateId)
{
    callVoid(QStringLiteral("SelectCandidate"), {providerId, candidateId},
             QStringLiteral("candidate"));
}

void SettingsViewModel::showInDock()
{
    callVoid(QStringLiteral("SetSessionHidden"), {false}, QStringLiteral("show"));
}

void SettingsViewModel::clearCache()
{
    callVoid(QStringLiteral("ClearCache"), {}, QStringLiteral("cache"));
}

void SettingsViewModel::refresh()
{
    if (m_serviceOwned)
        requestState();
}

void SettingsViewModel::onServiceOwnerChanged(const QString &, const QString &, const QString &newOwner)
{
    ++m_serviceGeneration;
    m_serviceOwned = !newOwner.isEmpty();
    m_stateRetryTimer.stop();
    m_stateRetryCount = 0;
    m_stateRequestPending = false;
    disconnectServiceSignals();
    if (newOwner.isEmpty()) {
        clearRemoteData();
        setServiceAvailable(false);
        return;
    }
    connectServiceSignals();
    requestState();
}

void SettingsViewModel::onStateChanged(const QVariantMap &state)
{
    QVariantMap normalized = state;
    normalized.insert(QStringLiteral("availablePlayers"),
                      variantList(state.value(QStringLiteral("availablePlayers"))));
    normalized.insert(QStringLiteral("candidates"),
                      variantList(state.value(QStringLiteral("candidates"))));
    if (m_state != normalized) {
        m_state = normalized;
        emit stateChanged();
    }
    const QVariantList candidates = normalized.value(QStringLiteral("candidates")).toList();
    if (m_candidates != candidates) {
        m_candidates = candidates;
        emit candidatesChanged();
    }
}

void SettingsViewModel::onFrameChanged(const QVariantMap &frame)
{
    if (m_frame == frame)
        return;
    m_frame = frame;
    emit frameChanged();
}

void SettingsViewModel::onCandidatesChanged(const QList<QVariantMap> &candidates)
{
    QVariantList values;
    values.reserve(candidates.size());
    for (const auto &candidate : candidates)
        values.append(candidate);
    m_candidates = values;
    emit candidatesChanged();
}

void SettingsViewModel::connectServiceSignals()
{
    if (m_signalsConnected)
        return;
    const bool stateConnected = m_connection.connect(
        QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
        QString::fromLatin1(interfaceName), QStringLiteral("StateChanged"),
        this, SLOT(onStateChanged(QVariantMap)));
    const bool frameConnected = m_connection.connect(
        QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
        QString::fromLatin1(interfaceName), QStringLiteral("FrameChanged"),
        this, SLOT(onFrameChanged(QVariantMap)));
    const bool candidatesConnected = m_connection.connect(
        QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
        QString::fromLatin1(interfaceName), QStringLiteral("CandidatesChanged"),
        this, SLOT(onCandidatesChanged(QList<QVariantMap>)));
    m_signalsConnected = stateConnected && frameConnected && candidatesConnected;
    if (!m_signalsConnected)
        disconnectServiceSignals();
}

void SettingsViewModel::disconnectServiceSignals()
{
    m_connection.disconnect(QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
                            QString::fromLatin1(interfaceName), QStringLiteral("StateChanged"),
                            this, SLOT(onStateChanged(QVariantMap)));
    m_connection.disconnect(QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
                            QString::fromLatin1(interfaceName), QStringLiteral("FrameChanged"),
                            this, SLOT(onFrameChanged(QVariantMap)));
    m_connection.disconnect(QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
                            QString::fromLatin1(interfaceName), QStringLiteral("CandidatesChanged"),
                            this, SLOT(onCandidatesChanged(QList<QVariantMap>)));
    m_signalsConnected = false;
}

void SettingsViewModel::requestState()
{
    if (!m_serviceOwned || m_stateRequestPending)
        return;
    m_stateRequestPending = true;
    const quint64 generation = m_serviceGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(QStringLiteral("GetState"))), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation] {
        const QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_serviceGeneration)
            return;
        m_stateRequestPending = false;
        if (reply.isError()) {
            qCWarning(settingsViewModelLog) << "GetState failed:" << reply.error().name();
            setServiceAvailable(false);
            if (m_serviceOwned && m_stateRetryCount < maximumStateRetryCount) {
                ++m_stateRetryCount;
                m_stateRetryTimer.start();
            }
            return;
        }
        m_stateRetryTimer.stop();
        m_stateRetryCount = 0;
        setServiceAvailable(true);
        onStateChanged(reply.value());
    });
}

void SettingsViewModel::callVoid(const QString &method,
                                 const QVariantList &arguments,
                                 const QString &operation,
                                 Completion completion)
{
    if (!m_serviceAvailable) {
        emit operationFailed(QStringLiteral("service-unavailable"));
        if (completion)
            completion(false);
        return;
    }

    QDBusMessage message = methodCall(method);
    message.setArguments(arguments);
    const quint64 generation = m_serviceGeneration;
    beginOperation();
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, operation, completion = std::move(completion)] {
        const QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        endOperation();
        if (generation != m_serviceGeneration)
            return;
        if (reply.isError()) {
            qCWarning(settingsViewModelLog) << "D-Bus operation failed:" << reply.error().name();
            emit operationFailed(stableErrorCode(reply.error().name()));
            if (completion)
                completion(false);
            return;
        }
        requestState();
        emit operationSucceeded(operation);
        if (completion)
            completion(true);
    });
}

void SettingsViewModel::setServiceAvailable(bool available)
{
    if (m_serviceAvailable == available)
        return;
    m_serviceAvailable = available;
    emit serviceAvailableChanged();
}

void SettingsViewModel::beginOperation()
{
    const bool wasBusy = busy();
    ++m_pendingOperations;
    if (!wasBusy)
        emit busyChanged();
}

void SettingsViewModel::endOperation()
{
    const bool wasBusy = busy();
    m_pendingOperations = qMax(0, m_pendingOperations - 1);
    if (wasBusy != busy())
        emit busyChanged();
}

void SettingsViewModel::clearRemoteData()
{
    const bool hadState = !m_state.isEmpty();
    const bool hadFrame = !m_frame.isEmpty();
    const bool hadCandidates = !m_candidates.isEmpty();
    m_state.clear();
    m_frame.clear();
    m_candidates.clear();
    if (hadState)
        emit stateChanged();
    if (hadFrame)
        emit frameChanged();
    if (hadCandidates)
        emit candidatesChanged();
}

QString SettingsViewModel::stableErrorCode(const QString &dbusErrorName) const
{
    static const QSet<QString> known{
        QStringLiteral("invalid-player"), QStringLiteral("offset-out-of-range"),
        QStringLiteral("track-not-searchable"), QStringLiteral("invalid-candidate"),
        QStringLiteral("network-unavailable"), QStringLiteral("rate-limited"),
        QStringLiteral("database-failed"), QStringLiteral("provider-failed"),
    };
    for (const QString &code : known) {
        QString suffix;
        bool upperNext = true;
        for (const QChar character : code) {
            if (character == QLatin1Char('-')) {
                upperNext = true;
                continue;
            }
            suffix.append(upperNext ? character.toUpper() : character);
            upperNext = false;
        }
        if (dbusErrorName.endsWith(suffix))
            return code;
    }
    return QStringLiteral("operation-failed");
}
