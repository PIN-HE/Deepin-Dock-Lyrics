#pragma once

#include "application/lyricsservicecontroller.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QTimer>

namespace deepin::lyrics {

using DbusVariantMapList = QList<QVariantMap>;

class LyricsDbusAdapter final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.LyricsDock1")

public:
    LyricsDbusAdapter(LyricsServiceController &controller,
                      const QDBusConnection &connection,
                      QObject *parent = nullptr);
    ~LyricsDbusAdapter() override;

    bool registerService(QString *errorCode = nullptr);

public slots:
    QVariantMap GetState() const;
    void SetEnabled(bool enabled);
    void SetPlayer(const QString &busName);
    void SetOffsetMs(int offsetMs);
    void SetAudioVisualizerEnabled(bool enabled);
    void SetLyricLayout(const QString &layout);
    void SearchCandidates();
    void SelectCandidate(const QString &providerId, const QString &candidateId);
    void SetSessionHidden(bool hidden);
    void ClearCache();

signals:
    void StateChanged(const QVariantMap &state);
    void FrameChanged(const QVariantMap &frame);
    void CandidatesChanged(const deepin::lyrics::DbusVariantMapList &candidates);
    void serviceOwnershipLost();

private:
    void replyWithError(const QString &errorCode);
    void queueFrame(const QVariantMap &frame);

    LyricsServiceController &m_controller;
    QDBusConnection m_connection;
    QDBusServiceWatcher m_serviceWatcher;
    QTimer m_frameTimer;
    QVariantMap m_pendingFrame;
    QVariantMap m_lastFrame;
    bool m_registered = false;
};

} // namespace deepin::lyrics
