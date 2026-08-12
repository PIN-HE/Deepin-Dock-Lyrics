#pragma once

#include <applet.h>

DS_USE_NAMESPACE

class LyricsDockApplet : public DApplet
{
    Q_OBJECT

public:
    explicit LyricsDockApplet(QObject *parent = nullptr);

    bool init() override;
};
