#include "infrastructure/audiospectrumanalyzer.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace deepin::lyrics {

VisualizerFrame AudioSpectrumAnalyzer::analyze(const QVector<float> &monoSamples) const
{
    VisualizerFrame frame;
    if (monoSamples.isEmpty())
        return frame;

    // 使用固定 16 段 DFT，只输出归一化能量，采样缓冲区不会越出此调用的生命周期。
    // Use a fixed 16-band DFT and emit normalized energy only; samples never outlive this call.
    const int sampleCount = monoSamples.size();
    for (int band = 0; band < visualizerBandCount; ++band) {
        const double frequencyBin = 1.0 + band * (sampleCount / 2.0 - 1.0)
            / (visualizerBandCount - 1);
        double real = 0.0;
        double imaginary = 0.0;
        for (int index = 0; index < sampleCount; ++index) {
            const double window = 0.5 - 0.5 * std::cos(2.0 * M_PI * index / (sampleCount - 1));
            const double angle = 2.0 * M_PI * frequencyBin * index / sampleCount;
            const double sample = std::clamp<double>(monoSamples.at(index), -1.0, 1.0) * window;
            real += sample * std::cos(angle);
            imaginary -= sample * std::sin(angle);
        }
        const double magnitude = std::sqrt(real * real + imaginary * imaginary) / sampleCount;
        frame.levels.at(band) = float(std::clamp(magnitude * 9.0, 0.0, 1.0));
    }
    return frame;
}

} // namespace deepin::lyrics
