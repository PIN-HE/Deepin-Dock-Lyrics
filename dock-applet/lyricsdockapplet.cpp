#include "lyricsdockapplet.h"
#include "lyricsdockviewmodel.h"

#include <lyricsui/qmlregistration.h>
#include <pluginfactory.h>

LyricsDockApplet::LyricsDockApplet(QObject *parent)
    : DApplet(parent)
    , m_viewModel(new LyricsDockViewModel(QDBusConnection::sessionBus(), this))
{
    // 在 dde-shell 创建 QML 根对象前注册令牌模块。
    // Register the token module before dde-shell creates the QML root object.
    deepin::lyrics::registerLyricsTokensQmlType();
}

bool LyricsDockApplet::init()
{
    return DApplet::init();
}

LyricsDockViewModel *LyricsDockApplet::viewModel() const
{
    return m_viewModel;
}

D_APPLET_CLASS(LyricsDockApplet)
#include "lyricsdockapplet.moc"
