#include "infrastructure/openccchinesescriptconverter.h"

#include <QTest>

using namespace deepin::lyrics;

class OpenCcChineseScriptConverterTest final : public QObject
{
    Q_OBJECT

private slots:
    void convertsTimedAndPlainLyrics();
    void acceptsEmptyText();
};

void OpenCcChineseScriptConverterTest::convertsTimedAndPlainLyrics()
{
    OpenCcChineseScriptConverter converter;
    QVERIFY(converter.isValid());

    const auto result = converter.toSimplified(
        QStringLiteral("[00:01.00]後來我總算學會了如何去愛\n[00:03.00]夢想與現實"));
    QVERIFY(result.has_value());
    QCOMPARE(*result,
             QStringLiteral("[00:01.00]后来我总算学会了如何去爱\n[00:03.00]梦想与现实"));

    const auto plain = converter.toSimplified(QStringLiteral("繁體中文與夢想"));
    QVERIFY(plain.has_value());
    QCOMPARE(*plain, QStringLiteral("繁体中文与梦想"));
}

void OpenCcChineseScriptConverterTest::acceptsEmptyText()
{
    OpenCcChineseScriptConverter converter;
    QVERIFY(converter.isValid());
    const auto result = converter.toSimplified({});
    QVERIFY(result.has_value());
    QVERIFY(result->isEmpty());
}

QTEST_MAIN(OpenCcChineseScriptConverterTest)
#include "test_openccchinesescriptconverter.moc"
