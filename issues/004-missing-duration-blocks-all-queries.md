# 004 缺少时长导致完全不发起查询 / Missing Duration Blocks All Queries

| 项 / Field | 值 / Value |
|---|---|
| 严重度 / Severity | 高 / High |
| 状态 / Status | 待修复 / Open |
| 影响面 / Impact | 不上报 `mpris:length` 的播放器，本机实测含 QQ音乐 / Players that omit `mpris:length`; QQ Music confirmed on this machine |
| 位置 / Location | [daemon/lyrics-dockd/mprisplayerdiscovery.cpp:88-93](../daemon/lyrics-dockd/mprisplayerdiscovery.cpp#L88-L93)、[lrcliblyricsadapter.cpp:48-51](../daemon/lyrics-dockd/infrastructure/lrcliblyricsadapter.cpp#L48-L51)、[lrclibprovider.cpp:82-85](../daemon/lyrics-dockd/infrastructure/lrclibprovider.cpp#L82-L85) |
| 发现日期 / Found | 2026-08-13 |

## 症状 / Symptom

**中文**

使用某些播放器时歌词功能完全无响应——不显示歌词，也不显示错误。与"匹配不准"不同，这是功能不工作：**一次网络请求都不会发出**。

**English**

With certain players the lyric feature is entirely inert — no lyrics and no error. Unlike a poor match, this is a non-functional path: **not a single network request is issued**.

## 复现依据 / Reproduction Evidence

**中文**

本机 2026-08-13 实测两个 MPRIS 播放器的元数据。两者均为 Electron 应用，总线名同为 `chromium.instance<pid>` 格式，但暴露字段差异显著：

**English**

Measured the metadata of both MPRIS players on this machine on 2026-08-13. Both are Electron applications registering as `chromium.instance<pid>`, yet they expose markedly different fields:

| 播放器 / Player | 总线名 / Bus name | 进程 / Process |
|---|---|---|
| 网易云 / NetEase | `org.mpris.MediaPlayer2.chromium.instance921757` | `open-orpheus` |
| QQ音乐 / QQ Music | `org.mpris.MediaPlayer2.chromium.instance2599990` | `qqmusic` |

```
网易云 / NetEase — 六字段齐全 / all six fields present
  mpris:artUrl   = file:///tmp/.org.chromium.Chromium.kEasM8
  mpris:length   = 280680000
  mpris:trackid  = /org/chromium/MediaPlayer2/TrackList/TrackB340D2CF...
  xesam:title    = 爱情讯息
  xesam:artist   = [郭静]
  xesam:album    = 下一个天亮

QQ音乐 / QQ Music — 仅三字段 / only three fields
  xesam:title    = キボウノカケラ (希望的碎片)
  xesam:artist   = [ボイジャー]
  xesam:album    = ""              ← 空 / empty
  mpris:length   = 缺失 / MISSING
  mpris:artUrl   = 缺失 / MISSING
  xesam:url      = 缺失 / MISSING
```

## 根因 / Root Cause

**中文**

三处串联的时长依赖，任一处即可阻断：

**English**

Three chained duration dependencies; any one of them blocks the path:

**1. `searchable` 判定 / The `searchable` predicate**

```cpp
// mprisplayerdiscovery.cpp:88-93
track.searchable = !track.title.isEmpty()
    && !track.artists.isEmpty()
    && std::any_of(...)
    && track.durationMs > 0;     // ← 缺 mpris:length 即 false
```

**2. 适配器短路 / Adapter short-circuit**

```cpp
// lrcliblyricsadapter.cpp:48-51
if (!track.searchable || track.durationMs <= 0) {
    emit noLyrics();             // ← 直接返回，不查询
    return;
}
```

**3. 精确查询前置校验 / Exact-query precondition**

```cpp
// lrclibprovider.cpp:82-85
if (track.durationMs <= 0) {
    callback(errorResult(..., QStringLiteral("track-not-searchable")));
    return;
}
```

**中文**

注释写明「LRCLIB 精确匹配依赖标题、艺人和时长，缺少任一项都不应联网」。该判断对 `/api/get` 成立——LRCLIB 的精确接口确实需要 duration。但它被错误地应用为**全局前置条件**，连不需要 duration 的 `/api/search` 也一并阻断。

实测确认 `/api/search` 支持无 duration 的关键词检索：

**English**

The comment states that LRCLIB exact matching needs title, artist, and duration, so no request should go out without all three. That is true for `/api/get` — the exact endpoint does require duration. But it is wrongly applied as a **global precondition**, also blocking `/api/search`, which needs no duration.

Measurements confirm `/api/search` supports keyword lookup without duration:

```
GET /api/search?q=夜曲 周杰伦        → HTTP 200, 返回结果 / results returned
GET /api/search?track_name=夜曲&artist_name=周杰伦  → HTTP 200
```

**中文**

`q=` 关键词检索在当前代码中完全未被使用（[lrclibprovider.cpp:97-106](../daemon/lyrics-dockd/infrastructure/lrclibprovider.cpp#L97-L106) 的 `search()` 只用结构化参数）。

**English**

The `q=` keyword search is entirely unused in the current code: `search()` at [lrclibprovider.cpp:97-106](../daemon/lyrics-dockd/infrastructure/lrclibprovider.cpp#L97-L106) uses structured parameters only.

## 修复方案 / Fix Plan

**中文**

1. **拆分 `searchable` 为两级能力判定。** 引入 `exactSearchable`（需 title + artist + duration）与 `keywordSearchable`（仅需 title + artist）。`mpris:length` 缺失时后者仍为真。

2. **适配器按能力分派。** `exactSearchable` 为真走 `/api/get`；仅 `keywordSearchable` 为真时跳过 `/api/get`，直接进入候选搜索。

3. **`LRCLIBProvider::search()` 增加 `q=` 关键词模式。** 作为结构化检索之后的降级层，见 [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §6 的 V4 变体。

4. **无 duration 时提高自动采纳阈值至 0.92。** 失去时长这一维度后，误匹配风险上升，须以更严格的阈值补偿；未达标则弹候选。

5. **考虑从其他来源补全 duration。** `webdb.dat` 索引可提供权威时长（见 [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §7），但该源默认关闭，不能作为本问题的唯一解。

**English**

1. **Split `searchable` into two capability levels.** Introduce `exactSearchable` (needs title, artist, duration) and `keywordSearchable` (needs title and artist only). The latter stays true when `mpris:length` is absent.

2. **Dispatch by capability in the adapter.** When `exactSearchable` holds, use `/api/get`; when only `keywordSearchable` holds, skip `/api/get` and go straight to candidate search.

3. **Add a `q=` keyword mode to `LRCLIBProvider::search()`.** It serves as the tier below structured search — variant V4 in [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §6.

4. **Raise auto-accept to 0.92 when duration is absent.** Losing that dimension raises mismatch risk, so compensate with a stricter bar and prompt for candidates when it is not met.

5. **Consider filling duration from another source.** The `webdb.dat` index can supply an authoritative duration (see [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §7), but that source defaults to off and cannot be the sole remedy here.

## 验收标准 / Acceptance Criteria

**中文**

- QQ音乐播放曲目时发出候选搜索请求，且能显示歌词或候选列表。
- 网易云行为不回归——仍优先走 `/api/get` 精确路径。
- 无 title 或无 artist 时仍不发起任何请求（该前置条件保持有效）。
- 无 duration 的自动采纳须达 0.92，否则弹候选。
- 新增 `tests/test_mprisplayerdiscovery.cpp` 用例：缺 `mpris:length` 的元数据产生 `keywordSearchable == true`、`exactSearchable == false`。
- 新增 `tests/test_lrclibprovider.cpp` 用例：无 duration 时 `search()` 使用 `q=` 参数且不调用 `/api/get`。

**English**

- With QQ Music playing, a candidate search request is issued and either lyrics or a candidate list appears.
- NetEase does not regress — it still prefers the `/api/get` exact path.
- With no title or no artist, still no request is issued (that precondition remains).
- Auto-accept without duration requires 0.92, otherwise prompt for candidates.
- Add a case to `tests/test_mprisplayerdiscovery.cpp`: metadata lacking `mpris:length` yields `keywordSearchable == true` and `exactSearchable == false`.
- Add a case to `tests/test_lrclibprovider.cpp`: without duration, `search()` uses the `q=` parameter and does not call `/api/get`.

## 备注 / Notes

**中文**

QQ音乐的标题 `キボウノカケラ (希望的碎片)` 同时踩中 [001](001-cjk-text-score-always-zero.md)：假名与括号中译名的组合按空白切词后仅得两个 token，与任何写法不同的记录均得极低分。因此仅修复本问题不足以让 QQ音乐可用，还需 001。

**English**

QQ Music's title `キボウノカケラ (希望的碎片)` also triggers [001](001-cjk-text-score-always-zero.md): kana plus a bracketed Chinese translation splits into just two tokens, scoring near zero against any differently-written record. Fixing this issue alone is therefore insufficient for QQ Music; 001 is also required.

## 相关问题 / Related Issues

- [001](001-cjk-text-score-always-zero.md) — QQ音乐的日文标题同时受其影响 / QQ Music's Japanese title is also affected
- [006](006-album-param-always-sent.md) — QQ音乐的 album 为空，触发该问题 / QQ Music's empty album triggers that issue
