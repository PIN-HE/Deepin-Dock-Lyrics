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

    // 对数频段更接近人耳听感；每段聚合相邻频率，避免线性单点采样使能量只集中在低频。
    // Logarithmic bands better match hearing; aggregate neighbouring bins so linear point samples
    // do not concentrate all visible energy in the low frequencies.
    const int sampleCount = monoSamples.size();
    const int highestBin = sampleCount / 2 - 1;
    if (highestBin < 1)
        return frame;

    for (int band = 0; band < visualizerBandCount; ++band) {
        const double startRatio = double(band) / visualizerBandCount;
        const double endRatio = double(band + 1) / visualizerBandCount;
        const int firstBin = std::max(1, int(std::floor(std::pow(highestBin, startRatio))));
        const int lastBin = std::max(firstBin, std::min(highestBin,
            int(std::ceil(std::pow(highestBin, endRatio)))));
        double energy = 0.0;
        for (int frequencyBin = firstBin; frequencyBin <= lastBin; ++frequencyBin) {
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
            energy += magnitude * magnitude;
        }
        const double rmsMagnitude = std::sqrt(energy / (lastBin - firstBin + 1));
        // 压缩动态范围，使安静但可听见的中高频仍有可见高度。
        // Compress dynamic range so quieter audible mid/high bands remain visible.
        frame.levels.at(band) = float(std::sqrt(std::clamp(rmsMagnitude * 36.0, 0.0, 1.0)));
    }
    return frame;
}

} // namespace deepin::lyrics
