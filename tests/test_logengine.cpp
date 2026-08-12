#include <lyricslogging/logengine.h>

#include <QTest>

using namespace deepin::lyrics;

class RecordingLogSink final : public LogSink
{
public:
    void write(LogLevel level, const LogEvent &event) override
    {
        lastLevel = level;
        lastEvent = event;
    }

    LogLevel lastLevel = LogLevel::Debug;
    LogEvent lastEvent;
};

class LogEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void keepsOnlySafeFields();
    void redactsFreeFormValues();
};

void LogEngineTest::keepsOnlySafeFields()
{
    RecordingLogSink sink;
    LogEngine engine(sink);

    engine.write(LogLevel::Warning, QStringLiteral("state"), QStringLiteral("state_changed"),
                 {{QStringLiteral("state_from"), QStringLiteral("Disabled")},
                  {QStringLiteral("state_to"), QStringLiteral("WaitingForPlayer")},
                  {QStringLiteral("trackTitle"), QStringLiteral("Private title")},
                  {QStringLiteral("lyrics"), QStringLiteral("Private lyrics")},
                  {QStringLiteral("rawMetadata"), QStringLiteral("Private metadata")},
                  {QStringLiteral("homePath"), QStringLiteral("/home/private")}});

    QCOMPARE(sink.lastLevel, LogLevel::Warning);
    QCOMPARE(sink.lastEvent.safeFields.size(), 2);
    QCOMPARE(sink.lastEvent.safeFields.value(QStringLiteral("state_from")).toString(),
             QStringLiteral("Disabled"));
    QVERIFY(!sink.lastEvent.safeFields.contains(QStringLiteral("trackTitle")));
    QVERIFY(!sink.lastEvent.safeFields.contains(QStringLiteral("lyrics")));
}

void LogEngineTest::redactsFreeFormValues()
{
    RecordingLogSink sink;
    LogEngine engine(sink);

    engine.write(LogLevel::Info, QStringLiteral("track title"), QStringLiteral("event with spaces"),
                 {{QStringLiteral("error_code"), QStringLiteral("private song title")},
                  {QStringLiteral("result_code"), QString()},
                  {QStringLiteral("record_id"), QStringLiteral("private song title")},
                  {QStringLiteral("offset_ms"), 20000},
                  {QStringLiteral("player_available_count"), -5}});

    QCOMPARE(sink.lastEvent.component, QStringLiteral("redacted"));
    QCOMPARE(sink.lastEvent.eventCode, QStringLiteral("redacted"));
    QCOMPARE(sink.lastEvent.safeFields.value(QStringLiteral("error_code")).toString(),
             QStringLiteral("redacted"));
    QVERIFY(!sink.lastEvent.safeFields.contains(QStringLiteral("result_code")));
    QCOMPARE(sink.lastEvent.safeFields.value(QStringLiteral("record_id")).toString(),
             QStringLiteral("redacted"));
    QCOMPARE(sink.lastEvent.safeFields.value(QStringLiteral("offset_ms")).toInt(), 10000);
    QCOMPARE(sink.lastEvent.safeFields.value(QStringLiteral("player_available_count")).toInt(), 0);

    engine.write(LogLevel::Info, QStringLiteral("lrclib"), QStringLiteral("record_confirmed"),
                 {{QStringLiteral("record_id"), QStringLiteral("1000000")}});
    QCOMPARE(sink.lastEvent.safeFields.value(QStringLiteral("record_id")).toString(),
             QStringLiteral("1000000"));
}

QTEST_MAIN(LogEngineTest)
#include "test_logengine.moc"
