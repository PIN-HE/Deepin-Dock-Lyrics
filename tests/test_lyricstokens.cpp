#include "lyricsui/lyricstokens.h"
#include "lyricsui/qmlregistration.h"

#include <DGuiApplicationHelper>
#include <DPalette>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QtTest>

using namespace deepin::lyrics;

class LyricsTokensTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesStableGeometry();
    void mapsDtkPalette();
    void refreshesQmlBindings();
    void registersQmlSingleton();
};

void LyricsTokensTest::exposesStableGeometry()
{
    LyricsTokens tokens;

    QCOMPARE(tokens.space1(), 4);
    QCOMPARE(tokens.space2(), 8);
    QCOMPARE(tokens.space3(), 12);
    QCOMPARE(tokens.space4(), 16);
    QCOMPARE(tokens.settingsPagePadding(), 24);
    QCOMPARE(tokens.settingsSectionGap(), 24);
    QCOMPARE(tokens.settingsRowGap(), 12);
    QCOMPARE(tokens.dockVisualHeight(), 36);
    QCOMPARE(tokens.dockLyricWidthMin(), 220);
    QCOMPARE(tokens.dockLyricWidthDefault(), 280);
    QCOMPARE(tokens.dockLyricWidthMax(), 360);
    QCOMPARE(tokens.dockCloseHitSize(), 28);
    QCOMPARE(tokens.motionFast(), tokens.reduceMotion() ? 0 : 120);
    QCOMPARE(tokens.motionStandard(), tokens.reduceMotion() ? 0 : 180);
    QCOMPARE(tokens.motionSlow(), tokens.reduceMotion() ? 0 : 240);
}

void LyricsTokensTest::mapsDtkPalette()
{
    LyricsTokens tokens;
    const auto palette = Dtk::Gui::DGuiApplicationHelper::instance()->applicationPalette();

    QCOMPARE(tokens.textPrimary(), palette.color(Dtk::Gui::DPalette::TextTitle));
    QCOMPARE(tokens.textSecondary(), palette.color(Dtk::Gui::DPalette::TextTips));
    QCOMPARE(tokens.borderSubtle(), palette.color(Dtk::Gui::DPalette::FrameBorder));
    QCOMPARE(tokens.accent(), palette.color(QPalette::Highlight));
    QCOMPARE(tokens.dockLyricCurrentColor(), tokens.textPrimary());
    QCOMPARE(tokens.dockLyricProgressColor(), tokens.accent());
    QCOMPARE(tokens.dockLyricTrackColor().alpha(), 51);
}

void LyricsTokensTest::refreshesQmlBindings()
{
    LyricsTokens tokens;
    QSignalSpy paletteSpy(&tokens, &LyricsTokens::paletteChanged);
    auto *helper = Dtk::Gui::DGuiApplicationHelper::instance();
    const auto originalPalette = helper->applicationPalette();
    auto changedPalette = originalPalette;
    const QColor originalTitleColor = originalPalette.color(Dtk::Gui::DPalette::TextTitle);
    const QColor replacementTitleColor = originalTitleColor.lighter(110);

    changedPalette.setColor(Dtk::Gui::DPalette::TextTitle, replacementTitleColor);
    helper->setApplicationPalette(changedPalette);

    QTRY_VERIFY(paletteSpy.count() > 0);
    QCOMPARE(tokens.textPrimary(), replacementTitleColor);

    helper->setApplicationPalette(originalPalette);
}

void LyricsTokensTest::registersQmlSingleton()
{
    registerLyricsTokensQmlType();

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQml
        import org.deepin.lyricsdock 1.0
        QtObject {
            property int previewHeight: LyricsTokens.dockVisualHeight
            property var previewColor: LyricsTokens.dockLyricCurrentColor
        }
    )", QUrl());

    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    QCOMPARE(object->property("previewHeight").toInt(), 36);
    QVERIFY(object->property("previewColor").value<QColor>().isValid());
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    LyricsTokensTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_lyricstokens.moc"
