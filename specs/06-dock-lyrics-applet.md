# S06 Dock 右侧歌词 Applet

**状态：** 待实施

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
