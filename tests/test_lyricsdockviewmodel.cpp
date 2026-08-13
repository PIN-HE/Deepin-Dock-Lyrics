#include "lyricsdockviewmodel.h"

#include <QDBusConnection>
#include <QSignalSpy>
#include <QThread>
#include <QtTest>

namespace {

constexpr auto serviceName = "org.deepin.LyricsDock1";
constexpr auto objectPath = "/org/deepin/LyricsDock1";

class FakeLyricsService final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.LyricsDock1")

public:
    QVariantMap state{
        {QStringLiteral("status"), QStringLiteral("LyricsReady")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("sessionHidden"), false},
    };

public slots:
    QVariantMap GetState() const { return state; }

    void SetSessionHidden(bool hidden)
    {
        state.insert(QStringLiteral("sessionHidden"), hidden);
        emit SessionHiddenCalled(hidden);
        emit StateChanged(state);
    }

signals:
    void SessionHiddenCalled(bool hidden);
    void StateChanged(const QVariantMap &state);
    void FrameChanged(const QVariantMap &frame);
};

class FakeServiceWorker final : public QObject
{
    Q_OBJECT

public:
    explicit FakeServiceWorker(QString connectionName,
                               QString initialStatus = QStringLiteral("LyricsReady"),
                               bool deferObjectRegistration = false)
        : m_connectionName(std::move(connectionName))
        , m_initialStatus(std::move(initialStatus))
        , m_deferObjectRegistration(deferObjectRegistration)
    {
    }

public slots:
    void initialize()
    {
        m_connection = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, m_connectionName);
        m_service = new FakeLyricsService;
        m_service->state.insert(QStringLiteral("status"), m_initialStatus);
        connect(m_service, &FakeLyricsService::SessionHiddenCalled,
                this, &FakeServiceWorker::sessionHiddenCalled);
        if (m_deferObjectRegistration) {
            emit serviceClaimed(m_connection.registerService(QString::fromLatin1(serviceName)));
            return;
        }

        const bool registered = registerObject()
            && m_connection.registerService(QString::fromLatin1(serviceName));
        emit ready(registered);
    }

    void publishFrame(const QVariantMap &value) { emit m_service->FrameChanged(value); }

    void registerDeferredObject() { emit ready(registerObject()); }

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
    void serviceClaimed(bool registered);
    void sessionHiddenCalled(bool hidden);

private:
    bool registerObject()
    {
        return m_connection.registerObject(
            QString::fromLatin1(objectPath), m_service,
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    }

    QString m_connectionName;
    QString m_initialStatus;
    bool m_deferObjectRegistration = false;
    QDBusConnection m_connection = QDBusConnection::sessionBus();
    FakeLyricsService *m_service = nullptr;
};

bool startWorker(FakeServiceWorker &worker, QThread &thread)
{
    QSignalSpy readySpy(&worker, &FakeServiceWorker::ready);
    QObject::connect(&thread, &QThread::started, &worker, &FakeServiceWorker::initialize);
    worker.moveToThread(&thread);
    thread.start();
    return readySpy.wait(2000) && readySpy.constFirst().constFirst().toBool();
}

bool startDeferredWorker(FakeServiceWorker &worker, QThread &thread)
{
    QSignalSpy serviceSpy(&worker, &FakeServiceWorker::serviceClaimed);
    QObject::connect(&thread, &QThread::started, &worker, &FakeServiceWorker::initialize);
    worker.moveToThread(&thread);
    thread.start();
    return serviceSpy.wait(2000) && serviceSpy.constFirst().constFirst().toBool();
}

void stopWorker(FakeServiceWorker &worker, QThread &thread)
{
    QMetaObject::invokeMethod(&worker, &FakeServiceWorker::shutdown,
                              Qt::BlockingQueuedConnection);
    thread.quit();
    QVERIFY(thread.wait(2000));
}

QVariantMap frame(int lineIndex, const QString &current, const QString &trackKey)
{
    return {
        {QStringLiteral("previousText"), lineIndex > 0 ? QStringLiteral("First") : QString()},
        {QStringLiteral("currentText"), current},
        {QStringLiteral("secondaryText"), QStringLiteral("Next")},
        {QStringLiteral("translationText"), QString()},
        {QStringLiteral("lineIndex"), lineIndex},
        {QStringLiteral("lineProgress"), 0.25},
        {QStringLiteral("timingCapability"), QStringLiteral("line")},
        {QStringLiteral("source"), QStringLiteral("lrclib")},
        {QStringLiteral("trackKey"), trackKey},
    };
}

} // namespace

class LyricsDockViewModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void followsStateFramesAndSessionHide();
    void reconnectsAfterServiceRestart();
    void retriesUntilServiceObjectIsRegistered();
};

void LyricsDockViewModelTest::cleanup()
{
    for (const QString &name : {
             QStringLiteral("lyrics-viewmodel-service-1"),
             QStringLiteral("lyrics-viewmodel-client-1"),
             QStringLiteral("lyrics-viewmodel-service-2"),
             QStringLiteral("lyrics-viewmodel-client-2"),
             QStringLiteral("lyrics-viewmodel-service-3"),
             QStringLiteral("lyrics-viewmodel-client-3")}) {
        QDBusConnection::disconnectFromBus(name);
    }
}

void LyricsDockViewModelTest::followsStateFramesAndSessionHide()
{
    const QString serviceConnectionName = QStringLiteral("lyrics-viewmodel-service-1");
    const QString clientConnectionName = QStringLiteral("lyrics-viewmodel-client-1");
    QThread serviceThread;
    FakeServiceWorker service(serviceConnectionName);
    QVERIFY(startWorker(service, serviceThread));
    QDBusConnection clientConnection = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);

    LyricsDockViewModel viewModel(clientConnection);
    QTRY_VERIFY(viewModel.serviceAvailable());
    QTRY_COMPARE(viewModel.status(), QStringLiteral("LyricsReady"));
    QVERIFY(viewModel.enabled());
    QSignalSpy sessionHiddenSpy(&service, &FakeServiceWorker::sessionHiddenCalled);

    QMetaObject::invokeMethod(&service, &FakeServiceWorker::publishFrame,
                              Qt::QueuedConnection,
                              frame(0, QStringLiteral("First"), QStringLiteral("track-a")));
    QTRY_COMPARE(viewModel.currentText(), QStringLiteral("First"));
    QCOMPARE(viewModel.lineProgress(), 0.25);
    QCOMPARE(viewModel.previousText(), QString());

    QMetaObject::invokeMethod(&service, &FakeServiceWorker::publishFrame,
                              Qt::QueuedConnection,
                              frame(1, QStringLiteral("Second"), QStringLiteral("track-a")));
    QTRY_COMPARE(viewModel.currentText(), QStringLiteral("Second"));
    QCOMPARE(viewModel.previousText(), QStringLiteral("First"));

    QVariantMap sameLine = frame(1, QStringLiteral("Second"), QStringLiteral("track-a"));
    sameLine.insert(QStringLiteral("lineProgress"), 0.75);
    QMetaObject::invokeMethod(&service, &FakeServiceWorker::publishFrame,
                              Qt::QueuedConnection, sameLine);
    QTRY_COMPARE(viewModel.lineProgress(), 0.75);
    QCOMPARE(viewModel.previousText(), QStringLiteral("First"));

    viewModel.setSessionHidden(true);
    QTRY_VERIFY(viewModel.sessionHidden());
    QTRY_COMPARE(sessionHiddenSpy.count(), 1);
    QCOMPARE(sessionHiddenSpy.constFirst().constFirst().toBool(), true);

    stopWorker(service, serviceThread);
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

void LyricsDockViewModelTest::retriesUntilServiceObjectIsRegistered()
{
    const QString serviceConnectionName = QStringLiteral("lyrics-viewmodel-service-3");
    const QString clientConnectionName = QStringLiteral("lyrics-viewmodel-client-3");
    QThread serviceThread;
    FakeServiceWorker service(serviceConnectionName, QStringLiteral("LyricsReady"), true);
    QVERIFY(startDeferredWorker(service, serviceThread));

    QDBusConnection clientConnection = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    LyricsDockViewModel viewModel(clientConnection);

    // 等待首次调用失败，且不改变服务所有者，然后补注册对象。
    // Wait for the first call to fail, then register the object without changing service ownership.
    QTest::qWait(300);
    QVERIFY(!viewModel.serviceAvailable());
    QSignalSpy readySpy(&service, &FakeServiceWorker::ready);
    QMetaObject::invokeMethod(&service, &FakeServiceWorker::registerDeferredObject,
                              Qt::QueuedConnection);
    QVERIFY(readySpy.wait(2000));
    QVERIFY(readySpy.constFirst().constFirst().toBool());

    QTRY_VERIFY(viewModel.serviceAvailable());
    QTRY_COMPARE(viewModel.status(), QStringLiteral("LyricsReady"));

    stopWorker(service, serviceThread);
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

void LyricsDockViewModelTest::reconnectsAfterServiceRestart()
{
    const QString serviceConnectionName = QStringLiteral("lyrics-viewmodel-service-2");
    const QString clientConnectionName = QStringLiteral("lyrics-viewmodel-client-2");
    QDBusConnection clientConnection = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    LyricsDockViewModel viewModel(clientConnection);
    QVERIFY(!viewModel.serviceAvailable());

    QThread firstThread;
    FakeServiceWorker first(serviceConnectionName);
    QVERIFY(startWorker(first, firstThread));
    QTRY_VERIFY(viewModel.serviceAvailable());
    QTRY_COMPARE(viewModel.status(), QStringLiteral("LyricsReady"));

    stopWorker(first, firstThread);
    QTRY_VERIFY(!viewModel.serviceAvailable());
    QCOMPARE(viewModel.status(), QStringLiteral("WaitingForService"));
    QVERIFY(viewModel.currentText().isEmpty());

    const QString restartedConnectionName = QStringLiteral("lyrics-viewmodel-service-2-restarted");
    QThread restartedThread;
    FakeServiceWorker restarted(restartedConnectionName, QStringLiteral("WaitingForPlayer"));
    QVERIFY(startWorker(restarted, restartedThread));
    QTRY_VERIFY(viewModel.serviceAvailable());
    QTRY_COMPARE(viewModel.status(), QStringLiteral("WaitingForPlayer"));

    stopWorker(restarted, restartedThread);
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

QTEST_GUILESS_MAIN(LyricsDockViewModelTest)
#include "test_lyricsdockviewmodel.moc"
