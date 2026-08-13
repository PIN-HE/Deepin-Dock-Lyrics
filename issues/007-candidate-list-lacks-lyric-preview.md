# 007 候选列表缺少歌词预览 / Candidate List Lacks Lyric Preview

| 项 / Field | 值 / Value |
|---|---|
| 严重度 / Severity | 中 / Medium |
| 状态 / Status | 待修复 / Open |
| 影响面 / Impact | 所有需手动选择候选的场景 / Every case requiring manual candidate selection |
| 位置 / Location | [common/lyrics-core/include/lyricscore/types.h:60-68](../common/lyrics-core/include/lyricscore/types.h#L60-L68)、[normalization.cpp:94-95](../common/lyrics-core/src/normalization.cpp#L94-L95) |
| 发现日期 / Found | 2026-08-13 |

## 症状 / Symptom

**中文**

候选列表仅展示曲名、艺人、专辑、时长与评分。当多个候选的元数据相近，或元数据写法与用户认知不一致时，用户无法判断哪一条是正确歌词。用户的实际判据是**歌词正文**——看到歌词内容才能确认匹配，而列表不提供该信息。

**English**

The candidate list shows only title, artist, album, duration, and score. When several candidates carry similar metadata, or when the metadata is written differently from what the user recognises, there is no way to tell which entry holds the right lyrics. The user's actual criterion is the **lyric body** — seeing the text is what confirms the match — and the list does not provide it.

## 复现依据 / Reproduction Evidence

**中文**

`LyricCandidate` 结构不含任何歌词内容字段：

**English**

The `LyricCandidate` struct carries no lyric-content field:

```cpp
// types.h:60-68
struct LyricCandidate {
    QString providerId;
    QString candidateId;
    QString title;
    QString artist;
    QString album;
    qint64  durationMs = -1;
    double  score = 0.0;
    // 无歌词正文字段 / no lyric body field
};
```

**中文**

而 `ProviderRecord` 在候选阶段**已经持有歌词正文**：

**English**

Yet `ProviderRecord` **already holds the lyric body** at candidate time:

```cpp
// lrclibprovider.cpp:33-34, recordFromJson()
record.payload.syncedLyrics = object.value(QStringLiteral("syncedLyrics")).toString();
record.payload.plainLyrics  = object.value(QStringLiteral("plainLyrics")).toString();
```

**中文**

`rankLyricCandidates` 在构造 `LyricCandidate` 时丢弃了这部分数据：

**English**

`rankLyricCandidates` discards that data when constructing `LyricCandidate`:

```cpp
// normalization.cpp:94-95
candidates.append({QStringLiteral("lrclib"), record.id, record.trackName,
                   record.artistName, record.albumName, record.durationMs, score});
                   // payload 未传递 / payload not carried through
```

**中文**

实测确认 LRCLIB 的 `/api/search` 响应本身即包含 `plainLyrics`：

**English**

Measurements confirm LRCLIB's `/api/search` response itself includes `plainLyrics`:

```
GET /api/search?q=夜曲 周杰伦
→ [{"id":30914663, "trackName":"夜曲", "artistName":"周杰伦",
    "plainLyrics":"一群嗜血的螞蟻 被腐肉所吸引 我面無表情 看孤獨的風景\n...", ...}]
```

**中文**

因此提供预览**无需额外网络请求**——数据已在手，只是被丢弃了。

**English**

Providing a preview therefore requires **no extra network request** — the data is already in hand and merely thrown away.

## 根因 / Root Cause

**中文**

设计假设是"元数据评分足以确定匹配"。该假设在元数据可靠时成立，但实践中不成立：

- 艺名写法差异使评分无法区分正确与不相干（见 [003](003-artist-name-variance-drops-candidates.md)，区分度仅 0.05）。
- 部分播放器上报的 album 为空或含版本标记（见 [006](006-album-param-always-sent.md)）。
- 同一曲目在 LRCLIB 上存在多条繁简写法不同的记录。

在这些情形下，元数据本身不足以支撑判断，而歌词正文是决定性的。

**English**

The design assumes metadata scoring is sufficient to determine a match. That holds when metadata is reliable, which in practice it is not:

- Artist-name variance leaves the score unable to separate correct from unrelated (see [003](003-artist-name-variance-drops-candidates.md), a margin of only 0.05).
- Some players report an empty or version-tagged album (see [006](006-album-param-always-sent.md)).
- The same track exists on LRCLIB as multiple records differing in script.

Under these conditions metadata cannot carry the decision, and the lyric body is decisive.

## 修复方案 / Fix Plan

**中文**

1. **`LyricCandidate` 新增 `previewText` 字段。** 存放歌词首 1–2 行，长度上限（建议 80 字符）以控制 D-Bus 载荷。

2. **`rankLyricCandidates` 填充该字段。** 从 `record.payload.syncedLyrics` 或 `plainLyrics` 提取首行，剥离 LRC 时间戳与元信息行（`[ar:]`、`[ti:]` 等）。

3. **`syncedLyrics` 为空但 `plainLyrics` 非空时同样提供预览。** 该情形下歌词不同步，但预览仍有助于判断。

4. **扩展 D-Bus 契约。** `CandidatesChanged` 信号的候选结构增加该字段。这是**破坏性变更**，须同步更新：
   - `lyricsdbusadapter.cpp` 的序列化
   - `settingsviewmodel.cpp` 的反序列化
   - `settingswindow.cpp` 的候选列表渲染
   - `LYRICS_DOCK_TECHNICAL_PLAN.md` 与 `DOCK_LYRICS_TECHNICAL_PLAN_ZH.md` 第 9 章的契约描述

5. **繁简统一后展示。** 预览文本须经 OpenCC `t2s` 转换再显示，与主歌词显示保持一致（当前 `lyricsservicecontroller.cpp:322-324` 已对主歌词做此处理）。

**English**

1. **Add a `previewText` field to `LyricCandidate`.** Hold the first one or two lyric lines with a length cap — 80 characters is a reasonable bound — to control D-Bus payload size.

2. **Populate it in `rankLyricCandidates`.** Take the first line from `record.payload.syncedLyrics` or `plainLyrics`, stripping LRC timestamps and metadata lines such as `[ar:]` and `[ti:]`.

3. **Provide a preview when `syncedLyrics` is empty but `plainLyrics` is not.** The lyrics are unsynced in that case, but the preview still aids the decision.

4. **Extend the D-Bus contract.** Add the field to the candidate structure in the `CandidatesChanged` signal. This is a **breaking change** requiring matching updates in:
   - serialisation in `lyricsdbusadapter.cpp`
   - deserialisation in `settingsviewmodel.cpp`
   - candidate list rendering in `settingswindow.cpp`
   - the contract described in section 9 of `LYRICS_DOCK_TECHNICAL_PLAN.md` and `DOCK_LYRICS_TECHNICAL_PLAN_ZH.md`

5. **Normalise script before display.** Run preview text through OpenCC `t2s` so it matches main lyric display, which `lyricsservicecontroller.cpp:322-324` already does.

## 隐私考量 / Privacy Considerations

**中文**

预览文本是歌词内容，属于 README 中日志允许清单明确拒绝的类别。须确认：

- 预览文本**不得写入日志**。现有 allowlist（`logengine.cpp`）会拒绝歌词内容，须验证新字段同样被拒绝而非绕过。
- 预览文本经 D-Bus 传输属正常功能路径，与日志不同，不受该限制。
- 不引入新的网络请求，故无新增出站数据。

**English**

Preview text is lyric content, a category the README's logging allowlist explicitly rejects. Confirm that:

- Preview text is **never written to logs**. The existing allowlist in `logengine.cpp` rejects lyric content; verify the new field is likewise rejected rather than slipping through.
- Preview text crossing D-Bus is a normal functional path, unlike logging, and is not subject to that restriction.
- No new network request is introduced, so there is no additional outbound data.

## 验收标准 / Acceptance Criteria

**中文**

- 候选列表每一条显示歌词首行预览。
- 预览文本长度受上限约束，超长截断且不破坏 UTF-8 字符边界。
- LRC 时间戳与元信息行不出现在预览中。
- 仅有 `plainLyrics` 的候选同样显示预览。
- 预览文本经繁简转换，与主歌词显示一致。
- 预览文本不出现在任何日志输出中。
- 不产生额外网络请求（请求数与修复前相同）。
- 新增 `tests/test_lyricscore.cpp` 用例：含时间戳与元信息行的 LRC 提取出正确首行。
- 新增 `tests/test_logengine.cpp` 用例：预览文本被日志 allowlist 拒绝。

**English**

- Every candidate row shows a first-line lyric preview.
- Preview text respects the length cap, truncating without breaking UTF-8 character boundaries.
- LRC timestamps and metadata lines do not appear in the preview.
- Candidates with only `plainLyrics` also show a preview.
- Preview text is script-normalised, consistent with main lyric display.
- Preview text appears in no log output.
- No additional network requests are made; the request count matches pre-fix behaviour.
- Add a case to `tests/test_lyricscore.cpp`: LRC containing timestamps and metadata lines yields the correct first line.
- Add a case to `tests/test_logengine.cpp`: preview text is rejected by the logging allowlist.

## 相关问题 / Related Issues

- [003](003-artist-name-variance-drops-candidates.md) — 元数据评分区分度不足，本问题提供人工判据补偿 / Metadata scoring lacks discrimination; this issue supplies a human criterion to compensate
- [002](002-exact-match-failure-reported-as-error.md) — 002 修复后候选列表使用频率上升，本问题的价值随之提高 / After 002 the candidate list is used far more often, raising this issue's value
