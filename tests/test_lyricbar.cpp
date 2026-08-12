#include <lyricsui/qmlregistration.h>

#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QtTest>

#include <memory>

class LyricBarTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void smoothsForwardProgressAndResetsBackward();
    void scrollsOnlyOverflowingText();

private:
    std::unique_ptr<QObject> createBar();

    QQmlEngine m_engine;
};

void LyricBarTest::initTestCase()
{
    deepin::lyrics::registerLyricsTokensQmlType();
}

std::unique_ptr<QObject> LyricBarTest::createBar()
{
    QQmlComponent component(&m_engine, QUrl::fromLocalFile(
        QStringLiteral(LYRICS_DOCK_PACKAGE_PATH "/qml/LyricBar.qml")));
    if (component.status() != QQmlComponent::Ready)
        qWarning().noquote() << component.errorString();
    return std::unique_ptr<QObject>(component.create());
}

void LyricBarTest::smoothsForwardProgressAndResetsBackward()
{
    auto bar = createBar();
    QVERIFY(bar);
    bar->setProperty("width", 280);
    bar->setProperty("height", 36);
    bar->setProperty("motionEnabled", true);
    bar->setProperty("progressAnimationDuration", 200);
    bar->setProperty("currentText", QStringLiteral("Current lyric"));
    bar->setProperty("progressVisible", true);
    bar->setProperty("lineProgress", 0.10);
    QTRY_COMPARE_WITH_TIMEOUT(bar->property("animatedProgress").toDouble(), 0.10, 300);

    bar->setProperty("lineProgress", 0.20);
    QTest::qWait(60);
    const double intermediate = bar->property("animatedProgress").toDouble();
    QVERIFY(intermediate > 0.10);
    QVERIFY(intermediate < 0.20);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(bar->property("animatedProgress").toDouble() - 0.20) < 0.01,
                             300);

    bar->setProperty("lineProgress", 0.05);
    QCOMPARE(bar->property("animatedProgress").toDouble(), 0.05);
}

void LyricBarTest::scrollsOnlyOverflowingText()
{
    auto bar = createBar();
    QVERIFY(bar);
    bar->setProperty("width", 280);
    bar->setProperty("height", 36);
    bar->setProperty("motionEnabled", true);
    bar->setProperty("marqueeStartDelay", 0);

    auto *line = bar->findChild<QQuickItem *>(QStringLiteral("currentLyricLine"));
    QVERIFY(line);
    bar->setProperty("currentText", QStringLiteral("Short lyric"));
    QCoreApplication::processEvents();
    QCOMPARE(line->property("maximumOffset").toDouble(), 0.0);
    QCOMPARE(line->property("contentOffset").toDouble(), 0.0);

    bar->setProperty("currentText", QString(120, QLatin1Char('W')));
    QTRY_VERIFY_WITH_TIMEOUT(line->property("maximumOffset").toDouble() > 0, 300);
    QTRY_VERIFY_WITH_TIMEOUT(line->property("contentOffset").toDouble() > 0, 500);
}

QTEST_MAIN(LyricBarTest)
#include "test_lyricbar.moc"
