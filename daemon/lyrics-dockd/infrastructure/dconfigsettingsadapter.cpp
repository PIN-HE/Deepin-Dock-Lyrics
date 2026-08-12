#include "infrastructure/dconfigsettingsadapter.h"

namespace deepin::lyrics {

namespace {

constexpr auto appId = "org.deepin.LyricsDock";
constexpr auto configName = "org.deepin.LyricsDock";

}

DConfigSettingsAdapter::DConfigSettingsAdapter(QObject *parent)
    : QObject(parent)
    , m_config(Dtk::Core::DConfig::create(QString::fromLatin1(appId),
                                          QString::fromLatin1(configName), {}, this))
{
}

bool DConfigSettingsAdapter::enabled() const
{
    return isValid() ? m_config->value(QStringLiteral("enabled"), false).toBool()
                     : m_fallbackEnabled;
}

QString DConfigSettingsAdapter::playerBusName() const
{
    return isValid() ? m_config->value(QStringLiteral("playerBusName"), {}).toString()
                     : m_fallbackPlayerBusName;
}

int DConfigSettingsAdapter::offsetMs() const
{
    return isValid() ? m_config->value(QStringLiteral("offsetMs"), 0).toInt()
                     : m_fallbackOffsetMs;
}

bool DConfigSettingsAdapter::audioVisualizerEnabled() const
{
    return isValid() ? m_config->value(QStringLiteral("audioVisualizerEnabled"), false).toBool()
                     : m_fallbackAudioVisualizerEnabled;
}

void DConfigSettingsAdapter::setEnabled(bool enabled)
{
    m_fallbackEnabled = enabled;
    if (isValid())
        m_config->setValue(QStringLiteral("enabled"), enabled);
}

void DConfigSettingsAdapter::setPlayerBusName(const QString &busName)
{
    m_fallbackPlayerBusName = busName;
    if (isValid())
        m_config->setValue(QStringLiteral("playerBusName"), busName);
}

void DConfigSettingsAdapter::setOffsetMs(int offsetMs)
{
    m_fallbackOffsetMs = offsetMs;
    if (isValid())
        m_config->setValue(QStringLiteral("offsetMs"), offsetMs);
}

void DConfigSettingsAdapter::setAudioVisualizerEnabled(bool enabled)
{
    m_fallbackAudioVisualizerEnabled = enabled;
    if (isValid())
        m_config->setValue(QStringLiteral("audioVisualizerEnabled"), enabled);
}

bool DConfigSettingsAdapter::isValid() const
{
    return m_config && m_config->isValid();
}

} // namespace deepin::lyrics
