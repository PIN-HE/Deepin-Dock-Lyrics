#pragma once

#include <QString>

namespace deepin::lyrics {

class SettingsPort
{
public:
    virtual ~SettingsPort() = default;
    virtual bool enabled() const = 0;
    virtual QString playerBusName() const = 0;
    virtual int offsetMs() const = 0;
    virtual bool audioVisualizerEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
    virtual void setPlayerBusName(const QString &busName) = 0;
    virtual void setOffsetMs(int offsetMs) = 0;
    virtual void setAudioVisualizerEnabled(bool enabled) = 0;
};

} // namespace deepin::lyrics
