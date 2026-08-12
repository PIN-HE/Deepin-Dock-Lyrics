# S02 MPRIS 播放器发现与播放快照

**状态：** 已完成（2026-08-12）

**前置：** S00
**后续：** S03

## 目标

在后台服务中可靠发现用户会话里可用的 MPRIS 播放器，并只跟踪用户选定播放器的曲目、播放状态与位置，为歌词同步提供单一可信快照。

## 范围

- 在 session bus 枚举以 `org.mpris.MediaPlayer2.` 开头的服务名。
- 监听 `org.freedesktop.DBus.NameOwnerChanged`，实时处理播放器启动、退出和重启。
- 读取 `org.mpris.MediaPlayer2.Player` 的 `Metadata`、`PlaybackStatus`、`Rate`，并调用 `Position` 获得位置。
- 监听 `org.freedesktop.DBus.Properties.PropertiesChanged`；播放中以 200 ms 周期采样 `Position`，暂停时停止高频采样。
- 通过歌曲元数据防抖，产出 `PlayerSnapshot` 与玩家可选列表。

## 非范围

- 不探测 Wine 窗口、PulseAudio 流、浏览器标签页或 Linux 之外的媒体控制协议。
- 不控制播放、暂停、切歌或音量。
- 不在此层进行歌词网络请求或缓存查询。

## 输入与输出

**输入：** session D-Bus MPRIS 服务；DConfig / S03 提供的已选 `playerBusName`。

**输出：**

```text
availablePlayersChanged(QList<PlayerDescriptor>)
snapshotChanged(PlayerSnapshot)
selectedPlayerAvailableChanged(bool)
trackChanged(TrackIdentity)
```

`PlayerDescriptor` 至少含 `busName`、`identity`、`desktopEntry`（可用时）与可用状态。设置页显示 `identity`，持久化时保存精确 `busName`，不以翻译后的显示名作为键。

## 规则

- `xesam:title` 为空、`xesam:artist` 为空或 `mpris:length` 不存在时，仍发布快照，但 `TrackIdentity` 标记为不可检索，不能请求 LRCLIB。
- `mpris:length` 从微秒转换为毫秒，非正值视为未知时长。
- `PlaybackStatus` 仅允许 `Playing`、`Paused`、`Stopped`；未知值降级为 `Stopped` 并记一条不含元数据的警告日志。
- 同一首歌元数据的短暂多次 `PropertiesChanged` 必须合并，400 ms 稳定后才发布 `trackChanged`。
- 播放器消失时立即停止位置采样、清空当前快照并发出不可用状态；不得继续使用旧位置推算歌词。
- 同时有多个播放器时，只有选中的一个可触发后续查询。未选择播放器时绝不猜测“当前活跃播放器”。

## 测试与验收

1. 通过可控 fake MPRIS 服务验证发现、出现、退出、重启、元数据变化、暂停、恢复和拖动进度。
2. 播放状态下位置在 250 ms 内更新；暂停 500 ms 后不再产生位置轮询。
3. 两个播放器同时运行时，切换选项前后只对应被选总线名发布快照。
4. 空标题、网络电台不稳定元数据和未知时长均不产生崩溃或歌词网络请求。

## 实现记录

- 新增独立的 `Lyrics::Mpris` 库，异步枚举 session bus、监听 `NameOwnerChanged` 和 `PropertiesChanged`，并跟踪用户明确选择的播放器。
- 播放时每 200 ms 读取 `Position`，暂停/停止后关闭轮询；`Seeked` 信号在暂停状态下也会立即刷新位置。
- 曲目元数据经过 400 ms 防抖后才发布 `trackChanged`。缺少标题、艺人或正时长时，`TrackIdentity.searchable=false`，供后续歌词层阻止联网。
- `mpris-player-discovery-test` 在临时 session D-Bus 中以独立连接模拟多个播放器，覆盖发现、切换、非选中播放器隔离、播放/暂停、拖动、曲目防抖、退出、同名重启和不完整元数据。
