#pragma once

#include "ports/httptransport.h"

#include <QNetworkAccessManager>

namespace deepin::lyrics {

class QtNetworkTransport final : public HttpTransport
{
    Q_OBJECT

public:
    explicit QtNetworkTransport(QObject *parent = nullptr);
    void get(const HttpRequest &request, Callback callback) override;

private:
    QNetworkAccessManager m_manager;
};

} // namespace deepin::lyrics
