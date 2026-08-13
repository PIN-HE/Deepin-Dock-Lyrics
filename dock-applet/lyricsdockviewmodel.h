#pragma once

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

class LyricsDockViewModel final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged FINAL)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool sessionHidden READ sessionHidden NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString previousText READ previousText NOTIFY frameChanged FINAL)
    Q_PROPERTY(QString currentText READ currentText NOTIFY frameChanged FINAL)
    Q_PROPERTY(QString secondaryText READ secondaryText NOTIFY frameChanged FINAL)
    Q_PROPERTY(QString translationText READ translationText NOTIFY frameChanged FINAL)
    Q_PROPERTY(QString timingCapability READ timingCapability NOTIFY frameChanged FINAL)
    Q_PROPERTY(QString source READ source NOTIFY frameChanged FINAL)
    Q_PROPERTY(double lineProgress READ lineProgress NOTIFY frameChanged FINAL)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY stateChanged FINAL)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool visualizerAvailable READ visualizerAvailable NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool audioVisualizerEnabled READ audioVisualizerEnabled NOTIFY stateChanged FINAL)
    Q_PROPERTY(QVariantList visualizerLevels READ visualizerLevels NOTIFY stateChanged FINAL)

public:
    explicit LyricsDockViewModel(
        const QDBusConnection &connection = QDBusConnection::sessionBus(),
        QObject *parent = nullptr);

    bool serviceAvailable() const;
    QString status() const;
    bool enabled() const;
    bool sessionHidden() const;
    QString previousText() const;
    QString currentText() const;
    QString secondaryText() const;
    QString translationText() const;
    QString timingCapability() const;
    QString source() const;
    double lineProgress() const;
    qint64 positionMs() const;
    qint64 durationMs() const;
    QString artUrl() const;
    bool visualizerAvailable() const;
    bool audioVisualizerEnabled() const;
    QVariantList visualizerLevels() const;

    Q_INVOKABLE void setSessionHidden(bool hidden);
    Q_INVOKABLE bool openSettings();
    Q_INVOKABLE void refresh();

signals:
    void serviceAvailableChanged();
    void stateChanged();
    void frameChanged();

private slots:
    void onServiceOwnerChanged(const QString &service,
                               const QString &oldOwner,
                               const QString &newOwner);
    void onStateChanged(const QVariantMap &state);
    void onFrameChanged(const QVariantMap &frame);

private:
    void connectServiceSignals();
    void disconnectServiceSignals();
    void requestState();
    void setServiceAvailable(bool available);
    void clearRemoteData();

    QDBusConnection m_connection;
    QDBusServiceWatcher m_serviceWatcher;
    QVariantMap m_state;
    QVariantMap m_frame;
    QTimer m_stateRetryTimer;
    bool m_serviceAvailable = false;
    bool m_serviceOwned = false;
    bool m_signalsConnected = false;
    bool m_stateRequestPending = false;
    int m_stateRetryCount = 0;
    quint64 m_serviceGeneration = 0;
};
