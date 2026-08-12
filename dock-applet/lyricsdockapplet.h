#pragma once

#include "lyricsdockviewmodel.h"

#include <applet.h>

DS_USE_NAMESPACE

class LyricsDockApplet : public DApplet
{
    Q_OBJECT
    Q_PROPERTY(LyricsDockViewModel *viewModel READ viewModel CONSTANT FINAL)

public:
    explicit LyricsDockApplet(QObject *parent = nullptr);

    bool init() override;
    LyricsDockViewModel *viewModel() const;

private:
    LyricsDockViewModel *m_viewModel = nullptr;
};
