# 002 精确匹配未达阈值被报为错误 / Sub-Threshold Exact Match Reported as Error

| 项 / Field | 值 / Value |
|---|---|
| 严重度 / Severity | 高 / High |
| 状态 / Status | 待修复 / Open |
| 影响面 / Impact | 所有评分未达 0.85 或时长差超 2s 的曲目 / Any track scoring below 0.85 or drifting over 2s |
| 位置 / Location | [daemon/lyrics-dockd/infrastructure/lrcliblyricsadapter.cpp:117](../daemon/lyrics-dockd/infrastructure/lrcliblyricsadapter.cpp#L117) |
| 发现日期 / Found | 2026-08-13 |

## 症状 / Symptom

**中文**

设置界面显示"歌词暂时不可用"，措辞暗示网络故障或服务异常。但同一首歌通过手动搜索候选可以正常找到并显示歌词。用户无法从提示中得知应当手动搜索。

**English**

The settings window shows "Lyrics are temporarily unavailable", wording that implies a network or service fault. Yet the same track can be found and displayed through manual candidate search. Nothing in the message tells the user that manual search would work.

## 复现依据 / Reproduction Evidence

**中文**

文案链路：

**English**

Message chain:

```
LyricsPort::failed("provider-failed")
  → LyricsServiceController::onFailed        lyricsservicecontroller.cpp:371
  → setStatus(ServiceStatus::Error, ...)
  → statusName() → "Error"
  → SettingsWindow::statusText("Error")      settingswindow.cpp:522
  → tr("Lyrics are temporarily unavailable") → 歌词暂时不可用
```

**中文**

自动路径与手动路径在同一条件下行为不同：

**English**

The automatic and manual paths behave differently under identical conditions:

| 路径 / Path | 入口 / Entry | 评分未达阈值时 / When below threshold |
|---|---|---|
| 自动 / Automatic | `beginExact()` | `emit failed("provider-failed")` → 状态 `Error` / status `Error` |
| 手动 / Manual | `searchCandidates()` → `handleCandidates()` | `emit candidatesChanged(...)` → 状态 `NeedsCandidateSelection` |

**中文**

即：同一首歌、同样的歌词、同样的评分，自动路径报"不可用"，手动路径给出候选列表供选择。

**English**

That is: the same track, the same lyrics, the same score — the automatic path reports "unavailable" while the manual path offers a candidate list.

## 根因 / Root Cause

**中文**

`beginExact()` 在 `/api/get` **成功返回歌词**后，若本地校验未通过，直接发出失败信号，而不是降级到候选搜索：

**English**

After `/api/get` **successfully returns lyrics**, `beginExact()` emits a failure signal when local validation does not pass, instead of degrading to candidate search:

```cpp
if (result.kind == ProviderResultKind::Success) {
    // ... instrumental / timing checks ...
    const double score = scoreLyricCandidate(track, result.record);
    const bool durationEligible = std::abs(track.durationMs - result.record.durationMs) <= 2000;
    if (score >= 0.85 && durationEligible) {
        // store and emit lyricsReady
        return;
    }
    emit failed(QStringLiteral("provider-failed"));   // ← 缺陷所在 / the defect
    return;
}
if (result.kind == ProviderResultKind::NotFound) {
    beginCandidateSearch(track, generation);          // ← 仅 404 才降级 / degrades only on 404
    return;
}
```

**中文**

降级到候选搜索的路径只在 HTTP `404` 时触发。评分不足与时长偏差被归类为**错误**，而它们实际上是**需要用户确认**的正常情形。

此外 `provider-failed` 这个错误码语义也不准确——歌词源工作正常，是本地判定拒绝了结果。

**English**

The degradation path fires only on HTTP `404`. Insufficient score and duration drift are classified as **errors**, when they are in fact the normal case of **needing user confirmation**.

The error code `provider-failed` is also semantically wrong: the provider worked correctly; local validation rejected the result.

## 修复方案 / Fix Plan

**中文**

1. 将 `emit failed("provider-failed")` 替换为 `beginCandidateSearch(track, generation)`，使评分不足时降级到候选搜索而非报错。
2. 保留 instrumental 与 `TimingCapability::None` 的负缓存分支不变——那些确实是"无歌词"，非"匹配不确定"。
3. 校验 `beginCandidateSearch` 在同一 `generation` 内被调用两次不会重复请求（`isCurrent()` 已有 generation 守卫，需确认覆盖此路径）。

**English**

1. Replace `emit failed("provider-failed")` with `beginCandidateSearch(track, generation)` so an insufficient score degrades to candidate search rather than an error.
2. Leave the instrumental and `TimingCapability::None` negative-cache branches unchanged — those genuinely mean "no lyrics", not "uncertain match".
3. Verify that calling `beginCandidateSearch` twice within the same `generation` does not duplicate requests. The `isCurrent()` generation guard exists; confirm it covers this path.

**中文**

此修复不改动评分算法，与 [001](001-cjk-text-score-always-zero.md) 和 [003](003-artist-name-variance-drops-candidates.md) 相互独立，可单独提交。修复后即使评分缺陷仍在，用户看到的也是"选择歌词匹配"而非"歌词暂时不可用"。

**English**

This fix touches no scoring logic and is independent of [001](001-cjk-text-score-always-zero.md) and [003](003-artist-name-variance-drops-candidates.md), so it can land on its own. Once fixed, even with the scoring defects still present, the user sees "Choose a lyric match" rather than "Lyrics are temporarily unavailable".

## 验收标准 / Acceptance Criteria

**中文**

- 精确查询返回歌词但评分 < 0.85 时，状态变为 `NeedsCandidateSelection`，不再是 `Error`。
- 精确查询返回歌词但时长差 > 2s 时，同上。
- instrumental 曲目仍进入负缓存并显示"未找到歌词"。
- 真实网络故障、`429`、数据库故障仍正确报 `Error`。
- 新增 `tests/test_lrcliblyricsadapter.cpp` 用例：低分精确结果触发候选搜索而非 `failed`。

**English**

- When an exact query returns lyrics but scores below 0.85, status becomes `NeedsCandidateSelection`, not `Error`.
- Same when an exact query returns lyrics but duration drifts beyond 2s.
- Instrumental tracks still enter the negative cache and display "No lyrics found".
- Genuine network faults, `429`, and database failures still report `Error` correctly.
- Add a case to `tests/test_lrcliblyricsadapter.cpp`: a low-scoring exact result triggers candidate search rather than `failed`.

## 相关问题 / Related Issues

- [001](001-cjk-text-score-always-zero.md) — 评分缺陷放大了本问题的触发频率 / The scoring defect greatly increases how often this triggers
- [003](003-artist-name-variance-drops-candidates.md) — 本问题修复后，候选列表仍可能为空 / Even after this fix the candidate list can still come back empty
