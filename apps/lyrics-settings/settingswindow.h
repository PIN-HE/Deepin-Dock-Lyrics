#pragma once

#include <DMainWindow>

#include <QVariantMap>

class SettingsViewModel;

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QWidget;

namespace Dtk::Widget {
class DComboBox;
class DSpinBox;
class DSwitchButton;
}

namespace deepin::lyrics {
class LyricsTokens;
}

class SettingsWindow final : public Dtk::Widget::DMainWindow
{
    Q_OBJECT

public:
    explicit SettingsWindow(SettingsViewModel &viewModel, QWidget *parent = nullptr);

private slots:
    void refreshUi();
    void refreshFrame();
    void refreshCandidates();
    void showOperationError(const QString &errorCode);
    void showOperationSuccess(const QString &operation);

private:
    QWidget *createIntroPage();
    QWidget *createSettingsPage();
    QWidget *createSection(const QString &title, QWidget *content);
    QWidget *createSettingRow(const QString &title,
                              const QString &description,
                              QWidget *control);
    void populatePlayers(const QVariantList &players, const QString &selectedBusName);
    QString statusText(const QString &status) const;
    QString errorText(const QString &errorCode) const;
    QString timingText(const QString &timing) const;
    QString trackText(const QVariantMap &state) const;
    QString formatDuration(qint64 durationMs) const;
    void setControlsEnabled(bool enabled);

    SettingsViewModel &m_viewModel;
    deepin::lyrics::LyricsTokens *m_tokens = nullptr;
    QStackedWidget *m_pages = nullptr;
    QWidget *m_introPage = nullptr;
    QWidget *m_settingsPage = nullptr;
    QLabel *m_introTitle = nullptr;
    QLabel *m_introDescription = nullptr;
    QPushButton *m_startButton = nullptr;
    QProgressBar *m_busyIndicator = nullptr;
    QLabel *m_messageLabel = nullptr;
    Dtk::Widget::DSwitchButton *m_enabledSwitch = nullptr;
    Dtk::Widget::DComboBox *m_playerCombo = nullptr;
    QLabel *m_playerHint = nullptr;
    QLabel *m_statusValue = nullptr;
    QLabel *m_trackValue = nullptr;
    QLabel *m_sourceValue = nullptr;
    Dtk::Widget::DSpinBox *m_offsetSpin = nullptr;
    Dtk::Widget::DSwitchButton *m_audioVisualizerSwitch = nullptr;
    QPushButton *m_searchButton = nullptr;
    QListWidget *m_candidateList = nullptr;
    QPushButton *m_selectCandidateButton = nullptr;
    QPushButton *m_showDockButton = nullptr;
    QPushButton *m_clearCacheButton = nullptr;
    QString m_lastStateErrorCode;
    bool m_updatingUi = false;
};
