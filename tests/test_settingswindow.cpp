#include "settingsviewmodel.h"
#include "settingswindow.h"

#include <DApplication>
#include <DComboBox>
#include <DGuiApplicationHelper>
#include <DSpinBox>
#include <DSwitchButton>

#include <QDBusConnection>
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QtTest>

DWIDGET_USE_NAMESPACE

class SettingsWindowTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesRecoverableUnavailablePage();
};

void SettingsWindowTest::exposesRecoverableUnavailablePage()
{
    const QString connectionName = QStringLiteral("settings-window-client");
    QDBusConnection connection = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, connectionName);
    SettingsViewModel model(connection);
    SettingsWindow window(model);
    window.show();
    QCoreApplication::processEvents();

    QCOMPARE(window.objectName(), QStringLiteral("settingsWindow"));
    auto *pages = window.findChild<QStackedWidget *>();
    QVERIFY(pages);
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("introPage"));
    auto *startButton = window.findChild<QPushButton *>(QStringLiteral("startButton"));
    QVERIFY(startButton);
    QVERIFY(!startButton->isEnabled());
    QVERIFY(window.findChild<DSwitchButton *>(QStringLiteral("enabledSwitch")));
    QVERIFY(window.findChild<DComboBox *>(QStringLiteral("playerCombo")));
    auto *offset = window.findChild<DSpinBox *>(QStringLiteral("offsetSpin"));
    QVERIFY(offset);
    QCOMPARE(offset->minimum(), -10000);
    QCOMPARE(offset->maximum(), 10000);
    QVERIFY(window.findChild<QListWidget *>(QStringLiteral("candidateList")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("showDockButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("clearCacheButton")));
    QVERIFY(window.findChild<QProgressBar *>(QStringLiteral("busyIndicator")));
    auto *messageLabel = window.findChild<QLabel *>(QStringLiteral("messageLabel"));
    QVERIFY(messageLabel);
    QVERIFY(QMetaObject::invokeMethod(&window, "showOperationError",
                                      Q_ARG(QString, QStringLiteral("network-unavailable"))));
    QVERIFY(!messageLabel->text().isEmpty());
    QVERIFY(QMetaObject::invokeMethod(&window, "showOperationSuccess",
                                      Q_ARG(QString, QStringLiteral("enabled"))));
    QVERIFY(messageLabel->text().isEmpty());

    const QPalette originalPalette = qApp->palette();
    const QColor lightWindow = window.palette().color(QPalette::Window);
    qApp->setPalette(Dtk::Gui::DGuiApplicationHelper::standardPalette(
        Dtk::Gui::DGuiApplicationHelper::DarkType));
    QTRY_VERIFY(window.palette().color(QPalette::Window) != lightWindow);
    const QImage darkSnapshot = window.grab().toImage();
    QVERIFY(!darkSnapshot.isNull());
    QCOMPARE(darkSnapshot.size(), window.size() * window.devicePixelRatioF());
    qApp->setPalette(originalPalette);

    QDBusConnection::disconnectFromBus(connectionName);
}

QTEST_MAIN(SettingsWindowTest)
#include "test_settingswindow.moc"
