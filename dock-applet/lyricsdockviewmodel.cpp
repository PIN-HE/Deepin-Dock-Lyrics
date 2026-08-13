#include "lyricsdockviewmodel.h"

#include <QDBusConnectionInterface>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QProcess>
#include <QStandardPaths>
#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lyricsDockViewModelLog, "deepin.lyrics.dock.viewmodel")

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

QVariantList dbusVariantList(const QVariant &value)
{
    const QVariantList direct = value.toList();
    if (!direct.isEmpty() || value.userType() != qMetaTypeId<QDBusArgument>())
        return direct;

    QVariantList result;
    const QDBusArgument argument = value.value<QDBusArgument>();
    if (argument.currentType() != QDBusArgument::BasicType
        && argument.currentType() != QDBusArgument::ArrayType) {
        return result;
    }
    argument.beginArray();
    while (!argument.atEnd()) {
        QVariant item;
        argument >> item;
        if (item.isValid())
            result.append(item);
    }
    argument.endArray();
    return result;
}

} // namespace

LyricsDockViewModel::LyricsDockViewModel(const QDBusConnection &connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
    , m_serviceWatcher(QString::fromLatin1(serviceName), connection,
                       QDBusServiceWatcher::WatchForOwnerChange, this)
{
    connect(&m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &LyricsDockViewModel::onServiceOwnerChanged);
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
        setServiceAvailable(true);
        connectServiceSignals();
    }
    // 无论服务当前是否已注册，都发起一次 GetState：名字无主时，method call
    // 本身会触发 D-Bus activation，把托盘退出的 daemon 拉起来。
    // Issue GetState regardless of registration: an unowned name is revived by
    // D-Bus activation triggered through this very method call.
    requestState();
}

bool LyricsDockViewModel::serviceAvailable() const
{
    return m_serviceAvailable;
}

QString LyricsDockViewModel::status() const
{
    return m_state.value(QStringLiteral("status"), QStringLiteral("WaitingForService")).toString();
}

bool LyricsDockViewModel::enabled() const
{
    return m_state.value(QStringLiteral("enabled")).toBool();
}

bool LyricsDockViewModel::sessionHidden() const
{
    return m_state.value(QStringLiteral("sessionHidden")).toBool();
}

QString LyricsDockViewModel::previousText() const
{
    return m_frame.value(QStringLiteral("previousText")).toString();
}

QString LyricsDockViewModel::currentText() const
{
    return m_frame.value(QStringLiteral("currentText")).toString();
}

QString LyricsDockViewModel::secondaryText() const
{
    return m_frame.value(QStringLiteral("secondaryText")).toString();
}

QString LyricsDockViewModel::translationText() const
{
    return m_frame.value(QStringLiteral("translationText")).toString();
}

QString LyricsDockViewModel::timingCapability() const
{
    return m_frame.value(QStringLiteral("timingCapability"), QStringLiteral("none")).toString();
}

QString LyricsDockViewModel::source() const
{
    return m_frame.value(QStringLiteral("source")).toString();
}

double LyricsDockViewModel::lineProgress() const
{
    return qBound(0.0, m_frame.value(QStringLiteral("lineProgress")).toDouble(), 1.0);
}

qint64 LyricsDockViewModel::positionMs() const
{
    return std::max<qint64>(0, m_state.value(QStringLiteral("positionMs"), 0).toLongLong());
}

qint64 LyricsDockViewModel::durationMs() const
{
    return m_state.value(QStringLiteral("durationMs"), -1).toLongLong();
}

QString LyricsDockViewModel::artUrl() const
{
    return m_state.value(QStringLiteral("trackArtUrl")).toString();
}

bool LyricsDockViewModel::visualizerAvailable() const
{
    return m_state.value(QStringLiteral("visualizerAvailable")).toBool();
}

bool LyricsDockViewModel::audioVisualizerEnabled() const
{
    return m_state.value(QStringLiteral("audioVisualizerEnabled")).toBool();
}

QVariantList LyricsDockViewModel::visualizerLevels() const
{
    return dbusVariantList(m_state.value(QStringLiteral("visualizerLevels")));
}

void LyricsDockViewModel::setSessionHidden(bool hidden)
{
    if (!m_serviceAvailable)
        return;

    const bool previousHidden = sessionHidden();
    if (previousHidden != hidden) {
        m_state.insert(QStringLiteral("sessionHidden"), hidden);
        emit stateChanged();
    }

    QDBusMessage message = methodCall(QStringLiteral("SetSessionHidden"));
    message.setArguments({hidden});
    const quint64 requestGeneration = m_serviceGeneration;
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, previousHidden, hidden, requestGeneration] {
        const QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (!reply.isError() || requestGeneration != m_serviceGeneration)
            return;

        qCWarning(lyricsDockViewModelLog)
            << "SetSessionHidden failed:" << reply.error().name();
        if (sessionHidden() == hidden) {
            m_state.insert(QStringLiteral("sessionHidden"), previousHidden);
            emit stateChanged();
        }
    });
}

bool LyricsDockViewModel::openSettings()
{
    const QString executable = QStandardPaths::findExecutable(
        QStringLiteral("deepin-lyrics-settings"));
    return !executable.isEmpty() && QProcess::startDetached(executable, {});
}

void LyricsDockViewModel::refresh()
{
    if (m_serviceOwned)
        requestState();
}

void LyricsDockViewModel::onServiceOwnerChanged(const QString &,
                                                const QString &,
                                                const QString &newOwner)
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

    setServiceAvailable(true);
    connectServiceSignals();
    requestState();
}

void LyricsDockViewModel::onStateChanged(const QVariantMap &state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void LyricsDockViewModel::onFrameChanged(const QVariantMap &frame)
{
    if (m_frame == frame)
        return;

    m_frame = frame;
    emit frameChanged();
}

void LyricsDockViewModel::connectServiceSignals()
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
    m_signalsConnected = stateConnected && frameConnected;
    if (!m_signalsConnected)
        disconnectServiceSignals();
}

void LyricsDockViewModel::disconnectServiceSignals()
{
    m_connection.disconnect(QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
                            QString::fromLatin1(interfaceName), QStringLiteral("StateChanged"),
                            this, SLOT(onStateChanged(QVariantMap)));
    m_connection.disconnect(QString::fromLatin1(serviceName), QString::fromLatin1(objectPath),
                            QString::fromLatin1(interfaceName), QStringLiteral("FrameChanged"),
                            this, SLOT(onFrameChanged(QVariantMap)));
    m_signalsConnected = false;
}

void LyricsDockViewModel::requestState()
{
    // 未拥有服务名时同样允许发起探测调用：method call 会触发 D-Bus activation。
    // A probe call is allowed even when the name is unowned: the method call
    // triggers D-Bus activation, which is how a stopped daemon gets revived.
    if (m_stateRequestPending)
        return;

    m_stateRequestPending = true;
    const quint64 requestGeneration = m_serviceGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(QStringLiteral("GetState"))), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, requestGeneration] {
        const QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (requestGeneration != m_serviceGeneration)
            return;

        m_stateRequestPending = false;
        if (reply.isError()) {
            // ServiceUnknown 是"名字无主"的预期结果（本轮调用已触发 activation，
            // 恢复由 serviceOwnerChanged 驱动）；其余错误才需要告警。
            // ServiceUnknown is expected when the name is unowned (this call just
            // triggered activation; recovery is driven by serviceOwnerChanged).
            if (reply.error().name() != QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown"))
                qCWarning(lyricsDockViewModelLog) << "GetState failed:" << reply.error().name();
            clearRemoteData();
            setServiceAvailable(false);
            // 服务名可能先于对象注册，有限重试可覆盖 daemon 启动竞态。
            // The service name may precede object registration; bounded retries cover daemon startup races.
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

void LyricsDockViewModel::setServiceAvailable(bool available)
{
    if (m_serviceAvailable == available)
        return;
    m_serviceAvailable = available;
    emit serviceAvailableChanged();
}

void LyricsDockViewModel::clearRemoteData()
{
    const bool hadState = !m_state.isEmpty();
    const bool hadFrame = !m_frame.isEmpty();
    m_state.clear();
    m_frame.clear();
    if (hadState)
        emit stateChanged();
    if (hadFrame)
        emit frameChanged();
}
