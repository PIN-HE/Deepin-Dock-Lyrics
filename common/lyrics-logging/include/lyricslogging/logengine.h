#pragma once

#include <QSet>
#include <QString>
#include <QVariantMap>

namespace deepin::lyrics {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Critical,
};

struct LogEvent {
    QString component;
    QString eventCode;
    QVariantMap safeFields;
};

class LogSink
{
public:
    virtual ~LogSink() = default;
    virtual void write(LogLevel level, const LogEvent &event) = 0;
};

class QtLogSink final : public LogSink
{
public:
    void write(LogLevel level, const LogEvent &event) override;
};

class LogEngine final
{
public:
    explicit LogEngine(LogSink &sink);

    void write(LogLevel level,
               const QString &component,
               const QString &eventCode,
               const QVariantMap &fields = {}) const;
    static QSet<QString> allowedFieldNames();

private:
    LogSink &m_sink;
};

} // namespace deepin::lyrics
