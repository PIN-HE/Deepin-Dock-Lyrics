# Changelog / 变更记录

## Unreleased / 未发布

- 使用 OpenCC `t2s.json` 在 daemon 解析歌词前将繁体中文转换为简体中文，缓存仍保留来源原文。
- Convert Traditional Chinese lyrics to Simplified Chinese with OpenCC `t2s.json` before daemon parsing while preserving source text in cache.
- Dock 状态、工具提示与设置应用根据系统语言自动切换简体中文或英文。
- Switch Dock status text, tooltips, and the settings application between Simplified Chinese and English according to the system locale.

## 1.0.1 - 2026-08-13

### 中文

- 首次发布完整 Debian 安装包，包含 Dock Applet、后台 daemon、DTK6 设置应用、D-Bus 激活与用户级 systemd 服务。
- 修复 daemon 退出后 Dock 右侧歌词区域仍保留空白占位的问题。
- 修复浅色主题下浮层文字与设置图标的可见性。
- 更新已知 MPRIS 播放器名称，包括网易云音乐。
- 增加构建环境中 `dde-shell` 运行时升级可能影响 Dock 主题同步的兼容性预警。

### English

- First complete Debian package release with the Dock applet, daemon, DTK6 settings application, D-Bus activation, and user systemd service.
- Fix the Dock lyric slot not being released after the daemon exits.
- Fix popup text and settings-icon visibility in light themes.
- Update known MPRIS player names, including NetEase Cloud Music.
- Document the development-environment warning that a dde-shell runtime upgrade can affect Dock theme synchronization.

## 1.0.0 - 2026-08-13

### 中文

V1 归档了 S00-S07 的功能源码基线。

- 支持发现并选择 Linux 原生 MPRIS 播放器，跟踪播放、暂停、拖动与切歌。
- 通过用户会话 D-Bus 提供 daemon 状态、歌词帧、候选选择和设置操作。
- 以 LRCLIB 作为唯一在线歌词源，包含请求节流、错误处理、SQLite 缓存和候选确认。
- 解析 LRC 行级时间轴，提供平滑行内视觉进度和长歌词自动滚动。
- 提供 `dde-shell` Dock Applet、关闭与恢复显示操作，以及 DTK6 设置应用。
- 支持亮暗主题、中文界面、本地隐私过滤日志和 15 项自动测试。

已知边界：

- 仅支持 Linux 原生 MPRIS 播放器，不支持 Wine/Windows 播放器。
- LRCLIB 可能无法匹配部分歌曲；LRC 是逐行同步，不提供真实逐字时间戳。
- 暂未集成 OpenCC 繁体转简体。
- 本标签是源码归档，不包含 systemd 用户服务、D-Bus 激活文件、桌面入口或 Debian 安装包；这些工作保留在 S08-S09。

### English

V1 archives the completed S00-S07 functional source baseline.

- Discovers and selects native Linux MPRIS players and tracks play, pause, seek, and track changes.
- Exposes daemon state, lyric frames, candidate selection, and settings over the user-session D-Bus.
- Uses LRCLIB as the sole online provider with request throttling, error handling, SQLite caching, and candidate confirmation.
- Parses line-timed LRC, renders smooth intra-line visual progress, and scrolls long lyrics automatically.
- Includes the `dde-shell` Dock applet, hide/restore actions, and the DTK6 settings application.
- Supports light and dark themes, Chinese UI translations, privacy-filtered local logging, and 15 automated tests.

Known boundaries:

- Native Linux MPRIS players only; Wine and Windows players are unsupported.
- LRCLIB does not cover every song. LRC is line-synchronised and does not provide true word timestamps.
- OpenCC Traditional-to-Simplified conversion is not integrated yet.
- This tag is a source archive. It does not include the systemd user service, D-Bus activation, desktop entry, or Debian package planned for S08-S09.
