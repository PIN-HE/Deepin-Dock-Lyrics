# S07 DTK6 设置应用

**状态：** 已完成（2026-08-13）

**前置：** S00、S01、S03、S04
**后续：** S08、S09

## 目标

提供一个简洁的 DTK6 设置应用，让用户明确启动任务栏歌词、选择当前运行并暴露 MPRIS 的播放器、校正同步偏移，并在匹配不确定时确认 LRCLIB 候选项。

## 范围

- 使用 Qt 6 + DTK6 Widgets 创建可执行文件 `deepin-lyrics-settings`。
- 第一次打开时呈现常规应用窗口，以“启动任务栏歌词”为主操作；启用后显示设置内容。
- 消费 S03 的 D-Bus 状态与方法，不直接读写歌词 SQLite 或请求 LRCLIB。
- 使用 DTK 原生开关、组合框、SpinBox、列表、图标按钮和确认对话框。
- 提供清除缓存和恢复会话显示的明确操作。

## 非范围

- 不实现音乐播放控制、音乐库、歌单、账号登录或第三方平台授权。
- 不把播放器枚举伪装为所有已安装桌面应用；只展示 S02 发现的 MPRIS 服务。
- 不在此规格实现多语言翻译歌词。

## 页面与操作

| 区域 | 内容 | D-Bus 操作 |
|---|---|---|
| 首次/停用状态 | 标题、简短状态、主按钮“启动任务栏歌词” | `SetEnabled(true)`、`SetSessionHidden(false)` |
| 启用开关 | 启用或暂停整个功能 | `SetEnabled(bool)` |
| 播放器 | 已发现 MPRIS 播放器下拉项、不可用提示 | `SetPlayer(busName)` |
| 当前状态 | 播放器、歌曲、服务状态、来源/时间能力 | 读取 `GetState` / `StateChanged` |
| 同步校正 | `-10000..10000 ms` SpinBox，标签“同步偏移（+提前，-延后）” | `SetOffsetMs(int)` |
| 匹配结果 | 当前确认记录或“未找到”；低置信度时展示候选列表 | `SearchCandidates()`、`SelectCandidate()` |
| 隐藏恢复 | “在 Dock 中显示歌词” | `SetSessionHidden(false)` |
| 隐私与缓存 | 说明仅向 LRCLIB 发送歌曲元数据；清除按钮 + 二次确认 | `ClearCache()` |

候选项必须显示标题、艺人、专辑（有值时）、时长和“可能匹配”状态，按评分降序。用户单击一个候选后，界面进入等待状态，直到 daemon 取得确认记录并发布新状态。不得先在 UI 本地假定选择成功。

## 视觉与可用性约束

- 使用 `DApplication` 初始化、在创建窗口前加载翻译、使用 DTK 主题图标和 S01 语义 Token；不得混用 Qt5/DTK5。
- 设置窗口采用紧凑但可扫描的表单布局，不将整页做成嵌套卡片。
- 纯文本歌词不显示进度调节以外的“同步准确”承诺；`Line` 模式文案为“逐行同步”。
- 所有异步操作具备禁用态或活动指示，网络/服务错误显示人类可理解的本地化文案，不显示 D-Bus 或 HTTP 原文。
- 支持键盘 Tab 顺序、焦点可见性和屏幕阅读器名称。

## 验收标准

1. 未启动 daemon、未发现播放器、播放器退出、无歌词、限流和网络失败均可在界面中恢复，不崩溃、不出现技术错误正文。
2. 点击启动后 Dock 在服务准备完成时出现；关闭后重新打开设置并点击恢复，Dock 立即重新显示。
3. 选定播放器、偏移值和启用开关在应用重启后保持；会话隐藏状态不持久化。
4. 低置信度候选必须由用户确认，自动路径只会采用 S04 定义的高置信度结果。
5. 清除缓存有二次确认，完成后 UI 重新读取状态，且不会清除 DConfig 中的播放器选择。

## 实现记录

- 新增独立 `Lyrics::SettingsViewModel`，异步消费 `GetState`、`StateChanged`、`FrameChanged` 和 `CandidatesChanged`，统一处理服务上下线、启动竞态、嵌套 D-Bus 列表解码、忙碌态及稳定错误码；设置窗口不直接访问 LRCLIB 或 SQLite。
- `deepin-lyrics-settings` 使用 `DApplication`、`DMainWindow`、`DSwitchButton`、`DComboBox`、`DSpinBox`、`DSuggestButton`、`DWarningButton` 和 `DDialog`。停用态展示启动主操作；启用后提供运行摘要、播放器、同步偏移、候选确认、Dock 恢复及带二次确认的缓存清理。
- S03 状态快照向后兼容地增加 `lyricsSource`、`timingCapability` 和 `candidates`，确保设置应用晚于候选信号启动或歌曲暂停时仍能恢复完整状态。候选在窗口侧按评分稳定降序，选择后等待 daemon 发布新状态，不做本地成功假设。
- S01 增加设置页间距与选中表面 Token；设置窗口使用 DTK 调色板和主题图标，没有硬编码 UI 色值。中英文文案通过 `tr()` 与内嵌 `zh_CN` QM 提供，翻译在窗口创建前加载。
- 新增临时 session D-Bus ViewModel 测试和离屏窗口测试，覆盖 daemon 缺席、服务操作、播放器/候选嵌套解码、主要控件、偏移范围及暗色调色板渲染。真实中文桌面验证显示 `open-orpheus` 播放器、当前歌曲、LRCLIB 来源和逐行同步状态，无文字重叠。
- 验证命令：`cmake -S . -B build-s07-verify -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr`、`cmake --build build-s07-verify -j2`、`ctest --test-dir build-s07-verify --output-on-failure`；15 项测试全部通过。`DESTDIR=/tmp/spec007-install cmake --install build-s07-verify` 确认设置应用安装到 `/usr/bin/deepin-lyrics-settings`。
