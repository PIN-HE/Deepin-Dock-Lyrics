#include "lyricscore/lrcparser.h"

#include <QMap>
#include <QRegularExpression>

namespace deepin::lyrics {

namespace {

QString cleanDisplayText(const QString &text)
{
    QString cleaned;
    cleaned.reserve(text.size());
    for (const QChar character : text) {
        if (character.category() != QChar::Other_Control)
            cleaned.append(character);
    }
    return cleaned.trimmed();
}

qint64 fractionToMilliseconds(const QString &fraction)
{
    if (fraction.size() == 1)
        return fraction.toLongLong() * 100;
    if (fraction.size() == 2)
        return fraction.toLongLong() * 10;
    return fraction.toLongLong();
}

QString firstPlainLine(const QString &plainLyrics)
{
    const QStringList lines = plainLyrics.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    for (const QString &line : lines) {
        const QString cleaned = cleanDisplayText(line);
        if (!cleaned.isEmpty())
            return cleaned;
    }
    return {};
}

} // namespace

ParsedLyrics parseLyrics(const LyricPayload &payload)
{
    static const QRegularExpression timedLineExpression(
        QStringLiteral("^\\s*((?:\\[\\d+:[0-5]\\d(?:\\.\\d{1,3})?\\]\\s*)+)(.*)$"));
    static const QRegularExpression timestampExpression(
        QStringLiteral("\\[(\\d+):([0-5]\\d)(?:\\.(\\d{1,3}))?\\]"));
    static const QRegularExpression offsetExpression(
        QStringLiteral("^\\s*\\[offset\\s*:\\s*([+-]?\\d+)\\]\\s*$"),
        QRegularExpression::CaseInsensitiveOption);

    ParsedLyrics result;
    QMap<qint64, QString> linesByTimestamp;
    const QStringList sourceLines = payload.syncedLyrics.split(
        QRegularExpression(QStringLiteral("\\r?\\n")));

    for (const QString &sourceLine : sourceLines) {
        const QRegularExpressionMatch offsetMatch = offsetExpression.match(sourceLine);
        if (offsetMatch.hasMatch()) {
            bool valid = false;
            const qint64 sourceOffsetMs = offsetMatch.captured(1).toLongLong(&valid);
            if (valid)
                result.sourceOffsetMs = sourceOffsetMs;
            continue;
        }

        const QRegularExpressionMatch lineMatch = timedLineExpression.match(sourceLine);
        if (!lineMatch.hasMatch())
            continue;
        const QString text = cleanDisplayText(lineMatch.captured(2));
        if (text.isEmpty())
            continue;

        // 同一时间戳由源文件中最后出现的有效文本覆盖，并由 QMap 保持时间顺序。
        // The last valid source text wins for a duplicate timestamp; QMap keeps chronological order.
        QRegularExpressionMatchIterator timestamps = timestampExpression.globalMatch(lineMatch.captured(1));
        while (timestamps.hasNext()) {
            const QRegularExpressionMatch timestamp = timestamps.next();
            const qint64 minutes = timestamp.captured(1).toLongLong();
            const qint64 seconds = timestamp.captured(2).toLongLong();
            const qint64 milliseconds = fractionToMilliseconds(timestamp.captured(3));
            linesByTimestamp.insert(((minutes * 60) + seconds) * 1000 + milliseconds, text);
        }
    }

    result.lines.reserve(linesByTimestamp.size());
    for (auto iterator = linesByTimestamp.cbegin(); iterator != linesByTimestamp.cend(); ++iterator)
        result.lines.append({iterator.key(), iterator.value()});

    if (!result.lines.isEmpty()) {
        result.timing = TimingCapability::Line;
        return result;
    }

    result.plainText = firstPlainLine(payload.plainLyrics);
    result.timing = result.plainText.isEmpty() ? TimingCapability::None : TimingCapability::Plain;
    return result;
}

} // namespace deepin::lyrics
