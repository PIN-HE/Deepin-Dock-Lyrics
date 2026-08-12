# S00 项目基础与公共契约

**状态：** 已完成（2026-08-12）

**前置：** 无
**后续：** S01-S09

## 目标

建立独立的 `deepin-dock-lyrics` 源码仓库骨架，使后台服务、DTK6 设置应用和 `dde-shell` Applet 能在同一 Qt 6/CMake 构建中开发，并冻结首版跨模块数据模型。

## 范围

- 建立顶层 CMake 项目，统一使用 C++17、Qt 6、AUTOMOC、AUTORCC、CTest 和 `compile_commands.json`。
- 建立三个可独立构建的目标：`lyrics-dockd`、`deepin-lyrics-settings` 和 `org.deepin.ds.lyrics-dock`。
- 建立无 Qt UI 依赖的 `lyrics-core` 静态库，放置领域对象、字符串规范化、时间与接口定义。
- 增加测试目标和 `tests/fixtures/`，保存脱敏的 MPRIS、LRCLIB、LRC fixture。
- 写入 `LICENSE`（MIT）、`README.md`、`.gitignore` 和贡献者需要的最小构建说明。

## 非范围

- 不实现真实网络请求、MPRIS 订阅、Dock UI 或设置 UI。
- 不复用 `dock-left-demo` 的安装包 ID。该目录保留为独立实验 Demo，不是正式产品源码的一部分。
- 不在本规格中创建 Debian 包。

## 目录与目标

```text
deepin-dock-lyrics/
  CMakeLists.txt
  cmake/
  common/lyrics-core/
  daemon/lyrics-dockd/
  apps/lyrics-settings/
  dock-applet/
    package/
  config/org.deepin.lyrics-dock/
  systemd/
  debian/
  tests/
    fixtures/
```

| 目标 | 类型 | 最低依赖 | 职责 |
|---|---|---|---|
| `lyrics-core` | 静态库 | Qt6 Core | 公共值类型、纯函数、接口 |
| `lyrics-dockd` | 可执行文件 | Core、DBus、Network、Sql | 用户会话后台服务 |
| `deepin-lyrics-settings` | 可执行文件 | Qt6 Widgets、DTK6 | 设置应用 |
| `ds-lyrics-dock` | `dde-shell` 插件库 | DDEShell、Qt6 QML/DBus | Applet C++ 桥接 |

`ds-lyrics-dock` 必须使用 `ds_install_package(PACKAGE org.deepin.ds.lyrics-dock TARGET ds-lyrics-dock)` 安装。其 C++ 实现必须含 `D_APPLET_CLASS(LyricsDockApplet)` 与相应 `.moc` 包含，防止构建成功但运行时无法实例化。

## 公共数据模型

定义以下 C++ 值类型，均不可依赖 QWidget、QML 或具体网络实现：

| 类型 | 必需字段 | 说明 |
|---|---|---|
| `TrackIdentity` | `title`、`artists`、`album`、`durationMs`、`playerBusName` | 当前曲目的规范化前身份 |
| `PlayerSnapshot` | `busName`、`identity`、`playbackStatus`、`positionMs`、`capturedAt` | MPRIS 采样结果 |
| `LyricLine` | `startMs`、`text` | 单个 LRC 行 |
| `ParsedLyrics` | `lines`、`plainText`、`sourceOffsetMs`、`timing` | 解析后的可渲染歌词 |
| `LyricCandidate` | `providerId`、`title`、`artist`、`album`、`durationMs`、`score` | 供用户确认的候选项 |
| `LyricFrame` | `track`、`currentText`、`secondaryText`、`translationText`、`lineIndex`、`lineProgress`、`timing` | UI 的单次显示帧 |

`translationText` 是未来兼容字段；MVP 只能为其提供空值，`secondaryText` 必须来自下一句而非自动翻译。

## 关键约束

- 全局应用 ID 为 `org.deepin.LyricsDock`；Dock 插件 ID 为 `org.deepin.ds.lyrics-dock`。二者不能混用。
- 所有持久化配置使用 DConfig，缓存使用 `QStandardPaths::CacheLocation` 下的 SQLite；不得使用 `QSettings` 或硬编码用户目录。
- 公共库中的时间单位统一为 `qint64` 毫秒；跨 D-Bus 传输的数值使用 `qlonglong` / `double`，不传 C++ 私有类型。
- 依赖版本由 CMake 显式检查：Qt6 Core/DBus/Network/Sql、DTK6、DDEShell。不要混用 Qt5/DTK5。

## 验收标准

1. 在干净构建目录执行 `cmake -S . -B build`、`cmake --build build`、`ctest --test-dir build --output-on-failure` 全部成功。
2. 三个可执行/插件目标与至少一个 `lyrics-core` 单元测试均被构建。
3. `dock-applet/package/metadata.json` 的 `Id`、`Parent`、安装包目录和 CMake 包名均为 `org.deepin.ds.lyrics-dock`，父级为 `org.deepin.ds.dock`。
4. 公共模型编译单元不包含任何 DTK、QML、Widgets、`QDBusInterface` 或 `QNetworkAccessManager` 头文件。

## 当前实现记录

- 已建立顶层 Qt 6/CMake 项目、`lyrics-core`、daemon/设置应用入口、带工厂注册的 `dde-shell` Applet，以及公共模型与规范化单元测试。
- 验证命令：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`、`cmake --build build -j2`、`ctest --test-dir build --output-on-failure`。
- 设置程序已使用 `DApplication` / `Dtk6::Widget` 并提供无界面检查模式；实际设置窗口属于 S07。整个项目始终使用 Qt6/DTK6，不混入 Qt5 或 DTK5。
