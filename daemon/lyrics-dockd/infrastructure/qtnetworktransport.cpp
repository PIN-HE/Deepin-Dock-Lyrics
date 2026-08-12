#include "infrastructure/qtnetworktransport.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace deepin::lyrics {

QtNetworkTransport::QtNetworkTransport(QObject *parent)
    : HttpTransport(parent)
{
}

void QtNetworkTransport::get(const HttpRequest &request, Callback callback)
{
    QNetworkRequest networkRequest(request.url);
    networkRequest.setTransferTimeout(request.timeoutMs);
    for (auto it = request.headers.cbegin(); it != request.headers.cend(); ++it)
        networkRequest.setRawHeader(it.key(), it.value());

    QNetworkReply *reply = m_manager.get(networkRequest);
    connect(reply, &QNetworkReply::finished, this, [reply, callback = std::move(callback)]() mutable {
        HttpResponse response;
        response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        response.body = reply->readAll();
        for (const auto &header : reply->rawHeaderPairs())
            response.headers.insert(header.first.toLower(), header.second);
        // Qt 会为 4xx/5xx 同时设置网络错误，HTTP 状态必须由协议层单独处理。
        // Qt reports 4xx/5xx as reply errors too; the protocol layer must handle HTTP status separately.
        if (response.statusCode == 0 && reply->error() != QNetworkReply::NoError)
            response.networkError = QString::number(static_cast<int>(reply->error()));
        reply->deleteLater();
        callback(std::move(response));
    });
}

} // namespace deepin::lyrics
