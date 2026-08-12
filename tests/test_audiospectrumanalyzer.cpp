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

QTEST_GUILESS_MAIN(AudioSpectrumAnalyzerTest)
#include "test_audiospectrumanalyzer.moc"
