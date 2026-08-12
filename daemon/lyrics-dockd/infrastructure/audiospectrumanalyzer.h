#pragma once

#include <lyricscore/types.h>

#include <QVector>

namespace deepin::lyrics {

class AudioSpectrumAnalyzer final
{
public:
    VisualizerFrame analyze(const QVector<float> &monoSamples) const;
};

} // namespace deepin::lyrics
