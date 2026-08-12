# S05 LRC 解析与逐行同步引擎

**状态：** 已完成（2026-08-12）

**前置：** S00、S03、S04
**后续：** S06

## 目标

将 LRCLIB 返回的 `syncedLyrics` 转换为稳定的逐行时间轴，并结合 MPRIS 位置和用户同步偏移生成 `LyricFrame`。此规格定义“行内视觉进度”，不实现或暗示真实逐字时间戳。

## 范围

- 解析常见 LRC 时间标记 `[mm:ss]`、`[mm:ss.d]`、`[mm:ss.dd]`、`[mm:ss.ddd]`。
- 支持同一文本行带多个时间标记，并按时间顺序形成多个 `LyricLine`。
- 忽略 `[ti:]`、`[ar:]`、`[al:]`、`[by:]` 等元标签；保留 `[offset:]` 作为诊断信息，但 MVP 不自动应用来源偏移。
- 处理重复时间戳、空行、无效行和排序，生成不可变 `ParsedLyrics`。
- 依据播放位置生成当前行、下一句、当前行进度和 `Line` / `Plain` / `None` 时间能力。

## 非范围

- 不解析 YRC、QRC、KRC 或任何逐字格式。
- 不生成机器翻译；`translationText` 必须保持空字符串。
- 不把单行时段平均切分为字符或单词时间戳。

## 解析与帧规则

| 输入情况 | 输出 |
|---|---|
| 有有效时间标记和非空行 | `timingCapability=Line`，按时间生成帧 |
| 只有 `plainLyrics` | `timingCapability=Plain`，显示简短非同步文本或状态 |
| `syncedLyrics` 为空且 `plainLyrics` 为空 | `timingCapability=None`，进入无歌词状态 |
| 单行多个标记 | 为每个时间标记创建同文本的行 |
| 连续行时间相同 | 当前行取最后一个同时间行，进度为 `0` |
| 播放位置在首行前 | 当前文本为空，二级文本为首行 |
| 已过最后一行 | 当前文本为最后一行，行进度固定 `1` |

同步公式固定为：

```text
effectivePositionMs = max(0, playerPositionMs + userOffsetMs)
lineProgress = clamp((effectivePositionMs - current.startMs)
                     / (next.startMs - current.startMs), 0, 1)
```

`userOffsetMs` 为正表示歌词相对播放器**提前**显示，为负表示延后显示。设置页必须使用这个明确含义，不能只显示歧义的“偏移”。若下一行不存在或时差不大于零，进度规则按表格处理。

## 输出约束

- `secondaryText` 在 `Line` 模式下为下一句；没有下一句时为空。
- 当前与下一句文本要在清理后去掉控制字符，但保留正常的中文、日文、韩文及其他 Unicode 文本。
- 在同一 `trackKey` 下，位置向前拖动、暂停、恢复和用户改变偏移时都必须重新计算，不能沿用先前行索引。
- 每次帧计算是纯函数；定时器位于 S03，解析器不持有 D-Bus、播放器或网络对象。

## 验收标准

1. fixture 覆盖中文、英文、多时间标记、毫秒精度、元标签、无效行、相同时间戳、首尾边界。
2. 给定 LRC 与位置的测试结果可精确断言当前文本、下一句、行索引和 `lineProgress`。
3. 拖动到任意位置后下一次帧更新不晚于 250 ms，且显示的行与该位置一致。
4. 代码与 UI 文案均不使用“逐字同步”“逐字高亮”描述 `Line` 模式。

## 实现记录

- `lyrics-core` 新增无状态的 `parseLyrics()` 与 `frameAt()`：解析器不持有网络、D-Bus 或播放器对象，同步器每次按当前位置二分查找，不沿用旧行索引。
- 解析覆盖 1/2/3 位小数、单行多时间标记、元标签、重复时间戳、排序和控制字符清理；`[offset:]` 仅写入 `sourceOffsetMs` 供诊断，不参与帧计算。
- `LyricsServiceController` 保存当前解析结果，并在 MPRIS 位置、暂停/恢复、Seek 和用户偏移变化时重新计算；相同帧会被去重，曲目、播放器和可用状态变化时清理旧帧。
- `Plain` 模式显示清理后的第一条非空文本，`None` 模式进入无歌词状态；`translationText` 始终为空。
- 新增 `complex.lrc` fixture、解析器测试、同步器边界测试和控制器集成测试。MPRIS 位置轮询为 200 ms，满足帧更新不晚于 250 ms 的要求。
- 本规格仍仅消费 S04 的 LRCLIB 载荷；Lyricify、SMTC、平台歌曲 ID 和繁简转换不属于本次实现。
