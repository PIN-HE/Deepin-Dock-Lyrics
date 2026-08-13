#pragma once

#include "ports/settingsport.h"

#include <DConfig>

#include <QObject>

namespace deepin::lyrics {

class DConfigSettingsAdapter final : public QObject, public SettingsPort
{
    Q_OBJECT

public:
    explicit DConfigSettingsAdapter(QObject *parent = nullptr);

    bool enabled() const override;
    QString playerBusName() const override;
    int offsetMs() const override;
    bool audioVisualizerEnabled() const override;
    QString lyricLayout() const override;
    void setEnabled(bool enabled) override;
    void setPlayerBusName(const QString &busName) override;
    void setOffsetMs(int offsetMs) override;
    void setAudioVisualizerEnabled(bool enabled) override;
    void setLyricLayout(const QString &layout) override;
    bool isValid() const;

private:
    Dtk::Core::DConfig *m_config = nullptr;
    bool m_fallbackEnabled = false;
    QString m_fallbackPlayerBusName;
    int m_fallbackOffsetMs = 0;
    bool m_fallbackAudioVisualizerEnabled = false;
    QString m_fallbackLyricLayout = QStringLiteral("classic");
};

} // namespace deepin::lyrics
