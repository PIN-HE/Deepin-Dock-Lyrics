#include "lyricsdockapplet.h"

#include <lyricsui/qmlregistration.h>
#include <pluginfactory.h>

LyricsDockApplet::LyricsDockApplet(QObject *parent)
    : DApplet(parent)
{
    // 在 dde-shell 创建 QML 根对象前注册令牌模块。
    // Register the token module before dde-shell creates the QML root object.
    deepin::lyrics::registerLyricsTokensQmlType();
}

bool LyricsDockApplet::init()
{
    return DApplet::init();
}

D_APPLET_CLASS(LyricsDockApplet)
#include "lyricsdockapplet.moc"
