# Deepin Dock Lyrics

`deepin-dock-lyrics` is a MIT-licensed lyric display tool for deepin/UOS v25.
It follows a user-selected, native Linux MPRIS music player and renders line-synchronised lyrics in the right side of the Dock. It does not play music or manage a music library.

Source repository: <https://github.com/PIN-HE/Deepin-Dock-Lyrics>

## V1 source archive

`v1.0.0` archives the completed S00-S07 functional source baseline: MPRIS discovery, the daemon and D-Bus contract, LRCLIB lookup and cache, line-synchronised rendering, the Dock applet, and the DTK6 settings application. See [CHANGELOG.md](CHANGELOG.md) for the bilingual release notes.

This tag is a source archive, not a distributable Debian release. The systemd user service, D-Bus activation file, desktop entry, Debian package, clean install/upgrade/uninstall verification, and final release gate remain in S08-S09.

## MVP scope

- Native Linux MPRIS players only. Windows and Wine players are out of scope.
- LRCLIB is the sole online lyric provider.
- LRC provides line timestamps. The product can show line progress, but does not claim word-level timing.
- The compact second line displays the next lyric. LRCLIB does not currently provide translated lyrics.
- The Dock applet is `org.deepin.ds.lyrics-dock`, attached to `org.deepin.ds.dock` with `dockOrder: 24`; it does not modify the Dock source code.

When online lyric lookup is enabled, the daemon will send the track title, artist, album, and duration to LRCLIB. The product has no account login, telemetry, or secondary lyric provider.

LRCLIB requests are serialized and cached locally in SQLite. A stable normalized track key,
confirmed LRCLIB record, raw synced/plain lyrics, negative lookup result, and rate-limit cooldown
may be stored in the application's cache directory. The cache is never uploaded and can be cleared
through the service. Tests use fake transports and do not contact or consume quota from LRCLIB.

## Build prerequisites

Target platform: deepin/UOS v25 with Qt 6, DTK6, and `dde-shell` 2.x.

```text
cmake
qt6-base-dev
qt6-declarative-dev
libdtk6core-dev
libdtk6gui-dev
libdtk6widget-dev
libdde-shell-dev
libopencc-dev
```

The settings application uses `DApplication` and DTK6 Widgets, so `libdtk6widget-dev` is required.
The daemon uses OpenCC `t2s.json` to convert Traditional Chinese lyric text to Simplified Chinese before display.

## Build and test

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

## Project layout

```text
common/lyrics-core/      Public data types and pure utility code
common/lyrics-logging/   Privacy-filtered local event logging
common/lyrics-ui/        Theme-aware DTK6/QML design tokens
daemon/lyrics-dockd/     User-session background daemon
apps/lyrics-settings/    DTK6 settings application
dock-applet/             dde-shell Dock applet package and bridge
config/                  DConfig metadata
systemd/                 Reserved for the S08 user service
debian/                  Reserved for S08 Debian packaging
specs/                   Implementation specifications
```

The implementation order and acceptance criteria are in [specs/README.md](specs/README.md).

## Local logs and privacy

`lyrics-dockd` uses the DTK file and console appenders for local diagnostics. Logs contain only
component events, state transitions, stable error codes, player counts, and bounded numeric or
boolean settings. Track titles, artists, albums, lyrics, LRCLIB queries, raw D-Bus payloads, and
user paths are rejected by an allowlist before a log sink receives them. The project has no
telemetry uploader and does not transmit these local logs.

## Code comments

New non-obvious code comments use concise Chinese and English pairs. Comments explain constraints or decisions that code cannot express directly; UI strings continue to use Qt translation APIs.

## Acknowledgements

- **端闼乐部（Ter-Music）**：终端音乐播放器，提供 `org.yxzl.ter_music.Lyrics` 会话 D-Bus 歌词接口，本项目的"外部帧源"（S10）直接消费其 A/B 双缓冲歌词帧。感谢开发者 **燕戏竹林** 及其开源仓库：
  <https://github.com/HuanSoft-Open-Source-Community/ter-music>
- **LRCLIB**：开放歌词社区库，本项目默认在线歌词源：<https://lrclib.net>
- **TaskbarLyrics**：Windows 任务栏歌词工具，其多源编排与位置外推设计为本项目提供了参考：<https://github.com/ANYNC/TaskbarLyrics>
