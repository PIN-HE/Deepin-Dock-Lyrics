# Deepin Dock Lyrics

`deepin-dock-lyrics` is a MIT-licensed lyric display tool for deepin/UOS v25.
It follows a user-selected, native Linux MPRIS music player and renders line-synchronised lyrics in the right side of the Dock. It does not play music or manage a music library.

## MVP scope

- Native Linux MPRIS players only. Windows and Wine players are out of scope.
- LRCLIB is the sole online lyric provider.
- LRC provides line timestamps. The product can show line progress, but does not claim word-level timing.
- The compact second line displays the next lyric. LRCLIB does not currently provide translated lyrics.
- The Dock applet is `org.deepin.ds.lyrics-dock`, attached to `org.deepin.ds.dock` with `dockOrder: 24`; it does not modify the Dock source code.

When online lyric lookup is enabled, the daemon will send the track title, artist, album, and duration to LRCLIB. The product has no account login, telemetry, or secondary lyric provider.

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
```

The S00 settings skeleton uses `DApplication` and `Dtk6::Widget`; `libdtk6widget-dev` is therefore required even before S07 adds the settings window.

## Build and test

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

## Project layout

```text
common/lyrics-core/      Public data types and pure utility code
daemon/lyrics-dockd/     User-session background daemon
apps/lyrics-settings/    DTK6 settings application
dock-applet/             dde-shell Dock applet package and bridge
config/                  DConfig metadata, added in S08
systemd/                 User-service metadata, added in S08
debian/                  Debian packaging, added in S08
specs/                   Implementation specifications
```

The implementation order and acceptance criteria are in [specs/README.md](specs/README.md).
