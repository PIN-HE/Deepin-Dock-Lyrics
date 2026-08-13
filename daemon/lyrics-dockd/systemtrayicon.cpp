#include "systemtrayicon.h"
#include "application/lyricsservicecontroller.h"

#include <QAction>
#include <QCoreApplication>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QProcess>
#include <QTimer>

namespace deepin::lyrics {

SystemTrayIcon::SystemTrayIcon(LyricsServiceController &controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
{
    // 图标路径与 resources.qrc 的 prefix + alias 对应
    QIcon icon = QIcon(QStringLiteral(":/icons/deepin-lyrics-dock.png"));
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("deepin-lyrics-dock"),
                                QIcon::fromTheme(QStringLiteral("preferences-system")));
    }
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip(tr("Deepin Dock Lyrics"));

    // 创建菜单
    m_enableAction = new QAction(tr("Enable lyrics"), m_menu);
    m_enableAction->setCheckable(true);
    m_enableAction->setChecked(true);
    connect(m_enableAction, &QAction::triggered, this, &SystemTrayIcon::onToggleEnabled);

    m_settingsAction = new QAction(tr("Settings..."), m_menu);
    connect(m_settingsAction, &QAction::triggered, this, &SystemTrayIcon::onOpenSettings);

    m_quitAction = new QAction(tr("Quit"), m_menu);
    connect(m_quitAction, &QAction::triggered, this, &SystemTrayIcon::onQuit);

    m_menu->addAction(m_enableAction);
    m_menu->addSeparator();
    m_menu->addAction(m_settingsAction);
    m_menu->addSeparator();
    m_menu->addAction(m_quitAction);

    m_trayIcon->setContextMenu(m_menu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &SystemTrayIcon::onActivated);
}

SystemTrayIcon::~SystemTrayIcon()
{
    delete m_menu;
}

void SystemTrayIcon::show()
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_trayIcon->show();
    } else {
        // X11/dxcb 下托盘主机可能比 daemon 启动晚，延迟 2 s 重试一次
        QTimer::singleShot(2000, this, [this] {
            m_trayIcon->show();
        });
    }
}

void SystemTrayIcon::hide()
{
    m_trayIcon->hide();
}

void SystemTrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        onOpenSettings();
    }
}

void SystemTrayIcon::onToggleEnabled()
{
    // 通过 D-Bus 调用切换状态（controller 不直接暴露 setEnabled）
    // 这里简化为启动设置应用让用户手动切换
    onOpenSettings();
}

void SystemTrayIcon::onOpenSettings()
{
    QProcess::startDetached(QStringLiteral("deepin-lyrics-settings"), {});
}

void SystemTrayIcon::onQuit()
{
    QCoreApplication::quit();
}

void SystemTrayIcon::updateMenu()
{
    // 更新启用状态（通过 D-Bus 获取当前状态）
    // 简化实现：始终显示为可用
    m_enableAction->setChecked(true);
}

} // namespace deepin::lyrics
