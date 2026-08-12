# S08 运行时配置、用户服务与 Debian 打包

**状态：** 待实施

**前置：** S00-S07
**后续：** S09

## 目标

把 MVP 交付为适用于 deepin/UOS v25 的单个 Debian 安装包：无需修改 Dock 源码，用户登录后后台服务可被按需或随图形会话启动，设置应用和 Dock Applet 都能找到同一份运行时组件。

## 范围

- 定义 DConfig meta、用户级 systemd 服务、session D-Bus 激活文件、`.desktop` 文件和 Debian 打包规则。
- 安装 daemon、设置应用、`dde-shell` package、插件库、翻译、DConfig meta 和文档。
- 支持全新安装、升级、卸载和用户缓存的显式清理。

## 非范围

- 不在安装脚本编辑或替换系统 Dock 的 QML/C++ 文件。
- 不以 root 身份运行 daemon，不创建系统级常驻服务，不收集遥测。
- 卸载时不自动删除用户缓存或 DConfig；用户数据由用户在设置页主动清除。

## DConfig

应用 ID：`org.deepin.LyricsDock`；配置名：`lyrics-dock`。meta 安装路径为：

```text
/usr/share/dsg/configs/org.deepin.LyricsDock/lyrics-dock.json
```

| 键 | 类型 / 默认值 | 可见性 | 说明 |
|---|---|---|---|
| `enabled` | `bool / false` | private | 总开关 |
| `playerBusName` | `string / ""` | private | 用户选定 MPRIS 服务 |
| `offsetMs` | `int / 0` | private | `+` 提前、`-` 延后 |

`sessionHidden` 不得写入 DConfig。DConfig 由 daemon 作为唯一写入方维护；设置应用通过 S03 的方法请求修改，避免跨进程并发写入。

## 用户服务与 D-Bus 激活

安装下列文件：

```text
/usr/libexec/lyrics-dockd
/usr/lib/systemd/user/lyrics-dockd.service
/usr/share/dbus-1/services/org.deepin.LyricsDock1.service
```

`lyrics-dockd.service` 使用用户 systemd，`Type=dbus`，`BusName=org.deepin.LyricsDock1`，且在 `graphical-session.target` 下可启动。D-Bus service 文件使用同一名称，并指向该 systemd 用户服务，保证 Applet 或设置应用先于 daemon 启动时仍可通过 D-Bus 激活服务。服务启动但 `enabled=false` 时不可进行 MPRIS 轮询或网络请求。

## 预期安装内容

```text
/usr/bin/deepin-lyrics-settings
/usr/libexec/lyrics-dockd
/usr/lib/<multiarch>/dde-shell/plugins/ds-lyrics-dock.so
/usr/share/dde-shell/org.deepin.ds.lyrics-dock/
/usr/share/applications/org.deepin.LyricsDock.desktop
/usr/share/dsg/configs/org.deepin.LyricsDock/lyrics-dock.json
/usr/lib/systemd/user/lyrics-dockd.service
/usr/share/dbus-1/services/org.deepin.LyricsDock1.service
```

## Debian 约束

- 源包和二进制包名称为 `deepin-dock-lyrics`；包版本从顶层 CMake 单一来源生成。
- `Build-Depends` 至少覆盖 CMake、debhelper-compat 13、Qt6 Core/DBus/Network/Sql/Widgets 开发包、DTK6 开发包和 `libdde-shell-dev`。
- 运行期共享库交由 `${shlibs:Depends}`，QML 所需模块和 `dde-shell` 显式进入 `Depends`。
- `dh_installsystemduser`（或等价的 debhelper 规则）处理用户服务；维护脚本不得强制 kill 用户桌面进程。
- 包内带有 MIT `copyright` 信息和 LRCLIB 隐私说明：启用后会向 LRCLIB 发送歌曲标题、艺人、专辑和时长以检索歌词。

## 验收标准

1. 在干净的 v25 虚拟机/容器构建出 `.deb`，安装后所有上表文件位于正确路径。
2. 从设置应用或 Applet 首次调用服务时，D-Bus 能激活 `org.deepin.LyricsDock1`，且只有一个 daemon 实例。
3. 注销再登录后，`enabled`、玩家选择、偏移仍有效；关闭 Applet 的会话隐藏状态已恢复。
4. 升级保留 DConfig 与缓存，卸载停止用户服务且不修改系统 Dock 文件；重新安装可正常工作。
