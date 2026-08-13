# 006 空 album 参数仍被发送 / Empty Album Parameter Still Sent

| 项 / Field | 值 / Value |
|---|---|
| 严重度 / Severity | 中 / Medium |
| 状态 / Status | 待修复 / Open |
| 影响面 / Impact | album 为空或含版本标记的曲目 / Tracks with empty or version-tagged albums |
| 位置 / Location | [daemon/lyrics-dockd/infrastructure/lrclibprovider.cpp:90](../daemon/lyrics-dockd/infrastructure/lrclibprovider.cpp#L90)、[lrclibprovider.cpp:103](../daemon/lyrics-dockd/infrastructure/lrclibprovider.cpp#L103) |
| 发现日期 / Found | 2026-08-13 |

## 症状 / Symptom

**中文**

部分曲目的精确查询返回了非最优记录，或返回的记录与不带 album 查询时不同。表现为歌词能显示但版本不对（如现场版对录音室版），或评分低于预期。

**English**

For some tracks the exact query returns a suboptimal record, or a different record than the same query without album. It surfaces as lyrics that display but for the wrong version — a live take instead of the studio recording — or as a lower score than expected.

## 复现依据 / Reproduction Evidence

**中文**

本机 2026-08-13 对 LRCLIB 实测三组查询。同一曲目，仅 album 参数不同：

**English**

Measured three query variants against LRCLIB on 2026-08-13. Same track, differing only in the album parameter:

```
A: track_name=夜曲 & artist_name=周杰伦 & album_name=十一月的蕭邦 & duration=229
   → HTTP 200, id=30067265, albumName="十一月的蕭邦"

B: track_name=夜曲 & artist_name=周杰伦 & duration=229          (无 album / no album)
   → HTTP 200, id=16978,    albumName="十一月的萧邦"

C: track_name=夜曲 & artist_name=周杰伦 & album_name= & duration=229   (空 album / empty album)
   → HTTP 200, id=16978,    albumName="十一月的萧邦"
```

**中文**

三点观察：

1. **A 与 B 命中不同记录**（id 30067265 vs 16978）。album 参数确实影响 LRCLIB 的匹配结果，不是无害的附加信息。
2. **B 与 C 命中相同记录**。LRCLIB 对空字符串 album 的处理等同于省略。此处当前行为无害，但依赖服务端的宽容处理而非显式契约。
3. A 命中的记录 `albumName` 为繁体，B 命中的为简体。二者是同一专辑的不同写法条目。

**English**

Three observations:

1. **A and B match different records** (id 30067265 versus 16978). The album parameter genuinely affects LRCLIB's matching and is not harmless extra context.
2. **B and C match the same record.** LRCLIB treats an empty-string album as omitted. The current behaviour is harmless here, but it relies on server leniency rather than an explicit contract.
3. A's record carries a Traditional `albumName` while B's is Simplified — two differently-written entries for the same album.

## 根因 / Root Cause

**中文**

`getExact()` 与 `search()` 均无条件添加 `album_name`，不检查其是否为空：

**English**

Both `getExact()` and `search()` add `album_name` unconditionally without checking for emptiness:

```cpp
// lrclibprovider.cpp:86-93 (getExact)
QUrlQuery query;
query.addQueryItem(QStringLiteral("track_name"), track.title);
query.addQueryItem(QStringLiteral("artist_name"), track.artists.join(QStringLiteral(", ")));
query.addQueryItem(QStringLiteral("album_name"), track.album);   // ← 无空值检查
query.addQueryItem(QStringLiteral("duration"), ...);

// lrclibprovider.cpp:99-104 (search) — 同样问题 / same problem
query.addQueryItem(QStringLiteral("album_name"), track.album);
```

**中文**

MPRIS 的 `xesam:album` 在实践中经常不可靠。本机实测 QQ音乐上报空字符串（见 [004](004-missing-duration-blocks-all-queries.md)）；其他播放器可能上报含版本标记的专辑名（如「2007世界巡回演唱会」）。此时 album 是**噪声而非信号**，参与匹配会降低命中质量。

现有评分逻辑中 album 权重为 0.10（[normalization.cpp:82](../common/lyrics-core/src/normalization.cpp#L82)），空 album 使该项恒为 0，间接压低总分——这与 [003](003-artist-name-variance-drops-candidates.md) 的过滤线问题叠加，是候选被丢弃的成因之一。

**English**

MPRIS `xesam:album` is often unreliable in practice. QQ Music reports an empty string on this machine (see [004](004-missing-duration-blocks-all-queries.md)); other players may report a version-tagged album such as "2007世界巡回演唱会". In those cases album is **noise, not signal**, and feeding it into matching lowers hit quality.

In the current scoring, album carries weight 0.10 ([normalization.cpp:82](../common/lyrics-core/src/normalization.cpp#L82)); an empty album pins that term to zero and indirectly depresses the total — compounding the filter-line problem in [003](003-artist-name-variance-drops-candidates.md) and contributing to dropped candidates.

## 修复方案 / Fix Plan

**中文**

1. **空值时省略参数。** `track.album.trimmed().isEmpty()` 为真时不调用 `addQueryItem`。适用于 `getExact()` 与 `search()` 两处。

2. **album 为空时重分配评分权重。** 将 0.10 转移至 title 与 artist，而非让该项恒为 0 拖低总分。参见 [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §5.3 的权重重分配表。

3. **将「省略 album」作为独立检索变体。** 即 [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §6 的 V2：带 album 的精确查询失败后，去掉 album 重试。实测 A 与 B 命中不同记录，说明此重试有实际价值。

**English**

1. **Omit the parameter when empty.** Skip `addQueryItem` when `track.album.trimmed().isEmpty()` holds, in both `getExact()` and `search()`.

2. **Reweight scoring when album is empty.** Shift the 0.10 onto title and artist rather than letting the term sit at zero and drag the total down. See the reweighting table in [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §5.3.

3. **Make "omit album" a distinct search variant.** This is variant V2 in [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §6: when the album-bearing exact query fails, retry without it. Since A and B demonstrably match different records, the retry carries real value.

## 验收标准 / Acceptance Criteria

**中文**

- `track.album` 为空或纯空白时，请求 URL 中不含 `album_name` 参数。
- `track.album` 非空时行为不变。
- album 为空时评分不因该项恒为 0 而低于同等条件下的非空 album 情形。
- V2 变体在 V1 失败后触发，且总请求数受上限约束（不得每首歌都发四次请求）。
- 新增 `tests/test_lrclibprovider.cpp` 用例：空 album 的 `TrackIdentity` 产生不含 `album_name` 的 URL。

**English**

- When `track.album` is empty or whitespace, the request URL contains no `album_name` parameter.
- Behaviour is unchanged when `track.album` is non-empty.
- With an empty album, the score is not lower than the equivalent non-empty case merely because the term is pinned to zero.
- Variant V2 fires after V1 fails, with total request count bounded — not four requests for every track.
- Add a case to `tests/test_lrclibprovider.cpp`: a `TrackIdentity` with an empty album produces a URL without `album_name`.

## 相关问题 / Related Issues

- [004](004-missing-duration-blocks-all-queries.md) — QQ音乐同时缺时长与 album / QQ Music lacks both duration and album
- [003](003-artist-name-variance-drops-candidates.md) — 空 album 压低总分，是候选被丢弃的成因之一 / An empty album depresses the total and contributes to dropped candidates
