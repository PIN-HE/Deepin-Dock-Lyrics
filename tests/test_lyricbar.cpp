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
    void transitionsLyricsWithAnUpwardBounce();
    void showsVisualizerOnlyWhenExplicitlyEnabled();

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
    // 系统关闭装饰动画时，歌词时间进度仍需连续呈现。
    // Lyric timing remains continuous when the system disables decorative motion.
    bar->setProperty("motionEnabled", false);
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
    QCOMPARE(bar->property("motionEnabled").toBool(), true);
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

void LyricBarTest::transitionsLyricsWithAnUpwardBounce()
{
    auto bar = createBar();
    QVERIFY(bar);
    bar->setProperty("width", 280);
    bar->setProperty("height", 36);
    bar->setProperty("lyricTransitionDuration", 180);
    bar->setProperty("currentText", QStringLiteral("First line"));
    bar->setProperty("secondaryText", QStringLiteral("Second line"));
    bar->setProperty("progressVisible", true);

    auto *currentLine = bar->findChild<QQuickItem *>(QStringLiteral("currentLyricLine"));
    auto *outgoingLine = bar->findChild<QQuickItem *>(QStringLiteral("outgoingLyricLine"));
    auto *secondaryLine = bar->findChild<QQuickItem *>(QStringLiteral("secondaryLyricLine"));
    QVERIFY(currentLine);
    QVERIFY(outgoingLine);
    QVERIFY(secondaryLine);

    bar->setProperty("currentText", QStringLiteral("Third line"));
    bar->setProperty("secondaryText", QStringLiteral("Fourth line"));

    QCOMPARE(bar->property("displayedCurrentText").toString(), QStringLiteral("Third line"));
    QCOMPARE(outgoingLine->property("text").toString(), QStringLiteral("First line"));
    QVERIFY(outgoingLine->isVisible());
    QVERIFY(currentLine->y() > 2.0);
    QVERIFY(currentLine->scale() < 1.0);

    QTRY_VERIFY_WITH_TIMEOUT(!bar->property("lyricTransitionActive").toBool(), 300);
    QCOMPARE(currentLine->property("text").toString(), QStringLiteral("Third line"));
    QCOMPARE(secondaryLine->property("text").toString(), QStringLiteral("Fourth line"));
    QVERIFY(!outgoingLine->isVisible());
    QCOMPARE(currentLine->y(), 2.0);
    QCOMPARE(currentLine->scale(), 1.0);
}

void LyricBarTest::showsVisualizerOnlyWhenExplicitlyEnabled()
{
    auto bar = createBar();
    QVERIFY(bar);
    bar->setProperty("width", 280);
    bar->setProperty("height", 36);
    auto *visualizer = bar->findChild<QQuickItem *>(QStringLiteral("audioVisualizer"));
    QVERIFY(visualizer);
    QVERIFY(!visualizer->isVisible());

    bar->setProperty("visualizerLevels", QVariantList{0.1, 0.3, 0.7, 1.0});
    bar->setProperty("visualizerVisible", true);
    QTRY_VERIFY(visualizer->isVisible());
    QCOMPARE(bar->findChild<QQuickItem *>(QStringLiteral("currentLyricLine"))->isVisible(), false);

    bar->setProperty("visualizerVisible", false);
    QTRY_VERIFY(!visualizer->isVisible());
}

QTEST_MAIN(LyricBarTest)
#include "test_lyricbar.moc"
