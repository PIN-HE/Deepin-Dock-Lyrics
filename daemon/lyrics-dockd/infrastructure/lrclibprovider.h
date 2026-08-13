#pragma once

#include "ports/httptransport.h"

#include <deepinlyrics/version.h>
#include <lyricscore/lyricprovider.h>

#include <QElapsedTimer>
#include <QQueue>
#include <QTimer>

namespace deepin::lyrics {

class LRCLIBProvider final : public QObject, public LyricProvider
{
    Q_OBJECT

public:
    explicit LRCLIBProvider(HttpTransport &transport, QObject *parent = nullptr);

    QString id() const override;
    void getExact(const TrackIdentity &track, ResultCallback callback) override;
    void search(const TrackIdentity &track, ResultCallback callback) override;
    void getById(const QString &recordId, ResultCallback callback) override;

    void setBaseUrl(const QUrl &baseUrl);
    void setUserAgent(const QByteArray &userAgent);

private:
    struct PendingRequest {
        QUrl url;
        bool listResponse = false;
        bool allowQueryFallback = false;
        int attempt = 0;
        ResultCallback callback;
    };

    void enqueue(PendingRequest request);
    void processQueue();
    void dispatch(PendingRequest request);
    void handleResponse(PendingRequest request, HttpResponse response);
    void complete(PendingRequest request, ProviderResult result);
    static ProviderResult parseRecord(const QByteArray &body);
    static ProviderResult parseRecords(const QByteArray &body);

    HttpTransport &m_transport;
    QUrl m_baseUrl = QUrl(QStringLiteral("https://lrclib.net"));
    QByteArray m_userAgent = QByteArrayLiteral(
        "DeepinDockLyrics/" DEEPIN_DOCK_LYRICS_VERSION
        " (https://github.com/PIN-HE/Deepin-Dock-Lyrics)");
    QQueue<PendingRequest> m_queue;
    QElapsedTimer m_lastDispatch;
    QTimer m_queueTimer;
    QDateTime m_cooldownUntil;
    bool m_inFlight = false;
};

} // namespace deepin::lyrics
