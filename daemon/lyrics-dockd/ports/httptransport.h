#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QUrl>

#include <functional>

namespace deepin::lyrics {

struct HttpRequest {
    QUrl url;
    QHash<QByteArray, QByteArray> headers;
    int timeoutMs = 10000;
};

struct HttpResponse {
    int statusCode = 0;
    QByteArray body;
    QHash<QByteArray, QByteArray> headers;
    QString networkError;
};

class HttpTransport : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(HttpResponse)>;

    explicit HttpTransport(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    virtual void get(const HttpRequest &request, Callback callback) = 0;
};

} // namespace deepin::lyrics
