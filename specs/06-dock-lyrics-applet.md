# S06 Dock 右侧歌词 Applet

**状态：** 已完成（2026-08-12）

**前置：** S00、S01、S03、S05
**后续：** S08、S09

## 目标

实现一个随 `dde-shell` 加载的 Dock Applet，在 Dock 右侧稳定、主题适配地显示后台服务的当前歌词与下一句，并提供详情弹窗和当前会话隐藏操作。

## 范围

- 创建插件 ID `org.deepin.ds.lyrics-dock`，`Parent` 为 `org.deepin.ds.dock`，`dockOrder: 24`。
- 使用小型 C++ `LyricsDockApplet` 桥接 session D-Bus；在 QML 暴露只读 `LyricsDockViewModel`。
- QML 仅消费 S03 的 `GetState`、`StateChanged`、`FrameChanged` 和 S01 的 `LyricsTokens`。
- 显示当前歌词、下一句、状态、行内进度、悬停信息、详情 `PanelPopup`、打开设置操作和关闭图标。
- 关闭图标调用 `SetSessionHidden(true)`；设置应用可调用 `SetSessionHidden(false)` 恢复。

## 非范围

- Applet 不直连 MPRIS、LRCLIB 或 SQLite。
- 不修改 `/usr/share/dde-shell/org.deepin.ds.dock/` 下的 Dock 源码或布局文件。
- 不提供拖拽排序、左右区域切换或自定义 Dock 区域。当前 `dockOrder: 24` 是不改宿主源码时的右侧固定位置。

## 插件与桥接

```text
dock-applet/
  lyricsdockapplet.h/.cpp       DApplet、工厂注册、QML context
  lyricsdockviewmodel.h/.cpp    QDBusInterface、状态转换、重连
  package/metadata.json
  package/main.qml
  package/qml/LyricBar.qml
  package/qml/LyricsPopup.qml
```

- `lyricsdockapplet.cpp` 必须包含 `<pluginfactory.h>`，以 `D_APPLET_CLASS(LyricsDockApplet)` 注册，并紧随 `#include "lyricsdockapplet.moc"`。
- ViewModel 首次连接与服务所有者变化时调用 `GetState`；信号只用于后续增量更新。
- D-Bus 服务不可用时显示紧凑等待状态，并在服务出现后自动恢复；不得在 QML 里不断轮询 D-Bus。

## 布局与交互

| 项 | 规定 |
|---|---|
| 区域 | 右侧，`dockOrder: 24` |
| 可见宽度 | 默认 280 px；受 Token 约束在 220-360 px 内 |
| 视觉高度 | 固定 36 px，并相对 `Panel.rootObject.dockSize` 垂直居中 |
| 当前歌词 | 12 px，中等字重，单行省略，不改变容器宽度 |
| 第二行 | 11 px，显示下一句；MVP 不显示虚构翻译 |
| 关闭按钮 | 16 px DCI/主题关闭图标，28 px 命中区域，带无障碍名称 |
| 进度 | 只对当前行使用 `lineProgress` 做裁切/填充；不可按字切分 |

`LyricsReady` 显示两行歌词。`LookingUpLyrics`、`WaitingForPlayer`、`NoLyrics`、`Error` 显示单行简短状态，不展示陈旧歌词。`sessionHidden=true` 时 Applet 的可见宽度为零且不接收指针事件。

点击歌词区域打开 `PanelPopup`：前一句、当前句、下一句、来源 `LRCLIB`、时间能力说明和“打开设置”图标。Popup 不显示原始 API 错误。键盘焦点、Esc 关闭和图标工具提示为必需交互。

## 验收标准

1. `dde-shell -p org.deepin.ds.lyrics-dock` 能找到并实例化插件；运行日志无 QML import、工厂注册或元数据父级错误。
2. 下、上、左、右 Dock 位置以及亮暗主题下，视觉容器始终垂直居中，不与同侧托盘/时钟重叠。
3. 切歌、暂停、继续、拖动进度后，Applet 仅渲染 S03 新帧；网络断开不影响 Shell 进程稳定性。
4. 点击关闭后本会话不显示且不占 Dock 空间；重启 daemon 后自动恢复，设置页也可立即恢复。
5. 长中文、拉丁文和 RTL 文本不溢出容器，图标按钮保持 28 px 命中范围。

## 实现记录

- 新增 `LyricsDockViewModel` C++ 桥接层，通过 session D-Bus 异步消费 `GetState`、`StateChanged` 和 `FrameChanged`；QML 不直接访问或轮询 D-Bus。
- ViewModel 监听服务所有者变化，清理旧状态并在 daemon 重启后自动恢复。服务名早于对象注册时，使用 200 ms 间隔、最多 5 次的有限状态重试，并通过服务代次丢弃旧 daemon 的延迟回复。
- `SetSessionHidden` 使用乐观更新保持关闭操作即时响应；异步调用失败时恢复先前状态，服务已更换时不让旧回复覆盖新会话。
- `LyricsDockApplet` 暴露只读 `viewModel`，保留 `D_APPLET_CLASS` 工厂注册；插件元数据使用 `org.deepin.ds.dock` 父级，包入口位于 `main.qml`。
- `LyricBar` 在 `dockOrder: 24` 的右侧区域显示当前句、下一句和逐行进度裁切；关闭按钮调用会话隐藏。上下 Dock 使用水平布局，左右 Dock 旋转内部歌词条，固定尺寸不会因文本或状态改变而移动布局。
- `LyricsPopup` 显示前一句、当前句、下一句、LRCLIB 来源和时间能力，并提供设置图标、工具提示、无障碍名称及 Esc 关闭。前一句仅在同一曲目行索引自然递增时由 ViewModel 推导。
- 新增真实临时 session D-Bus 测试，覆盖首次状态、帧更新、服务端收到隐藏调用、服务退出/重启恢复及“先注册服务名、后注册对象”的启动竞态；插件测试覆盖动态工厂实例化、`viewModel` 属性、元数据和 QML 包契约。
- 验证命令：`cmake -S . -B build-s06-verify -DCMAKE_BUILD_TYPE=Debug`、`cmake --build build-s06-verify -j2`、`ctest --test-dir build-s06-verify --output-on-failure`；12 项测试全部通过。
- 构建目录隔离执行 `dde-shell -p org.deepin.ds.lyrics-dock` 可成功创建插件和 QML 根对象，无 import、工厂或根对象创建错误。子 Applet 没有真实 Dock 父 Panel 时，宿主 `PanelPopup.qml` 会报告定位器为 `undefined`；组合加载真实 Dock 又会与当前桌面占用的 `/run/user/1000/dockplugin.lock` 冲突，因此未终止用户 Dock 进行该项替代验证。
