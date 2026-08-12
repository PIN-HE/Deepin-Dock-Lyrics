#include "lyricsdockapplet.h"

#include <pluginfactory.h>

LyricsDockApplet::LyricsDockApplet(QObject *parent)
    : DApplet(parent)
{
}

bool LyricsDockApplet::init()
{
    return DApplet::init();
}

D_APPLET_CLASS(LyricsDockApplet)
#include "lyricsdockapplet.moc"
