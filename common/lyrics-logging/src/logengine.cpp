#include <lyricslogging/logengine.h>

#include <QLoggingCategory>
#include <QRegularExpression>

#include <algorithm>

namespace deepin::lyrics {

namespace {

Q_LOGGING_CATEGORY(privacyLog, "org.deepin.lyricsdock.service")

QString serializeFields(const QVariantMap &fields)
{
    QStringList parts;
    for (auto it = fields.cbegin(); it != fields.cend(); ++it)
        parts.append(QStringLiteral("%1=%2").arg(it.key(), it.value().toString()));
    return parts.join(QLatin1Char(' '));
}

QString safeCode(const QString &value)
{
    static const QRegularExpression valid(QStringLiteral("^[A-Za-z][A-Za-z0-9_-]{0,63}$"));
    return valid.match(value).hasMatch() ? value : QStringLiteral("redacted");
}

QVariant safeFieldValue(const QString &name, const QVariant &value)
{
    if (name == QStringLiteral("state_from") || name == QStringLiteral("state_to")
        || name == QStringLiteral("error_code") || name == QStringLiteral("result_code")) {
        if (value.toString().isEmpty())
            return {};
        return safeCode(value.toString());
    }
    if (name == QStringLiteral("enabled") || name == QStringLiteral("session_hidden"))
        return value.toBool();
    if (name == QStringLiteral("player_available_count"))
        return std::clamp(value.toInt(), 0, 1000);
    if (name == QStringLiteral("offset_ms"))
        return std::clamp(value.toInt(), -10000, 10000);
    return {};
}

} // namespace

void QtLogSink::write(LogLevel level, const LogEvent &event)
{
    const QString message = QStringLiteral("component=%1 event_code=%2 %3")
                                .arg(event.component, event.eventCode,
                                     serializeFields(event.safeFields));
    switch (level) {
    case LogLevel::Debug:
        qCDebug(privacyLog).noquote() << message;
        break;
    case LogLevel::Info:
        qCInfo(privacyLog).noquote() << message;
        break;
    case LogLevel::Warning:
        qCWarning(privacyLog).noquote() << message;
        break;
    case LogLevel::Critical:
        qCCritical(privacyLog).noquote() << message;
        break;
    }
}

LogEngine::LogEngine(LogSink &sink)
    : m_sink(sink)
{
}

void LogEngine::write(LogLevel level,
                      const QString &component,
                      const QString &eventCode,
                      const QVariantMap &fields) const
{
    QVariantMap filtered;
    const QSet<QString> allowed = allowedFieldNames();
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        const QVariant safeValue = safeFieldValue(it.key(), it.value());
        if (allowed.contains(it.key()) && safeValue.isValid())
            filtered.insert(it.key(), safeValue);
    }

    // 业务层只能提交白名单字段，避免曲目、歌词和用户路径意外写入本地日志。
    // The application layer may emit only allowlisted fields so tracks, lyrics, and user paths never reach local logs.
    m_sink.write(level, {safeCode(component), safeCode(eventCode), filtered});
}

QSet<QString> LogEngine::allowedFieldNames()
{
    return {
        QStringLiteral("state_from"),
        QStringLiteral("state_to"),
        QStringLiteral("error_code"),
        QStringLiteral("player_available_count"),
        QStringLiteral("enabled"),
        QStringLiteral("session_hidden"),
        QStringLiteral("offset_ms"),
        QStringLiteral("result_code"),
    };
}

} // namespace deepin::lyrics
