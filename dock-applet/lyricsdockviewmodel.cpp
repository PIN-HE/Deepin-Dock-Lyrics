#include "lyricsdockviewmodel.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QProcess>
#include <QStandardPaths>
#include <QLoggingCategory>

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
        requestState();
    }
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
    return m_previousText;
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

    const QString oldCurrent = currentText();
    const QString newCurrent = frame.value(QStringLiteral("currentText")).toString();
    const QString newTrackKey = frame.value(QStringLiteral("trackKey")).toString();
    const bool sameTrack = !newTrackKey.isEmpty()
        && newTrackKey == m_frame.value(QStringLiteral("trackKey")).toString();

    // S03 当前未提供前一句字段，仅在同一曲目自然向前推进时保留上一当前句。
    // S03 does not expose a previous-line field, so retain it only for natural forward progress.
    const int oldIndex = m_frame.value(QStringLiteral("lineIndex"), -1).toInt();
    const int newIndex = frame.value(QStringLiteral("lineIndex"), -1).toInt();
    if (sameTrack && newIndex == oldIndex + 1)
        m_previousText = oldCurrent;
    else if (!sameTrack || newIndex != oldIndex)
        m_previousText.clear();
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
    if (!m_serviceOwned || m_stateRequestPending)
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
    const bool hadFrame = !m_frame.isEmpty() || !m_previousText.isEmpty();
    m_state.clear();
    m_frame.clear();
    m_previousText.clear();
    if (hadState)
        emit stateChanged();
    if (hadFrame)
        emit frameChanged();
}
