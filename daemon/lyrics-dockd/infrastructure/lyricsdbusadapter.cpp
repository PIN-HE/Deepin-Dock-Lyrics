#include "infrastructure/lyricsdbusadapter.h"

#include <QDBusConnectionInterface>
#include <QDBusMetaType>

namespace deepin::lyrics {

namespace {

constexpr auto serviceName = "org.deepin.LyricsDock1";
constexpr auto objectPath = "/org/deepin/LyricsDock1";
constexpr auto errorPrefix = "org.deepin.LyricsDock1.Error.";

QString dbusErrorName(const QString &errorCode)
{
    QString suffix;
    bool upperNext = true;
    for (const QChar character : errorCode) {
        if (character == QLatin1Char('-')) {
            upperNext = true;
            continue;
        }
        suffix.append(upperNext ? character.toUpper() : character);
        upperNext = false;
    }
    return QString::fromLatin1(errorPrefix) + suffix;
}

} // namespace

LyricsDbusAdapter::LyricsDbusAdapter(LyricsServiceController &controller,
                                     const QDBusConnection &connection,
                                     QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_connection(connection)
    , m_serviceWatcher(QString::fromLatin1(serviceName), connection,
                       QDBusServiceWatcher::WatchForUnregistration, this)
{
    qDBusRegisterMetaType<DbusVariantMapList>();
    m_frameTimer.setInterval(200);
    m_frameTimer.setSingleShot(true);

    connect(&m_controller, &LyricsServiceController::stateChanged,
            this, &LyricsDbusAdapter::StateChanged);
    connect(&m_controller, &LyricsServiceController::frameChanged,
            this, &LyricsDbusAdapter::queueFrame);
    connect(&m_controller, &LyricsServiceController::candidatesChanged,
            this, &LyricsDbusAdapter::CandidatesChanged);
    connect(&m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this,
            [this](const QString &) {
                if (m_registered)
                    emit serviceOwnershipLost();
            });
    connect(&m_frameTimer, &QTimer::timeout, this, [this] {
        if (!m_pendingFrame.isEmpty() && m_pendingFrame != m_lastFrame) {
            m_lastFrame = m_pendingFrame;
            emit FrameChanged(m_pendingFrame);
        }
        m_pendingFrame.clear();
    });
}

LyricsDbusAdapter::~LyricsDbusAdapter()
{
    if (!m_registered)
        return;
    m_registered = false;
    m_connection.unregisterService(QString::fromLatin1(serviceName));
    m_connection.unregisterObject(QString::fromLatin1(objectPath));
}

bool LyricsDbusAdapter::registerService(QString *errorCode)
{
    if (m_registered)
        return true;
    if (!m_connection.isConnected()) {
        if (errorCode)
            *errorCode = QStringLiteral("session-bus-unavailable");
        return false;
    }

    if (!m_connection.registerObject(QString::fromLatin1(objectPath), this,
                                     QDBusConnection::ExportAllSlots
                                         | QDBusConnection::ExportAllSignals)) {
        if (errorCode)
            *errorCode = QStringLiteral("object-registration-failed");
        return false;
    }

    const auto reply = m_connection.interface()->registerService(
        QString::fromLatin1(serviceName), QDBusConnectionInterface::DontQueueService,
        QDBusConnectionInterface::DontAllowReplacement);
    if (!reply.isValid() || reply.value() != QDBusConnectionInterface::ServiceRegistered) {
        m_connection.unregisterObject(QString::fromLatin1(objectPath));
        if (errorCode)
            *errorCode = QStringLiteral("service-name-unavailable");
        return false;
    }

    m_registered = true;
    return true;
}

QVariantMap LyricsDbusAdapter::GetState() const
{
    return m_controller.state();
}

void LyricsDbusAdapter::SetEnabled(bool enabled)
{
    QString errorCode;
    if (!m_controller.setEnabled(enabled, &errorCode))
        replyWithError(errorCode);
}

void LyricsDbusAdapter::SetPlayer(const QString &busName)
{
    QString errorCode;
    if (!m_controller.setPlayer(busName, &errorCode))
        replyWithError(errorCode);
}

void LyricsDbusAdapter::SetOffsetMs(int offsetMs)
{
    QString errorCode;
    if (!m_controller.setOffsetMs(offsetMs, &errorCode))
        replyWithError(errorCode);
}

void LyricsDbusAdapter::SetAudioVisualizerEnabled(bool enabled)
{
    QString errorCode;
    if (!m_controller.setAudioVisualizerEnabled(enabled, &errorCode))
        replyWithError(errorCode);
}

void LyricsDbusAdapter::SetLyricLayout(const QString &layout)
{
    QString errorCode;
    if (!m_controller.setLyricLayout(layout, &errorCode))
        replyWithError(errorCode);
}

void LyricsDbusAdapter::SearchCandidates()
{
    QString errorCode;
    if (!m_controller.searchCandidates(&errorCode))
        replyWithError(errorCode);
}

void LyricsDbusAdapter::SelectCandidate(const QString &providerId, const QString &candidateId)
{
    QString errorCode;
    if (!m_controller.selectCandidate(providerId, candidateId, &errorCode))
        replyWithError(errorCode);
}

void LyricsDbusAdapter::SetSessionHidden(bool hidden)
{
    m_controller.setSessionHidden(hidden);
}

void LyricsDbusAdapter::ClearCache()
{
    m_controller.clearCache();
}

void LyricsDbusAdapter::replyWithError(const QString &errorCode)
{
    if (calledFromDBus())
        sendErrorReply(dbusErrorName(errorCode), errorCode);
}

void LyricsDbusAdapter::queueFrame(const QVariantMap &frame)
{
    m_pendingFrame = frame;
    if (!m_frameTimer.isActive())
        m_frameTimer.start();
}

} // namespace deepin::lyrics
