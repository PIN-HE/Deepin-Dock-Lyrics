#pragma once

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class SettingsViewModel final : public QObject
{
    Q_OBJECT

public:
    explicit SettingsViewModel(
        const QDBusConnection &connection = QDBusConnection::sessionBus(),
        QObject *parent = nullptr);

    bool serviceAvailable() const;
    bool busy() const;
    QVariantMap state() const;
    QVariantMap frame() const;
    QVariantList candidates() const;

    void startLyrics();
    void setEnabled(bool enabled);
    void setPlayer(const QString &busName);
    void setOffsetMs(int offsetMs);
    void setAudioVisualizerEnabled(bool enabled);
    void setLyricLayout(const QString &layout);
    void searchCandidates();
    void selectCandidate(const QString &providerId, const QString &candidateId);
    void showInDock();
    void clearCache();
    void refresh();

signals:
    void serviceAvailableChanged();
    void busyChanged();
    void stateChanged();
    void frameChanged();
    void candidatesChanged();
    void operationFailed(const QString &errorCode);
    void operationSucceeded(const QString &operation);

private slots:
    void onServiceOwnerChanged(const QString &service,
                               const QString &oldOwner,
                               const QString &newOwner);
    void onStateChanged(const QVariantMap &state);
    void onFrameChanged(const QVariantMap &frame);
    void onCandidatesChanged(const QList<QVariantMap> &candidates);

private:
    using Completion = std::function<void(bool)>;

    void connectServiceSignals();
    void disconnectServiceSignals();
    void requestState();
    void callVoid(const QString &method,
                  const QVariantList &arguments,
                  const QString &operation,
                  Completion completion = {});
    void setServiceAvailable(bool available);
    void beginOperation();
    void endOperation();
    void clearRemoteData();
    QString stableErrorCode(const QString &dbusErrorName) const;

    QDBusConnection m_connection;
    QDBusServiceWatcher m_serviceWatcher;
    QTimer m_stateRetryTimer;
    QVariantMap m_state;
    QVariantMap m_frame;
    QVariantList m_candidates;
    bool m_serviceAvailable = false;
    bool m_serviceOwned = false;
    bool m_signalsConnected = false;
    bool m_stateRequestPending = false;
    int m_pendingOperations = 0;
    int m_stateRetryCount = 0;
    quint64 m_serviceGeneration = 0;
};
