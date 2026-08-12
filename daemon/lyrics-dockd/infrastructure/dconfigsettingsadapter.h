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
    void setEnabled(bool enabled) override;
    void setPlayerBusName(const QString &busName) override;
    void setOffsetMs(int offsetMs) override;
    bool isValid() const;

private:
    Dtk::Core::DConfig *m_config = nullptr;
    bool m_fallbackEnabled = false;
    QString m_fallbackPlayerBusName;
    int m_fallbackOffsetMs = 0;
};

} // namespace deepin::lyrics
