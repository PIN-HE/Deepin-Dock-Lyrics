# 005 缓存键含原始时长导致穿透 / Raw Duration in Cache Key Causes Misses

| 项 / Field | 值 / Value |
|---|---|
| 严重度 / Severity | 中 / Medium |
| 状态 / Status | 待修复 / Open |
| 影响面 / Impact | 上报时长存在抖动的播放器 / Players whose reported duration jitters |
| 位置 / Location | [common/lyrics-core/src/normalization.cpp:30](../common/lyrics-core/src/normalization.cpp#L30) |
| 发现日期 / Found | 2026-08-13 |

## 症状 / Symptom

**中文**

同一首歌重复播放时可能重复发起网络查询，未命中已有缓存。表现为切歌后仍有短暂"正在查找歌词"，以及 LRCLIB 请求量高于预期。

**English**

Replaying the same track can trigger a fresh network query instead of hitting the existing cache. It surfaces as a brief "Looking up lyrics" after a track change and as more LRCLIB requests than expected.

## 复现依据 / Reproduction Evidence

**中文**

`makeTrackKey` 将原始毫秒时长直接拼入缓存键：

**English**

`makeTrackKey` concatenates the raw millisecond duration straight into the cache key:

```cpp
// normalization.cpp:16-32
QString makeTrackKey(const TrackIdentity &track)
{
    constexpr QChar fieldSeparator(0x1f);
    constexpr QChar artistSeparator(0x1e);
    // ...
    return QStringList {
        normalizeText(track.title),
        artists.join(artistSeparator),
        normalizeText(track.album),
        QString::number(track.durationMs),   // ← 原始毫秒 / raw milliseconds
    }.join(fieldSeparator);
}
```

**中文**

`durationMs` 来自 MPRIS `mpris:length` 除以 1000（[mprisplayerdiscovery.cpp:81-84](../daemon/lyrics-dockd/mprisplayerdiscovery.cpp#L81-L84)）。播放器上报值若相差 1 微秒即导致毫秒值不同，键随之不同，缓存穿透。

本机实测网易云上报 `mpris:length = 280680000`（微秒），换算 280680 ms。该值在同一曲目内是否绝对稳定**未验证**——需长时观察或多播放器对比才能确认抖动实际发生频率。因此本问题的实际触发率标注为未量化。

**English**

`durationMs` comes from MPRIS `mpris:length` divided by 1000 ([mprisplayerdiscovery.cpp:81-84](../daemon/lyrics-dockd/mprisplayerdiscovery.cpp#L81-L84)). If the player's reported value differs by even one microsecond, the millisecond value changes, the key changes, and the cache is missed.

Measured on this machine, NetEase reports `mpris:length = 280680000` microseconds, i.e. 280680 ms. Whether that value is absolutely stable across playbacks of the same track is **unverified** — confirming real-world jitter frequency would need extended observation or comparison across players. The actual trigger rate is therefore recorded as unquantified.

## 根因 / Root Cause

**中文**

缓存键的设计目标是标识"同一首歌"，但毫秒精度的时长远超该目标所需的粒度。时长在此处的作用是消歧（区分同名不同版本），1 秒粒度即足够；毫秒精度反而把噪声引入键。

同一份 `makeTrackKey` 还被用于 `LyricFrame.trackKey`（[lyricsservicecontroller.cpp:52](../daemon/lyrics-dockd/application/lyricsservicecontroller.cpp#L52)）与负缓存键（[lrcliblyricsadapter.cpp:177](../daemon/lyrics-dockd/infrastructure/lrcliblyricsadapter.cpp#L177)），因此缺陷影响正缓存、负缓存与帧标识三处。

**English**

The cache key exists to identify "the same song", but millisecond precision far exceeds the granularity that goal needs. Duration serves as a disambiguator here — separating same-titled versions — for which one-second granularity suffices; millisecond precision instead injects noise into the key.

The same `makeTrackKey` also feeds `LyricFrame.trackKey` ([lyricsservicecontroller.cpp:52](../daemon/lyrics-dockd/application/lyricsservicecontroller.cpp#L52)) and the negative-cache key ([lrcliblyricsadapter.cpp:177](../daemon/lyrics-dockd/infrastructure/lrcliblyricsadapter.cpp#L177)), so the defect touches the positive cache, the negative cache, and frame identity.

## 修复方案 / Fix Plan

**中文**

1. 键中时长改为 1 秒粒度取整：`QString::number(track.durationMs / 1000)`。
2. 时长缺失（`-1`）时写入固定占位符而非 `-1`，避免与合法值混淆——参见 [004](004-missing-duration-blocks-all-queries.md)，无时长曲目将成为常态。
3. **须处理缓存迁移。** 键格式变更会使既有缓存条目全部失效。可接受的做法：在缓存表中记录键版本号，读取时若版本不符则视为未命中并重新写入；或在升级时清空缓存表。不可静默保留旧条目——它们永不再被命中，只占空间。

**English**

1. Round duration to one-second granularity in the key: `QString::number(track.durationMs / 1000)`.
2. When duration is absent (`-1`), write a fixed placeholder rather than `-1` to avoid collision with legitimate values — see [004](004-missing-duration-blocks-all-queries.md), where duration-less tracks become routine.
3. **Cache migration must be handled.** Changing the key format invalidates every existing entry. Acceptable approaches: record a key version in the cache table and treat a version mismatch as a miss with a rewrite, or clear the cache table on upgrade. Silently keeping stale entries is not acceptable — they will never be hit again and merely occupy space.

## 验收标准 / Acceptance Criteria

**中文**

- 时长相差 1–999 ms 的同一曲目产生**相同**缓存键。
- 时长相差 ≥ 1 s 的曲目仍产生**不同**键（消歧能力不回归）。
- 时长为 `-1` 的曲目产生稳定键，且不与任何合法时长键碰撞。
- 键格式变更后，既有缓存不产生错误命中。
- 现有测试 `tests/test_lyricscore.cpp:39-40` 须继续通过；新增抖动用例。

**English**

- The same track with durations differing by 1–999 ms yields an **identical** cache key.
- Tracks differing by at least one second still yield **different** keys, so disambiguation does not regress.
- A track with duration `-1` yields a stable key that collides with no legitimate duration key.
- After the key format change, existing cache entries produce no false hits.
- The existing test at `tests/test_lyricscore.cpp:39-40` must keep passing; add a jitter case.

## 相关问题 / Related Issues

- [004](004-missing-duration-blocks-all-queries.md) — 无时长曲目将成为常态，键中的占位符处理与其相关 / Duration-less tracks become routine, making the placeholder handling relevant
