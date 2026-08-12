#include "infrastructure/pipewirestreammatcher.h"

#include <QtTest>

using namespace deepin::lyrics;

class PipeWireStreamMatcherTest final : public QObject
{
    Q_OBJECT

private slots:
    void selectsOnlyTheExactProcessOutputStream();
    void rejectsMissingOrAmbiguousCandidates();
};

void PipeWireStreamMatcherTest::selectsOnlyTheExactProcessOutputStream()
{
    const QList<PipeWireClientInfo> clients{{10, 1234}, {11, 9999}};
    const QList<PipeWireNodeInfo> nodes{
        {20, 11, QStringLiteral("200"), true},
        {21, 10, QStringLiteral("201"), true},
        {22, 10, QStringLiteral("202"), false},
    };

    const auto result = selectExactPipeWireAudioStream(clients, nodes, 1234);
    QVERIFY(result.has_value());
    QCOMPARE(result->id, 21U);
}

void PipeWireStreamMatcherTest::rejectsMissingOrAmbiguousCandidates()
{
    const QList<PipeWireClientInfo> clients{{10, 1234}};
    QVERIFY(!selectExactPipeWireAudioStream(clients, {}, 1234).has_value());
    QVERIFY(!selectExactPipeWireAudioStream(clients,
                                             {{20, 10, QStringLiteral("200"), true},
                                              {21, 10, QStringLiteral("201"), true}},
                                             1234)
                 .has_value());
    QVERIFY(!selectExactPipeWireAudioStream(clients,
                                             {{20, 10, QStringLiteral("200"), true}},
                                             5678)
                 .has_value());
}

QTEST_GUILESS_MAIN(PipeWireStreamMatcherTest)
#include "test_pipewirestreammatcher.moc"
