# 003 艺名写法差异导致候选被丢弃 / Artist-Name Variance Drops Candidates

| 项 / Field | 值 / Value |
|---|---|
| 严重度 / Severity | 高 / High |
| 状态 / Status | 待修复 / Open |
| 影响面 / Impact | 艺名存在中英/罗马音/别名写法差异的曲目 / Tracks whose artist has CJK-Latin, romanised, or alias variants |
| 位置 / Location | [common/lyrics-core/src/normalization.cpp:92](../common/lyrics-core/src/normalization.cpp#L92)、[normalization.cpp:79-82](../common/lyrics-core/src/normalization.cpp#L79-L82) |
| 发现日期 / Found | 2026-08-13 |

## 症状 / Symptom

**中文**

部分曲目连手动搜索也找不到——候选列表为空。用户已知 LRCLIB 上存在该歌词（歌词正文一致），仅艺名写法不同。现象呈间歇性：同类曲目有时能搜到，有时搜不到。

**English**

For some tracks even manual search returns nothing — the candidate list is empty. The user knows the lyrics exist on LRCLIB (the lyric body matches) and only the artist name is written differently. The behaviour appears intermittent: similar tracks sometimes appear and sometimes do not.

## 复现依据 / Reproduction Evidence

**中文**

将完整评分逻辑抽出编译运行，标题与时长均精确匹配、仅艺名不同的情形：

**English**

Extracted the full scoring logic, compiled, and ran it for cases where title and duration match exactly and only the artist differs:

```
艺人不同 + album 匹配 + 时长精确    0.700   在候选列表内 / in candidate list
艺人不同 + album 空   + 时长精确    0.600   恰在边界 / exactly at the boundary
艺人不同 + album 空   + 时长差 3s   0.581   *** 被丢弃 / DROPPED ***
艺人不同 + album 空   + 时长差 6s   0.525   *** 被丢弃 / DROPPED ***
艺人不同 + 无时长（QQ音乐）          0.450   *** 被丢弃 / DROPPED ***
```

**中文**

0.60 这个值的构成恰好等于过滤线：

**English**

The 0.60 value is composed exactly at the filter line:

```
0.45 × title(1.0) + 0.30 × artist(0.0) + 0.10 × album(0.0) + 0.15 × duration(1.0) = 0.6000
过滤条件 / filter: if (score < 0.60) continue;
```

**中文**

罗马音与别名同属此机制：

**English**

Romanised forms and aliases follow the same mechanism:

```
五月天  vs Mayday      0.600   边界 / boundary
ボイジャー vs Voyager    0.600   边界 / boundary
周杰伦  vs 周杰伦, 费玉清  0.800   通过（子集匹配奏效）/ passes (subset match works)
```

**中文**

关键对照——区分度严重不足：

**English**

The critical control — discrimination is dangerously weak:

| 情形 / Situation | 得分 / Score | 结果 / Outcome |
|---|---|---|
| 对的歌，艺名写法不同 / Right song, artist written differently | 0.600 | 通过过滤 / passes filter |
| 不相干的歌，艺名正确 / Unrelated song, artist correct | 0.550 | 被丢弃 / dropped |

**中文**

两者仅差 0.05。"正确匹配"与"完全不相干"在评分上几乎无法区分。

**English**

A margin of just 0.05 separates them. A correct match and a completely unrelated record are nearly indistinguishable by score.

## 根因 / Root Cause

**中文**

三个因素叠加：

1. **艺人权重过高相对于过滤线。** 艺人占 0.30（[normalization.cpp:81](../common/lyrics-core/src/normalization.cpp#L81)），艺名不匹配即扣满 0.30，剩余理论最高分 0.70。过滤线 0.60 距其仅 0.10，容错空间被压缩到几乎为零。

2. **过滤线与 title+duration 满分恰好相等。** `0.45 + 0.15 = 0.60`，等于 `if (score < 0.60) continue` 的边界。任何额外扣分——album 为空、时长偏差、时长缺失——都会将正确匹配推到线下。

3. **艺名无别名归一。** `artistScore` 仅对每个艺人调用 `textScore` 取最大值，无中英对照、无罗马音映射。叠加 [001](001-cjk-text-score-always-zero.md) 的 CJK 缺陷后，`周杰伦` vs `周杰倫` 也得 0 分。

**English**

Three factors compound:

1. **Artist weight is too heavy relative to the filter line.** Artist carries 0.30 ([normalization.cpp:81](../common/lyrics-core/src/normalization.cpp#L81)); a mismatch forfeits all of it, capping the theoretical maximum at 0.70. The 0.60 filter sits just 0.10 below, compressing tolerance to almost nothing.

2. **The filter line exactly equals a perfect title-plus-duration score.** `0.45 + 0.15 = 0.60`, precisely the `if (score < 0.60) continue` boundary. Any further deduction — empty album, duration drift, absent duration — pushes a correct match below the line.

3. **No artist alias normalisation.** `artistScore` merely calls `textScore` per artist and takes the maximum, with no CJK-Latin table and no romanisation mapping. Compounded by the CJK defect in [001](001-cjk-text-score-always-zero.md), even `周杰伦` versus `周杰倫` scores zero.

**中文**

用户观察到的"间歇性"由此解释：能否搜到取决于 album 与时长是否恰好对上，而非随机。

**English**

This explains the intermittency the user observed: whether a track is findable depends on whether album and duration happen to align, not on chance.

## 修复方案 / Fix Plan

**中文**

四项，按风险递增：

1. **降低候选过滤线至 0.45。** 过滤线的职责是"是否值得展示给用户"，应当宽松；严格性由自动采纳阈值承担。0.45 允许"标题满分、其余全失"的记录进入列表。

2. **艺人不匹配时重分配权重，而非直接扣满。** 若 `artistScore == 0` 但 `titleScore >= 0.9`，将艺人权重的一部分转移至标题——标题完全一致本身即强信号。目的是拉开"对的歌不同艺名"与"不相干的歌"的差距，使 0.05 的区分度扩大。

3. **艺名归一：先做低风险部分。** 大小写与分隔符归一、多艺人子集匹配（已部分奏效）、去除 `feat.` 等修饰。中英对照与罗马音映射需别名表，留待后续，不在本次范围。

4. **提高自动采纳阈值以补偿放宽的过滤线。** 过滤线放宽后，自动采纳须相应收紧，避免误采纳。参见 [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §5.3 的分档阈值表。

**English**

Four items in ascending risk order:

1. **Lower the candidate filter to 0.45.** The filter's job is deciding what is worth showing the user and should be permissive; strictness belongs to the auto-accept threshold. At 0.45, a record with a perfect title and nothing else still reaches the list.

2. **Reweight rather than fully deduct when the artist mismatches.** When `artistScore == 0` but `titleScore >= 0.9`, shift part of the artist weight onto title — an exact title is itself a strong signal. The goal is to widen the 0.05 margin between "right song, different artist spelling" and "unrelated song".

3. **Artist normalisation: low-risk portion only.** Case and separator normalisation, multi-artist subset matching (already partly effective), and stripping modifiers such as `feat.`. CJK-Latin tables and romanisation mapping need an alias table and are out of scope here.

4. **Raise the auto-accept threshold to offset the looser filter.** With a permissive filter, auto-accept must tighten to avoid false acceptance. See the tiered threshold table in [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §5.3.

**中文**

注意：本问题的部分症状会随 [001](001-cjk-text-score-always-zero.md) 修复而缓解（`周杰伦` vs `周杰倫` 归一后得 1.0，不再扣分）。但拉丁与 CJK 跨脚本的艺名差异（`五月天` vs `Mayday`）不受 001 影响，仍需本问题的第 1、2 项。

**English**

Note that some symptoms here ease once [001](001-cjk-text-score-always-zero.md) is fixed: `周杰伦` versus `周杰倫` scores 1.0 after normalisation and loses nothing. But cross-script artist variance such as `五月天` versus `Mayday` is untouched by 001 and still requires items 1 and 2 here.

## 验收标准 / Acceptance Criteria

| 用例 / Case | 当前 / Current | 期望 / Expected |
|---|---|---|
| 标题时长精确、艺名不同、album 空 / exact title+duration, artist differs, empty album | 0.600 边界 / boundary | 稳定进入候选列表 / reliably in candidate list |
| 同上 + 时长差 3s / same, +3s drift | 0.581 丢弃 / dropped | 进入候选列表 / in candidate list |
| 同上 + 无时长 / same, no duration | 0.450 丢弃 / dropped | 进入候选列表 / in candidate list |
| 不相干标题、艺名正确 / unrelated title, artist correct | 0.550 丢弃 / dropped | 仍被丢弃 / still dropped |
| 与正确匹配的区分度 / margin against a correct match | 0.05 | ≥ 0.15 |

**中文**

最后一行是本次修复的核心指标。若区分度未扩大，仅降低过滤线会引入大量噪声候选，使用户更难选择。新增用例至 `tests/test_lyricscore.cpp`。

**English**

The last row is the core metric for this fix. If the margin does not widen, lowering the filter alone injects noisy candidates and makes the user's choice harder. Add cases to `tests/test_lyricscore.cpp`.

## 相关问题 / Related Issues

- [001](001-cjk-text-score-always-zero.md) — 同一文件，建议合并修复 / Same file; fix together
- [002](002-exact-match-failure-reported-as-error.md) — 002 修复后用户才能到达候选列表，本问题决定列表是否为空 / After 002 the user reaches the list at all; this issue decides whether it is empty
- [007](007-candidate-list-lacks-lyric-preview.md) — 用户依据歌词正文确认匹配，元数据评分无法覆盖此判断 / Users confirm by lyric body, which metadata scoring cannot capture
