#ifndef DEEPIN_LYRICS_SYSTEMTRAYICON_H
#define DEEPIN_LYRICS_SYSTEMTRAYICON_H

#include <QObject>
#include <QSystemTrayIcon>

QT_FORWARD_DECLARE_CLASS(QAction)
QT_FORWARD_DECLARE_CLASS(QMenu)

namespace deepin::lyrics {

class LyricsServiceController;

class SystemTrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit SystemTrayIcon(LyricsServiceController &controller, QObject *parent = nullptr);
    ~SystemTrayIcon() override;

    void show();
    void hide();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onToggleEnabled();
    void onOpenSettings();
    void onQuit();
    void updateMenu();

private:
    LyricsServiceController &m_controller;
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    QAction *m_enableAction;
    QAction *m_settingsAction;
    QAction *m_quitAction;
};

} // namespace deepin::lyrics

#endif // DEEPIN_LYRICS_SYSTEMTRAYICON_H
