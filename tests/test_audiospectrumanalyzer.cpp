#include "infrastructure/audiospectrumanalyzer.h"

#include <QtTest>

#include <cmath>

using namespace deepin::lyrics;

class AudioSpectrumAnalyzerTest final : public QObject
{
    Q_OBJECT

private slots:
    void returnsZeroForSilence();
    void returnsBoundedEnergyForAudio();
    void distributesMixedToneEnergyAcrossBands();
};

void AudioSpectrumAnalyzerTest::returnsZeroForSilence()
{
    AudioSpectrumAnalyzer analyzer;
    const VisualizerFrame frame = analyzer.analyze(QVector<float>(128, 0.0F));
    for (const float level : frame.levels)
        QCOMPARE(level, 0.0F);
}

void AudioSpectrumAnalyzerTest::returnsBoundedEnergyForAudio()
{
    QVector<float> samples;
    samples.reserve(128);
    for (int index = 0; index < 128; ++index)
        samples.append(float(0.8 * std::sin(2.0 * M_PI * 8.0 * index / 128.0)));

    AudioSpectrumAnalyzer analyzer;
    const VisualizerFrame frame = analyzer.analyze(samples);
    bool hasEnergy = false;
    for (const float level : frame.levels) {
        QVERIFY(level >= 0.0F);
        QVERIFY(level <= 1.0F);
        hasEnergy = hasEnergy || level > 0.05F;
    }
    QVERIFY(hasEnergy);
}

void AudioSpectrumAnalyzerTest::distributesMixedToneEnergyAcrossBands()
{
    QVector<float> samples;
    samples.reserve(256);
    for (int index = 0; index < 256; ++index) {
        const double low = 0.55 * std::sin(2.0 * M_PI * 3.0 * index / 256.0);
        const double mid = 0.30 * std::sin(2.0 * M_PI * 36.0 * index / 256.0);
        const double high = 0.15 * std::sin(2.0 * M_PI * 92.0 * index / 256.0);
        samples.append(float(low + mid + high));
    }

    AudioSpectrumAnalyzer analyzer;
    const VisualizerFrame frame = analyzer.analyze(samples);
    int visibleBandCount = 0;
    for (const float level : frame.levels)
        visibleBandCount += level > 0.08F;
    QVERIFY(visibleBandCount >= 3);
}

QTEST_GUILESS_MAIN(AudioSpectrumAnalyzerTest)
#include "test_audiospectrumanalyzer.moc"
