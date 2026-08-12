#pragma once

#include <lyricscore/types.h>

#include <QObject>

namespace deepin::lyrics {

class AudioVisualizerPort : public QObject
{
    Q_OBJECT

public:
    explicit AudioVisualizerPort(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    virtual void setEnabled(bool enabled) = 0;
    virtual void setPlayerProcessId(qint64 processId) = 0;
    virtual VisualizerState state() const = 0;
    virtual StreamMatchConfidence matchConfidence() const = 0;
    virtual void stop() = 0;

signals:
    void stateChanged(VisualizerState state, StreamMatchConfidence confidence);
    void frameChanged(const VisualizerFrame &frame);
};

} // namespace deepin::lyrics
