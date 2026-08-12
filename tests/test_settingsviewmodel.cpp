#include "settingsviewmodel.h"

#include <QDBusConnection>
#include <QSignalSpy>
#include <QThread>
#include <QtTest>

namespace {

constexpr auto serviceName = "org.deepin.LyricsDock1";
constexpr auto objectPath = "/org/deepin/LyricsDock1";

class FakeSettingsService final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.LyricsDock1")

public:
    QVariantMap state{
        {QStringLiteral("status"), QStringLiteral("Disabled")},
        {QStringLiteral("enabled"), false},
        {QStringLiteral("sessionHidden"), true},
        {QStringLiteral("playerBusName"), QString()},
        {QStringLiteral("availablePlayers"), QVariantList{
            QVariantMap{{QStringLiteral("busName"), QStringLiteral("org.mpris.MediaPlayer2.demo")},
                        {QStringLiteral("identity"), QStringLiteral("Demo Player")},
                        {QStringLiteral("available"), true}}}},
        {QStringLiteral("offsetMs"), 0},
        {QStringLiteral("canSearchCandidates"), true},
        {QStringLiteral("lyricsSource"), QStringLiteral("lrclib")},
        {QStringLiteral("timingCapability"), QStringLiteral("line")},
        {QStringLiteral("candidates"), QVariantList{
            QVariantMap{{QStringLiteral("providerId"), QStringLiteral("lrclib")},
                        {QStringLiteral("candidateId"), QStringLiteral("42")},
                        {QStringLiteral("title"), QStringLiteral("Candidate")}}}},
    };

public slots:
    QVariantMap GetState() const { return state; }

    void SetEnabled(bool enabled)
    {
        state.insert(QStringLiteral("enabled"), enabled);
        state.insert(QStringLiteral("status"), enabled
            ? QStringLiteral("WaitingForPlayer") : QStringLiteral("Disabled"));
        emit enabledCalled(enabled);
        emit StateChanged(state);
    }

    void SetSessionHidden(bool hidden)
    {
        state.insert(QStringLiteral("sessionHidden"), hidden);
        emit sessionHiddenCalled(hidden);
        emit StateChanged(state);
    }

    void SetPlayer(const QString &busName) { emit playerCalled(busName); }
    void SetOffsetMs(int offsetMs) { emit offsetCalled(offsetMs); }
    void SetAudioVisualizerEnabled(bool enabled) { emit audioVisualizerCalled(enabled); }
    void SearchCandidates() { emit searchCalled(); }
    void SelectCandidate(const QString &providerId, const QString &candidateId)
    {
        emit candidateCalled(providerId, candidateId);
    }
    void ClearCache() { emit clearCacheCalled(); }

signals:
    void enabledCalled(bool enabled);
    void sessionHiddenCalled(bool hidden);
    void playerCalled(const QString &busName);
    void offsetCalled(int offsetMs);
    void audioVisualizerCalled(bool enabled);
    void searchCalled();
    void candidateCalled(const QString &providerId, const QString &candidateId);
    void clearCacheCalled();
    void StateChanged(const QVariantMap &state);
    void FrameChanged(const QVariantMap &frame);
    void CandidatesChanged(const QList<QVariantMap> &candidates);
};

class FakeServiceWorker final : public QObject
{
    Q_OBJECT

public:
    explicit FakeServiceWorker(QString connectionName)
        : m_connectionName(std::move(connectionName))
    {
    }

public slots:
    void initialize()
    {
        m_connection = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, m_connectionName);
        m_service = new FakeSettingsService;
        connect(m_service, &FakeSettingsService::enabledCalled,
                this, &FakeServiceWorker::enabledCalled);
        connect(m_service, &FakeSettingsService::sessionHiddenCalled,
                this, &FakeServiceWorker::sessionHiddenCalled);
        connect(m_service, &FakeSettingsService::playerCalled,
                this, &FakeServiceWorker::playerCalled);
        connect(m_service, &FakeSettingsService::offsetCalled,
                this, &FakeServiceWorker::offsetCalled);
        connect(m_service, &FakeSettingsService::audioVisualizerCalled,
                this, &FakeServiceWorker::audioVisualizerCalled);
        connect(m_service, &FakeSettingsService::searchCalled,
                this, &FakeServiceWorker::searchCalled);
        connect(m_service, &FakeSettingsService::candidateCalled,
                this, &FakeServiceWorker::candidateCalled);
        connect(m_service, &FakeSettingsService::clearCacheCalled,
                this, &FakeServiceWorker::clearCacheCalled);
        const bool registered = m_connection.registerObject(
            QString::fromLatin1(objectPath), m_service,
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)
            && m_connection.registerService(QString::fromLatin1(serviceName));
        emit ready(registered);
    }

    void publishCandidates(const QList<QVariantMap> &candidates)
    {
        emit m_service->CandidatesChanged(candidates);
    }

    void shutdown()
    {
        m_connection.unregisterService(QString::fromLatin1(serviceName));
        m_connection.unregisterObject(QString::fromLatin1(objectPath));
        delete m_service;
        m_service = nullptr;
        QDBusConnection::disconnectFromBus(m_connectionName);
    }

signals:
    void ready(bool registered);
    void enabledCalled(bool enabled);
    void sessionHiddenCalled(bool hidden);
    void playerCalled(const QString &busName);
    void offsetCalled(int offsetMs);
    void audioVisualizerCalled(bool enabled);
    void searchCalled();
    void candidateCalled(const QString &providerId, const QString &candidateId);
    void clearCacheCalled();

private:
    QString m_connectionName;
    QDBusConnection m_connection = QDBusConnection::sessionBus();
    FakeSettingsService *m_service = nullptr;
};

bool startWorker(FakeServiceWorker &worker, QThread &thread)
{
    QSignalSpy readySpy(&worker, &FakeServiceWorker::ready);
    QObject::connect(&thread, &QThread::started, &worker, &FakeServiceWorker::initialize);
    worker.moveToThread(&thread);
    thread.start();
    return readySpy.wait(2000) && readySpy.constFirst().constFirst().toBool();
}

void stopWorker(FakeServiceWorker &worker, QThread &thread)
{
    QMetaObject::invokeMethod(&worker, &FakeServiceWorker::shutdown,
                              Qt::BlockingQueuedConnection);
    thread.quit();
    QVERIFY(thread.wait(2000));
}

} // namespace

class SettingsViewModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void followsStateAndInvokesSettingsOperations();
    void survivesMissingService();
};

void SettingsViewModelTest::followsStateAndInvokesSettingsOperations()
{
    const QString serviceConnectionName = QStringLiteral("settings-service");
    const QString clientConnectionName = QStringLiteral("settings-client");
    QThread serviceThread;
    FakeServiceWorker service(serviceConnectionName);
    QVERIFY(startWorker(service, serviceThread));
    QDBusConnection client = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);

    SettingsViewModel model(client);
    QTRY_VERIFY(model.serviceAvailable());
    QTRY_COMPARE(model.state().value(QStringLiteral("status")).toString(),
                 QStringLiteral("Disabled"));
    QCOMPARE(model.candidates().size(), 1);
    const QVariantList players = model.state().value(QStringLiteral("availablePlayers")).toList();
    QCOMPARE(players.size(), 1);
    QCOMPARE(players.constFirst().toMap().value(QStringLiteral("identity")).toString(),
             QStringLiteral("Demo Player"));

    QSignalSpy enabledSpy(&service, &FakeServiceWorker::enabledCalled);
    QSignalSpy hiddenSpy(&service, &FakeServiceWorker::sessionHiddenCalled);
    QSignalSpy playerSpy(&service, &FakeServiceWorker::playerCalled);
    QSignalSpy offsetSpy(&service, &FakeServiceWorker::offsetCalled);
    QSignalSpy visualizerSpy(&service, &FakeServiceWorker::audioVisualizerCalled);
    QSignalSpy searchSpy(&service, &FakeServiceWorker::searchCalled);
    QSignalSpy candidateSpy(&service, &FakeServiceWorker::candidateCalled);
    QSignalSpy cacheSpy(&service, &FakeServiceWorker::clearCacheCalled);

    model.startLyrics();
    QTRY_COMPARE(enabledSpy.count(), 1);
    QTRY_COMPARE(hiddenSpy.count(), 1);
    QCOMPARE(hiddenSpy.constFirst().constFirst().toBool(), false);
    model.setPlayer(QStringLiteral("org.mpris.MediaPlayer2.demo"));
    model.setOffsetMs(700);
    model.setAudioVisualizerEnabled(true);
    model.searchCandidates();
    model.selectCandidate(QStringLiteral("lrclib"), QStringLiteral("42"));
    model.clearCache();
    QTRY_COMPARE(playerSpy.count(), 1);
    QTRY_COMPARE(offsetSpy.count(), 1);
    QTRY_COMPARE(visualizerSpy.count(), 1);
    QTRY_COMPARE(searchSpy.count(), 1);
    QTRY_COMPARE(candidateSpy.count(), 1);
    QTRY_COMPARE(cacheSpy.count(), 1);
    QCOMPARE(offsetSpy.constFirst().constFirst().toInt(), 700);
    QCOMPARE(candidateSpy.constFirst().at(1).toString(), QStringLiteral("42"));

    const QList<QVariantMap> replacement{
        {{QStringLiteral("providerId"), QStringLiteral("lrclib")},
         {QStringLiteral("candidateId"), QStringLiteral("84")}}};
    QMetaObject::invokeMethod(&service, &FakeServiceWorker::publishCandidates,
                              Qt::QueuedConnection, replacement);
    QTRY_COMPARE(model.candidates().constFirst().toMap()
                     .value(QStringLiteral("candidateId")).toString(),
                 QStringLiteral("84"));

    stopWorker(service, serviceThread);
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

void SettingsViewModelTest::survivesMissingService()
{
    const QString connectionName = QStringLiteral("settings-missing-client");
    QDBusConnection client = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, connectionName);
    SettingsViewModel model(client);
    QVERIFY(!model.serviceAvailable());
    QSignalSpy failureSpy(&model, &SettingsViewModel::operationFailed);
    model.startLyrics();
    QCOMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.constFirst().constFirst().toString(),
             QStringLiteral("service-unavailable"));
    QDBusConnection::disconnectFromBus(connectionName);
}

QTEST_GUILESS_MAIN(SettingsViewModelTest)
#include "test_settingsviewmodel.moc"
