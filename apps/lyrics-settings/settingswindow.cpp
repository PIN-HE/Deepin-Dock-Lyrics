#include "settingswindow.h"

#include "settingsviewmodel.h"

#include <lyricsui/lyricstokens.h>

#include <DComboBox>
#include <DDialog>
#include <DIconTheme>
#include <DSpinBox>
#include <DSuggestButton>
#include <DSwitchButton>
#include <DTitlebar>
#include <DWarningButton>

#include <QAbstractItemView>
#include <QAccessible>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

using deepin::lyrics::LyricsTokens;

namespace {

constexpr int windowWidth = 680;
constexpr int windowHeight = 720;

QLabel *descriptionLabel(const QString &text, QWidget *parent = nullptr)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

} // namespace

SettingsWindow::SettingsWindow(SettingsViewModel &viewModel, QWidget *parent)
    : DMainWindow(parent)
    , m_viewModel(viewModel)
    , m_tokens(new LyricsTokens(this))
{
    setObjectName(QStringLiteral("settingsWindow"));
    setMinimumSize(560, 600);
    resize(windowWidth, windowHeight);
    titlebar()->setTitle(tr("Deepin Dock Lyrics"));
    titlebar()->setIcon(QIcon(QStringLiteral(":/icons/deepin-lyrics-dock.png")));
    titlebar()->setFullScreenButtonVisible(false);

    m_pages = new QStackedWidget(this);
    m_introPage = createIntroPage();
    m_settingsPage = createSettingsPage();
    m_pages->addWidget(m_introPage);
    m_pages->addWidget(m_settingsPage);
    setCentralWidget(m_pages);

    connect(&m_viewModel, &SettingsViewModel::serviceAvailableChanged,
            this, &SettingsWindow::refreshUi);
    connect(&m_viewModel, &SettingsViewModel::busyChanged,
            this, &SettingsWindow::refreshUi);
    connect(&m_viewModel, &SettingsViewModel::stateChanged,
            this, &SettingsWindow::refreshUi);
    connect(&m_viewModel, &SettingsViewModel::stateChanged,
            this, &SettingsWindow::refreshFrame);
    connect(&m_viewModel, &SettingsViewModel::frameChanged,
            this, &SettingsWindow::refreshFrame);
    connect(&m_viewModel, &SettingsViewModel::candidatesChanged,
            this, &SettingsWindow::refreshCandidates);
    connect(&m_viewModel, &SettingsViewModel::operationFailed,
            this, &SettingsWindow::showOperationError);
    connect(&m_viewModel, &SettingsViewModel::operationSucceeded,
            this, &SettingsWindow::showOperationSuccess);
    connect(m_tokens, &LyricsTokens::paletteChanged, this, [this] {
        QPalette palette = m_messageLabel->palette();
        palette.setColor(QPalette::WindowText, m_tokens->textSecondary());
        m_messageLabel->setPalette(palette);
        refreshUi();
    });

    refreshUi();
    refreshFrame();
    refreshCandidates();
}

QWidget *SettingsWindow::createIntroPage()
{
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("introPage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(m_tokens->settingsPagePadding(),
                               m_tokens->settingsPagePadding(),
                               m_tokens->settingsPagePadding(),
                               m_tokens->settingsPagePadding());
    layout->setSpacing(m_tokens->space4());
    layout->addStretch();

    auto *icon = new QLabel(page);
    icon->setPixmap(QIcon(QStringLiteral(":/icons/deepin-lyrics-dock.png")).pixmap(56, 56));
    icon->setAlignment(Qt::AlignCenter);
    icon->setAccessibleName(tr("Deepin Dock Lyrics icon"));
    layout->addWidget(icon, 0, Qt::AlignCenter);

    m_introTitle = new QLabel(tr("Show lyrics in the Dock"), page);
    QFont titleFont = m_introTitle->font();
    titleFont.setPixelSize(20);
    titleFont.setWeight(QFont::DemiBold);
    m_introTitle->setFont(titleFont);
    m_introTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_introTitle);

    m_introDescription = descriptionLabel(
        tr("Follow the current song from a compatible music player and display synced lyrics in the Dock."),
        page);
    m_introDescription->setAlignment(Qt::AlignCenter);
    m_introDescription->setMaximumWidth(440);
    layout->addWidget(m_introDescription, 0, Qt::AlignCenter);

    m_startButton = new DSuggestButton(tr("Start Dock Lyrics"), page);
    m_startButton->setObjectName(QStringLiteral("startButton"));
    m_startButton->setAccessibleName(tr("Start Dock Lyrics"));
    connect(m_startButton, &QPushButton::clicked, &m_viewModel, &SettingsViewModel::startLyrics);
    layout->addWidget(m_startButton, 0, Qt::AlignCenter);
    layout->addStretch();
    return page;
}

QWidget *SettingsWindow::createSettingsPage()
{
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("settingsPage"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(scroll);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(m_tokens->settingsPagePadding(),
                               m_tokens->space4(),
                               m_tokens->settingsPagePadding(),
                               m_tokens->settingsPagePadding());
    layout->setSpacing(m_tokens->settingsSectionGap());

    auto *summary = new QFrame(content);
    summary->setObjectName(QStringLiteral("statusSummary"));
    summary->setAutoFillBackground(true);
    auto *summaryLayout = new QVBoxLayout(summary);
    summaryLayout->setContentsMargins(m_tokens->space4(), m_tokens->space3(),
                                      m_tokens->space4(), m_tokens->space3());
    summaryLayout->setSpacing(m_tokens->space1());
    m_statusValue = new QLabel(summary);
    QFont statusFont = m_statusValue->font();
    statusFont.setWeight(QFont::DemiBold);
    m_statusValue->setFont(statusFont);
    m_trackValue = descriptionLabel({}, summary);
    m_sourceValue = descriptionLabel({}, summary);
    summaryLayout->addWidget(m_statusValue);
    summaryLayout->addWidget(m_trackValue);
    summaryLayout->addWidget(m_sourceValue);
    layout->addWidget(summary);

    m_enabledSwitch = new DSwitchButton(content);
    m_enabledSwitch->setObjectName(QStringLiteral("enabledSwitch"));
    m_enabledSwitch->setAccessibleName(tr("Enable Dock Lyrics"));
    connect(m_enabledSwitch, &DSwitchButton::checkedChanged, this, [this](bool checked) {
        if (!m_updatingUi)
            m_viewModel.setEnabled(checked);
    });
    layout->addWidget(createSection(
        tr("General"),
        createSettingRow(tr("Enable Dock Lyrics"),
                         tr("Pause or resume lyric lookup and Dock display."),
                         m_enabledSwitch)));

    auto *playerContent = new QWidget(content);
    auto *playerLayout = new QVBoxLayout(playerContent);
    playerLayout->setContentsMargins(0, 0, 0, 0);
    playerLayout->setSpacing(m_tokens->space2());
    m_playerCombo = new DComboBox(playerContent);
    m_playerCombo->setObjectName(QStringLiteral("playerCombo"));
    m_playerCombo->setAccessibleName(tr("Music player"));
    connect(m_playerCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!m_updatingUi && index >= 0)
            m_viewModel.setPlayer(m_playerCombo->itemData(index).toString());
    });
    m_playerHint = descriptionLabel({}, playerContent);
    playerLayout->addWidget(createSettingRow(
        tr("Music player"), tr("Only running players that support MPRIS are shown."),
        m_playerCombo));
    playerLayout->addWidget(m_playerHint);
    layout->addWidget(createSection(tr("Player"), playerContent));

    m_offsetSpin = new DSpinBox(content);
    m_offsetSpin->setObjectName(QStringLiteral("offsetSpin"));
    m_offsetSpin->setRange(-10000, 10000);
    m_offsetSpin->setSingleStep(100);
    m_offsetSpin->setSuffix(QStringLiteral(" ms"));
    m_offsetSpin->setAccessibleName(tr("Sync offset"));
    connect(m_offsetSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_updatingUi)
            m_viewModel.setOffsetMs(value);
    });
    layout->addWidget(createSection(
        tr("Synchronization"),
        createSettingRow(tr("Sync offset (+ earlier, - later)"),
                         tr("Adjust when a timed lyric line becomes active."), m_offsetSpin)));

    m_audioVisualizerSwitch = new DSwitchButton(content);
    m_audioVisualizerSwitch->setObjectName(QStringLiteral("audioVisualizerSwitch"));
    m_audioVisualizerSwitch->setAccessibleName(tr("Show current player's audio visualizer"));
    connect(m_audioVisualizerSwitch, &DSwitchButton::checkedChanged, this, [this](bool checked) {
        if (!m_updatingUi)
            m_viewModel.setAudioVisualizerEnabled(checked);
    });
    layout->addWidget(createSection(
        tr("Audio visualizer"),
        createSettingRow(tr("Show current player's audio visualizer"),
                         tr("Audio is processed only in memory from the selected player's exact matching stream. It is never recorded, saved, or uploaded. If no exact match is available, system audio is not read."),
                         m_audioVisualizerSwitch)));

    // 歌词布局样式：经典（两行左对齐、当前行上滚）或卡拉 OK（当前行居左、下一行居右）。
    // Lyric layout style: classic (left-aligned lines, current line on top) or
    // karaoke (current line left, next line right).
    auto *layoutContent = new QWidget(content);
    auto *layoutRow = new QHBoxLayout(layoutContent);
    layoutRow->setContentsMargins(0, 0, 0, 0);
    layoutRow->setSpacing(m_tokens->space3());
    m_layoutCombo = new DComboBox(layoutContent);
    m_layoutCombo->setObjectName(QStringLiteral("lyricLayoutCombo"));
    m_layoutCombo->setAccessibleName(tr("Lyric layout"));
    m_layoutCombo->addItem(tr("Classic"), QStringLiteral("classic"));
    m_layoutCombo->addItem(tr("Karaoke"), QStringLiteral("karaoke"));
    connect(m_layoutCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!m_updatingUi && index >= 0)
            m_viewModel.setLyricLayout(m_layoutCombo->itemData(index).toString());
    });
    layoutRow->addWidget(m_layoutCombo, 1);
    layout->addWidget(createSection(
        tr("Layout"),
        createSettingRow(tr("Dock lyric layout"),
                         tr("Karaoke places the current line left and the next line right."),
                         layoutContent)));

    auto *candidateContent = new QWidget(content);
    auto *candidateLayout = new QVBoxLayout(candidateContent);
    candidateLayout->setContentsMargins(0, 0, 0, 0);
    candidateLayout->setSpacing(m_tokens->space2());
    m_candidateList = new QListWidget(candidateContent);
    m_candidateList->setObjectName(QStringLiteral("candidateList"));
    m_candidateList->setAccessibleName(tr("Possible lyric matches"));
    m_candidateList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_candidateList->setMinimumHeight(120);
    connect(m_candidateList, &QListWidget::itemSelectionChanged, this, [this] {
        m_selectCandidateButton->setEnabled(
            !m_viewModel.busy() && m_candidateList->currentItem());
    });
    auto *candidateButtons = new QHBoxLayout;
    m_searchButton = new QPushButton(DIconTheme::findQIcon(QStringLiteral("view-refresh")),
                                     tr("Search again"), candidateContent);
    m_searchButton->setAccessibleName(tr("Search lyric matches again"));
    connect(m_searchButton, &QPushButton::clicked,
            &m_viewModel, &SettingsViewModel::searchCandidates);
    m_selectCandidateButton = new DSuggestButton(tr("Use selected match"), candidateContent);
    m_selectCandidateButton->setObjectName(QStringLiteral("selectCandidateButton"));
    m_selectCandidateButton->setEnabled(false);
    connect(m_selectCandidateButton, &QPushButton::clicked, this, [this] {
        auto *item = m_candidateList->currentItem();
        if (!item)
            return;
        const QVariantMap candidate = item->data(Qt::UserRole).toMap();
        m_viewModel.selectCandidate(candidate.value(QStringLiteral("providerId")).toString(),
                                    candidate.value(QStringLiteral("candidateId")).toString());
    });
    candidateButtons->addWidget(m_searchButton);
    candidateButtons->addStretch();
    candidateButtons->addWidget(m_selectCandidateButton);
    candidateLayout->addWidget(descriptionLabel(
        tr("Choose a result only when the current song was not matched correctly."),
        candidateContent));
    candidateLayout->addWidget(m_candidateList);
    candidateLayout->addLayout(candidateButtons);
    layout->addWidget(createSection(tr("Lyric match"), candidateContent));

    auto *dockContent = new QWidget(content);
    auto *dockLayout = new QHBoxLayout(dockContent);
    dockLayout->setContentsMargins(0, 0, 0, 0);
    dockLayout->setSpacing(m_tokens->space2());
    dockLayout->addWidget(descriptionLabel(
        tr("Restore the lyric area after closing it from the Dock."), dockContent), 1);
    m_showDockButton = new QPushButton(DIconTheme::findQIcon(QStringLiteral("view-visible")),
                                       tr("Show in Dock"), dockContent);
    m_showDockButton->setObjectName(QStringLiteral("showDockButton"));
    m_showDockButton->setAccessibleName(tr("Show lyrics in the Dock"));
    connect(m_showDockButton, &QPushButton::clicked,
            &m_viewModel, &SettingsViewModel::showInDock);
    dockLayout->addWidget(m_showDockButton);
    layout->addWidget(createSection(tr("Dock display"), dockContent));

    auto *privacyContent = new QWidget(content);
    auto *privacyLayout = new QHBoxLayout(privacyContent);
    privacyLayout->setContentsMargins(0, 0, 0, 0);
    privacyLayout->setSpacing(m_tokens->space3());
    privacyLayout->addWidget(descriptionLabel(
        tr("Only song title, artist, album, and duration are sent to LRCLIB. Playback history and lyric text are not collected."),
        privacyContent), 1);
    m_clearCacheButton = new DWarningButton(privacyContent);
    m_clearCacheButton->setText(tr("Clear cache"));
    m_clearCacheButton->setObjectName(QStringLiteral("clearCacheButton"));
    m_clearCacheButton->setAccessibleName(tr("Clear lyric cache"));
    connect(m_clearCacheButton, &QPushButton::clicked, this, [this] {
        DDialog dialog(tr("Clear lyric cache?"),
                       tr("Downloaded lyrics and match choices will be removed. Player selection and sync offset will be kept."),
                       this);
        dialog.addButton(tr("Cancel"));
        dialog.addButton(tr("Clear"), true, DDialog::ButtonWarning);
        if (dialog.exec() == 1)
            m_viewModel.clearCache();
    });
    privacyLayout->addWidget(m_clearCacheButton);
    layout->addWidget(createSection(tr("Privacy and cache"), privacyContent));

    m_busyIndicator = new QProgressBar(content);
    m_busyIndicator->setObjectName(QStringLiteral("busyIndicator"));
    m_busyIndicator->setRange(0, 0);
    m_busyIndicator->setTextVisible(false);
    m_busyIndicator->setFixedHeight(m_tokens->space1());
    m_busyIndicator->setAccessibleName(tr("Operation in progress"));
    m_messageLabel = descriptionLabel({}, content);
    m_messageLabel->setObjectName(QStringLiteral("messageLabel"));
    m_messageLabel->setMinimumHeight(20);
    layout->addWidget(m_busyIndicator);
    layout->addWidget(m_messageLabel);
    layout->addStretch();

    scroll->setWidget(content);
    return scroll;
}

QWidget *SettingsWindow::createSection(const QString &title, QWidget *content)
{
    auto *section = new QWidget(this);
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(m_tokens->settingsRowGap());
    auto *label = new QLabel(title, section);
    QFont font = label->font();
    font.setPixelSize(16);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    layout->addWidget(label);
    content->setParent(section);
    layout->addWidget(content);
    return section;
}

QWidget *SettingsWindow::createSettingRow(const QString &title,
                                          const QString &description,
                                          QWidget *control)
{
    auto *row = new QWidget(this);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(m_tokens->space3());
    auto *labels = new QWidget(row);
    auto *labelsLayout = new QVBoxLayout(labels);
    labelsLayout->setContentsMargins(0, 0, 0, 0);
    labelsLayout->setSpacing(m_tokens->space1());
    auto *titleLabel = new QLabel(title, labels);
    auto *descriptionText = descriptionLabel(description, labels);
    labelsLayout->addWidget(titleLabel);
    labelsLayout->addWidget(descriptionText);
    layout->addWidget(labels, 1);
    control->setParent(row);
    layout->addWidget(control, 0, Qt::AlignVCenter);
    return row;
}

void SettingsWindow::refreshUi()
{
    const QVariantMap state = m_viewModel.state();
    const bool serviceAvailable = m_viewModel.serviceAvailable();
    const bool enabled = state.value(QStringLiteral("enabled")).toBool();
    const bool busy = m_viewModel.busy();
    m_pages->setCurrentWidget(serviceAvailable && enabled ? m_settingsPage : m_introPage);

    if (!serviceAvailable) {
        m_introTitle->setText(tr("Lyrics service is not available"));
        m_introDescription->setText(
            tr("The background service is not running yet. This page will reconnect automatically when it becomes available."));
        m_startButton->setText(tr("Start Dock Lyrics"));
        m_startButton->setEnabled(false);
        return;
    }

    m_introTitle->setText(tr("Show lyrics in the Dock"));
    m_introDescription->setText(
        tr("Follow the current song from a compatible music player and display synced lyrics in the Dock."));
    m_startButton->setText(busy ? tr("Starting...") : tr("Start Dock Lyrics"));
    m_startButton->setEnabled(!busy);

    m_updatingUi = true;
    {
        const QSignalBlocker enabledBlocker(m_enabledSwitch);
        m_enabledSwitch->setChecked(enabled);
        const QSignalBlocker offsetBlocker(m_offsetSpin);
        m_offsetSpin->setValue(state.value(QStringLiteral("offsetMs")).toInt());
        const QSignalBlocker visualizerBlocker(m_audioVisualizerSwitch);
        m_audioVisualizerSwitch->setChecked(
            state.value(QStringLiteral("audioVisualizerEnabled")).toBool());
        const QSignalBlocker layoutBlocker(m_layoutCombo);
        const int layoutIndex = m_layoutCombo->findData(
            state.value(QStringLiteral("lyricLayout"), QStringLiteral("classic")).toString());
        m_layoutCombo->setCurrentIndex(layoutIndex >= 0 ? layoutIndex : 0);
        // 位置轮询会使 StateChanged 高频到达；播放器列表与选中项未变化时不重建
        // 下拉框，否则每次位置更新都会打断用户的选择操作（下拉框"抽搐"）。
        // Position polling fires StateChanged frequently; rebuild the player
        // combo only when the player list or selection actually changed,
        // otherwise every position tick would interrupt the user's selection.
        const QVariantList players = state.value(QStringLiteral("availablePlayers")).toList();
        const QString selectedPlayer = state.value(QStringLiteral("playerBusName")).toString();
        if (players != m_lastPlayers || selectedPlayer != m_lastSelectedPlayer) {
            populatePlayers(players, selectedPlayer);
            m_lastPlayers = players;
            m_lastSelectedPlayer = selectedPlayer;
        }
    }
    m_updatingUi = false;

    const QString status = state.value(QStringLiteral("status")).toString();
    m_statusValue->setText(statusText(status));
    m_trackValue->setText(trackText(state));
    m_busyIndicator->setVisible(busy || status == QStringLiteral("LookingUpLyrics"));
    m_showDockButton->setEnabled(!busy && state.value(QStringLiteral("sessionHidden")).toBool());

    QPalette summaryPalette = m_statusValue->parentWidget()->palette();
    summaryPalette.setColor(QPalette::Window, m_tokens->surfaceSelected());
    summaryPalette.setColor(QPalette::WindowText,
                            status == QStringLiteral("Error")
                                ? m_tokens->statusError()
                                : m_tokens->textPrimary());
    m_statusValue->parentWidget()->setPalette(summaryPalette);
    m_statusValue->parentWidget()->setAutoFillBackground(true);
    setControlsEnabled(!busy && status != QStringLiteral("LookingUpLyrics"));

    const QString stateError = state.value(QStringLiteral("errorCode")).toString();
    if (stateError != m_lastStateErrorCode) {
        m_lastStateErrorCode = stateError;
        if (stateError.isEmpty())
            m_messageLabel->clear();
        else
            showOperationError(stateError);
    }
}

void SettingsWindow::refreshFrame()
{
    const QVariantMap frame = m_viewModel.frame();
    const QVariantMap state = m_viewModel.state();
    const QString source = frame.value(
        QStringLiteral("source"), state.value(QStringLiteral("lyricsSource"))).toString();
    const QString timing = timingText(frame.value(
        QStringLiteral("timingCapability"), state.value(QStringLiteral("timingCapability"))).toString());
    if (source.isEmpty() && timing.isEmpty()) {
        m_sourceValue->clear();
        return;
    }
    m_sourceValue->setText(tr("Source: %1 · %2").arg(source.isEmpty() ? tr("Unknown") : source,
                                                     timing));
}

void SettingsWindow::refreshCandidates()
{
    // Keep the coordinator's order; re-sorting here can detach the visible row from its payload.
    // 保留协调器排序；设置页二次排序会导致可见行与实际候选数据错位。
    const QVariantList candidates = m_viewModel.candidates();
    m_candidateList->clear();
    for (const QVariant &value : candidates) {
        const QVariantMap candidate = value.toMap();
        QStringList details;
        const QString artist = candidate.value(QStringLiteral("artist")).toString();
        const QString album = candidate.value(QStringLiteral("album")).toString();
        if (!artist.isEmpty())
            details.append(artist);
        if (!album.isEmpty())
            details.append(album);
        const qint64 durationMs = candidate.value(QStringLiteral("durationMs")).toLongLong();
        if (durationMs > 0)
            details.append(formatDuration(durationMs));
        details.append(tr("Possible match"));
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(candidate.value(QStringLiteral("title")).toString(),
                                          details.join(QStringLiteral(" · "))),
            m_candidateList);
        item->setData(Qt::UserRole, candidate);
        item->setToolTip(item->text());
    }
    if (candidates.isEmpty()) {
        auto *emptyItem = new QListWidgetItem(tr("No alternative matches found"),
                                               m_candidateList);
        emptyItem->setFlags(Qt::NoItemFlags);
    }
    m_selectCandidateButton->setEnabled(false);
}

void SettingsWindow::showOperationError(const QString &errorCode)
{
    m_messageLabel->setText(errorText(errorCode));
    QPalette palette = m_messageLabel->palette();
    palette.setColor(QPalette::WindowText, m_tokens->statusError());
    m_messageLabel->setPalette(palette);
}

void SettingsWindow::showOperationSuccess(const QString &operation)
{
    QString message;
    if (operation == QStringLiteral("cache"))
        message = tr("Lyric cache cleared");
    else if (operation == QStringLiteral("show"))
        message = tr("Lyrics are visible in the Dock");
    else if (operation == QStringLiteral("candidate"))
        message = tr("Loading the selected lyric match...");
    m_messageLabel->setText(message);
    QPalette palette = m_messageLabel->palette();
    palette.setColor(QPalette::WindowText, m_tokens->statusSuccess());
    m_messageLabel->setPalette(palette);
}

void SettingsWindow::populatePlayers(const QVariantList &players, const QString &selectedBusName)
{
    const QSignalBlocker blocker(m_playerCombo);
    m_playerCombo->clear();
    m_playerCombo->addItem(tr("Select a running player"), QString());
    for (const QVariant &value : players) {
        const QVariantMap player = value.toMap();
        if (!player.value(QStringLiteral("available")).toBool())
            continue;
        const QString busName = player.value(QStringLiteral("busName")).toString();
        QString identity = player.value(QStringLiteral("identity")).toString();
        if (identity.isEmpty())
            identity = busName;
        m_playerCombo->addItem(identity, busName);
    }
    const int selectedIndex = m_playerCombo->findData(selectedBusName);
    m_playerCombo->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    const bool hasPlayers = m_playerCombo->count() > 1;
    m_playerHint->setText(hasPlayers
        ? tr("Select which running player the Dock should follow.")
        : tr("No compatible running music player was found."));
}

QString SettingsWindow::statusText(const QString &status) const
{
    if (status == QStringLiteral("Disabled")) return tr("Dock Lyrics is paused");
    if (status == QStringLiteral("WaitingForPlayer")) return tr("Waiting for a music player");
    if (status == QStringLiteral("WaitingForTrack")) return tr("Waiting for a song");
    if (status == QStringLiteral("LookingUpLyrics")) return tr("Looking up lyrics...");
    if (status == QStringLiteral("LyricsReady")) return tr("Lyrics are ready");
    if (status == QStringLiteral("NoLyrics")) return tr("No lyrics found");
    if (status == QStringLiteral("NeedsCandidateSelection")) return tr("Choose a lyric match");
    if (status == QStringLiteral("Error")) return tr("Lyrics are temporarily unavailable");
    return tr("Connecting to the lyrics service...");
}

QString SettingsWindow::errorText(const QString &errorCode) const
{
    if (errorCode == QStringLiteral("network-unavailable"))
        return tr("Check the network connection and try again.");
    if (errorCode == QStringLiteral("rate-limited"))
        return tr("LRCLIB is receiving too many requests. Try again later.");
    if (errorCode == QStringLiteral("database-failed"))
        return tr("The local lyric cache could not be updated.");
    if (errorCode == QStringLiteral("invalid-player"))
        return tr("The selected player is no longer available.");
    if (errorCode == QStringLiteral("track-not-searchable"))
        return tr("Play a song with title and artist information, then try again.");
    if (errorCode == QStringLiteral("service-unavailable"))
        return tr("The background lyrics service is not available.");
    return tr("The operation could not be completed. Try again.");
}

QString SettingsWindow::timingText(const QString &timing) const
{
    if (timing == QStringLiteral("line")) return tr("Line synced");
    if (timing == QStringLiteral("plain")) return tr("Plain lyrics");
    return {};
}

QString SettingsWindow::trackText(const QVariantMap &state) const
{
    const QString title = state.value(QStringLiteral("trackTitle")).toString();
    const QStringList artists = state.value(QStringLiteral("trackArtists")).toStringList();
    if (title.isEmpty())
        return tr("No song information yet");
    return artists.isEmpty() ? title
                             : tr("%1 — %2").arg(title, artists.join(QStringLiteral(", ")));
}

QString SettingsWindow::formatDuration(qint64 durationMs) const
{
    const qint64 totalSeconds = durationMs / 1000;
    return QStringLiteral("%1:%2")
        .arg(totalSeconds / 60)
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

void SettingsWindow::setControlsEnabled(bool enabled)
{
    m_enabledSwitch->setEnabled(enabled);
    m_playerCombo->setEnabled(enabled && m_playerCombo->count() > 1);
    m_offsetSpin->setEnabled(enabled);
    m_audioVisualizerSwitch->setEnabled(enabled);
    m_searchButton->setEnabled(enabled
        && m_viewModel.state().value(QStringLiteral("canSearchCandidates")).toBool());
    m_selectCandidateButton->setEnabled(enabled && m_candidateList->currentItem());
    m_clearCacheButton->setEnabled(enabled);
}
